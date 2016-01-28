#include "Grid.hpp"
#include "AnalyticalGridFunctions.hpp"
#include <cmath>

namespace AGF
{
    Grid AnalyticalGridFill0 (const uint lineLength, const uint numLines, const f64 voltage,
                              const f64 r2, const f64 r1, const f64 cellsPerMeter)
    {
        // This function takes the above arguments and returns a grid with the analytical solution for problem 0
        // Currently radii are user specified and not obtained directly from the image that depicts the situation

        Grid grid(false, false);
        grid.lineLength = lineLength;
        grid.numLines = numLines;
        // loop over y
        grid.voltages.reserve(numLines*lineLength);
        const f64 cx = (f64)lineLength / 2.0 / cellsPerMeter;
        const f64 cy = (f64)numLines / 2.0 / cellsPerMeter;

        for (uint j = 0; j < numLines; j++)

        {
            // loops over x
            for (uint i = 0; i < lineLength; i++)
            {
                const f64 x = (f64)i / cellsPerMeter;
                const f64 y = (f64)j / cellsPerMeter;
                // tests whether a point (i, j) lies inside the circle of radius r1
                if (pow((x-cx),2) + pow((y-cy),2) <=  pow(r1,2.0) )

                {
                    // sets potential to 0 at that point
                    grid.voltages.push_back(0.0);
                }
                // tests whether a point (i, j) lies outside the circle of radius r2
                else if (pow((x-cx),2) + pow((y-cy),2) > pow(r2,2.0) )

                {
                    // sets potential to 10 for such points - matches our image
                    grid.voltages.push_back(10.0);
                }
                else
                {
                    // sets potential to be our solution for all other points
                    grid.voltages.push_back(voltage/log(r2/r1)*(log(std::hypot(x-cx, y-cy)/r1)));
                }


            }
        }
        return grid;
    }



    Grid AnalyticalGridFill1 (const uint lineLength, const uint numLines, const f64 voltage, const double r2, const double r1)
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
        for (uint j = 0; j < numLines; j++)

        {
            // loops over x
            for (uint i = 0; i < lineLength; i++)
            {
                // tests whether a point i, j lies inside circle of radius r1
                if (pow((i-r2),2) + pow((j-r2),2) <=  pow(r1,2.0) )
                {
                    // sets potential to zero for such points
                    grid.voltages.push_back(0.0);
                }
                // tests whether a point lies outwith circle of radius r2
                else if (pow((i-r2),2) + pow((j-r2),2) >  pow(r2,2.0) )
                {
                    // potential takes form of parralel plate solution at such points
                    // (SHOULD WE JUST ASSUME THAT POLAR SOLUTION BELOW CORRECTLY DESCRIBES POTENTIAL OUTSIDE r2 AND REMOVE THIS TEST??
                    grid.voltages.push_back(-(2*voltage*i)/lineLength);
                }
                else
                {
                    // create new variables according to polar coorodinate rules
                    double r = sqrt(pow(i, 2.0) + pow(j, 2.0));
                    double costheta = (double)i/r;
                    // set potential to be our polar solution for all remaining points
                    grid.voltages.push_back((r-r1) - (voltage/(r2-r1))*costheta);
                }


            }
        }
        return grid;
    }
}
