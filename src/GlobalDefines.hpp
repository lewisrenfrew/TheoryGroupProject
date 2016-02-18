// -*- c++ -*-
#if !defined(GLOBALDEFINES_H)
/* ==========================================================================
   $File: GlobalDefines.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define GLOBALDEFINES_H
#include "Types.h"
#include "Debug/Debug.hpp"

class JsonOutputStream;
class AnalyticsDaemon;

/// Globally available logging objects -- some accesible via singleton
namespace Log
{
    extern Lethani::Logfile log;
    JsonOutputStream& GetJsonOutStream();
    AnalyticsDaemon& GetAnalytics();
}

/// Header for time structure -- impl in main
struct TimedFunction
{
public:
    TimedFunction(const char* fnName);

    ~TimedFunction();
private:
    const char* fn_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_;
};

/// Macros for timing functions, insert TIME_FUNCTION() at the start
/// of the function you want to time.
#define TIME_FUNCTION_IMPL(s, l) TimedFunction t_##l(s)
#define TIME_FUNCTION() TIME_FUNCTION_IMPL(__FUNCTION__, __LINE__)

/// Certain hard-coded useful colours, primarily used in development
namespace Color
{
    // NOTE(Chris): Modern computers are almost all little-endian i.e.
    // AABBGGRR
    constexpr const u32 White = 0xFFFFFFFF;
    constexpr const u32 Red = 0xFF0000FF;
    constexpr const u32 PaintRed = 0XFF241CED;
    constexpr const u32 Green = 0xFF00FF00;
    constexpr const u32 Blue = 0xFFFF0000;
    constexpr const u32 Black = 0xFF000000;
}

/// Program Info
namespace Version
{
    constexpr const char* ProgramName = "Gridle";
    constexpr const char* DefaultLog = "Gridle.log";
    constexpr const char* VersionNumber = "0.0.1";
}

/// Necessary for logging timed functions
#include "OutputStream.hpp"
#endif
