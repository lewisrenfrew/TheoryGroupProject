/* ==========================================================================
   $File: Plot.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#include "Plot.hpp"
#include "Grid.hpp"
#include "GradientGrid.hpp"
#include "Jasnah.hpp"
#include "Utility.hpp"

#include <cmath>

namespace Plot
{
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

    bool
    WriteGradientGridForGnuplot(const GradientGrid& grid,
                                const uint stepSize,
                                const char* filename)
    {
#define GRADIENT_DEBUG
        FILE* file = fopen(filename, "w");
        if (!file)
        {
            return false;
        }

        JasUnpack(grid, gradients, numLines, lineLength);

#ifdef GRADIENT_DEBUG
        auto VecNorm = [](const V2d& vec) -> f64
            {
                return std::sqrt(vec.x*vec.x + vec.y*vec.y);
            };

        f64 maxNorm = 0.0;
#endif

        for (uint y = 0; y < numLines; ++y)
        {
            if (y % stepSize == 0)
                continue;

            for (uint x = 0; x < lineLength; ++x)
            {
                if (x % stepSize == 0)
                    continue;

#ifdef GRADIENT_DEBUG
                f64 norm = VecNorm(gradients[y * lineLength + x]);
                if (norm > maxNorm)
                    maxNorm = norm;
#endif

                fprintf(file, "%u %u %f %f\n", x, y,
                        gradients[y * lineLength + x].x,
                        gradients[y * lineLength + x].y);
            }
            fprintf(file, "\n");
        }

        fclose(file);

#ifdef GRADIENT_DEBUG
        LOG("Max norm: %f", maxNorm);
#endif
        return true;
    }

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
                "set terminal canvas rounded size 1280,720 enhanced mousing fsize 10 lw 1.6 fontscale 1 standalone\n"
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
                "set terminal canvas rounded size 1280,720 enhanced mousing fsize 10 lw 1.6 fontscale 1 standalone\n"
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
                // "set terminal pngcairo size 2560,1440\n"
                // "set output \"GradientGrid.png\"\n"
                "set terminal canvas rounded size 1280,720 enhanced mousing fsize 10 lw 1.6 fontscale 1 standalone\n"
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
}
