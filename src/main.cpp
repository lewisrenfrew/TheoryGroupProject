/* ==========================================================================
   $File: main.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "GlobalDefines.hpp"
#include "FDM.hpp"
#include "Grid.hpp"
#include "GradientGrid.hpp"
#include "Plot.hpp"
#include "JSON.hpp"
#include <tclap/CmdLine.h>

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

#if 1
struct CommandLineFlags
{
    bool lastMatrix;
    std::string infofilepath;
};


CommandLineFlags ParseArguments(int argc, const char* argv[])
{
    try
    {
        using namespace TCLAP;

        // Aguments are in order: discription, seperation char, version number.
        CmdLine cmd("A tool for solving the electric and the voltage fields of an electrostatic problem.",
                           ' ', Version::VersionNumber);

        // Aguments are in order: '-' flag, "--" flag,
        // discription,add to an object(cmd line), default state.
        TCLAP::SwitchArg lastMatrix("m", "lastMatrix", "Runs with the last matrix used",
                                    cmd,false);

        // Aguments are in order: '-' flag, "--" flag,
        // discription, default state, default string value , path, add to an object(cmd line).
        ValueArg<std::string> infofile("i" , "infofile","holds the information about the matrix",
                                              true, "", "path", cmd);

        cmd.parse(argc, argv);

        CommandLineFlags ret;

        ret.lastMatrix = lastMatrix.getValue();
        ret.infofilepath = infofile.getValue();

        if(ret.lastMatrix)
        {
            //TODO, get the previous matrix
        }
        else
        {
            //TODO,
        }

        return ret;

    }
    catch(TCLAP::ArgException& ex)
    {
        LOG("Parsing error %s for arg %s", ex.error().c_str(), ex.argId().c_str());
    }
}
#endif

#ifndef CATCH_CONFIG_MAIN
int main(int argc, const char* argv[])
{
    auto args = ParseArguments(argc, argv);
    auto cfg = Cfg::LoadGridConfigFile(args.infofilepath.c_str());
    if (!cfg)
    {
        LOG("Cannot understand config file, exiting");
        return EXIT_FAILURE;
    }

    JasUnpack((*cfg), imagePath, zeroTol, scaleFactor, pixelsPerMeter, maxIter);

    Grid grid;
    grid.LoadFromImage(imagePath.c_str(), cfg->constraints, scaleFactor.ValueOr(1));

    // NOTE(Chris): Staggered leapfrog seems to follow a 1/(x^2) convergence, we can exploit this...
    FDM::SolveGridLaplacianZero(&grid, zeroTol.ValueOr(0.001), maxIter.ValueOr(20000));

    GradientGrid grad;
    grad.CalculateNegGradient(grid, pixelsPerMeter.ValueOr(100.0));

    using namespace Plot;

    WriteGridForGnuplot(grid);
    WriteGnuplotColormapFile(grid);
    WriteGnuplotContourFile(grid);

    WriteGradientGridForGnuplot(grad);
    WriteGnuplotGradientFile(grad, 0.08);

    return EXIT_SUCCESS;
}
#endif
