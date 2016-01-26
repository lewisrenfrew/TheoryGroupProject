// -*- c++ -*-
/* ==========================================================================
   $File: MatrixInversion.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Olle Windeman. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */


#include "MatrixInversion.hpp"
#include "Grid.hpp"
#include "Utility.hpp"

#include <cmath>
#include <atomic>
#include <algorithm>
#include <eigen/Dense>

namespace MatrixInversion
{
    void
    MatrixInversionMethod(Grid* grid, const f64 stopPoint , const u64 maxIter)
    {
         
        
        // get the matrix from grid
        auto V  = *grid.voltage;
        Matrix<f64, *grid.lineLength, *grid.numLines> A;
        Matrix<f64, 1, *grid.numLines> C;
        
        // find the nodes
        // *grid.lineLength  (how to find the line wrapping)
        // start loop through number of nodes
        for (int y = 1; y < numLines -1; ++y)
        {
            for (uint x = 1; x < lineLength - 1; ++x)
            {
        //     find the nodes adjesent to the node looking at
        //     take the voltage at that point
        //     if (value is known)
            if (fixedPoints.count(y * lineLength + x) != 0)
            {
        //         put that value is a cloulum vector C
                C(1,x)= -V(y * lineLength + x);
            }        
        //     else
            else
            {
        //         put in matrix A
                A(x,y)= -4*V(y * lineLength + x) + V(y * lineLength + x) +V(y * lineLength + x)
            }
        // end loop
            }
        }
        // invert A
        // calculate the V bu mutilpying (invers of A) * (C)
        // log V
    }

}
