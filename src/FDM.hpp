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
    /// Solves the Grid using a finite difference method, set parallel
    /// to false to run single threaded, the zeroTol and maxIter
    /// parameters control the convergence breaking on whichever comes first.
    void
    FDMSolver(Grid* grid, const f64 zeroTol,
              const u64 maxIter, bool parallel = true);
}
#endif
