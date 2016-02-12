// -*- c++ -*-
#if !defined(GNUPLOT_H)
/* ==========================================================================
   $File: Gnuplot.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define GNUPLOT_H
#include "GlobalDefines.hpp"
#include "Jasnah.hpp"
#include "Grid.hpp"
#include "GradientGrid.hpp"

namespace Cfg
{
    enum class OperationMode;
}

namespace Plot
{
    // NOTE(Chris): Look at using gnuplot special filenames to improve
    // this mess -- generate plots directly
    namespace SingleSimFiles
    {
        static constexpr const char* gridPlot = "Grid.html";
        static constexpr const char* contourPlot = "Contour.html";
        static constexpr const char* vectorPlot = "Vector.html";
    };

    namespace CompareProbFiles
    {
        static constexpr const char* gridPlot = "Grid.html";
        static constexpr const char* contourPlot = "Contour.html";
        static constexpr const char* vectorPlot = "Vector.html";
        static constexpr const char* gridAnalyticPlot = "GridAnalytic.html";
        static constexpr const char* contourAnalyticPlot = "ContourAnalytic.html";
        static constexpr const char* vectorAnalyticPlot = "VectorAnalytic.html";
        static constexpr const char* differencePlot = "Diff.html";
    };

    namespace CompareTwoFiles
    {
        static constexpr const char* gridOnePlot = "Grid1.html";
        static constexpr const char* contourOnePlot = "Contour1.html";
        static constexpr const char* vectorOnePlot = "Vector1.html";
        static constexpr const char* gridTwoPlot = "Grid2.html";
        static constexpr const char* contourTwoPlot = "Contour2.html";
        static constexpr const char* vectorTwoPlot = "Vector2.html";
        static constexpr const char* differencePlot = "Diff.html";
    };

    using Jasnah::Option;
    struct PlottableGrids
    {
        Option<Grid> singleSimGrid;
        Option<GradientGrid> singleSimVector;
        Option<Grid> grid2;
        Option<GradientGrid> vector2;
        Option<Grid> difference;
        // Option<GradientGrid> differenceVector;
    };

    enum class LogOutputPaths
    {
        None,
        StdOut,
        JsonStdOut
    };

    bool
    WritePlotFiles(const PlottableGrids& grids,
                   const Cfg::OperationMode mode,
                   const LogOutputPaths log = LogOutputPaths::None);

    // /// Writes the gnuplot data file, to be used with WriteGnuplotFile
    // /// Returns true on successful write
    // bool
    // WriteGridForGnuplot(const Grid& grid, const char* filename = "Plot/Grid.dat");

    // /// Use this to auto-scale the number of vectors created. Writes
    // /// the files for gnuplot to plot the vector field and also its
    // /// data file. Returns true on success
    // bool
    // WriteGradientFiles(const GradientGrid& grid,
    //                    const uint maxVerPerSide = 50,
    //                    const char* gridDataFile = "Plot/GradientGrid.dat",
    //                    const char* plotFile = "Plot/PlotGradient.gpi");

    // /// Prints a script config file for gnuplot, the makefile can run
    // /// this. Returns true on successful write
    // bool
    // WriteGnuplotColormapFile(const Grid& grid,
    //                          const char* gridDataFile = "Plot/Grid.dat",
    //                          const char* filename = "Plot/PlotFinal.gpi");

    // // Prints a script config file for gnuplot to plot the contour map.
    // // Returns true on succesful write
    // bool
    // WriteGnuplotContourFile(const Grid& grid,
    //                         const char* gridDataFile = "Plot/Grid.dat",
    //                         const char* filename = "Plot/PlotContour.gpi");
}
#endif
