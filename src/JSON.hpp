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
    /// Enum for the method to use for a certain calcuation
    enum class CalculationMode
    {
        FiniteDiff,
        MatrixInversion,
        RedBlack,
        GaussSeidel,
    };

    /// Mode the program is operating. The entire program is
    /// essentially a state machine
    enum class OperationMode
    {
        SingleSimulation,
        CompareProblem0,
        CompareProblem1,
        CompareTwo,
        Preprocess
    };

    /// Holds the data parsed from a JSON config file, much of it is
    /// optional but we let each mode handle its own defaults and
    /// simply return None.
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

    /// This is the data we output when asked to preprocess an image
    /// for the GUI
    struct JSONPreprocConfigVars
    {
        std::string imgPath;
        std::vector<RGBA> colorMap;
    };

    /// Loads the config file path, returns an Optional set of data,
    /// none indicating that there was an error
    Jasnah::Option<GridConfigData>
    LoadGridConfigFile(const char* path);

    /// Same as above but parses a string
    Jasnah::Option<GridConfigData>
    LoadGridConfigString(const std::string& data);

    /// Writes the Preprocessed data to stdout, returns true on success
    bool
    WriteJSONPreprocFile(const JSONPreprocConfigVars& vars);
}

#endif
