#if !defined(AGF_H)

#define AGF_H

#include "GlobalDefines.hpp"
class Grid;


namespace AGF
{
    Grid
    AnalyticalGridFill0 (const uint lineLength, const uint numLines, const f64 voltage,
                         const f64 r2, const f64 r1, const f64 cellsPerMeter);

    Grid
    AnalyticalGridFill1 (const uint lineLength, const uint numLines, const f64 voltage,
                         const double r2, const double r1, const f64 cellsPerMeter);
}


#endif
