#pragma once
#ifndef HOLEDEMO_H
#define HOLEDEMO_H

#include "gscorefwd.h"
#include "gsanalysisroutine.h"
#include "analysisroutineimpl.h"
#include "propnames.h"

//
//

namespace demo
{

class HoleDemo : public gs::AnalysisRoutineImplT<HoleDemo>
{
public:
   
	// Required AnalysisRoutine overrides;
	virtual void        analyzeImpl(const gs::AnalysisContext& ctx) override;

	// Define the parameters that we expose to the outside world
	BEGIN_GS_PROPERTIES(HoleDemo, "HoleDemo", "Hole Demo")
		PARAM_BOUNDED("estdiameter",    "Estimated Diameter",   double,    5.400,      gs::Unit::MM,      0.010, std::numeric_limits<double>::max())
		PARAM_BOUNDED("slope",          "Edge slope",           double,    30,         gs::Unit::Degree,  0.000, 90.0)
	    PROP_ENTRY   ("primaryshapeid", "Input Circle",         int,       -1,         gs::Unit::None)

		// Outputs
		DECLARE_OUTPUT("diameter",    "Diameter",       gs::Length,   gs::Unit::MM)
		DECLARE_OUTPUT_EX("circle",   "Circle",         gs::CircleD,  gs::Unit::Pixel, gs::Usage::Any, gs::Aspect::Image)
	END_GS_PROPERTIES()

	BEGIN_GS_INPUTS()
		INPUT_REQUIRED("Heightmap", gs::HeightMap)
        INPUT_SHAPE_OPTIONAL_EX("Circle", PK_PRIMARYSHAPEID, SM_CIRCLE, PK_CIRCLE)
	END_GS_INPUTS()

	DECLARE_REQUIRES_INITIAL_PARAMETERS();

private:

	gs::CircleD initialEstimate(const gs::HeightMap& hm, const gs::CircleD& cinit, const gs::AnalysisContext&  ctx);
	gs::PointDs refineEstimate(const gs::HeightMap& hm, const gs::CircleD& cinit, const gs::AnalysisContext&  ctx);

	// Algorithm settings
	double m_circszpx;
	double m_filtsgpx;
	double m_edgewidthmm;
	double m_edgesearchmm;
	double m_circlesmpmm;
	double m_slopeangle;

};


} // namespace demo

#endif
