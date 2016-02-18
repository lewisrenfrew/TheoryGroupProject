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
    FDMParaNoZip(Grid* grid, const std::vector<uint>& redPts, const std::vector<uint>& blkPts, const StopParams& stop)
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
        const uint numWorkChunks = (voltages.size() / 2 / 10000 > 0) ? voltages.size() / 2 /  10000 : 1;
        const uint numThreads = (voltages.size() / 2 / omp_get_max_threads() >= 10000)
            ? (omp_get_max_threads() > MaxThreads
               ? MaxThreads
               : omp_get_max_threads())
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

            // Loop over threads (split per thread and complete work chunk)
            // Unlikely means the branch predictor will always go the
            // other way, it will have to backtrack on the very rare
            // cases that this comes up (1 in errorChunk times). This
            // is a penalty of < 200 cycles on those rare occasions
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

                // Else set new target if possible. If we plot the error
                // per iteration we not that it remains fixed at 1.0 until
                // all cells have been filled, after this point it drops
                // roughly as k*i^{-2} where k is a constant and i is the
                // number of iterations. => err_i * i^2 = err_j * j^2. So
                // when err_j is zeroTol, and err_i has been calculated we
                // can find an approximate value for j
                if (maxErr < 1.0)
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
FDMParaZip(Grid* grid, const std::vector<uint>& coordRange,
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

    // Create the previous voltage array from the current array
    decltype(grid->voltages) prevVoltages(voltages);

    // Check error every 500 iterations at first
    uint errorChunk = 500;

    // Hand-waving 10k iterations per thread as minimum to not be dominated by context switches etc.
    const uint numWorkChunks = (prevVoltages.size() / 10000 > 0) ? prevVoltages.size() / 10000 : 1;
    const uint numThreads = (prevVoltages.size() / omp_get_max_threads() >= 10000)
        ? (omp_get_max_threads() > MaxThreads ? MaxThreads : omp_get_max_threads())
        : numWorkChunks;
    omp_set_num_threads(numThreads);
    // remaining points that don't divide between the threads
    const uint rem = prevVoltages.size() % numThreads;
    // size of work chunk for each thread
    const uint blockSize = (prevVoltages.size() - rem) / numThreads;
    LOG("Num threads %u, remainders %u", numThreads, rem);

    // Atomic value for concurrent multi-threaded access
    std::atomic<f64> maxErr(0.0);

    // Main loop - start with 1 so as not to take slow path on first iter
    for (u64 i = 1; i <= stop.maxIter; ++i)
    {
        // We will just double buffer these 2 vectors to avoid reallocations new<->old
        std::swap(prevVoltages, voltages);
        // const ref to avoid damage again
        const decltype(grid->voltages)& pVoltage = prevVoltages;

        // Used to wrap zip access
        const auto WrapGridAccessNewVal =
            [&pVoltage, lineLength, numLines] (const std::pair<uint,uint>& pt) -> f64
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

                const f64 newVal = 0.25*(pVoltage[id1] + pVoltage[id2]
                                         + pVoltage[id3] + pVoltage[id4]);
                return newVal;
            };

        // Loop over threads (split per thread and complete work chunk)
#pragma omp parallel for default(none) shared(errorChunk, coordRange, lineLength, voltages, maxErr, i, pVoltage)
        for (uint thread = 0; thread < numThreads; ++thread)
        {
            // chunk start and end
            const auto start = coordRange.begin() + thread * blockSize;
            const auto end = (start + blockSize > coordRange.end()) ? coordRange.end() : start + blockSize;
            // Unlikely means the branch predictor will always go the
            // other way, it will have to backtrack on the very rare
            // cases that this comes up (1 in errorChunk times). This
            // is a penalty of < 200 cycles on those rare occasions
            if (unlikely(i % errorChunk ==0))
            {
                // Atomic comparisons are slow compared to normal
                // scalars, so declare a thread-local maxErr and then
                // just update the main one at the end
                f64 threadMaxErr = 0.0;
                maxErr = 0.0;

                // Loop over the non-fixed points in the threaded region
                for (auto coord = start; coord < end; ++coord)
                {
                    // Apply finite difference method and calculate error
                    const f64 newVal = 0.25 * (pVoltage[(*coord) + 1] + pVoltage[(*coord) - 1] + pVoltage[(*coord) - lineLength] + pVoltage[(*coord) + lineLength]);
                    voltages[*coord] = newVal;
                    const f64 absErr = std::abs((pVoltage[*coord] - newVal)/newVal);

                    if (absErr > threadMaxErr)
                    {
                        threadMaxErr = absErr;
                    }
                }

                // Update global error
                if (threadMaxErr > maxErr)
                {
                    maxErr = threadMaxErr;
                }
            }
            else // normal path
            {
                // Loop over the non-fixed points and apply the FDM only
                for (auto coord = start; coord < end; ++coord)
                {
                    const f64 newVal = 0.25 * (pVoltage[(*coord) + 1] + pVoltage[(*coord) - 1] + pVoltage[(*coord) - lineLength] + pVoltage[(*coord) + lineLength]);
                    voltages[*coord] = newVal;
                }
            }
        }

        // Handle the remaining non-fixed cells of the array that were
        // not handled by the threaded section - otherwise almost the
        // same as immediately above, but also reassigns the fixed
        // points
        const auto remBegin = coordRange.begin() + (numThreads) * blockSize;
        const auto remEnd = coordRange.end();
        if (unlikely(i % errorChunk == 0)) // several differences in this branch
        {
            f64 localMaxErr = 0.0;
            const auto remBegin = coordRange.begin() + (numThreads) * blockSize;
            const auto remEnd = coordRange.end();
            for (auto coord = remBegin; coord < remEnd; ++coord)
            {
                const f64 newVal = 0.25 * (pVoltage[(*coord) + 1] + pVoltage[(*coord) - 1] + pVoltage[(*coord) - lineLength] + pVoltage[(*coord) + lineLength]);
                voltages[*coord] = newVal;
                const f64 absErr = std::abs((pVoltage[*coord] - newVal)/newVal);

                if (absErr > localMaxErr)
                    localMaxErr = absErr;
            }

            for (const auto& coord : hZip)
            {
                const f64 newVal = WrapGridAccessNewVal(coord);
                const uint index = coord.second * lineLength + coord.first;
                voltages[index] = newVal;
                const f64 absErr = std::abs((pVoltage[coord.second * lineLength + coord.first] - newVal)/newVal);

                if (absErr > localMaxErr)
                {
                    localMaxErr = absErr;
                }
            }

            for (const auto& coord : vZip)
            {
                const f64 newVal = WrapGridAccessNewVal(coord);
                const uint index = coord.second * lineLength + coord.first;
                voltages[index] = newVal;
                const f64 absErr = std::abs((pVoltage[coord.second * lineLength + coord.first] - newVal)/newVal);

                if (absErr > localMaxErr)
                {
                    localMaxErr = absErr;
                }
            }

            for (const auto& coord : hvZip)
            {
                const f64 newVal = WrapGridAccessNewVal(coord);
                const uint index = coord.second * lineLength + coord.first;
                voltages[index] = newVal;
                const f64 absErr = std::abs((pVoltage[coord.second * lineLength + coord.first] - newVal)/newVal);

                if (absErr > localMaxErr)
                {
                    localMaxErr = absErr;
                }
            }

            if (localMaxErr > maxErr)
            {
                maxErr = localMaxErr;
            }

            // If we have converged, then break by leaving the function
            // TODO(Chris): Time logging
            if (maxErr < stop.zeroTol)
            {
                LOG("Performed %u iterations, max error: %e", (unsigned)i, maxErr.load());
                return;
            }

            // Else set new target if possible. If we plot the error
            // per iteration we not that it remains fixed at 1.0 until
            // all cells have been filled, after this point it drops
            // roughly as k*i^{-2} where k is a constant and i is the
            // number of iterations. => err_i * i^2 = err_j * j^2. So
            // when err_j is zeroTol, and err_i has been calculated we
            // can find an approximate value for j
            if (maxErr < 1.0)
            {
                // If this is calculated near the beginning it tends
                // to overshoot, go to a quarter to refine the counter
                // (we use a modulo anyway so it will still stop at
                // the prediction)
                errorChunk = 0.25 * sqrt(maxErr * (f64)Square(i) / stop.zeroTol);
                LOG("New target index divisor %u", errorChunk);
            }
        }
        else
        {
            for (auto coord = remBegin; coord < remEnd; ++coord)
            {
                const f64 newVal = 0.25 * (pVoltage[(*coord) + 1] + pVoltage[(*coord) - 1] + pVoltage[(*coord) - lineLength] + pVoltage[(*coord) + lineLength]);
                voltages[*coord] = newVal;
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

/// Single threaded finite difference implementation that ignores
/// the outer row/column of points where points may need to be
/// fixed. Thus, these all need to be fixed points. If handled by
/// the dispatch function then this is all handled automagically
static
void
FDMSingleNoZip(Grid* grid, const std::vector<uint>& redPts, const std::vector<uint>& blkPts, const StopParams& stop)
{
    // NOTE(Chris): We need d2phi/dx^2 + d2phi/dy^2 = 0
    // => 1/h^2 * ((phi(x+1,y) - 2phi(x,y) + phi(x-1,y))
    //           + (phi(x,y+1) - 2phi(x,y) + phi(x,y-1))
    // => phi(x,y) = 1/4 * (phi(x+1,y) + phi(x-1,y) + phi(x,y+1) + phi(x,y-1))

    JasUnpack((*grid), voltages, lineLength);

    // Check error every 500 iterations at first
    const uint errorChunk = 500;

    f64 maxErr = 0.0;
    // Main loop - start from 1 so as not to calculate error on first iteration
    for (u64 i = 1; i <= stop.maxIter; ++i)
    {
        // Unlikely means the branch predictor will always go the
        // other way, it will have to backtrack on the very rare
        // cases that this comes up (1 in errorChunk times). This
        // is a penalty of < 200 cycles on those rare occasions
        if (unlikely(i % errorChunk == 0))
        {
            // Atomic comparisons are slow compared to normal
            // scalars, so declare a thread-local maxErr and then
            // just update the main one at the end
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

            // Else set new target if possible. If we plot the error
            // per iteration we not that it remains fixed at 1.0 until
            // all cells have been filled, after this point it drops
            // roughly as k*i^{-2} where k is a constant and i is the
            // number of iterations. => err_i * i^2 = err_j * j^2. So
            // when err_j is zeroTol, and err_i has been calculated we
            // can find an approximate value for j
            if (maxErr < 1.0)
            {
                // If this is calculated near the beginning it tends
                // to overshoot, go to a quarter to refine the counter
                // (we use a modulo anyway so it will still stop at
                // the prediction)
                LOG("Relative change after %u iterations %f", (unsigned)i, maxErr);
            }
        }
        else // normal path
        {
            // Loop over the non-fixed points and apply the FDM only
            // for (auto coord = coordRange.begin(); coord < coordRange.end(); ++coord)
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
FDMSingleZip(Grid* grid, const std::vector<uint>& coordRange,
             const StopParams& stop, const PreprocessedGridZips& zips)
{
    // NOTE(Chris): Single Threaded variant

    // NOTE(Chris): We need d2phi/dx^2 + d2phi/dy^2 = 0
    // => 1/h^2 * ((phi(x+1,y) - 2phi(x,y) + phi(x-1,y))
    //           + (phi(x,y+1) - 2phi(x,y) + phi(x,y-1))
    // => phi(x,y) = 1/4 * (phi(x+1,y) + phi(x-1,y) + phi(x,y+1) + phi(x,y-1))

    JasUnpack((*grid), voltages, numLines, lineLength);
    JasUnpack(zips, hZip, vZip, hvZip);

    // Create the previous voltage array from the current array
    decltype(grid->voltages) prevVoltages(voltages);

    // Check error every 500 iterations at first
    uint errorChunk = 500;

    f64 maxErr = 0.0;

    // Main loop - start from 1 so as not to calculate error on first iteration
    for (u64 i = 1; i <= stop.maxIter; ++i)
    {
        // We will just double buffer these 2 vectors to avoid reallocations new<->old
        std::swap(prevVoltages, voltages);
        // const ref to avoid damage again
        const decltype(grid->voltages)& pVoltage = prevVoltages;

        // Used to wrap zip access
        auto WrapGridAccessNewVal = [&pVoltage, numLines, lineLength] (const std::pair<uint,uint>& pt) -> f64
            {
                const uint id1 = pt.second * lineLength + ((int)pt.first - 1 < 0
                                                           ? lineLength - 1
                                                           : pt.first - 1);

                const uint id2 = pt.second * lineLength + (pt.first + 1 >= lineLength
                                                           ? 0
                                                           : pt.first + 1);
                const uint id3 = ((int)pt.second - 1 < 0
                                  ? numLines - 1
                                  : pt.second - 1) * lineLength + pt.first;

                const uint id4 = (pt.second + 1 >= numLines
                                  ? 0
                                  : pt.second + 1) * lineLength + pt.first;

                const f64 newVal = 0.25*(pVoltage[id1] + pVoltage[id2]
                                         + pVoltage[id3] + pVoltage[id4]);
                return newVal;
            };

        // Unlikely means the branch predictor will always go the
        // other way, it will have to backtrack on the very rare
        // cases that this comes up (1 in errorChunk times). This
        // is a penalty of < 200 cycles on those rare occasions
        if (unlikely(i % errorChunk == 0))
        {
            // Atomic comparisons are slow compared to normal
            // scalars, so declare a thread-local maxErr and then
            // just update the main one at the end
            f64 threadMaxErr = 0.0;
            maxErr = 0.0;

            // Loop over the non-fixed points
            for (auto coord = coordRange.begin(); coord < coordRange.end(); ++coord)
            {
                // Apply finite difference method and calculate error
                const f64 newVal = 0.25 * (pVoltage[(*coord) + 1] + pVoltage[(*coord) - 1] + pVoltage[(*coord) - lineLength] + pVoltage[(*coord) + lineLength]);
                voltages[*coord] = newVal;
                const f64 absErr = std::abs((pVoltage[*coord] - newVal)/newVal);

                if (absErr > threadMaxErr)
                {
                    threadMaxErr = absErr;
                }
            }

            for (const auto& coord : hZip)
            {
                const f64 newVal = WrapGridAccessNewVal(coord);
                const uint index = coord.second * lineLength + coord.first;
                voltages[index] = newVal;
                const f64 absErr = std::abs((pVoltage[coord.second * lineLength + coord.first] - newVal)/newVal);

                if (absErr > threadMaxErr)
                {
                    threadMaxErr = absErr;
                }
            }

            for (const auto& coord : vZip)
            {
                const f64 newVal = WrapGridAccessNewVal(coord);
                const uint index = coord.second * lineLength + coord.first;
                voltages[index] = newVal;
                const f64 absErr = std::abs((pVoltage[coord.second * lineLength + coord.first] - newVal)/newVal);

                if (absErr > threadMaxErr)
                {
                    threadMaxErr = absErr;
                }
            }

            for (const auto& coord : hvZip)
            {
                const f64 newVal = WrapGridAccessNewVal(coord);
                const uint index = coord.second * lineLength + coord.first;
                voltages[index] = newVal;
                const f64 absErr = std::abs((pVoltage[coord.second * lineLength + coord.first] - newVal)/newVal);

                if (absErr > threadMaxErr)
                {
                    threadMaxErr = absErr;
                }
            }

            // Update global error
            if (threadMaxErr > maxErr)
            {
                maxErr = threadMaxErr;
            }

            // If we have converged, then break by leaving the function
            // TODO(Chris): Time logging
            if (maxErr < stop.zeroTol)
            {
                LOG("Performed %u iterations, max error: %f", (unsigned)i, maxErr);
                return;
            }

            // Else set new target if possible. If we plot the error
            // per iteration we not that it remains fixed at 1.0 until
            // all cells have been filled, after this point it drops
            // roughly as k*i^{-2} where k is a constant and i is the
            // number of iterations. => err_i * i^2 = err_j * j^2. So
            // when err_j is zeroTol, and err_i has been calculated we
            // can find an approximate value for j
            if (maxErr < 1.0)
            {
                // If this is calculated near the beginning it tends
                // to overshoot, go to a quarter to refine the counter
                // (we use a modulo anyway so it will still stop at
                // the prediction)
                errorChunk = 0.25 * sqrt(maxErr * (f64)Square(i) / stop.zeroTol);
                LOG("New target index divisor %u", errorChunk);
            }
        }
        else // normal path
        {
            // Loop over the non-fixed points and apply the FDM only
            for (auto coord = coordRange.begin(); coord < coordRange.end(); ++coord)
            {
                const f64 newVal = 0.25 * (pVoltage[(*coord) + 1] + pVoltage[(*coord) - 1] + pVoltage[(*coord) - lineLength] + pVoltage[(*coord) + lineLength]);
                voltages[*coord] = newVal;
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
/// to 1 of 4 worked functions, depending on whether it has zips,
/// and whether we are running parallel code or not
void
SolveGridLaplacianZero(Grid* grid, const f64 zeroTol,
                       const u64 maxIter, bool parallel)
{
    TIME_FUNCTION();

    // NOTE(Chris): This function dispatches the calculation to
    // the most appropriate function (zips or not, parallel or
    // not), after having preprocessed the grid to check its
    // validity

    // NOTE(Chris): The only way I can think of to improve this from
    // here is SIMD. But that also poses certain problems, we'd need
    // to iterate over all cells (for memory alignedness), rather than
    // just the non-fixed cells, then we would need to reassign the
    // fixed cells (would blow the cache), even with AVX we probably
    // wouldn't get more than a 2.5x gain unless I'm missing something
    // algorithmically

    // NOTE(Chris): We need d2phi/dx^2 + d2phi/dy^2 = 0
    // => 1/h^2 * ((phi(x+1,y) - 2phi(x,y) + phi(x-1,y))
    //           + (phi(x,y+1) - 2phi(x,y) + phi(x,y-1))
    // => phi(x,y) = 1/4 * (phi(x+1,y) + phi(x-1,y) + phi(x,y+1) + phi(x,y-1))

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

    if (!verticZip && !horizZip)
    {
        if (parallel)
        {
            FDMParaNoZip(grid, coordRangeRed, coordRangeBlack, StopParams(zeroTol, maxIter));
        }
        else
        {
            FDMSingleNoZip(grid, coordRangeRed, coordRangeBlack, StopParams(zeroTol, maxIter));
        }
        return;
    }

    auto zips = PreprocessGridZips(*grid);

    if (parallel)
    {
        LOG("Sorry, not yet implemented");
        // FDMParaZip(grid, coordRangeRed, StopParams(zeroTol, maxIter), zips);
    }
    else
    {
        LOG("Sorry, not yet implemented");
        // FDMSingleZip(grid, coordRangeRed, StopParams(zeroTol, maxIter), zips);
    }
}
}
