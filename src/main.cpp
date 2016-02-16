/* ==========================================================================
   $File: main.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "GlobalDefines.hpp"
#include "FDM.hpp"
#include "FDMwithSOR.hpp"
#include "MatrixInversion.hpp"
#include "AnalyticalGridFunctions.hpp"
#include "Compare.hpp"
#include "Grid.hpp"
#include "GradientGrid.hpp"
#include "Plot.hpp"
#include "JSON.hpp"
#include "OutputStream.hpp"
#include "Utility.hpp"
#include <tclap/CmdLine.h>
#include <iostream>

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


TimedFunction::TimedFunction(const char* fnName) :  fn_(fnName)
{
    start_  = std::chrono::high_resolution_clock::now();
}

TimedFunction::~TimedFunction()
{
    end_  = std::chrono::high_resolution_clock::now();
    auto diff = end_ - start_;
    Log::GetAnalytics().
        ReportTimedFunction(fn_,
                            std::chrono::duration_cast<std::chrono::milliseconds>(diff));
}

struct CommandLineFlags
{
    // bool lastMatrix;
    bool jsonStdin;
    bool guiMode;
    Cfg::OperationMode mode;
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
        // We're not using this for now
        // SwitchArg lastMatrix("m", "lastMatrix", "Runs with the last matrix used", false);
        SwitchArg jsonStdin("j", "jsonStdin",
                            "Read json from stdin, complete objects separated by \"EOF\\n\"",
                            cmd, false);
        SwitchArg gui("g", "gui", "Run in gui mode (output json on stdout)", cmd, false);


        // Aguments are in order: '-' flag, "--" flag,
        // discription, default state, default string value , path, add to an object(cmd line).
        SwitchArg infoFile("i" , "infofile","JSON file providing simulation info", false);

        SwitchArg cmp0("0", "compareprob0", "Compares the input file against the analytical solution to problem 0", false);
        SwitchArg cmp1("1", "compareprob1", "Compares the input file against the analytical solution to problem 1", false);
        SwitchArg cmp2("c", "compare", "Compares the the two input files against each other", false);
        SwitchArg preprocess1("E", "preprocess",
                              "Preprocesses the image outputting the json to be filled in with required info",
                              false);
        std::vector<Arg*> cmpArgs({&cmp0, &cmp1, &cmp2, &infoFile, &preprocess1});
        cmd.xorAdd(cmpArgs);

        // TODO(Chris): Need params for analytical solutions - json?

        UnlabeledMultiArg<std::string> cmpNames("compNames", "Paths of the JSON definition files to compare", false, "File paths", cmd);

        cmd.parse(argc, argv);

        CommandLineFlags ret;

        // ret.lastMatrix = lastMatrix.getValue();

        if (cmp0.getValue())
        {
            ret.mode = Cfg::OperationMode::CompareProblem0;
        }
        else if (cmp1.getValue())
        {
            ret.mode = Cfg::OperationMode::CompareProblem1;
        }
        else if (cmp2.getValue())
        {
            ret.mode = Cfg::OperationMode::CompareTwo;
        }
        else if (preprocess1.getValue())
        {
            ret.mode = Cfg::OperationMode::Preprocess;
        }
        else
        {
            ret.mode = Cfg::OperationMode::SingleSimulation;
        }
        ret.inputPaths = cmpNames.getValue();
        ret.jsonStdin = jsonStdin.getValue();

        if (jsonStdin.getValue())
        {
            ret.inputPaths.resize(0);
            std::string json;
            std::string in;
            while (std::cin >> in)
            {
                if (in == "EOF")
                {
                    ret.inputPaths.push_back(json);
                    json = "";
                }
                else
                {
                    json += in;
                }
            }
            ret.inputPaths.push_back(json);
        }

        ret.guiMode = gui.getValue();

        return ret;
    }
    catch(TCLAP::ArgException& ex)
    {
        LOG("Parsing error %s for arg %s", ex.error().c_str(), ex.argId().c_str());
    }
}

// TODO(Chris): Could solve this with polymorphism, but we would still need instances of each
static
void
DispatchSolver(Jasnah::Option<Cfg::CalculationMode> mode, Grid* grid, f64 zeroTol, f64 maxIter)
{

    if (!mode)
    {
        LOG("Using FDM");
        FDM::SolveGridLaplacianZero(grid, zeroTol, maxIter);
        return;
    }

    switch (*mode)
    {
    case Cfg::CalculationMode::FiniteDiff:
    {
        FDM::SolveGridLaplacianZero(grid, zeroTol, maxIter);
    } break;

    case Cfg::CalculationMode::MatrixInversion:
    {
        MatrixInversion::MatrixInversionMethod(grid, zeroTol, maxIter);
    } break;

    case Cfg::CalculationMode::SOR:
    {
        // LOG("Not yet implemented");
        SOR::SolveGridLaplacianZero(grid, zeroTol, maxIter);
    } break;

    case Cfg::CalculationMode::AMR:
    {
        LOG("Not yet implemented");
    } break;
    }
}

static
int
CompareProblem0(const bool pathsAreJson, const std::vector<std::string>& paths)
{
    if (paths.size() != 1)
    {
        LOG("Expected 1 path");
        return EXIT_FAILURE;
    }

    Jasnah::Option<Cfg::GridConfigData> cfg;

    if (pathsAreJson)
    {
        cfg = Cfg::LoadGridConfigString(paths.front());
    }
    else
    {
        cfg = Cfg::LoadGridConfigFile(paths.front().c_str());
    }

    if (!cfg)
    {
        LOG("Cannot understand config file, exiting");
        return EXIT_FAILURE;
    }

    JasUnpack((*cfg), imagePath, zeroTol, scaleFactor, pixelsPerMeter, maxIter);

    Grid grid(cfg->horizZip.ValueOr(false), cfg->verticZip.ValueOr(false));
    if (!grid.LoadFromImage(imagePath.c_str(), cfg->constraints, scaleFactor.ValueOr(1)))
        return EXIT_FAILURE;

    DispatchSolver(cfg->mode, &grid, zeroTol.ValueOr(0.001), maxIter.ValueOr(20000));
    //FDM::SolveGridLaplacianZero(&grid, zeroTol.ValueOr(0.001), maxIter.ValueOr(20000));

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
    Jasnah::Option<Grid> diff = Cmp::Difference(grid, analytic, DifferenceType::Absolute);

    if (!diff)
    {
        return EXIT_FAILURE;
    }

    using namespace Plot;

    PlottableGrids grids;
    grids.singleSimGrid = grid;
    grids.grid2 = analytic;
    grids.singleSimVector = gradGrid;
    grids.difference = diff;

    WritePlotFiles(grids, Cfg::OperationMode::CompareProblem0);

    // WriteGridForGnuplot(grid);
    // WriteGnuplotColormapFile(grid);
    // WriteGnuplotContourFile(grid);

    // WriteGridForGnuplot(*diff, "Plot/GridDiff.dat");
    // WriteGnuplotColormapFile(*diff, "Plot/GridDiff.dat", "Plot/GridDiff.gpi");

    // WriteGridForGnuplot(analytic, "Plot/GridAnalytic.dat");
    // WriteGnuplotColormapFile(analytic, "Plot/GridAnalytic.dat", "Plot/GridAnalytic.gpi");
    // WriteGnuplotContourFile(analytic, "Plot/GridAnalytic.dat", "Plot/ContourAnalytic.gpi");

    // WriteGridForGnuplot(analytic);
    // WriteGnuplotColormapFile(analytic);
    // WriteGnuplotContourFile(analytic);
    return EXIT_SUCCESS;
}

static
int
CompareProblem1(const bool pathsAreJson, const std::vector<std::string>& paths)
{
    if (paths.size() != 1)
    {
        LOG("Expected 1 path");
        return EXIT_FAILURE;
    }

    Jasnah::Option<Cfg::GridConfigData> cfg;

    if (pathsAreJson)
    {
        cfg = Cfg::LoadGridConfigString(paths.front());
    }
    else
    {
        cfg = Cfg::LoadGridConfigFile(paths.front().c_str());
    }

    if (!cfg)
    {
        LOG("Cannot understand config file, exiting");
        return EXIT_FAILURE;
    }

    JasUnpack((*cfg), imagePath, zeroTol, scaleFactor, pixelsPerMeter, maxIter);

    Grid grid(cfg->horizZip.ValueOr(false), cfg->verticZip.ValueOr(false));
    if (!grid.LoadFromImage(imagePath.c_str(), cfg->constraints, scaleFactor.ValueOr(1)))
        return EXIT_FAILURE;

    DispatchSolver(cfg->mode, &grid, zeroTol.ValueOr(0.001), maxIter.ValueOr(20000));

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

    PlottableGrids grids;
    grids.singleSimGrid = grid;
    grids.grid2 = analytic;
    grids.singleSimVector = gradGrid;
    grids.difference = diff;

    WritePlotFiles(grids, Cfg::OperationMode::CompareProblem1);

    // WriteGridForGnuplot(grid);
    // WriteGnuplotColormapFile(grid);
    // WriteGnuplotContourFile(grid);

    // WriteGridForGnuplot(*diff, "Plot/GridDiff.dat");
    // WriteGnuplotColormapFile(*diff, "Plot/GridDiff.dat", "Plot/GridDiff.gpi");

    // WriteGridForGnuplot(analytic, "Plot/GridAnalytic.dat");
    // WriteGnuplotColormapFile(analytic, "Plot/GridAnalytic.dat", "Plot/GridAnalytic.gpi");
    // WriteGnuplotContourFile(analytic, "Plot/GridAnalytic.dat", "Plot/ContourAnalytic.gpi");

    return EXIT_SUCCESS;
}

static
int
CompareTwo(const bool pathsAreJson, const std::vector<std::string>& paths)
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
SingleSimulation(const bool pathIsJson, const std::string& path)
{
    Jasnah::Option<Cfg::GridConfigData> cfg;

    if (pathIsJson)
    {
        cfg = Cfg::LoadGridConfigString(path);
    }
    else
    {
        cfg = Cfg::LoadGridConfigFile(path.c_str());
    }

    if (!cfg)
    {
        LOG("Cannot understand config file, exiting");
        return EXIT_FAILURE;
    }

    JasUnpack((*cfg), imagePath, zeroTol, scaleFactor, pixelsPerMeter, maxIter);

    Grid grid(cfg->horizZip.ValueOr(false), cfg->verticZip.ValueOr(false));
    if (!grid.LoadFromImage(imagePath.c_str(), cfg->constraints, scaleFactor.ValueOr(1)))
        return EXIT_FAILURE;

    DispatchSolver(cfg->mode, &grid, zeroTol.ValueOr(0.001), maxIter.ValueOr(20000));
    //FDM::SolveGridLaplacianZero(&grid, zeroTol.ValueOr(0.001), maxIter.ValueOr(20000));

    GradientGrid gradGrid;
    gradGrid.CalculateNegGradient(grid, pixelsPerMeter.ValueOr(100.0));

    using namespace Plot;
    PlottableGrids grids;
    grids.singleSimGrid = grid;
    grids.singleSimVector = gradGrid;

    WritePlotFiles(grids, Cfg::OperationMode::SingleSimulation);

    // WriteGridForGnuplot(grid);
    // WriteGnuplotColormapFile(grid);
    // WriteGnuplotContourFile(grid);
    // WriteGradientFiles(gradGrid);

    return EXIT_SUCCESS;
}

static
int
Preprocess(const std::string& path)
{
    Image img;
    if (!img.LoadImage(path.c_str(), 4))
    {
        return EXIT_FAILURE;
    }

    auto info = img.GetInfo();

    u32* texel = (u32*)img.GetData();

    std::vector<RGBA> tx;
    tx.assign(texel,
              texel + (info.numScanlines
                       * info.pxPerLine));

    std::sort(tx.begin(), tx.end());
    tx.erase(std::unique(tx.begin(), tx.end()), tx.end());
    // NOTE(Chris): Ignore White
    auto iter = std::find(tx.begin(), tx.end(), RGBA(255, 255, 255, 255));
    if (iter != tx.end())
        tx.erase(iter);

    Cfg::JSONPreprocConfigVars json;
    json.imgPath = path;
    json.colorMap = tx;

    if (!Cfg::WriteJSONPreprocFile(json))
        return EXIT_FAILURE;


    return EXIT_SUCCESS;
}

// NOTE(Chris): Meyers' C++11 singleton pattern
namespace Log
{
    JsonOutputStream&
    GetJsonOutStream()
    {
        static JsonOutputStream s(&Log::log);
        return s;
    }

    AnalyticsDaemon&
    GetAnalytics()
    {
        static AnalyticsDaemon a;
        return a;
    }
}

#ifndef CATCH_CONFIG_MAIN
int main(int argc, const char* argv[])
{
    auto args = ParseArguments(argc, argv);
    if (args.guiMode)
    {
        // NOTE(Chris): Try and work out why this changes the output stream mode :P -- yes, it's valid, idiomatic C++11
        Log::GetJsonOutStream().EnqueueMessage("");
        Log::GetAnalytics().SetMode(AnalyticsDaemon::Mode::JSONStdOut, &Log::GetJsonOutStream());
    }
    else
    {
        Log::GetAnalytics().SetMode(AnalyticsDaemon::Mode::StdOut);
    }

    int result = EXIT_SUCCESS;
    switch (args.mode)
    {
    case Cfg::OperationMode::CompareProblem0:
    {
        result = CompareProblem0(args.jsonStdin, args.inputPaths);
    } break;

    case Cfg::OperationMode::CompareProblem1:
    {
        result = CompareProblem1(args.jsonStdin, args.inputPaths);
    } break;

    case Cfg::OperationMode::CompareTwo:
    {
        result = CompareTwo(args.jsonStdin, args.inputPaths);
    } break;

    case Cfg::OperationMode::SingleSimulation:
    {
        result = SingleSimulation(args.jsonStdin, args.inputPaths.front());
    } break;

    case Cfg::OperationMode::Preprocess:
    {
        result = Preprocess(args.inputPaths.front());
    } break;

    default:
        LOG("Unknown mode");

    }

    return result;
}
#endif
