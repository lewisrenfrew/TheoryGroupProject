// -*- c++ -*-
#if !defined(GRADIENTGRID_H)
/* ==========================================================================
   $File: GradientGrid.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define GRADIENTGRID_H
#include "GlobalDefines.hpp"
#include "Utility.hpp"

class Grid;
/// Calculates and holds the result of the gradient of a simulation grid
class GradientGrid
{
public:
    // Constructors and operators
    GradientGrid() : gradients(), lineLength(0), numLines(0) {}
    ~GradientGrid() = default;
    GradientGrid(const GradientGrid&) = default;
    GradientGrid& operator=(const GradientGrid&) = default;

    /// Storage for the vectors in an array matching the cells
    std::vector<V2d> gradients;
    // As for Grid
    uint lineLength;
    uint numLines;

    /// Calculate and store the negative gradient (i.e. the E-field
    /// from the potential)
    void
    CalculateNegGradient(const Grid& grid, const f64 cellsToMeters);
};
#endif
