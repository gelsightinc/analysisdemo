// demoplugin.cpp : Defines the exported functions for the DLL application.
//

#include "gsanalysisroutine.h"
#include "gscorecommon.h"
#include "stdutil.h"
#include "analysisroutineimpl.h"
#include "analysisplugin.h"
#include "propnames.h"

#include "holedemo.h"

#include "plugindecl.h"

#include <string>
#include <memory>


namespace demo
{

class DemoPlugin : public ::gs::AnalysisPlugin
{
protected:

    std::map<std::string, std::function<gs::RoutinePtr()>, su::StrLessNcA> m_factory;
public:
    DemoPlugin();
    virtual ~DemoPlugin() {}

    virtual void              initialize() const override {}
    virtual std::string       name() const override;
    virtual gs::Strings       routines() const override;
    virtual gs::RoutinePtr    newRoutine(const std::string& name) const override;

};

DemoPlugin::DemoPlugin()
{
      m_factory["HoleDemo"] = HoleDemo::Create;

      // Add other analysis routines to the map
      //m_factory["HoleDemo"] = HoleDemo::Create;

}

std::string     DemoPlugin::name() const
{
    return std::string("Analysis Demo Plugin");
}

gs::Strings       DemoPlugin::routines() const
{
    gs::Strings out;
    try
    {
        out.reserve(m_factory.size());

        for (auto&& pr : m_factory)
            out.push_back(pr.first);
    }
    catch (std::exception& ex)
    {
    	std::cerr << "Analysis Plugin failure " << ex.what();
    }

    return out;
}

gs::RoutinePtr DemoPlugin::newRoutine(const std::string& name) const
{
    auto it = m_factory.find(name);

    if (m_factory.end() == it)
        gs::ThrowError(gs::Error::ItemNotFound, "No such routine " + name);
    
    const auto routine = it->second.operator()();
    if (!routine)
        gs::ThrowError(gs::Error::Failed, "No creation function for routine " + name);

    return routine;
}


}


//
// The analysis routine loader in GelSightSdk looks for a dll that has the suffix _v142 (the build suffix)
// Then after loading for the dll, it checks for this function
//
extern "C" PluginApi const ::gs::AnalysisPlugin* GetAnalysisPlugin(std::string& errMsg)
{
	try
	{
		static demo::DemoPlugin plugin;
		return &plugin;
	}
	catch (std::exception& ex)
	{
		errMsg = ex.what() ? ex.what() : "Unknown error";
	}

	return nullptr;

}










