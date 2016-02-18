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

// static constexpr const char* const gnuplot = "gnuplot 2> /dev/null";

static constexpr const char* const gnuplot = "gnuplot";

static
std::string
PlotColorMapString(const Grid& grid, const char* outputName, const char* titleName)
{
    #if 0
    FILE* gp;
    gp = popen(gnuplot, "w");
    if (!gp)
    {
        LOG("Error opening gnuplot");
        return false;
    }

    JasUnpack(grid, lineLength, numLines);

    fprintf(gp,
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

    for (uint y = 0; y < numLines; ++y)
    {
        for (uint x = 0; x < lineLength; ++x)
        {
            fprintf(gp, "%u %u %f\n", x, y, grid.voltages[y * grid.lineLength + x]);
        }
        fprintf(gp, "\n");
    }
    fprintf(gp, "e\n");

    if (!pclose(gp))
        return false;

    return true;
    #else
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

    #endif
}

static
std::string
PlotContourMapString(const Grid& grid, const char* outputName, const char* titleName)
{
    #if 0
    FILE* gp;
    gp = popen(gnuplot, "w");
    if (!gp)
    {
        LOG("Error opening gnuplot");
        return false;
    }

    JasUnpack(grid, lineLength, numLines);

    fprintf(gp,
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

    for (uint y = 0; y < numLines; ++y)
    {
        for (uint x = 0; x < lineLength; ++x)
        {
            fprintf(gp, "%u %u %f\n", x, y, grid.voltages[y * grid.lineLength + x]);
        }
        fprintf(gp, "\n");
    }
    fprintf(gp, "e\n");

    if (!pclose(gp))
        return false;

    return true;
    #else
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

    #endif
}

static
std::string
PlotVectorFieldString(const GradientGrid& grid, const char* outputName, const char* titleName)
{
    const uint maxVecPerSide = 50;
    #if 0
    FILE* gp;
    gp = popen(gnuplot, "w");
    if (!gp)
    {
        LOG("Error opening gnuplot");
        return false;
    }

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

    fprintf(gp,
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

    for (uint y = 0; y < numLines; y += stepSize)
    {
        for (uint x = 0; x < lineLength; x += stepSize)
        {
            fprintf(gp, "%u %u %f %f\n", x, y,
                    grid.gradients[y * lineLength + x].x,
                    grid.gradients[y * lineLength + x].y);
        }
        fprintf(gp, "\n");
    }
    fprintf(gp, "e\n");

    if (!pclose(gp))
        return false;

    return true;
    #else

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
    // fprintf(gp, "e\n");
    result += "e\n";
    // fprintf(gp, "e\n");
    return result;
    #endif
}


#if 0
static
bool
WriteGridForGnuplot(const Grid& grid, const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    for (uint y = 0; y < grid.numLines; ++y)
    {
        for (uint x = 0; x < grid.lineLength; ++x)
        {
            fprintf(file, "%u %u %f\n", x, y, grid.voltages[y * grid.lineLength + x]);
        }
        fprintf(file, "\n");
    }

    fclose(file);
    return true;
}

static
bool
WriteGradientGridForGnuplot(const GradientGrid& grid,
                            const uint stepSize,
                            const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    JasUnpack(grid, gradients, numLines, lineLength);

    for (uint y = 0; y < numLines; y += stepSize)
    {
        for (uint x = 0; x < lineLength; x += stepSize)
        {
            fprintf(file, "%u %u %f %f\n", x, y,
                    gradients[y * lineLength + x].x,
                    gradients[y * lineLength + x].y);
        }
        fprintf(file, "\n");
    }

    fclose(file);
    return true;
}

static
bool
WriteGnuplotColormapFile(const Grid& grid,
                         const char* gridDataFile,
                         const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    JasUnpack(grid, lineLength, numLines);

    fprintf(file,
            // "set terminal pngcairo size 2560,1440\n"
            // "set output \"Grid.png\"\n"
            "set terminal canvas rounded size 700,500 enhanced mousing fsize 10 lw 1.6 fontscale 1 standalone\n"
            "set output \"Grid.html\"\n"
            "load 'Plot/MorelandColors.plt'\n"
            "set xlabel \"x\"\nset ylabel \"y\"\n"
            "set xrange [0:%u]; set yrange [0:%u]\n"
            "set size ratio %f\n"
            "set style data lines\n"
            "set title \"Stable Potential (V)\"\n"
            "plot \"%s\" with image title \"Numeric Solution\"",
            lineLength-1,
            numLines-1,
            (f64)numLines / (f64)lineLength,
            gridDataFile);

    fclose(file);
    return true;
}

static
bool
WriteGnuplotContourFile(const Grid& grid,
                        const char* gridDataFile,
                        const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    JasUnpack(grid, lineLength, numLines);

    fprintf(file,
            // "set terminal pngcairo size 2560,1440\n"
            // "set output \"GridContour.png\"\n"
            "set terminal canvas rounded size 700,500 enhanced mousing fsize 10 lw 1.6 fontscale 1 standalone\n"
            "set output \"GridContour.html\"\n"
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
            "set title \"Stable Potential (V)\"\n"
            "splot \"%s\" with lines title \"\"\n"
            "set key default\n",
            lineLength-1,
            numLines-1,
            (f64)numLines / (f64)lineLength,
            gridDataFile);

    return true;
}

static
bool
WriteGnuplotGradientFile(const GradientGrid& grid,
                         const f64 scaling,
                         const char* gridDataFile,
                         const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    JasUnpack(grid, lineLength, numLines);

    fprintf(file,
            // "set terminal png size 2560,1440\n"
            // "set output \"GradientGrid.png\"\n"
            // "set terminal canvas rounded size 1280,720 enhanced mousing fsize 10 lw 1.6 fontscale 1 standalone\n"
            "set terminal canvas butt size 700,500 enhanced mousing fsize 10 lw 1 fontscale 1 standalone\n"
            "set output \"GradientGrid.html\"\n"
            "load 'Plot/MorelandColors.plt'\n"
            "set xlabel \"x\"\nset ylabel \"y\"\n"
            "set xrange [0:%u]; set yrange [0:%u]\n"
            "set size ratio %f\n"
            "set style data lines\n"
            "set title \"E-field (V/m)\"\n"
            "scaling = %f\n"
            "plot \"%s\" using 1:2:($3*scaling):($4*scaling):(sqrt($3*$3+$4*$4)) with vectors"
            " filled lc palette title \"\"",
            lineLength-1,
            numLines-1,
            (f64)numLines / (f64)lineLength,
            scaling,
            gridDataFile);

    return true;
}

static
bool
WriteGradientFiles(const GradientGrid& grid,
                   const uint maxVecPerSide,
                   const char* gridDataFile,
                   const char* plotFile)
{
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
    LOG("Max norm %f", maxNorm);

    JasUnpack(grid, numLines, lineLength);

    const uint maxSide = numLines > lineLength ? numLines : lineLength;
    const uint stepSize = (maxSide / maxVecPerSide > 0) ? maxSide / maxVecPerSide : 1;
    const f64 scaling = maxSide / (0.5 * (f64)maxVecPerSide * maxNorm);

    bool ret1 = WriteGradientGridForGnuplot(grid, stepSize, gridDataFile);
    bool ret2 = WriteGnuplotGradientFile(grid, scaling, gridDataFile, plotFile);

    return ret1 && ret2;
}
#endif

static
bool
GnuplotString(const std::string& data)
{
#if 0
    FILE* gp;
    gp = popen(gnuplot, "w");
    if (!gp)
    {
        LOG("Error opening gnuplot");
        return false;
    }
    fprintf(gp, "%s\n", data.c_str());
    fflush(gp);

    if (!pclose(gp))
        return false;

    return true;
#else
    FILE* gph;
    const char * const name = "PlotFile.gpi";
    gph = fopen(name, "w");
    if (!gph)
    {
        return false;
    }

    fprintf(gph, "%s\n", data.c_str());
    fclose(gph);
    std::string gplot(gnuplot);
    gplot += " ";
    gplot += name;

    FILE* gp;
    gp = popen(gplot.c_str(), "w");
    if (!gp)
    {
        LOG("Error opening gnuplot");
        return false;
    }
    if (!pclose(gp))
        return false;

    return true;

#endif
}

static
bool
PlotSingleSim(const Plot::PlottableGrids& grids, const Plot::LogOutputPaths log)
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

    GnuplotString(gpStr);

    // Serialize output files if reqd
    Log::GetAnalytics().ReportGraphOutput(gridPlot, "Voltage Plot");
    Log::GetAnalytics().ReportGraphOutput(contourPlot, "Voltage Contour Plot");
    Log::GetAnalytics().ReportGraphOutput(vectorPlot, "E-field Plot");

    return true;
}

static
bool
PlotCompareProb(const Plot::PlottableGrids& grids, const Plot::LogOutputPaths log)
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

    GnuplotString(gpStr);

    Log::GetAnalytics().ReportGraphOutput(gridPlot, "Voltage Plot");
    Log::GetAnalytics().ReportGraphOutput(contourPlot, "Voltage Contour Plot");
    Log::GetAnalytics().ReportGraphOutput(vectorPlot, "E-field Plot");

    Log::GetAnalytics().ReportGraphOutput(gridAnalyticPlot, "Voltage Plot (Analytic)");
    Log::GetAnalytics().ReportGraphOutput(contourAnalyticPlot, "Voltage Contour Plot (Analytic)");
    Log::GetAnalytics().ReportGraphOutput(vectorAnalyticPlot, "E-field Plot (Analytic)");

    Log::GetAnalytics().ReportGraphOutput(differencePlot, "Voltage Difference Between Solutions");

    // Serialize output files if reqd

    return true;
}

static
bool
PlotCompareTwo(const Plot::PlottableGrids& grids, const Plot::LogOutputPaths log)
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

    GnuplotString(gpStr);

    // Serialize output files if reqd
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
                   const Cfg::OperationMode mode,
                   const LogOutputPaths log)
    {
        using Cfg::OperationMode;
        bool result;
        switch (mode)
        {
        case OperationMode::SingleSimulation:
        {
            result = PlotSingleSim(grids, log);
        } break;

        case OperationMode::CompareProblem0:
            // FALL THROUGH
        case OperationMode::CompareProblem1:
        {
            result = PlotCompareProb(grids, log);
        } break;

        case OperationMode::CompareTwo:
        {
            result = PlotCompareTwo(grids, log);
        } break;

        default:
        {
            LOG("Wut have you done?!");
        } break;
        }
        return result;
    }

}
