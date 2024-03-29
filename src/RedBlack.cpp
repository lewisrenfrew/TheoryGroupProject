/* ==========================================================================
   $File: RedBlack.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "RedBlack.hpp"
#include "Grid.hpp"
#include "Utility.hpp"

#include <cmath>
#include <atomic>
#include <algorithm>

// #include <xmmintrin.h>
// // To enable
// _MM_SET_EXCEPTION_MASK(_MM_GET_EXCEPTION_MASK() & ~_MM_MASK_INVALID);
// // to disable - will enable if disabled
// _MM_SET_EXCEPTION_MASK(_MM_GET_EXCEPTION_MASK() ^ _MM_MASK_INVALID);

namespace RedBlack
{
    /// Max number of threads to be used by OpenMP, empirical testing
    /// suggests that this is close to the peak speed, as fewer
    /// threads imposes reduces throughput and more threads have
    /// additional overheads that slow down the computation time.
    /// Obviously this only applies to the parallel functions
    const uint MaxThreads = 30;

    /// Holds the 3 vectors of points to be zipped to pass to the
    /// functions that handle zipping
    struct PreprocessedGridZips
    {
        typedef std::vector<std::pair<uint, uint> >  VecZip;
        PreprocessedGridZips(VecZip&& h, VecZip&& v, VecZip&& hv)
            : hZip(std::move(h)),
              vZip(std::move(v)),
              hvZip(std::move(hv))
        {}
        PreprocessedGridZips() = delete;
        const VecZip hZip;
        const VecZip vZip;
        const VecZip hvZip;
    };

    /// Data type to hold the two stop conditions (we stop on
    /// whichever comes first)
    struct StopParams
    {
        const f64 zeroTol;
        const u64 maxIter;
        StopParams(f64 _zeroTol, u64 _maxIter) : zeroTol(_zeroTol), maxIter(_maxIter) {}
    };

    /// Multi-threaded implementation of the finite difference method
    /// not taking into account the outer-most row/column (thus these
    /// points will need to be fixed and zipping is not enabled). It
    /// is recommended to use the disptach function to call this
    /// function after verifying its appropriateness
    static
    void
    RedBlackParaNoZip(Grid* grid, const std::vector<uint>& redPts, const std::vector<uint>& blkPts, const StopParams& stop)
    {
        // NOTE(Chris): Multi-threaded variant

        // NOTE(Chris): We need d2phi/dx^2 + d2phi/dy^2 = 0
        // => 1/h^2 * ((phi(x+1,y) - 2phi(x,y) + phi(x-1,y))
        //           + (phi(x,y+1) - 2phi(x,y) + phi(x,y-1))
        // => phi(x,y) = 1/4 * (phi(x+1,y) + phi(x-1,y) + phi(x,y+1) + phi(x,y-1))

        // NOTE(Chris): We never write to the fixed points, so we don't
        // need to re-set them (as long as they were set properly in the
        // incoming grid using AddFixedPoint)

        JasUnpack((*grid), voltages, numLines, lineLength);

        // Check error every 500 iterations at first
        uint errorChunk = 500;

        // Hand-waving 10k iterations per thread as minimum to not be dominated by context switches etc.
        const uint numWorkChunks = (voltages.size() / 20000 > 0) ? (voltages.size() / 20000) : 1;
        const uint numThreads = (numWorkChunks > omp_get_max_threads())
            ? ((MaxThreads > omp_get_max_threads())
               ? omp_get_max_threads()
               : MaxThreads)
            : numWorkChunks;

        omp_set_num_threads(numThreads);

        LOG("Num threads %u", numThreads);

        // Atomic value for concurrent multi-threaded access
        std::atomic<f64> maxErr(0.0);

        // Main loop - start with 1 so as not to take slow path on first iter
        for (u64 i = 1; i <= stop.maxIter; ++i)
        {
            // We will just double buffer these 2 vectors to avoid reallocations new<->old
            // const ref to avoid damage again

            if (unlikely(i % errorChunk == 0))
            {
                maxErr = 0.0;

#pragma omp parallel for default(none) shared(redPts, lineLength, voltages, maxErr)
                for (auto c = redPts.begin(); c < redPts.end(); ++c)
                {
                    const f64 prev = voltages[*c];
                    const f64 newVal = 0.25 * (voltages[*c + 1] + voltages[*c - 1] + voltages[*c - lineLength] + voltages[*c + lineLength]);
                    voltages[*c] = newVal;

                    const f64 absErr = std::abs((prev - newVal)/newVal);

                    const f64 currentMax =  maxErr.load();
                    if (absErr > currentMax)
                    {
                        maxErr = absErr;
                    }
                }

#pragma omp parallel for default(none) shared(blkPts, lineLength, voltages, maxErr)
                for (auto c = blkPts.begin(); c < blkPts.end(); ++c)
                {
                    const f64 prev = voltages[*c];
                    const f64 newVal = 0.25 * (voltages[*c + 1] + voltages[*c - 1] + voltages[*c - lineLength] + voltages[*c + lineLength]);
                    voltages[*c] = newVal;

                    const f64 absErr = std::abs((prev - newVal)/newVal);

                    if (absErr > maxErr)
                    {
                        maxErr = absErr;
                    }
                }

                if (maxErr < stop.zeroTol)
                {
                    LOG("Performed %u iterations, max error: %e", (unsigned)i, maxErr.load());
                    return;
                }

                // NOTE(Chris): Report error every 5000 iterations
                if (i % 5000 == 0)
                {
                    // If this is calculated near the beginning it tends
                    // to overshoot, go to a quarter to refine the counter
                    // (we use a modulo anyway so it will still stop at
                    // the prediction)
                    LOG("Relative change after %u iterations %f", (unsigned)i, maxErr.load());
                }
            }
            else // normal path
            {

#pragma omp parallel for default(none) shared(redPts, lineLength, voltages)
                for (auto c = redPts.begin(); c < redPts.end(); ++c)
                {
                    const f64 newVal = 0.25 * (voltages[*c + 1] + voltages[*c - 1] + voltages[*c - lineLength] + voltages[*c + lineLength]);
                    voltages[*c] = newVal;
                }

#pragma omp parallel for default(none) shared(blkPts, lineLength, voltages)
                for (auto c = blkPts.begin(); c < blkPts.end(); ++c)
                {
                    const f64 newVal = 0.25 * (voltages[*c + 1] + voltages[*c - 1] + voltages[*c - lineLength] + voltages[*c + lineLength]);
                    voltages[*c] = newVal;
                }
            }
        }

    LOG("Overran max iteration counter (%u), max error: %f", (unsigned)stop.maxIter, maxErr.load());
}

/// Multi-threaded implementation of the finite difference method
/// that takes into account that some of the points on the outer
/// rows may need to be zipped. The dispatch function can determine this
static
void
RedBlackParaZip(Grid* grid, const std::vector<uint>& redPts, const std::vector<uint>& blkPts,
           const StopParams& stop, const PreprocessedGridZips& zips)
{
        // NOTE(Chris): Multi-threaded variant

        // NOTE(Chris): We need d2phi/dx^2 + d2phi/dy^2 = 0
        // => 1/h^2 * ((phi(x+1,y) - 2phi(x,y) + phi(x-1,y))
        //           + (phi(x,y+1) - 2phi(x,y) + phi(x,y-1))
        // => phi(x,y) = 1/4 * (phi(x+1,y) + phi(x-1,y) + phi(x,y+1) + phi(x,y-1))

        // NOTE(Chris): We never write to the fixed points, so we don't
        // need to re-set them (as long as they were set properly in the
        // incoming grid using AddFixedPoint)

        JasUnpack((*grid), voltages, numLines, lineLength);
        JasUnpack(zips, hZip, vZip, hvZip);

        // Check error every 500 iterations at first
        uint errorChunk = 500;

        // Hand-waving 10k iterations per thread as minimum to not be dominated by context switches etc.
        const uint numWorkChunks = (voltages.size() / 20000 > 0) ? (voltages.size() / 20000) : 1;
        const uint numThreads = (numWorkChunks > omp_get_max_threads())
            ? ((MaxThreads > omp_get_max_threads())
               ? omp_get_max_threads()
               : MaxThreads)
            : numWorkChunks;

        omp_set_num_threads(numThreads);

        LOG("Num threads %u", numThreads);

        const auto WrapGridAccessNewVal =
            [&voltages, lineLength, numLines] (const std::pair<uint,uint>& pt) -> f64
            {
                const uint id1 = pt.second * lineLength + (((int)pt.first - 1) < 0
                                                           ? lineLength - 1
                                                           : pt.first - 1);

                const uint id2 = pt.second * lineLength + (pt.first + 1 >= lineLength
                                                           ? 0
                                                           : pt.first + 1);
                const uint id3 = (((int)pt.second - 1) < 0
                                  ? numLines - 1
                                  : pt.second - 1) * lineLength + pt.first;

                const uint id4 = (pt.second + 1 >= numLines
                                  ? 0
                                  : pt.second + 1) * lineLength + pt.first;

                const f64 newVal = 0.25*(voltages[id1] + voltages[id2]
                                         + voltages[id3] + voltages[id4]);
                return newVal;
            };

        // Atomic value for concurrent multi-threaded access
        std::atomic<f64> maxErr(0.0);

        // Main loop - start with 1 so as not to take slow path on first iter
        for (u64 i = 1; i <= stop.maxIter; ++i)
        {
            // We will just double buffer these 2 vectors to avoid reallocations new<->old
            // const ref to avoid damage again

            if (unlikely(i % errorChunk == 0))
            {
                maxErr = 0.0;

#pragma omp parallel for default(none) shared(redPts, lineLength, voltages, maxErr)
                for (auto c = redPts.begin(); c < redPts.end(); ++c)
                {
                    const f64 prev = voltages[*c];
                    const f64 newVal = 0.25 * (voltages[*c + 1] + voltages[*c - 1] + voltages[*c - lineLength] + voltages[*c + lineLength]);
                    voltages[*c] = newVal;

                    const f64 absErr = std::abs((prev - newVal)/newVal);

                    const f64 currentMax =  maxErr.load();
                    if (absErr > currentMax)
                    {
                        maxErr = absErr;
                    }
                }

#pragma omp parallel for default(none) shared(blkPts, lineLength, voltages, maxErr)
                for (auto c = blkPts.begin(); c < blkPts.end(); ++c)
                {
                    const f64 prev = voltages[*c];
                    const f64 newVal = 0.25 * (voltages[*c + 1] + voltages[*c - 1] + voltages[*c - lineLength] + voltages[*c + lineLength]);
                    voltages[*c] = newVal;

                    const f64 absErr = std::abs((prev - newVal)/newVal);

                    if (absErr > maxErr)
                    {
                        maxErr = absErr;
                    }
                }

                for (const auto& coord : hZip)
                {
                    const uint index = coord.second * lineLength + coord.first;
                    const f64 prev = voltages[index];
                    const f64 newVal = WrapGridAccessNewVal(coord);
                    voltages[index] = newVal;
                    const f64 absErr = std::abs((prev - newVal)/newVal);

                    if (absErr > maxErr)
                    {
                        maxErr = absErr;
                    }
                }

                for (const auto& coord : vZip)
                {
                    const uint index = coord.second * lineLength + coord.first;
                    const f64 prev = voltages[index];
                    const f64 newVal = WrapGridAccessNewVal(coord);
                    voltages[index] = newVal;
                    const f64 absErr = std::abs((prev - newVal)/newVal);

                    if (absErr > maxErr)
                    {
                        maxErr = absErr;
                    }
                }

                for (const auto& coord : hvZip)
                {
                    const uint index = coord.second * lineLength + coord.first;
                    const f64 prev = voltages[index];
                    const f64 newVal = WrapGridAccessNewVal(coord);
                    voltages[index] = newVal;
                    const f64 absErr = std::abs((prev - newVal)/newVal);

                    if (absErr > maxErr)
                    {
                        maxErr = absErr;
                    }
                }


                if (maxErr < stop.zeroTol)
                {
                    LOG("Performed %u iterations, max error: %e", (unsigned)i, maxErr.load());
                    return;
                }

                // NOTE(Chris): Report error every 5000 iterations
                if (i % 5000 == 0)
                {
                    // If this is calculated near the beginning it tends
                    // to overshoot, go to a quarter to refine the counter
                    // (we use a modulo anyway so it will still stop at
                    // the prediction)
                    LOG("Relative change after %u iterations %f", (unsigned)i, maxErr.load());
                }
            }
            else // normal path
            {

#pragma omp parallel for default(none) shared(redPts, lineLength, voltages)
                for (auto c = redPts.begin(); c < redPts.end(); ++c)
                {
                    const f64 newVal = 0.25 * (voltages[*c + 1] + voltages[*c - 1] + voltages[*c - lineLength] + voltages[*c + lineLength]);
                    voltages[*c] = newVal;
                }

#pragma omp parallel for default(none) shared(blkPts, lineLength, voltages)
                for (auto c = blkPts.begin(); c < blkPts.end(); ++c)
                {
                    const f64 newVal = 0.25 * (voltages[*c + 1] + voltages[*c - 1] + voltages[*c - lineLength] + voltages[*c + lineLength]);
                    voltages[*c] = newVal;
                }

                for (const auto& coord : hZip)
                {
                    const f64 newVal = WrapGridAccessNewVal(coord);
                    const uint index = coord.second * lineLength + coord.first;
                    voltages[index] = newVal;
                }

                for (const auto& coord : vZip)
                {
                    const f64 newVal = WrapGridAccessNewVal(coord);
                    const uint index = coord.second * lineLength + coord.first;
                    voltages[index] = newVal;
                }

                for (const auto& coord : hvZip)
                {
                    const f64 newVal = WrapGridAccessNewVal(coord);
                    const uint index = coord.second * lineLength + coord.first;
                    voltages[index] = newVal;
                }
            }
        }

    LOG("Overran max iteration counter (%u), max error: %f", (unsigned)stop.maxIter, maxErr.load());
}

/// Single threaded RedBlack implementation that ignores
/// the outer row/column of points where points may need to be
/// fixed. Thus, these all need to be fixed points. If handled by
/// the dispatch function then this is all handled automagically
static
void
RedBlackSingleNoZip(Grid* grid, const std::vector<uint>& redPts, const std::vector<uint>& blkPts, const StopParams& stop)
{
    JasUnpack((*grid), voltages, lineLength);

    // Check error every 500 iterations at first
    const uint errorChunk = 500;

    // Max relative change
    f64 maxErr = 0.0;
    // Main loop - start from 1 so as not to calculate error on first iteration
    for (u64 i = 1; i <= stop.maxIter; ++i)
    {
        // Unlikely means the branch predictor will always go the
        // other way, it will have to backtrack on the very rare
        // cases that this comes up (1 in errorChunk times). This
        // is a penalty of < 200 cycles on those rare occasions

        // Calculate and check error every chunk (500 iterations by default)
        if (unlikely(i % errorChunk == 0))
        {
            maxErr = 0.0;

            // Loop over the non-fixed points
            for (const auto c : redPts)
            {
                const f64 prevVal = voltages[c];
                const f64 newVal = 0.25 * (voltages[c + 1] + voltages[c - 1] + voltages[c - lineLength] + voltages[c + lineLength]);

                voltages[c] = newVal;
                const f64 absErr = std::abs((prevVal - newVal)/newVal);

                if (absErr > maxErr)
                {
                    maxErr = absErr;
                }
            }
            for (const auto c : blkPts)
            {
                const f64 prevVal = voltages[c];
                const f64 newVal = 0.25 * (voltages[c + 1] + voltages[c - 1] + voltages[c - lineLength] + voltages[c + lineLength]);

                voltages[c] = newVal;
                const f64 absErr = std::abs((prevVal - newVal)/newVal);

                if (absErr > maxErr)
                {
                    maxErr = absErr;
                }
            }

            // If we have converged, then break by leaving the function
            if (maxErr < stop.zeroTol)
            {
                LOG("Performed %u iterations, max error: %f", (unsigned)i, maxErr);
                return;
            }

            if (i % 5000 == 0)
            {
                // Output current error every 5000 iterations
                LOG("Relative change after %u iterations %f", (unsigned)i, maxErr);
            }
        }
        else // normal path
        {
            // Loop over the non-fixed points and apply the RedBlack,
            // no error calculations -- that comparison is costly for
            // the cache
            for (const auto c : redPts)
            {
                const f64 newVal = 0.25 * (voltages[c + 1] + voltages[c - 1] + voltages[c - lineLength] + voltages[c + lineLength]);
                voltages[c] = newVal;
            }
            for (const auto c : blkPts)
            {
                const f64 newVal = 0.25 * (voltages[c + 1] + voltages[c - 1] + voltages[c - lineLength] + voltages[c + lineLength]);
                voltages[c] = newVal;
            }
        }
    }
    LOG("Overran max iteration counter (%u), max error: %f", (unsigned)stop.maxIter, maxErr);
}

/// Single threaded finite difference method that takes into
/// account that some points need to be zipped
static
void
RedBlackSingleZip(Grid* grid, const std::vector<uint>& redPts, const std::vector<uint>& blkPts,
             const StopParams& stop, const PreprocessedGridZips& zips)
{
    JasUnpack((*grid), voltages, lineLength, numLines);
    JasUnpack(zips, hZip, vZip, hvZip);

    const auto WrapGridAccessNewVal =
        [&voltages, lineLength, numLines] (const std::pair<uint,uint>& pt) -> f64
        {
            const uint id1 = pt.second * lineLength + (((int)pt.first - 1) < 0
                                                        ? lineLength - 1
                                                        : pt.first - 1);

            const uint id2 = pt.second * lineLength + (pt.first + 1 >= lineLength
                                                        ? 0
                                                        : pt.first + 1);
            const uint id3 = (((int)pt.second - 1) < 0
                                ? numLines - 1
                                : pt.second - 1) * lineLength + pt.first;

            const uint id4 = (pt.second + 1 >= numLines
                                ? 0
                                : pt.second + 1) * lineLength + pt.first;

            const f64 newVal = 0.25*(voltages[id1] + voltages[id2]
                                        + voltages[id3] + voltages[id4]);
            return newVal;
        };

    // Check error every 500 iterations at first
    const uint errorChunk = 500;

    f64 maxErr = 0.0;
    // Main loop - start from 1 so as not to calculate error on first iteration
    for (u64 i = 1; i <= stop.maxIter; ++i)
    {
        if (unlikely(i % errorChunk == 0))
        {
            maxErr = 0.0;

            // Loop over the non-fixed points
            for (const auto c : redPts)
            {
                const f64 prevVal = voltages[c];
                const f64 newVal = 0.25 * (voltages[c + 1] + voltages[c - 1] + voltages[c - lineLength] + voltages[c + lineLength]);

                voltages[c] = newVal;
                const f64 absErr = std::abs((prevVal - newVal)/newVal);

                if (absErr > maxErr)
                {
                    maxErr = absErr;
                }
            }

            for (const auto c : blkPts)
            {
                const f64 prevVal = voltages[c];
                const f64 newVal = 0.25 * (voltages[c + 1] + voltages[c - 1] + voltages[c - lineLength] + voltages[c + lineLength]);

                voltages[c] = newVal;
                const f64 absErr = std::abs((prevVal - newVal)/newVal);

                if (absErr > maxErr)
                {
                    maxErr = absErr;
                }
            }

            for (const auto& coord : hZip)
            {
                const uint index = coord.second * lineLength + coord.first;
                const f64 prev = voltages[index];
                const f64 newVal = WrapGridAccessNewVal(coord);
                voltages[index] = newVal;
                const f64 absErr = std::abs((prev - newVal)/newVal);

                if (absErr > maxErr)
                {
                    maxErr = absErr;
                }
            }

            for (const auto& coord : vZip)
            {
                const uint index = coord.second * lineLength + coord.first;
                const f64 prev = voltages[index];
                const f64 newVal = WrapGridAccessNewVal(coord);
                voltages[index] = newVal;
                const f64 absErr = std::abs((prev - newVal)/newVal);

                if (absErr > maxErr)
                {
                    maxErr = absErr;
                }
            }

            for (const auto& coord : hvZip)
            {
                const uint index = coord.second * lineLength + coord.first;
                const f64 prev = voltages[index];
                const f64 newVal = WrapGridAccessNewVal(coord);
                voltages[index] = newVal;
                const f64 absErr = std::abs((prev - newVal)/newVal);

                if (absErr > maxErr)
                {
                    maxErr = absErr;
                }
            }

            // If we have converged, then break by leaving the function
            if (maxErr < stop.zeroTol)
            {
                LOG("Performed %u iterations, max error: %f", (unsigned)i, maxErr);
                return;
            }

            // Log error every 5000 iterations
            if (i % 5000 == 0)
            {
                LOG("Relative change after %u iterations %f", (unsigned)i, maxErr);
            }
        }
        else // normal path
        {
            // Loop over the non-fixed points and apply the RedBlack
            for (const auto c : redPts)
            {
                const f64 newVal = 0.25 * (voltages[c + 1] + voltages[c - 1] + voltages[c - lineLength] + voltages[c + lineLength]);
                voltages[c] = newVal;
            }
            for (const auto c : blkPts)
            {
                const f64 newVal = 0.25 * (voltages[c + 1] + voltages[c - 1] + voltages[c - lineLength] + voltages[c + lineLength]);
                voltages[c] = newVal;
            }

            // Handle the exterior Zip points by wrapping around the grid
            for (const auto& coord : hZip)
            {
                const f64 newVal = WrapGridAccessNewVal(coord);
                const uint index = coord.second * lineLength + coord.first;
                voltages[index] = newVal;
            }

            for (const auto& coord : vZip)
            {
                const f64 newVal = WrapGridAccessNewVal(coord);
                const uint index = coord.second * lineLength + coord.first;
                voltages[index] = newVal;
            }

            for (const auto& coord : hvZip)
            {
                const f64 newVal = WrapGridAccessNewVal(coord);
                const uint index = coord.second * lineLength + coord.first;
                voltages[index] = newVal;
            }
        }
    }
    LOG("Overran max iteration counter (%u), max error: %f", (unsigned)stop.maxIter, maxErr);
}

/// Types of possible problems with Zip definition
enum class ZipDefinitionProblem
{
    Horizontal,
        Vertical,
        Both,
        None
        };

/// Checks the validity of the grid WRT the zip settings and
/// returns the problem with it (if there is one)
static
ZipDefinitionProblem
CheckGridZips(const Grid& grid)
{
    JasUnpack(grid, verticZip, horizZip, numLines, lineLength, fixedPoints);

    if (!horizZip || !verticZip)
    {
        if (fixedPoints.count(0) == 0
            || fixedPoints.count(lineLength - 1) == 0
            || fixedPoints.count((numLines - 1) * lineLength) == 0
            || fixedPoints.count((numLines - 1) * lineLength + lineLength - 1) == 0)
        {
            LOG("Badly described grid, some outer points are not set, but the relevant zip is not enabled");
            return ZipDefinitionProblem::Both;
        }
    }

    if (!verticZip)
    {
        // Check first and final column for empty pixels (corners require more specific check)
        for (uint y = 1; y < numLines - 1; ++y)
        {
            if (fixedPoints.count(y * lineLength) == 0
                || fixedPoints.count(y * lineLength + lineLength - 1) == 0)
            {
                LOG("Badly described grid, some outer points are not set, but the relevant zip is not enabled");
                return ZipDefinitionProblem::Vertical;
            }
        }
    }

    if (!horizZip)
    {
        // Check first and final row for empty pixels (corners require more specific check)
        for (uint x = 1; x < lineLength - 1; ++x)
        {
            if (fixedPoints.count(x) == 0
                || fixedPoints.count((numLines - 1) * lineLength + x) == 0)
            {
                LOG("Badly described grid, some outer points are not set, but the relevant zip is not enabled");
                return ZipDefinitionProblem::Horizontal;
            }
        }
    }
    return ZipDefinitionProblem::None;
}

/// Prepares vectors of the zipped points (the ones which require
/// special overlap treatment), and returns a struct of these 3
/// vectors
static
PreprocessedGridZips
PreprocessGridZips(const Grid& grid)
{
    std::vector<std::pair<uint, uint> > horizZipPoints;
    std::vector<std::pair<uint, uint> > verticZipPoints;
    std::vector<std::pair<uint, uint> > horizAndVerticZipPoints;

    JasUnpack(grid, verticZip, horizZip, lineLength, numLines, fixedPoints);
    if (horizZip)
    {
        horizZipPoints.reserve(2 * lineLength);
        for (uint x = 1; x < lineLength - 1; ++x)
        {
            if (fixedPoints.count(x) == 0)
            {
                horizZipPoints.push_back(std::make_pair(x, 0));
            }

            if (fixedPoints.count((numLines - 1) * lineLength + x) == 0)
            {
                horizZipPoints.push_back(std::make_pair(x, numLines - 1));
            }
        }
    }
    // Sort automatically compares first, then second types for pairs
    std::sort(horizZipPoints.begin(), horizZipPoints.end());

    if (verticZip)
    {
        verticZipPoints.reserve(2*numLines);
        for (uint y = 1; y < numLines - 1; ++y)
        {
            if (fixedPoints.count(y * lineLength) == 0)
            {
                verticZipPoints.push_back(std::make_pair(0, y));
            }
            if (fixedPoints.count(y * lineLength + lineLength - 1) == 0)
            {
                verticZipPoints.push_back(std::make_pair(lineLength - 1, y));
            }
        }
    }
    std::sort(verticZipPoints.begin(), verticZipPoints.end());

    if (horizZip && verticZip)
    {
        horizAndVerticZipPoints.reserve(4);
        if (fixedPoints.count(0) == 0)
        {
            horizAndVerticZipPoints.push_back(std::make_pair(0,0));
        }
        if (fixedPoints.count(lineLength - 1) == 0)
        {
            horizAndVerticZipPoints.push_back(std::make_pair(lineLength, 0));
        }
        if (fixedPoints.count((numLines - 1) * lineLength) == 0)
        {
            horizAndVerticZipPoints.push_back(std::make_pair(0, numLines - 1));
        }
        if (fixedPoints.count((numLines - 1) * lineLength + lineLength - 1) == 0)
        {
            horizAndVerticZipPoints.push_back(std::make_pair(lineLength - 1, numLines - 1));
        }
    }
    std::sort(horizAndVerticZipPoints.begin(), horizAndVerticZipPoints.end());

    return PreprocessedGridZips(std::move(horizZipPoints),
                                std::move(verticZipPoints),
                                std::move(horizAndVerticZipPoints));
}

/// The dispatch function for finite difference method. Checks the
/// validity of the grid WRT zip parameters and then dispatches it
/// to 1 of 4 worker functions, depending on whether it has zips,
/// and whether we are running parallel code or not
void
RedBlackSolver(Grid* grid, const f64 zeroTol,
               const u64 maxIter, bool parallel)
{
    TIME_FUNCTION();

    // NOTE(Chris): This function dispatches the calculation to
    // the most appropriate function (zips or not, parallel or
    // not), after having preprocessed the grid to check its
    // validity

    JasUnpack((*grid), horizZip, verticZip, numLines, lineLength, fixedPoints);

    // NOTE(Chris): We choose to trade off some memory for computation
    // speed inside the loop
    std::vector<uint> coordRangeRed; //non-fixed red points
    coordRangeRed.reserve(numLines*lineLength / 2);

    std::vector<uint> coordRangeBlack; //non-fixed red points
    coordRangeBlack.reserve(numLines*lineLength / 2);

    // Ignore outer boundary (handled by zips)
    for (uint y = 1; y < numLines - 1; ++y)
        for (uint x = 1; x < lineLength - 1; ++x)
        {
            const uint index = y * lineLength + x;
            if (fixedPoints.count(index) == 0)
            {
                if ((index % lineLength) % 2 == 0)
                {
                    coordRangeRed.push_back(index);
                }
                else
                {
                    coordRangeBlack.push_back(index);
                }

            }
        }

    switch (CheckGridZips(*grid))
    {
    case ZipDefinitionProblem::Both:
    {
        LOG("Check both zip definitions");
        return;
    } break;

    case ZipDefinitionProblem::Horizontal:
    {
        LOG("Check horizontal zip definition");
        return;
    } break;

    case ZipDefinitionProblem::Vertical:
    {
        LOG("Check vertical zip definition");
        return;
    } break;

    default:
    {

    } break;
    }

    // NOTE(Chris): No need to use the more complex parallel routines
    // if we only have 1 thread available
    if (omp_get_max_threads() == 1)
        parallel = false;

    if (!verticZip && !horizZip)
    {
        if (parallel)
        {
            RedBlackParaNoZip(grid, coordRangeRed, coordRangeBlack, StopParams(zeroTol, maxIter));
        }
        else
        {
            RedBlackSingleNoZip(grid, coordRangeRed, coordRangeBlack, StopParams(zeroTol, maxIter));
        }
        return;
    }

    auto zips = PreprocessGridZips(*grid);

    if (parallel)
    {
        RedBlackParaZip(grid, coordRangeRed, coordRangeBlack, StopParams(zeroTol, maxIter), zips);
    }
    else
    {
        RedBlackSingleZip(grid, coordRangeRed, coordRangeBlack, StopParams(zeroTol, maxIter), zips);
    }
}
}
