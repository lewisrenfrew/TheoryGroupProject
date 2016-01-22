/* ==========================================================================
   $File: grid.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "GlobalDefines.hpp"
#include "FDM.hpp"
#include "Grid.hpp"
#include "GradientGrid.hpp"
#include "Plot.hpp"

// NOTE(Chris): It's going to be a bit of work to get tests running
// again now
#ifdef CATCH_CONFIG_MAIN
#define LOG
#else
// NOTE(Chris): Provide logging for everyone - create in main TU to
// avoid ordering errors.
namespace Log
{
    Lethani::Logfile log;
}
#endif

#if 0
struct CommandLineFlags
{
    bool lastMatrix;
    std::string infofilepath;
};


CommandLineFlags ParseArguments()
{
    try
    {
        // Aguments are in order: discription, seperation char, version number.
        TCLAP::CmdLine cmd("Solving the electric and the voltage fields of a electrostatic prblem ",
                           ' ', Version::Gridle);

        // Aguments are in order: '-' flag, "--" flag,
        // discription,add to an object(cmd line), default state.
        TCLAP::SwitchArg lastMatrix("m", "lastMatrix", "Runs with the last matrix used",
                                    cmd,false);

        // Aguments are in order: '-' flag, "--" flag,
        // discription, default state, default string value , path, add to an object(cmd line).
        TCLAP::ValueArg<std::string> infofile("i" , "infofile","holds the information about the matrix",
                                              true, "", "path", cmd);

        CommandLineFlags ret;

        ret.lastMatrix = lastMatrix.getValue();
        ret.infofilepath = infofile.getValue();

        if(lastMatrix)
        {
            //TODO, get the previous matrix
        }
        else
        {
            //TODO,
        }


    }
    catch(TCLAP::ArgException& ex)
    {
        LOG("Parsing error %s for arg %s", ex.error().c_str(), ex.argId().c_str());
    }
}
#endif

#ifndef CATCH_CONFIG_MAIN
int main(void)
{
    // This looks better and far more generic
    std::unordered_map<u32, Constraint> colorMap;
    colorMap.emplace(Color::Black, std::make_pair(ConstraintType::CONSTANT, 0.0));
    colorMap.emplace(Color::Red, std::make_pair(ConstraintType::CONSTANT, 10.0));
    colorMap.emplace(Color::PaintRed, std::make_pair(ConstraintType::CONSTANT, 10.0));
    colorMap.emplace(Color::Blue, std::make_pair(ConstraintType::CONSTANT, -10.0));
    colorMap.emplace(Color::Green, std::make_pair(ConstraintType::LERP_HORIZ, 0.0));
    // Grid grid("prob1.png", colorMap);
    Grid grid;
    grid.LoadFromImage("prob0.png", colorMap, 8);

    // NOTE(Chris): Staggered leapfrog seems to follow a 1/(x^2) convergence, we can exploit this...
    // SolveGridLaplacianZero(&grid, 0.0001, 10000);
    FDM::SolveGridLaplacianZero(&grid, 0.001, 10000);

    const f64 cellsToMeters = 100.0;
    GradientGrid grad;
    grad.CalculateNegGradient(grid, cellsToMeters);

    using namespace Plot;

    WriteGridForGnuplot(grid);
    WriteGnuplotColormapFile(grid);
    WriteGnuplotContourFile(grid);

    WriteGradientGridForGnuplot(grad);
    WriteGnuplotGradientFile(grad, 0.08);

    return 0;
}
#endif
