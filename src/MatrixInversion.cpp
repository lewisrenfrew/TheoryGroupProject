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

#include "GlobalDefines.hpp"


#include <cmath>
#include <atomic>
#include <algorithm>
#include <Eigen/Dense>

namespace MatrixInversion
{
    void
    MatrixInversionMethod(Grid* grid, const f64 stopPoint , const u64 maxIter)
    {
        TIME_FUNCTION();
        using namespace Eigen;
        
        // checking if square matrix if not return nothing
        if(grid->lineLength != grid->numLines)
        {
            return;
        }
             
        // create three matricies and fill them with zeroes
        MatrixXd A = MatrixXd::Zero(grid->lineLength * grid->numLines
                      , grid->lineLength * grid->numLines);
              
        MatrixXd V = MatrixXd::Zero(1,grid->lineLength * grid->numLines);
        MatrixXd known = MatrixXd::Zero(1,grid->lineLength * grid->numLines);

        // counter what line on the actual grid the program is in 
        int k=0;

        LOG("Setting up grid.");
        
        // grid->lineLength  (how to find the line wrapping)
        // Loop through the number of columns on A
        for (int y = 0; y < A.cols() -2;)
        {
            // Loop through the number of columns of the grid 
            for (uint x = 0; x <grid->lineLength ; ++x)
            {
                //check if the current pixel is known
                if ((*grid).fixedPoints.count(k * grid->numLines + x) != 0)
                {
                    // If so add one to that place in A and add the known pixel
                    // to that place in known
                    A(y,y) = 1;                   
                    known(0,y)=(*grid).voltages[k * grid->numLines + x];
                   
                    // The way A and known are structured y need to be incremented here
                    y++;
                        
                }

                else
                {
                   
                    // If not known add a -4 in A for current pixel and 1 for
                    //each adjecent pixel
                    A(y,y)= -4;

                    //put 1 in A one left and one right of current place
                    A(y,y+1) = 1;     
                    A(y,y-1) = 1;
                     
                    // put 1 in A one up and one down of current place
                    A(y,y+grid->numLines) = 1;
                    A(y,y-(grid->numLines)) = 1 ;                  

                    y++;                     
                }
            }
            // add one to the k value here
            k++;
            
        }
        
        LOG("Solving grid");
        // calculate the V by mutilpying (invers of A) * (known)
        V = A.inverse() * known.transpose();
        
        LOG("Creating output grid");
        // Fill in the grid with calculated values
        for (int i = 1; i < V.rows() ; i++ )
        {            
            grid->voltages[i] = V(i);
        }
        
    }

}




