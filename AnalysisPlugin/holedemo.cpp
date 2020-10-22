#include "holedemo.h"
#include "gscorecommon.h"
#include "gscorefwd.h"
#include "modelfitting.h"
#include "imageproc.h"
#include "imageio.h"
#include "geometry.h"
#include "utilities.h"
#include "vectormath.h"

//  ----------------------------------------------------------------------------
//  HoleDemo
//  ----------------------------------------------------------------------------
//      INPUTS
//
//      PARAMS
//
//      OUTPUTS
//
//

namespace {


gs::ImageF makeCircle(int tempsz, double circrdpx)
{
	const auto sz = 2*tempsz + 1;
	gs::ImageF cmask(gs::SizeI(sz,sz), 0.0f);

	for (int y = -tempsz; y <= tempsz; ++y) {
		auto yy = static_cast<double>(y);
		for (int x = -tempsz; x <= tempsz; ++x) {
			auto xx = static_cast<double>(x);
			auto v = 0.0;
			if (xx*xx + yy*yy < circrdpx*circrdpx) {
				v = -std::exp( -(xx*xx + yy*yy) / (circrdpx*circrdpx) );
			}
			cmask.setpel(y+tempsz, x+tempsz, v);
		}
	}

	return cmask;
}

bool inside(const gs::PointD& pt, const gs::SizeI& sz)
{
	return (pt.x >= 0.0 && pt.y >= 0.0 && pt.x < sz.width-1 && pt.y < sz.height-1);
}

} // namespace


