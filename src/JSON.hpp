// -*- c++ -*-
#if !defined(JSON_H)
/* ==========================================================================
   $File: JSON.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define JSON_H
#include "GlobalDefines.hpp"
#include "Jasnah.hpp"
#include "Grid.hpp"
#include <string>
#include <unordered_map>

namespace Cfg
{
    enum class CalculationMode
    {
        FiniteDiff,
        MatrixInversion,
        SOR,
        AMR
    };

    struct GridConfigData
    {
        std::string imagePath;
        std::unordered_map<u32, Constraint> constraints;
        Jasnah::Option<uint> scaleFactor;
        Jasnah::Option<f64> pixelsPerMeter;
        Jasnah::Option<u64> maxIter;
        Jasnah::Option<f64> zeroTol;
        Jasnah::Option<bool> horizZip;
        Jasnah::Option<bool> verticZip;
        Jasnah::Option<f64> analyticInner;
        Jasnah::Option<f64> analyticOuter;
        Jasnah::Option<CalculationMode> mode;
    };

    struct JSONPreprocConfigVars
    {
        std::string imgPath;
        std::vector<RGBA> colorMap;
    };

    /// Loads the config file path, returns an Optional set of data,
    /// none indicating that there was an error
    Jasnah::Option<GridConfigData>
    LoadGridConfigFile(const char* path);

    bool
    WriteJSONPreprocFile(const JSONPreprocConfigVars& vars);
}

#endif
