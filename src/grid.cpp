/* ==========================================================================
   $File: grid.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "GlobalDefines.hpp"
#include <vector>
#include <utility>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <atomic>


typedef std::vector<f64> DoubleVec;

// NOTE(Chris): Provide logging for everyone - create in main TU to
// avoid ordering errors, maybe reduce number of TU's...
namespace Log
{
    Lethani::Logfile log;
}

/// Stores the state of the grid. This API will be VERY prone to
/// breakage for now
struct Grid
{
    /// Stores the voltage for each cell
    DoubleVec voltages;
    /// Width of the simulation area
    uint lineLength;
    /// Height of the simulation area
    uint numLines;
    /// Stores the indices and values of the fixed points
    std::vector<std::pair<MemIndex, f64> > fixedPoints;

    /// Default constructors and assignment operators to keep
    /// everything working
    Grid() = default;
    Grid(const Grid&) = default;
    Grid(Grid&&) = default;
    Grid& operator=(const Grid&) = default;
    Grid& operator=(Grid&&) = default;

    /// Sets the two boundary plates for the basic box
    void
    InitialiseBasicGrid(f64 plusWall, f64 minusWall)
    {
        voltages.assign(lineLength*numLines, 0.0);

        for (MemIndex i = 0; i < numLines; ++i)
        {
            AddFixedPoint(0, i, plusWall);
            AddFixedPoint(lineLength - 1, i, minusWall);
        }
    }

    /// Prints the grid to stdout, useful for debugging at low res
    void
    Print()
    {
        for (uint i = 0; i < numLines; ++i)
        {
            for (uint j = 0; j < lineLength; ++j)
            {
                // The code in here is to centre the value within a 7
                // space width as much as possible
                const MemIndex maxBuf = 8;
                char buf[maxBuf];
                const uint len = snprintf(buf, maxBuf, "%1.1g", voltages[i*lineLength + j]);
                for (uint i = 0; i < (maxBuf-len-1)/2; ++i)
                {
                    char tempBuf[maxBuf];
                    memcpy(tempBuf, buf, maxBuf);
                    snprintf(buf, maxBuf, " %s ", tempBuf);
                }
                if ((maxBuf - len - 1) & 1)
                {
                    char tempBuf[maxBuf];
                    memcpy(tempBuf, buf, maxBuf);
                    snprintf(buf, maxBuf, "%s ", tempBuf);
                }
                printf("%s", buf);
            }
            printf("\n");
        }
    }

    /// Fixes this grid's point x, y to val for the duration of this grid
    void
    AddFixedPoint(uint x, uint y, f64 val)
    {
        const uint index = y * lineLength + x;
        auto found = std::find_if(fixedPoints.begin(), fixedPoints.end(),
                                  [index, this](decltype(fixedPoints.front()) val)
                                  {
                                      return val.first == index;
                                  });

        if (found != fixedPoints.end())
        {
            fixedPoints.erase(found);
        }

        voltages[index] = val;
        fixedPoints.push_back(std::make_pair(index, val));
    }
};

/// The function that does the work for now. Repeatedly applies a
/// finite difference scheme to the grid, until the maximum relative
/// change over one iteration is less zeroTol or maxIter iterations
/// have taken place.
void
SolveGridLaplacianZero(Grid* grid, f64 zeroTol, u64 maxIter)
{
    // NOTE(Chris): We need d2phi/dx^2 + d2phi/dy^2 = 0
    // => 1/h^2 * ((phi(x+1,y) - 2phi(x,y) + phi(x-1,y))
    //           + (phi(x,y+1) - 2phi(x,y) + phi(x,y-1))
    // => phi(x,y) = 1/4 * (phi(x+1,y) + phi(x-1,y) + phi(x,y+1) + phi(x,y-1))

    for (u64 i = 0; i < maxIter; ++i)
    {

        // TODO(Chris): If we get hit by slowdown due to the memory
        // allocator (possible on big grids), then implement double
        // buffering
        const DoubleVec prevVoltages(grid->voltages);
        const decltype(prevVoltages)* pVoltage = &prevVoltages;

        // Lambda returning Phi(x,y)
        const auto Phi = [grid, pVoltage](uint x, uint y) -> const f64
            {
                return (*pVoltage)[y * grid->lineLength + x];
            };

#ifndef GOMP
        f64 maxErr = 0.0;
#else
        std::atomic<f64> maxErr(0.0);
#pragma omp parallel for default(none) shared(grid, maxErr)
#endif

        // Loop over array and apply scheme at each point
        for (uint y = 1; y < grid->numLines - 1; ++y)
            for (uint x = 1; x < grid->lineLength - 1; ++x)
            {
                const f64 newVal = 0.25 * (Phi(x+1, y) + Phi(x-1, y) + Phi(x, y+1) + Phi(x, y-1));
                const MemIndex index = y * grid->lineLength + x;

                const auto found = std::find_if(grid->fixedPoints.begin(), grid->fixedPoints.end(),
                                                [index](decltype(grid->fixedPoints.front()) val)
                                                {
                                                    return val.first == index;
                                                });

                if (found != grid->fixedPoints.end())
                {
                    grid->voltages[index] = found->second;
                    // NOTE(Chris): No error on fixed points
                }
                else
                {
                    grid->voltages[index] = newVal;

                    const f64 absErr = std::abs((Phi(x,y) - newVal)/newVal);
                    // Dividing by the old value (Phi) is often dividing
                    // by 0 => infinite err, this will converge towards
                    // the relErr

                    // TODO(Chris): This is merking performance in
                    // multi-core, store for each row then single
                    // thread select over it? - I assume it's this
                    // anyway
                    if (absErr > maxErr)
                    {
                        maxErr = absErr;
                    }
                }


            }

        // Log iteration number and error
#ifndef GOMP
        LOG("Iteration %d, err: %f", i, maxErr);
#else
        LOG("Iteration %d, err: %f", i, maxErr.load());
#endif

        // If we have converged, then break
        if (maxErr < zeroTol)
            break;
    }
}

/// Returns a vector of numPoints points linearly interpolated between
/// v1 and v2, useful for some boundary conditions (top and bottom of
/// finite lengths in problem 1)
DoubleVec
LerpNPointsBetweenVoltages(f64 v1, f64 v2, uint numPoints)
{
    // Set x0 as first point, field has potential v1 here, and v2 at
    // xn. We can interpolate them with the equation of a line:
    // a*xi+b = vi but we let xi = 0 then WLOG b = v1, a = (v2-v1)/(numPts-1)

    f64 a = (v2 - v1)/(f64)(numPoints-1);
    f64 b = v1;

    DoubleVec result;
    result.reserve(numPoints);

    for (uint i = 0; i < numPoints; ++i)
        result.push_back(a*i + b);

    return result;
}

/// Writes the gnuplot data file, to be used with WriteGnuplotFile
/// Returns true on successful write
bool
WriteGridForGnuplot(const Grid& grid, const char* filename = "Plot/Grid.dat")
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    for (uint y = 0; y < grid.numLines; ++y)
    {
        for (uint x = 0; x < grid.lineLength; ++x)
        {
            fprintf(file, "%f ", grid.voltages[y * grid.lineLength + x]);
        }
        fprintf(file, "\n");
    }

    fclose(file);
    return true;
}

/// Prints a script config file for gnuplot, the makefile can run
/// this. Returns true on successful write
bool
WriteGnuplotFile(const Grid& grid,
                 const char* gridDataFile = "Plot/Grid.dat",
                 const char* filename = "Plot/PlotFinal.gpi")
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    fprintf(file, "set terminal png size 1280,720\n"
            "load 'Plot/MorelandColors.plt'\n"
            "set output \"Grid.png\"\n"
            "set xlabel \"x\"\nset ylabel \"y\"\n"
            "set xrange [0:%u]; set yrange [0:%u]\n"
            "set style data lines\n"
            "set title \"Stable Potential (V)\"\n"
            "plot \"%s\" matrix with image title \"Numeric Solution\"",
            grid.lineLength-1,
            grid.numLines-1,
            gridDataFile);

    fclose(file);
    return true;
}

int main(void)
{
    // Initialise
    Grid grid;
    grid.lineLength = 50;
    grid.numLines   = 250;

    grid.InitialiseBasicGrid(10.0, -10.0);

    // NOTE(Chris): Impose central box - assume even lengths here for now
    const uint halfX = grid.lineLength / 2;
    const uint halfY = grid.numLines / 2;

    grid.AddFixedPoint(halfX, halfY, 0.0);
    grid.AddFixedPoint(halfX-1, halfY-1, 0.0);
    grid.AddFixedPoint(halfX, halfY-1, 0.0);
    grid.AddFixedPoint(halfX-1, halfY, 0.0);

    // NOTE(Chris): Fix top and bottom to 0
    // for (uint i = 0; i < grid.lineLength; ++i)
    // {
    //     grid.AddFixedPoint(i, 0, 0.0);
    //     grid.AddFixedPoint(i, grid.numLines-1, 0.0);
    // }

    // Interpolate top and bottom lines
    const DoubleVec topAndBottom = LerpNPointsBetweenVoltages(10, -10, grid.lineLength);
    for (uint i = 0; i < grid.lineLength; ++i)
    {
        grid.AddFixedPoint(i, 0, topAndBottom[i]);
        grid.AddFixedPoint(i, grid.numLines-1, topAndBottom[i]);
    }


    // grid.Print();

    // Solve, converge to 0.1% or stop at 10000 iterations
    SolveGridLaplacianZero(&grid, 0.001, 10000);
    // grid.Print();

    // Write output
    WriteGridForGnuplot(grid);
    WriteGnuplotFile(grid);

    return 0;
}
