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

class Grid;
class GradientGrid;

namespace Plot
{
    // TODO(Chris): Refactor this


    /// Writes the gnuplot data file, to be used with WriteGnuplotFile
    /// Returns true on successful write
    bool
    WriteGridForGnuplot(const Grid& grid, const char* filename = "Plot/Grid.dat");

    /// Writes the gnuplot data file for gradients (E-field), to be used
    /// with WriteGnuplotGradientFile. Returns true on successful write.
    /// stepSize ignores some of the data so as not to overcrowd the plot
    /// with lines
    bool
    WriteGradientGridForGnuplot(const GradientGrid& grid,
                                const uint stepSize = 2,
                                const char* filename = "Plot/GradientGrid.dat");

    /// Prints a script config file for gnuplot, the makefile can run
    /// this. Returns true on successful write
    bool
    WriteGnuplotColormapFile(const Grid& grid,
                             const char* gridDataFile = "Plot/Grid.dat",
                             const char* filename = "Plot/PlotFinal.gpi");

    // Prints a script config file for gnuplot to plot the contour map.
    // Returns true on succesful write
    bool
    WriteGnuplotContourFile(const Grid& grid,
                            const char* gridDataFile = "Plot/Grid.dat",
                            const char* filename = "Plot/PlotContour.gpi");

    // Prints a script config file for gnuplot to plot the E-field map.
    // Returns true on succesful write. scaling scales the magnitude of
    // the vectors
    bool
    WriteGnuplotGradientFile(const GradientGrid& grid,
                             const f64 scaling = 2.5,
                             const char* gridDataFile = "Plot/GradientGrid.dat",
                             const char* filename = "Plot/PlotGradient.gpi");
}
#endif
