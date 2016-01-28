// -*- c++ -*-
#if !defined(COMPARE_H)
/* ==========================================================================
   $File: Compare.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define COMPARE_H
#include "GlobalDefines.hpp"
#include "Jasnah.hpp"

class Grid;
class GradientGrid;
namespace Cmp
{
    /// Returns a grid where each cell is the difference between the
    /// two grid provided. Returns None if the two grids are
    /// incompatible. Currently the grids need to be the same size
    Jasnah::Option<Grid>
    Difference(const Grid& gridA, const Grid& gridB);

    /// Does the same as above but for gradient grids, same limitations.
    Jasnah::Option<GradientGrid>
    Difference(const GradientGrid& gridA, const GradientGrid& gridB);
}

#endif
