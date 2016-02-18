// -*- c++ -*-
#if !defined(REDBLACK_H)
/* ==========================================================================
   $File: GS.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define REDBLACK_H
// Finite difference method implementations

#include "GlobalDefines.hpp"

class Grid;

namespace RedBlack
{
    /// Method that behaves very similarly to FDM, just using the Red-Black iterative method instead
    void
    RedBlackSolver(Grid* grid, const f64 zeroTol,
                   const u64 maxIter, bool parallel = true);
}
#endif
