/* ==========================================================================
   $File: Compare.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "Compare.hpp"
#include "Grid.hpp"
#include "GradientGrid.hpp"

namespace Cmp
{
    Jasnah::Option<Grid>
    Difference(const Grid& gridA, const Grid& gridB)
    {
        if (gridA.lineLength != gridB.lineLength
            && gridA.numLines != gridB.numLines)
        {
            return Jasnah::None;
        }

        Grid result(false, false);
        result.lineLength = gridA.lineLength;
        result.numLines = gridA.numLines;

        for (uint i = 0; i < gridA.voltages.size(); ++i)
        {
            result.voltages.push_back(gridA.voltages[i] - gridB.voltages[i]);
        }

        return result;
    }

    Jasnah::Option<GradientGrid>
    Difference(const GradientGrid& gridA, const GradientGrid& gridB)
    {
        if (gridA.lineLength != gridB.lineLength
            && gridA.numLines != gridB.numLines)
        {
            return Jasnah::None;
        }

        GradientGrid result;
        result.lineLength = gridA.lineLength;
        result.numLines = gridA.numLines;

        for (uint i = 0; i < gridA.gradients.size(); ++i)
        {
            result.gradients.push_back(gridA.gradients[i] - gridB.gradients[i]);
        }

        return result;
    }
}
