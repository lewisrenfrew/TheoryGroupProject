/* ==========================================================================
   $File: FDM.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "FDM.hpp"
#include "Grid.hpp"
#include "Utility.hpp"

#include <cmath>
#include <atomic>

namespace FDM
{
    void
    SolveGridLaplacianZero(Grid* grid, const f64 zeroTol, const u64 maxIter)
    {
        // NOTE(Chris): The only way I can think of to improve this from
        // here is SIMD. But that also poses certain problems, we'd need
        // to iterate over all cells (for memory alignedness), rather than
        // just the non-fixed cells, then we would need to reassign the
        // fixed cells (would blow the cache), even with AVX we probably
        // wouldn't get more than a 2.5x gain unless I'm missing something
        // algorithmically
#ifdef GOMP
        // NOTE(Chris): Multi-threaded variant

        // NOTE(Chris): We need d2phi/dx^2 + d2phi/dy^2 = 0
        // => 1/h^2 * ((phi(x+1,y) - 2phi(x,y) + phi(x-1,y))
        //           + (phi(x,y+1) - 2phi(x,y) + phi(x,y-1))
        // => phi(x,y) = 1/4 * (phi(x+1,y) + phi(x-1,y) + phi(x,y+1) + phi(x,y-1))

        // NOTE(Chris): We never write to the fixed points, so we don't
        // need to re-set them (as long as they were set properly in the
        // incoming grid using AddFixedPoint)

        JasUnpack((*grid), voltages, numLines, lineLength, fixedPoints);

        // NOTE(Chris): We choose to trade off some memory for computation
        // speed inside the loop
        std::vector<uint> coordRange; // non fixed points
        coordRange.reserve(numLines*lineLength);

        for (uint y = 0; y < numLines; ++y)
            for (uint x = 0; x < lineLength; ++x)
            {
                if (fixedPoints.count(y * lineLength + x) == 0)
                {
                    coordRange.push_back(y * lineLength + x);
                }
            }
        // Make a const ref so we don't mess with the contents
        const decltype(coordRange)& cRange = coordRange;

        // Create the previous voltage array from the current array
        decltype(grid->voltages) prevVoltages(voltages);

        // Check error every 500 iterations at first
        uint errorChunk = 500;

        // Hand-waving 10k iterations per thread as minimum to not be dominated by context switches etc.
        const uint numWorkChunks = (prevVoltages.size() / 10000 > 0) ? prevVoltages.size() / 10000 : 1;
        const uint numThreads = (prevVoltages.size() / omp_get_max_threads() >= 10000)
            ? omp_get_max_threads()
            : numWorkChunks;
        // remaining points that don't divide between the threads
        const uint rem = prevVoltages.size() % numThreads;
        // size of work chunk for each thread
        const uint blockSize = (prevVoltages.size() - rem) / numThreads;
        LOG("Num threads %u, remainders %u", numThreads, rem);

        // Atomic value for concurrent multi-threaded access
        std::atomic<f64> maxErr(0.0);

        // Main loop - start with 1 so as not to take slow path on first iter
        for (u64 i = 1; i <= maxIter; ++i)
        {
            // We will just double buffer these 2 vectors to avoid reallocations new<->old
            std::swap(prevVoltages, voltages);
            // const ref to avoid damage again
            const decltype(grid->voltages)& pVoltage = prevVoltages;

            // Loop over threads (split per thread and complete work chunk)
#pragma omp parallel for default(none) shared(errorChunk, cRange, lineLength, voltages, maxErr, i, pVoltage)
            for (uint thread = 0; thread < numThreads; ++thread)
            {
                // chunk start and end
                const auto start = cRange.begin() + thread * blockSize;
                const auto end = (start + blockSize > cRange.end()) ? cRange.end() : start + blockSize;
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
            const auto remBegin = cRange.begin() + (numThreads) * blockSize;
            const auto remEnd = cRange.end();
            if (unlikely(i % errorChunk == 0)) // several differences in this branch
            {
                f64 localMaxErr = 0.0;
                const auto remBegin = cRange.begin() + (numThreads) * blockSize;
                const auto remEnd = cRange.end();
                for (auto coord = remBegin; coord < remEnd; ++coord)
                {
                    const f64 newVal = 0.25 * (pVoltage[(*coord) + 1] + pVoltage[(*coord) - 1] + pVoltage[(*coord) - lineLength] + pVoltage[(*coord) + lineLength]);
                    voltages[*coord] = newVal;
                    const f64 absErr = std::abs((pVoltage[*coord] - newVal)/newVal);

                    if (absErr > localMaxErr)
                        localMaxErr = absErr;
                }

                if (localMaxErr > maxErr)
                {
                    maxErr = localMaxErr;
                }

                // If we have converged, then break by leaving the function
                // TODO(Chris): Time logging
                if (maxErr < zeroTol)
                {
                    LOG("Performed %u iterations, max error: %f", (unsigned)i, maxErr.load());
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
                    errorChunk = 0.25 * sqrt(maxErr * (f64)Square(i) / zeroTol);
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
            }
        }
        LOG("Overran max iteration counter (%u), max error: %f", (unsigned)maxIter, maxErr.load());
#else

        // NOTE(Chris): Single Threaded variant

        // NOTE(Chris): We need d2phi/dx^2 + d2phi/dy^2 = 0
        // => 1/h^2 * ((phi(x+1,y) - 2phi(x,y) + phi(x-1,y))
        //           + (phi(x,y+1) - 2phi(x,y) + phi(x,y-1))
        // => phi(x,y) = 1/4 * (phi(x+1,y) + phi(x-1,y) + phi(x,y+1) + phi(x,y-1))

        JasUnpack((*grid), voltages, numLines, lineLength, fixedPoints);

        // NOTE(Chris): We choose to trade off some memory for computation
        // speed inside the loop
        std::vector<uint> coordRange; // non fixed points
        coordRange.reserve(numLines*lineLength);

        for (uint y = 0; y < numLines; ++y)
            for (uint x = 0; x < lineLength; ++x)
            {
                if (fixedPoints.count(y * lineLength + x) == 0)
                {
                    coordRange.push_back(y * lineLength + x);
                }
            }
        // Make a const ref so we don't mess with the contents
        const decltype(coordRange)& cRange = coordRange;

        // Create the previous voltage array from the current array
        decltype(grid->voltages) prevVoltages(voltages);

        // Check error every 500 iterations at first
        uint errorChunk = 500;

        f64 maxErr = 0.0;

        // Main loop - start from 1 so as not to calculate error on first iteration
        for (u64 i = 1; i <= maxIter; ++i)
        {
            // We will just double buffer these 2 vectors to avoid reallocations new<->old
            std::swap(prevVoltages, voltages);
            // const ref to avoid damage again
            const decltype(grid->voltages)& pVoltage = prevVoltages;

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
                for (auto coord = cRange.begin(); coord < cRange.end(); ++coord)
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

                // If we have converged, then break by leaving the function
                // TODO(Chris): Time logging
                if (maxErr < zeroTol)
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
                    errorChunk = 0.25 * sqrt(maxErr * (f64)Square(i) / zeroTol);
                    LOG("New target index divisor %u", errorChunk);
                }
            }
            else // normal path
            {
                // Loop over the non-fixed points and apply the FDM only
                for (auto coord = cRange.begin(); coord < cRange.end(); ++coord)
                {
                    const f64 newVal = 0.25 * (pVoltage[(*coord) + 1] + pVoltage[(*coord) - 1] + pVoltage[(*coord) - lineLength] + pVoltage[(*coord) + lineLength]);
                    voltages[*coord] = newVal;
                }
            }
        }
        LOG("Overran max iteration counter (%u), max error: %f", (unsigned)maxIter, maxErr);
#endif
    }
}
