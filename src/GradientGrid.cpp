/* ==========================================================================
   $File: GradientGrid.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "GradientGrid.hpp"
#include "Grid.hpp"

void
GradientGrid::CalculateNegGradient(const Grid& grid, const f64 cellsToMeters)
{
    // Currently using the symmetric derivative method over the
    // inside of the grid: f'(a) ~ (f(a+h) - f(a-h)) / 2h. As we
    // don't have pixels to metres yet, h is 1 from one cell to
    // the adjacent. Outer points are currently set to (0,0),
    // other points are set to (-d/dx Phi, -d/dy Phi)
    JasUnpack(grid, voltages);
    numLines = grid.numLines;
    lineLength = grid.lineLength;

    if (gradients.size() != 0)
        gradients.clear();

    // Yes there's excess construction here, I don't think it will
    // matter though, probably less impact than adding branching
    // to the loop
    gradients.assign(voltages.size(), V2d(0.0,0.0));

    const auto Phi = [&voltages, this](uint x, uint y) -> f64
        {
            return voltages[y*lineLength + x];
        };

    for (uint y = 1; y < numLines - 1; ++y)
        for (uint x = 1; x < lineLength - 1; ++ x)
        {
            const f64 xDeriv = (Phi(x+1, y) - Phi(x-1, y)) / (2.0 / cellsToMeters);
            const f64 yDeriv = (Phi(x, y+1) - Phi(x, y-1)) / (2.0 / cellsToMeters);

            gradients[y * lineLength + x] = V2d(-xDeriv, -yDeriv);
        }
}
