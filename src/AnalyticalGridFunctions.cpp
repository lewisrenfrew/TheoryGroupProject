#include "Grid.hpp"
#include "GradientGrid.hpp"
#include "AnalyticalGridFunctions.hpp"
#include "Utility.hpp"
#include <cmath>

namespace AGF
{
    std::pair<Grid,GradientGrid> AnalyticalGridFill0 (const uint lineLength, const uint numLines, const f64 voltage,
                                                      const f64 r2, const f64 r1, const f64 cellsPerMeter)
    {
        // This function takes the above arguments and returns a grid with the analytical solution for problem 0
        // Currently radii are user specified and not obtained directly from the image that depicts the situation

        Grid grid(false, false);
        grid.lineLength = lineLength;
        grid.numLines = numLines;
        // loop over y
        grid.voltages.reserve(numLines*lineLength);
        const f64 cx = (f64)(lineLength-1) / (2.0 * cellsPerMeter);
        const f64 cy = (f64)(numLines-1) / (2.0 * cellsPerMeter);

        GradientGrid efield;
        efield.lineLength = lineLength;
        efield.numLines = numLines;




        for (uint j = 0; j < numLines; j++)
        {
            // loops over x
            for (uint i = 0; i < lineLength; i++)
            {
                const f64 x = (f64)i / cellsPerMeter;
                const f64 y = (f64)j / cellsPerMeter;
                const f64 r = std::hypot(x-cx, y-cy);
                // tests whether a point (i, j) lies inside the circle of radius r1
                if (r < r1)
                {
                    // sets potential to 0 at that point
                    grid.voltages.push_back(0.0);
                    //set gradients in gradientgrid to zero
                    efield.gradients.push_back(V2d(0.0,0.0));
                }
                // tests whether a point (i, j) lies outside the circle of radius r2
                else if (r > r2)
                {
                    // sets potential to 10 for such points - matches our image
                    grid.voltages.push_back(voltage);
                    efield.gradients.push_back(V2d(0.0,0.0));
                    //set gradients int gradientgrid to zero
                }
                else
                {
                    // sets potential to be our solution for all other points
                    grid.voltages.push_back(voltage/log(r2/r1)*(std::log(std::hypot(x-cx, y-cy)/(r1))));
                    //insert analytic solution of gradient and pushback into gradient grid
                    efield.gradients.push_back(V2d(
                                                   -(1/log(r2/r1))*voltage*(x-cx)*(1/((x-cx)*(x-cx)+(y-cy)*(y-cy))),
                                                   -(1/log(r2/r1))*voltage*(y-cy)*(1/((x-cx)*(x-cx)+(y-cy)*(y-cy)))));
                }


            }
        }
        return std::make_pair(grid,efield);
    }



    std::pair<Grid,GradientGrid> AnalyticalGridFill1 (const uint lineLength, const uint numLines, const f64 voltage,
                                                      const double r2, const double r1, const f64 cellsPerMeter)
    {
        // This function fills a grid with the analytical solution for problem 1
        // Recall that the solution used the idea of solving in polar coords  between the circular ground radius r1 and the circle r2 that had diameter = plate spacing
        // Currently radii are user specified and not obtained directly from the image that depicts the situation
        // It is assumed that the parallel plates are the lines x=0 and x=lineLength, i.e the vertical borders of the image with spacing lineLength between them

        Grid grid(false, false);
        grid.lineLength = lineLength;
        grid.numLines = numLines;
        // loops over y
        grid.voltages.reserve(numLines*lineLength);
        const f64 cx = (f64)(lineLength-1) / 2.0 / cellsPerMeter;
        const f64 cy = (f64)(numLines-1) / 2.0 / cellsPerMeter;


        GradientGrid efield;
        efield.lineLength = lineLength;
        efield.numLines = numLines;

        for (uint j = 0; j < numLines; j++)
        {
            // loops over x
            for (uint i = 0; i < lineLength; i++)
            {
                const f64 x = (f64)i / cellsPerMeter;
                const f64 y = (f64)j / cellsPerMeter;
                const f64 r = std::hypot(x-cx,y-cy);
                // tests whether a point i, j lies inside circle of radius r1
                if (r <=  r1)
                {
                    // sets potential to zero for such points
                    grid.voltages.push_back(0.0);
                    efield.gradients.push_back(V2d(0.0, 0.0));
                }
                // tests whether a point lies outwith circle of radius r2
                else if (r >  r2)
                {
                    // potential takes form of parralel plate solution at such points
                    // (SHOULD WE JUST ASSUME THAT POLAR SOLUTION BELOW CORRECTLY DESCRIBES POTENTIAL OUTSIDE r2 AND REMOVE THIS TEST??
                    grid.voltages.push_back(-(2.0*voltage*(x-cx)) / (lineLength / cellsPerMeter));
                    efield.gradients.push_back(V2d((2.0*voltage) / (lineLength / cellsPerMeter),0.0));
                }
                else
                {
                    // create new variables according to polar coorodinate rules
                    double costheta = (x-cx)/r;
                    // set potential to be our polar solution for all remaining points
                    const f64 xx= (x-cx)*(x-cx);
                    const f64 yy= (y-cy)*(y-cy);
                    grid.voltages.push_back((r-r1)*( -voltage/(r2-r1))*costheta);
                    efield.gradients.push_back(V2d
                                               (-(voltage/(r1-r2))-((r1*voltage*(yy))/((r1-r2)*(std::pow(xx+yy,3.0/2.0)))),
                                                -(r1*voltage*(x-cx)*(y-cy)/((r1-r2)*(std::pow(xx+yy,3.0/2.0))))));
                }
            }
        }
        return std::make_pair(grid,efield);
    }
}
