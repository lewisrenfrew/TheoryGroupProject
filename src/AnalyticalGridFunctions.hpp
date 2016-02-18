#if !defined(AGF_H)

#define AGF_H

#include "GlobalDefines.hpp"
#include "GradientGrid.hpp"
class Grid;


namespace AGF
{
    /// Generates the analytical solution to Problem 0 based on the
    /// provided grid sizes and voltages and provided simulation
    /// parameters
    std::pair<Grid,GradientGrid>
    AnalyticalGridFill0 (const uint lineLength, const uint numLines, const f64 voltage,
                         const f64 r2, const f64 r1, const f64 cellsPerMeter);

    /// Generates the analytical solution to Problem 1 based on the
    /// provided grid sizes and voltages and provided simulation
    /// parameters
    std::pair<Grid,GradientGrid>
    AnalyticalGridFill1 (const uint lineLength, const uint numLines, const f64 voltage,
                         const double r2, const double r1, const f64 cellsPerMeter);
}


#endif