namespace demo
{


gs::CircleD HoleDemo::initialEstimate(
    const gs::HeightMap& hm, const gs::CircleD& cinit, const gs::AnalysisContext&  ctx)
{
	const auto radpx = cinit.radius();
	const auto res = hm.resolution();

	// Resize image so that the fastener matches the circle detection template
	auto sc = m_circszpx / (2*radpx);
	auto smallhm = gs::math::imresize(hm, sc, ctx.taskInfo());

	// Make template
	const auto circrdpx = m_circszpx / 2.0;
	const auto tempsz = static_cast<int>(std::round(circrdpx * 1.1));

	// Make template bigger than the circle size with circle in the middle
	auto tmpl = makeCircle(tempsz, circrdpx);

	// Normalized cross-correlation of gradient magnitude with template
	auto xc = gs::math::normxcorr2(tmpl, smallhm, ctx.taskInfo());

	// Circle center in xc space
	const auto circx = cinit.centerX()*sc + tempsz;
	const auto circy = cinit.centerY()*sc + tempsz;
	const auto circr = cinit.radius()*sc;

	// Weight normalized cross-correlation by circle location
	for (int y = 0; y < xc.height(); ++y) {
        ctx.throwIfCanceled();
		auto yy = static_cast<double>(y) - circy;
		for (int x = 0; x < xc.width(); ++x) {
			auto xx = static_cast<double>(x) - circx;
			auto wt = std::exp( -(xx*xx + yy*yy)/(16*circr*circr));
			auto v = xc.getpel(y,x);
			xc.setpel(y,x,v*wt);
		}
	}

	// Find peak and map back to resized image coordinates
	float mnv, mxv;
	gs::PointI mnloc, mxloc;
	gs::math::minMaxLoc(xc, mnv, mxv, mnloc, mxloc);

	auto cx = mxloc.x - std::floor(tmpl.width()/2);
	auto cy = mxloc.y - std::floor(tmpl.height()/2);
	
	// Resize coordinates to full image and return circle
	return gs::CircleD(cx/sc, cy/sc, radpx);
}


gs::PointDs HoleDemo::refineEstimate(
    const gs::HeightMap& hm, 
    const gs::CircleD& cinit,
    const gs::AnalysisContext&  ctx)
{
	const auto piv = gs::pi<double>();
	const auto mmpp = hm.resolution();
	const auto sz = hm.size();

	const auto ntheta = static_cast<int>(std::round( 2.0*piv*cinit.radius()*mmpp / m_circlesmpmm ));
	// Add 1 because we don't need 2*pi
	gs::Doubles thetas;
    gs::math::linspace(0.0, 2.0*piv, ntheta+1, thetas);


	// Use edge width as distance from center of derivative filter to edge
	auto ewpx = static_cast<int>(std::round(m_edgewidthmm / mmpp));

    auto kd = gs::math::DerivativeOfGaussianKernel<double>(ewpx/6.0);
	ewpx = static_cast<int>((kd.size()-1)/2);
	// Normalize
	auto mxv = gs::math::max(kd);
	std::transform(std::begin(kd), std::end(kd), std::begin(kd), [&mxv](double v) { return v/mxv;});

	const auto marginpx = static_cast<int>(std::round(std::min(cinit.radius(), m_edgesearchmm / mmpp / 2.0)));
	//std::cout << "marginpx: " << marginpx << ", ewpx: " << ewpx << std::endl;

	// Slope threshold
	const auto slpthr = -std::tan(m_slopeangle/180.0*piv);
	//std::cout << "slpthr: " << slpthr << std::endl;

	gs::PointDs edgepts;
	edgepts.reserve(ntheta);

	const auto np = 2*(marginpx+ewpx) + 1;
	const auto rad = cinit.radius();
	for (int i = 0; i < ntheta; ++i) {
        ctx.throwIfCanceled();
		const auto th = thetas[i];

		// Profile start and end
		const auto p0 = gs::PointD(cinit.x() + (rad-marginpx-ewpx)*std::cos(th), 
			                       cinit.y() + (rad-marginpx-ewpx)*std::sin(th));

		const auto p1 = gs::PointD(cinit.x() + (rad+marginpx+ewpx)*std::cos(th),
                                   cinit.y() + (rad+marginpx+ewpx)*std::sin(th));

		// If either are outside the image, skip
		if (!(inside(p0, sz) && inside(p1, sz))) {
			continue;
		}

		// Sample from outside to inside
		gs::PointDs pts;
		gs::math::linspace(p1, p0, np, pts);


		// Extract profile
		gs::Doubles zvals, tvals;
        zvals.reserve(np);
		tvals.reserve(np);
        
        for (auto&& p : pts) {
			auto zv = gs::math::interpbicubic(hm, p.y, p.x);
			zvals.push_back(zv);
			tvals.push_back(mmpp*dist(p, p1));
		}

		// Calculate gradient
		auto dz = gs::math::conv(zvals, kd);
		auto dt = gs::math::conv(tvals, kd);
		//std::cout << "zval size: " << zvals.size() << ", dz size: " << dz.size() << std::endl;

		// Calculate max point (cropping by ewpx to avoid filter edge)
		auto mnix = ewpx;
		auto mndz = dz[mnix];
		for (int x = 0; x < 2*marginpx+1; ++x) 
		{
			const auto v = dz[x + ewpx];
			if (v < mndz) {
				mndz = v;
				mnix = x+ewpx;
			}
		}
		//std::cout << "mnix: " << mnix << std::endl;

		// Adjust for slope - find first location below threshold
		auto slp = dz[mnix]/dt[mnix];
		//std::cout << "t: " << tvals[0] << " " << tvals[1] << std::endl;
		//std::cout << "slp at mnix: " << dz[mnix] << " " << dt[mnix] << " " << slp << std::endl;
		if (std::abs(dt[mnix]) > 1e-3 && slp < slpthr) {
			for (int x = ewpx; x < mnix; ++x) {
				slp = dz[x]/dt[x];
				if (std::abs(dt[x]) > 1e-3 && slp < slpthr) {
					mnix = x;
					break;
				}
			}
		}


		edgepts.push_back(pts[mnix]);
	}


	return edgepts;
}






void HoleDemo::analyzeImpl(const gs::AnalysisContext& ctx)
{
	// Algorithm parameters
	m_circszpx     = 35;        // Size of circle detection template
	m_filtsgpx     = 0.75;      // Filter sigma in pixels
	m_edgewidthmm  = 0.05;
	m_edgesearchmm = 4.0;
	m_circlesmpmm  = 0.050;     // Circle sampling in mm
	m_slopeangle   = getDouble(PK_SLOPE);

    ctx.throwIfCanceled();


    // validate
    validateOrThrow(ctx);
    
	const auto& hm = ctx.heightMap();
    if (hm.empty())
        ThrowError(gs::Error::InvalidFile, "Heightmap is empty");

	const auto offset = gs::SizeD(hm.offset());

    // Get ROI
    const auto wd = ctx.size().width;
    const auto ht = ctx.size().height;
	const auto mmpp = ctx.resolution();

	auto diametermm = getLength(PK_ESTDIAMETER).asMm();
    const auto shapeid = getInt(PK_PRIMARYSHAPEID);

	// Get first circle
	gs::CircleD cinit;
	gs::CirclePtr cptr;
	if (ctx.findShapeT(shapeid, cptr)) {
		cinit = cptr->circle();

		// Need to shift if heightmap is cropped
		auto ct = cinit.center();
		cinit.setCenter(ct - offset);
	}

	// If circle was not specified, use specified diameter
	if (cinit.radius() < 0.05) {
		cinit.setCenter(gs::PointD(wd/2.0, ht/2.0));
		cinit.setRadius(diametermm/2.0/mmpp);
	}
	
	cinit = initialEstimate(hm, cinit, ctx);
	auto edgepts = refineEstimate(hm, cinit, ctx);

    if (edgepts.size() < 10)
        gs::ThrowError(gs::Error::InvalidInput, "Unable to find circle:  Not enough points");

	
	// L1 circle fit
    gs::CircleD circ(cinit);
    gs::math::FitShapeSettings st;
    st.fitnrm = gs::math::FitNorm::L1;
	auto b = gs::math::TryFitCircle(edgepts, st, circ);

	if (!b)
		circ = cinit;


	// Shift circ back to full image coordinates
	auto ct = circ.center();
	circ.setCenter(ct + offset);

	set(PK_DIAMETER,    gs::Length(2.0*circ.radius()*mmpp, gs::LengthUnit::MM));
	set(PK_CIRCLE,      circ);
}

}
