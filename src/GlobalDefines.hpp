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

#endif
