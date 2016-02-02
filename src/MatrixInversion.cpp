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
#include <Eigen/Dense>

//using Eigen::MatrixXd;

namespace MatrixInversion
{
    void
    MatrixInversionMethod(Grid* grid, const f64 stopPoint , const u64 maxIter)
    {
         
        
        // get the matrix from grid
        Matrix<f64, *grid.lineLength-1, *grid.numLines-1> A;
        Matrix<f64, 1, *grid.numLines-1 * *grid.lineLength-1 > known;
        Matrix<f64, 1, *grid.numLines-1 *  *grid.lineLength-1> V;
        
        // find the nodes
        // *grid.lineLength  (how to find the line wrapping)
        // start loop through number of nodes
        for (int y = 1; y < numLines -1; ++y)
        {
            for (uint x = 1; x < lineLength - 1; ++x)
            {

                //check if node currently at is known or not. if it is
                //add it to C and if not then add it to A.
                if (fixedPoints.count(y * lineLength + x) != 0)
                {
                    // put that value in the cloulum vector C
                    known(1,x)= *grid.voltage(y * lineLength + x);
                    A(x,y) = 1;
                    //continue;
                        
                }

                else
                {
                    // put -4 in A at current place
                    A(x,y)= -4;

                    // put 1 in A one left and one right of current place
                    A(x+1,y) = 1;
                    A(x-1,y) = 1;

                    // put 1 in A one up and one down of current place
                    A(x,y+1) = 1;
                    A(x,y-1) = 1;

                    // put a 0 in the known  because don't know this point
                    known(1,x) = 0;
                    
                }               
                // end of x-loop                
            }           
            // end y-loop
        }
        
        // calculate the V bu mutilpying (invers of A) * (known)
        V = A.inverse() * known;
        // log V
        for (int i = 1; i < V.cols() ; i++ )
        {
            *grid.voltage(i) = V(i-1)
        }
        
    }
    
}



//This is just a back up incase something makes me go back to this method. #oldmethod
                // // These if statements take care of if the ones adjecent
                // // are known or not. if they are add them to C and 
                // // if not then add them to A.

                // // if one to the right
                // if (fixedPoints.count(y * lineLength + x+1) == 0)
                // {
                //     A(x,y) += *grid.voltage(y * lineLength + x+1);
                // }
                
                // else
                // {
                //     known(1,x)= -*grid.voltage(y * lineLength + x+1);
                // }
                
                // // if one to the left
                // if (fixedPoints.count(y * lineLength + x-1) == 0)
                // {
                //     A(x,y) += *grid.voltage(y * lineLength + x-1);
                // }
                
                // else
                // {
                //     known(1,x)= -*grid.voltage(y * lineLength + x-1);
                // } 
                
                // // if one down
                // if (fixedPoints.count(y+1 * lineLength + x) == 0)
                // {
                //     A(x,y) += *grid.voltage(y+1 * lineLength + x);
                // }
                
                // else
                // {
                //     known(1,x)= -*grid.voltage(y+1 * lineLength + x);
                // }
                
                // // if one up
                // if (fixedPoints.count(y-1 * lineLength + x) == 0)
                // {
                //     A(x,y) += *grid.voltage(y-1 * lineLength + x);
                // }
                
                // else
                // {
                //     known(1,x)= -*grid.voltage(y-1 * lineLength +x);
                // }
