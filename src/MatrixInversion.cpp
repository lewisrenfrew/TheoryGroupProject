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

namespace MatrixInversion
{
    void
    MatrixInversionMethod(Grid* grid, const f64 stopPoint , const u64 maxIter)
    {
        using namespace Eigen;
        // get the matrix from grid
        // Matrix<f64, Dynamic, Dynamic> Ad;
        // Matrix<f64, 1, Dynamic> known;
        // Matrix<f64, 1, Dynamic> V;
        //ArrayXXf A = ArrayXXf::Zero(grid->lineLength*grid->numLines
        //, grid->lineLength * grid->numLines);
        
        MatrixXd A = MatrixXd::Zero(grid->lineLength * grid->numLines
                      , grid->lineLength * grid->numLines);
              
        MatrixXd V = MatrixXd::Zero(1,(*grid).lineLength * (*grid).numLines);
        MatrixXd known(1,(*grid).lineLength * (*grid).numLines);
        int k=0;
        int j=1;

        // find the nodes
        // grid->lineLength  (how to find the line wrapping)
        // start loop through number of nodes
        for (int y = 0; y < A.cols() -2;)
        {
            
            //std::cout<<"y is: "<<y<< "\n";           
            for (uint x = 0; x <grid->lineLength ; ++x)
            {
                
                if(y >= A.cols())
                {
                    continue;
                }
                if ((*grid).fixedPoints.count(k * grid->numLines + x) != 0)
                {
                    
                   
                    //put that value in the cloulum vector C
                    A(y,y) = 1;
                   
                    known(0,y)=(*grid).voltages[k * grid->numLines + x];
                    std::cout<<known(0,y);
                    //std::cout<<A(y,y);
                    //std::cin>>j;
                    //std::cout<<y<<A.row(y)<<"\n"; 
                    y++;
                    j++;
                    
                    
                        
                }

                else
                {
                   
                    
                     A(y,y)= -4;
                    //put 1 in A one left and one right of current place
                     if(y<A.cols()-1)
                     {
                         
                         A(y,y+1) = 1;
                         
                     }
                     //if(y<A.cols()-1)
                     {
                                                  
                         A(y,y-1) = 1;
                         
                     }
                     
                     // put 1 in A one up and one down of current place
                     if( y+grid->numLines<A.cols())
                     {
                         
                         A(y,y+grid->numLines) = 1;
                                               
                     }

                     if(!(y <= grid->numLines))
                     {                         
                         A(y,y-(grid->numLines)) = 1 ;                  
                     }                   
                     
                     // put a 0 in the known  because don't know this point
                     known(0,y) = 0;
                     //std::cout<<y<<A.row(y)<<"\n"; 
                     std::cout<<known(0,y);
                     y++;
                     j++;
                }               
                
               
            }
            
            k++;
            
            std::cout<<"\n";
            if (y != 0 )
            {
                // std::cout<<"y: "<<y <<" \n";
            }
        }
        
        //std::cout<<A<<"\n";
        // calculate the V bu mutilpying (invers of A) * (known)
        
       
        // FullPivLU<decltype(A)> LU(A);
        if(1==1)//LU.isInvertible())
        {
            std::cout<<"it passed!! : "<< "\n"; 
            V = A.inverse() * known.transpose();
        }
        // //TODO change to transpose
        // else
        // {
        //     throw std::invalid_argument("not invertible");
        // }
               
           
        
    //(std::exception& ex)
        // {
        //     std::cout << ex.what() << std::endl;
        // }
 
        
        std::cout<<"filling in \n"; 
        for (int i = 1; i < V.rows() ; i++ )
        {
        
            //std::cout<<V(i-1)<<" \n";
            grid->voltages[i] = V(i);
        }
        std::cout<<" done \n\n";
    }

}




