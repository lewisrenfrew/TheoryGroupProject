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

namespace Log
{
    extern Lethani::Logfile log;
}

namespace Version
{
    constexpr const char* ProgramName = "Gridle";
    constexpr const char* DefaultLog = "Gridle.log";
    constexpr const char* VersionNumber = "0.0.1";
}

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

#endif
