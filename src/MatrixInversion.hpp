// -*- c++ -*-
#if !defined(MATRIXINVERSION_H)
/* ==========================================================================
   $File: MatrixInversion.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Olle Windeman. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define MATRIXINVERSION_H

#include "GlobalDefines.hpp"

class Grid;
namespace MatrixInversion
{

    void
    MatrixInversionMethod(Grid* grid, const f64 stopPoint , const u64 maxIter);
    
}

#endif

