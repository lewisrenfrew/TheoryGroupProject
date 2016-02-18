/* ==========================================================================
   $File: Plot.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#include "GlobalDefines.hpp"
#include "Plot.hpp"
#include "Grid.hpp"
#include "GradientGrid.hpp"
#include "Jasnah.hpp"
#include "Utility.hpp"
#include "JSON.hpp"

#include <cmath>
#include <cstdio>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef GNUPLOT_VERBOSE
static constexpr const char* const gnuplot = "gnuplot 2> /dev/null";
#else
static constexpr const char* const gnuplot = "gnuplot";
#endif

/// Creates a string with the plotting command and embedded data for
/// gnuplot to produce a colormap
static
std::string
PlotColorMapString(const Grid& grid, const char* outputName, const char* titleName)
{
    JasUnpack(grid, lineLength, numLines);
    // NOTE(Chris): Yes, I'm lazy here
    char buf[4096];
    snprintf(buf, sizeof(buf)-1,
            "set terminal canvas rounded size 700,500 enhanced mousing fsize 10 "
            "lw 1.6 fontscale 1 standalone\n"
            "set output \"%s\"\n"
            "load 'Plot/MorelandColors.plt'\n"
            "set xlabel \"x\"\nset ylabel \"y\"\n"
            "set xrange [0:%u]; set yrange [0:%u]\n"
            "set size ratio %f\n"
            "set style data lines\n"
            "set title \"Stable Potential (V)\"\n"
            "plot '-' with image title \"%s\"\n",
            outputName,
            lineLength-1,
            numLines-1,
            (f64)numLines / (f64)lineLength,
            titleName);
    std::string result(buf);

    for (uint y = 0; y < numLines; ++y)
    {
        for (uint x = 0; x < lineLength; ++x)
        {
            char buf1[1024];
            snprintf(buf1, sizeof(buf1)-1, "%u %u %f\n", x, y, grid.voltages[y * grid.lineLength + x]);
            result += buf1;
        }
        // fprintf(gp, "\n");
        result += "\n";
    }
    // fprintf(gp, "e\n");
    result += "e\n";
    return result;
}

/// Creates a string with the plotting command and embedded data for
/// gnuplot to produce a contourmap
static
std::string
PlotContourMapString(const Grid& grid, const char* outputName, const char* titleName)
{
    JasUnpack(grid, lineLength, numLines);

    char buf[4096];
    snprintf(buf, sizeof(buf) - 1,
            "set terminal canvas rounded size 700,500 enhanced mousing "
            "fsize 10 lw 1.6 fontscale 1 standalone\n"
            "set output \"%s\"\n"
            "load 'Plot/MorelandColors.plt'\n"
            "set key outside\n"
            "set view map\n"
            "unset surface\n"
            "set contour base\n"
            "set cntrparam bspline\n"
            "set cntrparam levels auto 20\n"
            "set xlabel \"x\"\nset ylabel \"y\"\n"
            "set xrange [0:%u]; set yrange [0:%u]\n"
            "set size ratio %f\n"
            "set style data lines\n"
            "set title \"%s\"\n"
            "set key default\n"
            "splot '-' with lines title \"\"\n",
            outputName,
            lineLength-1,
            numLines-1,
            (f64)numLines / (f64)lineLength,
            titleName);
    std::string result(buf);

    for (uint y = 0; y < numLines; ++y)
    {
        for (uint x = 0; x < lineLength; ++x)
        {
            char buf1[1024];
            snprintf(buf1, sizeof(buf1)-1, "%u %u %f\n", x, y, grid.voltages[y * grid.lineLength + x]);
            result += buf1;
        }
        result += "\n";
    }
    result += "e\n";
    // fprintf(gp, "e\n");
    return result;
}

/// Creates a string with the plotting command and embedded data for
/// gnuplot to produce a vectorfield
static
std::string
PlotVectorFieldString(const GradientGrid& grid, const char* outputName, const char* titleName)
{
    const uint maxVecPerSide = 50;

    JasUnpack(grid, lineLength, numLines);

    using namespace Jasnah;
    auto VecNorm = [](const V2d& vec) -> f64
        {
            return std::sqrt(vec.x*vec.x + vec.y*vec.y);
        };
    auto Max = [](f64 x, f64 y)
        {
            return x > y
            ? x
            : y;
        };

    const f64 maxNorm = grid.gradients | (Map << VecNorm) | (Reduce << 0.0 << Max);
    // LOG("Max norm %f", maxNorm);

    const uint maxSide = numLines > lineLength ? numLines : lineLength;
    const uint stepSize = (maxSide / maxVecPerSide > 0) ? maxSide / maxVecPerSide : 1;
    const f64 scaling = maxSide / (0.5 * (f64)maxVecPerSide * maxNorm);

    char buf[4096];
    snprintf(buf, sizeof(buf) - 1,
            "set terminal canvas butt size 700,500 enhanced mousing "
            "fsize 10 lw 1 fontscale 1 standalone\n"
            "set output \"%s\"\n"
            "load 'Plot/MorelandColors.plt'\n"
            "set xlabel \"x\"\nset ylabel \"y\"\n"
            "set xrange [0:%u]; set yrange [0:%u]\n"
            "set size ratio %f\n"
            "set style data lines\n"
            "set title \"E-field (V/m)\"\n"
            "scaling = %f\n"
            "plot '-' using 1:2:($3*scaling):($4*scaling):(sqrt($3*$3+$4*$4)) with vectors"
            " filled lc palette title \"\"\n",
            outputName,
            lineLength-1,
            numLines-1,
            (f64)numLines / (f64)lineLength,
            scaling);

    std::string result(buf);

    for (uint y = 0; y < numLines; y += stepSize)
    {
        for (uint x = 0; x < lineLength; x += stepSize)
        {
            char buf1[1024];
            snprintf(buf1, sizeof(buf1) - 1, "%u %u %f %f\n", x, y,
                    grid.gradients[y * lineLength + x].x,
                    grid.gradients[y * lineLength + x].y);
            result += buf1;
        }
        result += "\n";
    }
    result += "e\n";
    return result;
}

/// Plots the provided string using gnuplot and a temporary file.
/// Returns true on success. This relies on POSIX functionality
static
bool
GnuplotString(const std::string& data)
{
    // NOTE(Chris): We use posix mkstemp to open a temporary file
    char fileName[] = "/tmp/GridleXXXXXX";
    int fd = mkstemp(fileName);
    if (fd < 0)
    {
        LOG("Unable to get temporary file descriptor");
        return false;
    }
    // NOTE(Chris): By default our file's permissions are 0600, we
    // need gnuplot to read it so change settings to allow this
    fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP);
    // NOTE(Chris): Open file as FILE* to use formatted IO
    FILE* gph = fdopen(fd, "w");
    if (!gph)
    {
        close(fd);
        LOG("Unable to convert fd to fp");
        return false;
    }

    fprintf(gph, "%s\n", data.c_str());
    fflush(gph);
    fclose(gph);
    // NOTE(Chris): Close FILE* after flushing

    // NOTE(Chris): Gnuplot command string
    std::string gplot(gnuplot);
    gplot += " ";
    gplot += fileName;

    // NOTE(Chris): Run gnuplot
    FILE* gp = popen(gplot.c_str(), "w");
    if (!gp)
    {
        LOG("Error opening gnuplot");
        close(fd);
        return false;
    }

    if (pclose(gp) != 0)
    {
        LOG("Error closing gnuplot");
        close(fd);
        return false;
    }

    close(fd);
    // NOTE(Chris): Remove from filesystem when done
    unlink(fileName);

    return true;
}

/// Verifies and Plots all of the graphs generated in a single
/// simulation mode. Returns true on success. Also logs the output
/// files.
static
bool
PlotSingleSim(const Plot::PlottableGrids& grids)
{
    TIME_FUNCTION();
    // expects the first two plottable grids
    if (!grids.singleSimGrid || !grids.singleSimVector)
        return false;
    // NOTE(Chris): These assume that everything works

    using namespace Plot::SingleSimFiles;
    std::string gpStr;
    gpStr += PlotColorMapString(*grids.singleSimGrid, gridPlot, "Stable Voltage (V)");
    gpStr += PlotContourMapString(*grids.singleSimGrid, contourPlot, "Stable Voltage (V)");
    gpStr += PlotVectorFieldString(*grids.singleSimVector, vectorPlot, "Electric Field (V/m)");

    if (!GnuplotString(gpStr))
        return false;

    Log::GetAnalytics().ReportGraphOutput(gridPlot, "Voltage Plot");
    Log::GetAnalytics().ReportGraphOutput(contourPlot, "Voltage Contour Plot");
    Log::GetAnalytics().ReportGraphOutput(vectorPlot, "E-field Plot");

    return true;
}

/// Verifies and Plots all of the graphs generated in a comparison
/// with analytic problem simulation mode. Returns true on success.
/// Also logs the output files.
static
bool
PlotCompareProb(const Plot::PlottableGrids& grids)
{
    TIME_FUNCTION();
    // expects all plottable grids
    if (!grids.singleSimGrid
        || !grids.singleSimVector
        || !grids.grid2
        || !grids.vector2
        || !grids.difference)
        return false;
    // NOTE(Chris): These assume that everything works

    using namespace Plot::CompareProbFiles;
    std::string gpStr;
    gpStr += PlotColorMapString(*grids.singleSimGrid, gridPlot, "Stable Voltage (V)");
    gpStr += PlotContourMapString(*grids.singleSimGrid, contourPlot, "Stable Voltage (V)");
    gpStr += PlotVectorFieldString(*grids.singleSimVector, vectorPlot, "Electric Field (V/m)");
    gpStr += PlotColorMapString(*grids.grid2, gridAnalyticPlot, "Stable Voltage - Analytic - (V)");
    gpStr += PlotContourMapString(*grids.grid2, contourAnalyticPlot, "Stable Voltage - Analytic - (V)");
    gpStr += PlotVectorFieldString(*grids.vector2, vectorAnalyticPlot, "Electric Field (V/m)");
    gpStr += PlotColorMapString(*grids.difference, differencePlot, "Difference (V)");

    if (!GnuplotString(gpStr))
        return false;

    Log::GetAnalytics().ReportGraphOutput(gridPlot, "Voltage Plot");
    Log::GetAnalytics().ReportGraphOutput(contourPlot, "Voltage Contour Plot");
    Log::GetAnalytics().ReportGraphOutput(vectorPlot, "E-field Plot");

    Log::GetAnalytics().ReportGraphOutput(gridAnalyticPlot, "Voltage Plot (Analytic)");
    Log::GetAnalytics().ReportGraphOutput(contourAnalyticPlot, "Voltage Contour Plot (Analytic)");
    Log::GetAnalytics().ReportGraphOutput(vectorAnalyticPlot, "E-field Plot (Analytic)");

    Log::GetAnalytics().ReportGraphOutput(differencePlot, "Voltage Difference Between Solutions");


    return true;
}

/// Verifies and Plots all of the graphs generated in a comparison
/// simulation mode. Returns true on success. Also logs the output
/// files.
static
bool
PlotCompareTwo(const Plot::PlottableGrids& grids)
{
    TIME_FUNCTION();
    // expects all plottable grids
    if (!grids.singleSimGrid
        || !grids.singleSimVector
        || !grids.grid2
        || !grids.vector2
        || !grids.difference)
        return false;

    using namespace Plot::CompareTwoFiles;

    std::string gpStr;
    gpStr += PlotColorMapString(*grids.singleSimGrid, gridOnePlot, "Stable Voltage (V)");
    gpStr += PlotContourMapString(*grids.singleSimGrid, contourOnePlot, "Stable Voltage (V)");
    gpStr += PlotVectorFieldString(*grids.singleSimVector, vectorOnePlot, "Electric Field (V/m)");
    gpStr += PlotColorMapString(*grids.grid2, gridTwoPlot, "Stable Voltage (V)");
    gpStr += PlotContourMapString(*grids.grid2, contourTwoPlot, "Stable Voltage (V)");
    gpStr += PlotVectorFieldString(*grids.vector2, vectorTwoPlot, "Electric Field (V/m)");
    gpStr += PlotColorMapString(*grids.difference, differencePlot, "Difference (V)");

    if(!GnuplotString(gpStr))
        return false;

    Log::GetAnalytics().ReportGraphOutput(gridOnePlot, "Voltage One Plot");
    Log::GetAnalytics().ReportGraphOutput(contourOnePlot, "Voltage One Contour Plot");
    Log::GetAnalytics().ReportGraphOutput(vectorOnePlot, "E-field One Plot");

    Log::GetAnalytics().ReportGraphOutput(gridTwoPlot, "Voltage Two Plot");
    Log::GetAnalytics().ReportGraphOutput(contourTwoPlot, "Voltage Two Contour Plot");
    Log::GetAnalytics().ReportGraphOutput(vectorTwoPlot, "E-field Two Plot");

    Log::GetAnalytics().ReportGraphOutput(differencePlot, "Voltage Difference Between Solutions");

    return true;
}


namespace Plot
{
    bool
    WritePlotFiles(const PlottableGrids& grids,
                   const Cfg::OperationMode mode)
    {
        // NOTE(Chris): Dispatch to the correct solver
        using Cfg::OperationMode;
        bool result = false;
        switch (mode)
        {
        case OperationMode::SingleSimulation:
        {
            result = PlotSingleSim(grids);
        } break;

        case OperationMode::CompareProblem0:
            // FALL THROUGH
        case OperationMode::CompareProblem1:
        {
            result = PlotCompareProb(grids);
        } break;

        case OperationMode::CompareTwo:
        {
            result = PlotCompareTwo(grids);
        } break;

        default:
        {
            LOG("Wut have you done?! You need to be in a simulation mode to plot output");
        } break;
        }
        return result;
    }

}
