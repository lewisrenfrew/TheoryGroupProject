/* ==========================================================================
   $File: main.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "GlobalDefines.hpp"
#include "FDM.hpp"
#include "MatrixInversion.hpp"
#include "AnalyticalGridFunctions.hpp"
#include "Compare.hpp"
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

enum class OperationMode
{
    SingleSimulation,
    CompareProblem0,
    CompareProblem1,
    CompareTwo
};

struct CommandLineFlags
{
    bool lastMatrix;
    OperationMode mode;
    std::vector<std::string> inputPaths;
};

static
CommandLineFlags
ParseArguments(const int argc, const char* argv[])
{
    try
    {
        using namespace TCLAP;

        // Aguments are in order: discription, seperation char, version number.
        CmdLine cmd("A tool for solving the electric and the voltage fields of an electrostatic problem.",
                           ' ', Version::VersionNumber);

        // Aguments are in order: '-' flag, "--" flag,
        // discription,add to an object(cmd line), default state.
        SwitchArg lastMatrix("m", "lastMatrix", "Runs with the last matrix used", false);


        // Aguments are in order: '-' flag, "--" flag,
        // discription, default state, default string value , path, add to an object(cmd line).
        ValueArg<std::string> infoFile("i" , "infofile","Holds the information about each matrix", false, "",
                                              "path");

        SwitchArg cmp0("0", "compareprob0", "Compares the input file against the analytical solution to problem 0", false);
        SwitchArg cmp1("1", "compareprob1", "Compares the input file against the analytical solution to problem 1", false);
        SwitchArg cmp2("c", "compare", "Compares the the two input files against each other", false);
        std::vector<Arg*> cmpArgs({&cmp0, &cmp1, &cmp2, &infoFile});
        cmd.xorAdd(cmpArgs);

        // TODO(Chris): Need params for analytical solutions - json?

        UnlabeledMultiArg<std::string> cmpNames("compNames", "Paths of the JSON definition files to compare", false, "File paths", cmd);

        cmd.parse(argc, argv);

        CommandLineFlags ret;

        ret.lastMatrix = lastMatrix.getValue();

        if (cmp0.getValue())
        {
            ret.mode = OperationMode::CompareProblem0;
            ret.inputPaths = cmpNames.getValue();
        }
        else if (cmp1.getValue())
        {
            ret.mode = OperationMode::CompareProblem1;
            ret.inputPaths = cmpNames.getValue();
        }
        else if (cmp2.getValue())
        {
            ret.mode = OperationMode::CompareTwo;
            ret.inputPaths = cmpNames.getValue();
        }
        else
        {
            ret.mode = OperationMode::SingleSimulation;
            ret.inputPaths = {infoFile.getValue()};
        }

        return ret;
    }
    catch(TCLAP::ArgException& ex)
    {
        LOG("Parsing error %s for arg %s", ex.error().c_str(), ex.argId().c_str());
    }
}

static
int
CompareProblem0(const std::vector<std::string>& paths)
{
    if (paths.size() != 1)
    {
        LOG("Expected 1 path");
        return EXIT_FAILURE;
    }

    auto cfg = Cfg::LoadGridConfigFile(paths.front().c_str());
    if (!cfg)
    {
        LOG("Cannot understand config file, exiting");
        return EXIT_FAILURE;
    }

    JasUnpack((*cfg), imagePath, zeroTol, scaleFactor, pixelsPerMeter, maxIter);

    Grid grid(cfg->horizZip.ValueOr(false), cfg->verticZip.ValueOr(false));
    grid.LoadFromImage(imagePath.c_str(), cfg->constraints, scaleFactor.ValueOr(1));

    //FDM::SolveGridLaplacianZero(&grid, zeroTol.ValueOr(0.001), maxIter.ValueOr(20000));
    MatrixInversion::MatrixInversionMethod(&grid, zeroTol.ValueOr(0.001) ,maxIter.ValueOr(20000));

    GradientGrid gradGrid;
    const f64 ppm = pixelsPerMeter.ValueOr(100.0);
    gradGrid.CalculateNegGradient(grid, ppm);

    // const f64 bigRad = 298.0/pixelsPerMeter.ValueOr(100.0);
    // const f64 smallRad = 20.0/pixelsPerMeter.ValueOr(100.0);
    const f64 bigRad = cfg->analyticOuter.ValueOr(298.0) / ppm;
    const f64 smallRad = cfg->analyticInner.ValueOr(20.0) / ppm;

    Grid analytic = AGF::AnalyticalGridFill0(grid.lineLength, grid.numLines, 10.0,
                                             bigRad,
                                             smallRad,
                                             scaleFactor.ValueOr(1) * ppm);
    Jasnah::Option<Grid> diff = Cmp::Difference(grid, analytic);

    if (!diff)
    {
        return EXIT_FAILURE;
    }

    using namespace Plot;

    WriteGridForGnuplot(grid);
    WriteGnuplotColormapFile(grid);
    WriteGnuplotContourFile(grid);

    WriteGridForGnuplot(*diff, "Plot/GridDiff.dat");
    WriteGnuplotColormapFile(*diff, "Plot/GridDiff.dat", "Plot/GridDiff.gpi");

    WriteGridForGnuplot(analytic, "Plot/GridAnalytic.dat");
    WriteGnuplotColormapFile(analytic, "Plot/GridAnalytic.dat", "Plot/GridAnalytic.gpi");
    WriteGnuplotContourFile(analytic);

    // WriteGridForGnuplot(analytic);
    // WriteGnuplotColormapFile(analytic);
    // WriteGnuplotContourFile(analytic);
    return EXIT_SUCCESS;
}

static
int
CompareProblem1(const std::vector<std::string>& paths)
{
    if (paths.size() != 1)
    {
        LOG("Expected 1 path");
        return EXIT_FAILURE;
    }

    auto cfg = Cfg::LoadGridConfigFile(paths.front().c_str());
    if (!cfg)
    {
        LOG("Cannot understand config file, exiting");
        return EXIT_FAILURE;
    }

    JasUnpack((*cfg), imagePath, zeroTol, scaleFactor, pixelsPerMeter, maxIter);

    Grid grid(cfg->horizZip.ValueOr(false), cfg->verticZip.ValueOr(false));
    grid.LoadFromImage(imagePath.c_str(), cfg->constraints, scaleFactor.ValueOr(1));

    //FDM::SolveGridLaplacianZero(&grid, zeroTol.ValueOr(0.001), maxIter.ValueOr(20000));
    MatrixInversion::MatrixInversionMethod(&grid, zeroTol.ValueOr(0.001) ,maxIter.ValueOr(20000)); 

    GradientGrid gradGrid;
    const f64 ppm = pixelsPerMeter.ValueOr(100.0);
    gradGrid.CalculateNegGradient(grid, ppm);

    // NOTE(Chris): Since we need bigRad to equal the width in number
    // of the image in px, we may as well just use that number here
    const f64 bigRad = grid.lineLength / (2.0*ppm);
    const f64 smallRad = cfg->analyticInner.ValueOr(50.0) / ppm;

    // NOTE(Chris): Small
    // const f64 bigRad = 25.0 / pixelsPerMeter.ValueOr(100.0);
    // const f64 smallRad = 3.0 / pixelsPerMeter.ValueOr(100.0);

    Grid analytic = AGF::AnalyticalGridFill1(grid.lineLength, grid.numLines, 10.0,
                                             bigRad,
                                             smallRad,
                                             scaleFactor.ValueOr(1) * ppm);
    Jasnah::Option<Grid> diff = Cmp::Difference(grid, analytic);

    if (!diff)
    {
        return EXIT_FAILURE;
    }

    using namespace Plot;

    WriteGridForGnuplot(grid);
    WriteGnuplotColormapFile(grid);
    WriteGnuplotContourFile(grid);

    WriteGridForGnuplot(*diff, "Plot/GridDiff.dat");
    WriteGnuplotColormapFile(*diff, "Plot/GridDiff.dat", "Plot/GridDiff.gpi");

    WriteGridForGnuplot(analytic, "Plot/GridAnalytic.dat");
    WriteGnuplotColormapFile(analytic, "Plot/GridAnalytic.dat", "Plot/GridAnalytic.gpi");
    WriteGnuplotContourFile(analytic, "Plot/GridAnalytic.dat", "Plot/ContourAnalytic.gpi");

    return EXIT_SUCCESS;
}

static
int
CompareTwo(const std::vector<std::string>& paths)
{
    if (paths.size() != 2)
    {
        LOG("Expected 2 paths");
        return EXIT_FAILURE;
    }

    LOG("Not yet implemented");
    return EXIT_SUCCESS;

}

static
int
SingleSimulation(const std::string& path)
{
    auto cfg = Cfg::LoadGridConfigFile(path.c_str());
    if (!cfg)
    {
        LOG("Cannot understand config file, exiting");
        return EXIT_FAILURE;
    }

    JasUnpack((*cfg), imagePath, zeroTol, scaleFactor, pixelsPerMeter, maxIter);

    Grid grid(cfg->horizZip.ValueOr(false), cfg->verticZip.ValueOr(false));
    grid.LoadFromImage(imagePath.c_str(), cfg->constraints, scaleFactor.ValueOr(1));

    //FDM::SolveGridLaplacianZero(&grid, zeroTol.ValueOr(0.001), maxIter.ValueOr(20000));
     MatrixInversion::MatrixInversionMethod(&grid, zeroTol.ValueOr(0.001) ,maxIter.ValueOr(20000));

    GradientGrid gradGrid;
    gradGrid.CalculateNegGradient(grid, pixelsPerMeter.ValueOr(100.0));

    using namespace Plot;

    WriteGridForGnuplot(grid);
    WriteGnuplotColormapFile(grid);
    WriteGnuplotContourFile(grid);
    WriteGradientFiles(gradGrid);

    return EXIT_SUCCESS;
}

#ifndef CATCH_CONFIG_MAIN
int main(int argc, const char* argv[])
{
    auto args = ParseArguments(argc, argv);

    int result = EXIT_SUCCESS;
    switch (args.mode)
    {
    case OperationMode::CompareProblem0:
    {
        result = CompareProblem0(args.inputPaths);
    } break;

    case OperationMode::CompareProblem1:
    {
        result = CompareProblem1(args.inputPaths);
    } break;

    case OperationMode::CompareTwo:
    {
        result = CompareTwo(args.inputPaths);
    } break;

    case OperationMode::SingleSimulation:
    {
        result = SingleSimulation(args.inputPaths.front());
    } break;

    default:
        LOG("Unknown mode");

    }

    return result;
}
#endif
