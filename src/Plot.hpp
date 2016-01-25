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

    /// Use this to auto-scale the number of vectors created. Writes
    /// the files for gnuplot to plot the vector field and also its
    /// data file. Returns true on success
    bool
    WriteGradientFiles(const GradientGrid& grid,
                       const char* gridDataFile = "Plot/GradientGrid.dat",
                       const char* plotFile = "Plot/PlotGradient.gpi");

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
}
#endif
