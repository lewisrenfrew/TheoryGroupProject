// -*- c++ -*-
#if !defined(FDM_H)
/* ==========================================================================
   $File: FDM.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define FDM_H
// Finite difference method implementations

#include "GlobalDefines.hpp"

class Grid;
namespace FDM
{
    /// The function that does the work for now. Repeatedly applies a
    /// finite difference scheme to the grid, until the maximum relative
    /// change over one iteration is less zeroTol or maxIter iterations
    /// have taken place.
    void
    SolveGridLaplacianZero(Grid* grid, const f64 zeroTol, const u64 maxIter);
}
#endif
