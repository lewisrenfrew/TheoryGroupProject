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
    /// Names used for graph output depending on mode. This is the
    /// CANONICAL list of HTML output files from gnuplot
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

    /// Used to pass the possible output from simulation to plotting
    /// routines, not all graphs are required in all modes
    using Jasnah::Option;
    struct PlottableGrids
    {
        Option<Grid> singleSimGrid;
        Option<GradientGrid> singleSimVector;
        Option<Grid> grid2;
        Option<GradientGrid> vector2;
        Option<Grid> difference;
    };

    /// Uses gnuplot to produce the plots based on the on the provided plots and mode
    bool
    WritePlotFiles(const PlottableGrids& grids,
                   const Cfg::OperationMode mode);

}
#endif
