#pragma once

#ifndef PLUGINDECL_H
#define PLUGINDECL_H

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the PluginApi
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// GsInternalApi functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.

#if defined _WIN32 || defined __CYGWIN__

#   ifdef PLUGIN_EXPORTS
#       ifdef __GNUC__
#           define PluginApi __attribute__ ((dllexport))
#       else
#           define PluginApi __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#       endif
#   else
#       ifdef __GNUC__
#           define PluginApi __attribute__ ((dllimport))
#       else
#           define PluginApi __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#       endif
#   endif

#   define DLL_LOCAL

#else

#   if __GNUC__ >= 4
#       define PluginApi __attribute__ ((visibility ("default")))
#       define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#   else
#       define PluginApi
#       define DLL_LOCAL
#   endif

#endif


#endif 
