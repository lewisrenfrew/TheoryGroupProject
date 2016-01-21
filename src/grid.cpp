/* ==========================================================================
   $File: grid.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "GlobalDefines.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STB_ONLY_PNG
#include "stb_image.h"
#include "Jasnah.hpp"
#include <vector>
#include <utility>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <unordered_map>
#include "boost/align.hpp"
#include <x86intrin.h>
#ifdef GOMP
#include <omp.h>
#else
extern "C" int omp_get_max_threads() { return 1; } // Glorious hack for clang not supporting OpenMP in mainline yet
#endif

// NOTE(Chris): Macros from linux kernel for branch optimization
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#ifdef CATCH_CONFIG_MAIN
#define LOG
#else
// NOTE(Chris): Provide logging for everyone - create in main TU to
// avoid ordering errors, maybe reduce number of TU's...
namespace Log
{
    Lethani::Logfile log;
}
#endif

#if 0
// Scary inline assembly for profiling
static void
Escape(void* p)
{
    asm volatile("" : : "g"(p) : "memory");
}

static void
Clobber()
{
    asm volatile("" : : : "memory");
}
#endif


// http://www.graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
inline
bool
IsPow2(uint val)
{
    return val && !(val & (val - 1));
}

template <typename T>
inline constexpr
T Square(T val) { return val * val; }

typedef std::vector<f64> DoubleVec;
// Getting bored of typing this one
template <typename T>
using RemRefT = typename std::remove_reference<T>::type;

/// 2D vector type for the gradient stuff
template <typename T>
struct V2
{
    // Storage
    T x, y;
    // Constructors
    V2() : x(0), y(0) {}
    V2(T x, T y) : x(x), y(y) {}
    V2(const V2&) = default;
    V2& operator=(const V2&) = default;

    // Addition operator
    const V2 operator+(const V2& other) const
    {
        return V2(x+other.x, y+other.y);
    }
    // Subtraction operator
    const V2 operator-(const V2& other) const
    {
        return V2(x-other.x, y-other.y);
    }
};

typedef V2<f64> V2d;

/// Convert easily between 32-bit RGBA and individual components
union RGBA
{
    u32 rgba;
    struct
    {
        u8 r;
        u8 g;
        u8 b;
        u8 a;
    };

    RGBA(u32 color) : rgba(color) {}
    RGBA(u8 r, u8 g, u8 b, u8 a) : r(r), g(g), b(b), a(a) {}
    RGBA() = default;
    RGBA(const RGBA&) = default;
    RGBA& operator=(const RGBA&) = default;
    RGBA& operator=(u32 other) { rgba = other; return *this; }
};

/// Data that can be returned from the Image class
struct ImageInfo
{
    ImageInfo(uint x, uint y, uint n, uint fileN)
        : pxPerLine(x),
          numScanlines(y),
          numComponents(n),
          fileNumComponents(fileN)
    {}

    const uint pxPerLine;
    const uint numScanlines;
    const uint numComponents;
    const uint fileNumComponents;
};

/// RAII Wrapper for image loading
class Image
{
public:
    /// Construct with nullptr
    Image()
        : x(0),
          y(0),
          n(0),
          fileN(0),
          data(nullptr)
    {}

    Image(const Image& other) = default;
    Image(Image&& other) = default;
    Image& operator=(const Image& other) = default;
    Image& operator=(Image&& other) = default;
    ~Image() = default;

    /// Loads an image at the given path with the desired componentes
    /// (e.g. rgba is 4), returns true on success
    bool
    LoadImage(const char* path, const uint desiredComponents)
    {
        int sizeX, sizeY, sizeN;
        const u8* imageData = stbi_load(path, &sizeX, &sizeY, &sizeN, desiredComponents);
        if (!imageData)
        {
            LOG("Image loading failed: %s", stbi_failure_reason());
            return false;
        }
        data = std::shared_ptr<const u8*>(new const u8*(imageData), [](const u8** ptr)
                                          {
                                                if (*ptr)
                                                {
                                                    // LOG("Freeing");
                                                    stbi_image_free(const_cast<u8*>(*ptr));
                                                }
                                          });

        x = sizeX;
        y = sizeY;
        n = sizeN;
        fileN = desiredComponents;
        return true;
    }

    /// Returns an ImageInfo struct
    ImageInfo
    GetInfo() const
    {
        return ImageInfo(x, y, n, fileN);
    }

    /// Returns a pointer to the first byte of image data
    const u8*
    GetData() const
    {
        return *data.get();
    }

private:
    /// Image data, width, height, number of components in the parsed
    /// data and the original file
    uint x, y, n, fileN;
    /// Ref-counted pointer to the image data
    std::shared_ptr<const u8*> data;
};

/// Image loading interface returning Option. Returns None on failure
Jasnah::Option<Image>
LoadImage(const char* path, const uint desiredComponents)
{
    Jasnah::Option<Image> im(Jasnah::ConstructInPlace);
    bool success = im->LoadImage(path, desiredComponents);

    if (!success)
        return Jasnah::None;

    return im;
}


namespace Color
{
    // NOTE(Chris): Modern computers are almost all little-endian i.e.
    // AABBGGRR
    constexpr const u32 White = 0xFFFFFFFF;
    constexpr const u32 Red = 0xFF0000FF;
    constexpr const u32 PaintRed = 0XFF241CED;
    constexpr const u32 Green = 0xFF00FF00;
    constexpr const u32 Blue = 0xFFFF0000;
    constexpr const u32 Black = 0xFF000000;
}

// TODO(Chris): Script API here
// This is a placeholder until it's done with dynamic scripting
enum class ConstraintType
{
    CONSTANT, // Constant value
    OUTSIDE, // Ignore
    LERP_HORIZ, // Lerp along x
    LERP_VERTIC, // Lerp along y
    ZIP_X, // Join linked rows at this point
    ZIP_Y, // Join linked columns at this point
};

typedef std::pair<ConstraintType, f64> Constraint;

/// Returns a vector of numPoints points linearly interpolated between
/// v1 and v2, useful for some boundary conditions (top and bottom of
/// finite lengths in problem 1)
DoubleVec
LerpNPointsBetweenVoltages(const f64 v1, const f64 v2, const uint numPoints)
{
    // Set x0 as first point, field has potential v1 here, and v2 at
    // xn. We can interpolate them with the equation of a line:
    // a*xi+b = vi but we let xi = 0 then WLOG b = v1, a = (v2-v1)/(numPts-1)

    // We need at least 2 points between 2 voltages
    if (numPoints < 2)
        return DoubleVec();

    const f64 a = (v2 - v1)/(f64)(numPoints-1);
    const f64 b = v1;

    DoubleVec result;
    result.reserve(numPoints);

    for (uint i = 0; i < numPoints; ++i)
        result.push_back(a*i + b);

    return result;
}

/// Stores the state of the grid. This API will be VERY prone to
/// breakage for now
class Grid
{
public:
    /// Stores the voltage for each cell
    // DoubleVec voltages;
    std::vector<f64, boost::alignment::aligned_allocator<f64, 32> > voltages;
    /// Width of the simulation area
    uint lineLength;
    /// Height of the simulation area
    uint numLines;
    /// Stores the indices and values of the fixed points
    // std::vector<std::pair<MemIndex, f64> > fixedPoints;
    std::unordered_map<MemIndex, f64> fixedPoints;

    /// Default constructors and assignment operators to keep
    /// everything working
    Grid() = default;
    Grid(const Grid&) = default;
    Grid(Grid&&) = default;
    Grid& operator=(const Grid&) = default;
    Grid& operator=(Grid&&) = default;

    // TODO(Chris): Add scaling (super/sub-sampling)
    /// Initialise grid from image
    bool
    LoadFromImage(const char* imagePath,
                  const std::unordered_map<u32, Constraint>& colorMapping,
                  uint scaleFactor = 1)
    {
        const Jasnah::Option<Image> image = LoadImage(imagePath, 4);
        if (!image)
        {
            LOG("Loading failed");
            // throw std::invalid_argument("Bad path or something");
            return false;
        }

        ImageInfo info = image->GetInfo();
        JasUnpack(info, pxPerLine, numScanlines);

        lineLength = pxPerLine;
        numLines = numScanlines;
        // const u32* rgbaData = (const u32*)data;
        const u32* rgbaData = (const u32*)image->GetData();

        voltages.assign(pxPerLine * numScanlines, 0.0);

        for (uint yLoc = 0; yLoc < numScanlines; ++yLoc)
            for (uint xLoc = 0; xLoc < pxPerLine; ++xLoc)
            {
                const RGBA texel = rgbaData[yLoc * lineLength + xLoc];
                if (texel.rgba != Color::White)
                {
                    auto iter = colorMapping.find(texel.rgba);
                    if (iter == colorMapping.end())
                    {
                        LOG("Unknown color %u <%u, %u, %u, %u> encountered at (%u, %u), ignoring",
                            texel.rgba, (unsigned)texel.r, (unsigned)texel.g,
                            (unsigned)texel.b, (unsigned)texel.a, xLoc, yLoc);

                        continue;
                    }

                    switch (iter->second.first)
                    {
                    case ConstraintType::CONSTANT:
                        AddFixedPoint(xLoc, yLoc, iter->second.second); // Yes, this reeks of hack for now
                        break;
                    case ConstraintType::OUTSIDE:
                        // Do nothing - maybe set to constant 0?
                        break;
                    case ConstraintType::LERP_HORIZ:
                        // Need to scan and handle these later
                        break;
                    case ConstraintType::LERP_VERTIC:
                        // Need to scan and handle these later
                        break;
                    default:
                        LOG("Not yet implemented");
                        break;
                    }
                }
            }

        // Assuming only one horizontal lerp colour
        // TODO(Chris): Make this check more rigourous
        auto horizLerp = std::find_if(std::begin(colorMapping), std::end(colorMapping),
                                      [](RemRefT<decltype(colorMapping)>::value_type val)
                                      {
                                          return val.second.first == ConstraintType::LERP_HORIZ;
                                      });

        if (horizLerp != colorMapping.end())
        {
            for (uint yLoc = 0; yLoc < numScanlines; ++yLoc)
                for (uint xLoc = 0; xLoc < pxPerLine; ++xLoc)
                {
                    const u32* texel = &rgbaData[yLoc * lineLength + xLoc];
                    if (*texel == horizLerp->first)
                    {
                        uint len = 0;
                        const uint maxLen = lineLength - xLoc;

                        while (*texel == horizLerp->first && len <= maxLen)
                        {
                            ++texel;
                            ++len;
                        }
                        // Texel now points to the pixel following the final lerp pixel

                        // Need a pixel before and after the lerp for boundaries
                        if (len == maxLen || xLoc == 0)
                        {
                            LOG("Lerp specification error, ignoring, make sure\n"
                                "you have a constant pixel on each side");
                            // continue outer loop
                            break;
                        }

                        // Constants are already set at this point, so use them
                        DoubleVec lerp = LerpNPointsBetweenVoltages(voltages[yLoc * lineLength + xLoc - 1],
                                                                    voltages[yLoc * lineLength + xLoc + len],
                                                                    len + 2);

                        // Set the values
                        for (uint i = 0; i < len+2; ++i)
                        {
                            AddFixedPoint(xLoc - 1 + i, yLoc, lerp[i]);
                        }
                    }
                }
        }
        // Same can be done trivially for vertical lerp

        // Do scaling here
        if (!IsPow2(scaleFactor) && scaleFactor != 0)
        {
            LOG("Image scale factor must be a power of 2 and non-zero, ignoring");
            scaleFactor = 1;
        }

        if (scaleFactor != 1)
        {
            // DoubleVec scaledImage;
            decltype(voltages) scaledImage;
            // Reserve new size
            // scaledImage.reserve(Square(scaleFactor) * voltages.size());

            // Set new dimensions
            numLines *= scaleFactor;
            lineLength *= scaleFactor;

            // set new image to 0
            scaledImage.assign(numLines * lineLength, 0.0);

            std::swap(voltages, scaledImage);

            // Assign scaled fixed points from previous fixed points
            decltype(fixedPoints) fp;
            std::swap(fixedPoints, fp);

            for (uint y = 0; y < numLines; ++y)
            {
                for (uint x = 0; x < lineLength; ++x)
                {
                    // unscaled is the index that this point had in the original unscaled image
                    const uint unscaled = y / scaleFactor * lineLength / scaleFactor + x / scaleFactor;

                    // See if unscaled is in the list of fixed points
                    auto found = fp.find(unscaled);

                    // If it is add (x,y) to the new list of fixed points
                    if (found != fp.end())
                    {
                        AddFixedPoint(x, y, found->second);
                    }
                }
            }
        }
        return true;
    }

    /// Sets the two boundary plates for the basic box
    void
    InitialiseBasicGrid(const f64 plusWall, const f64 minusWall)
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
    AddFixedPoint(const uint x, const uint y, const f64 val)
    {
        const uint index = y * lineLength + x;
        auto found = fixedPoints.find(index);

        if (found != fixedPoints.end())
        {
            fixedPoints.erase(found);
        }

        voltages[index] = val;
        // fixedPoints.push_back(std::make_pair(index, val));
        fixedPoints.emplace(std::make_pair(index, val));
    }
};

/// Calculates and holds the result of the gradient of a simulation grid
class GradientGrid
{
public:
    // Constructors and operators
    GradientGrid() : gradients(), lineLength(0), numLines(0) {}
    ~GradientGrid() = default;
    GradientGrid(const GradientGrid&) = default;
    GradientGrid& operator=(const GradientGrid&) = default;

    /// Storage for the vectors in an array matching the cells
    std::vector<V2d> gradients;
    // As for Grid
    uint lineLength;
    uint numLines;

    /// Calculate and store the negative gradient (i.e. the E-field
    /// from the potential)
    void
    CalculateNegGradient(const Grid& grid, const f64 cellsToMeters)
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
};

/// The function that does the work for now. Repeatedly applies a
/// finite difference scheme to the grid, until the maximum relative
/// change over one iteration is less zeroTol or maxIter iterations
/// have taken place.
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
    const uint numThreads = (prevVoltages.size() / omp_get_max_threads() >= 10000)
        ? omp_get_max_threads()
        : prevVoltages.size() / 10000;
    // remaining points that don't divide between the threads
    const uint rem = prevVoltages.size() % numThreads;
    // size of work chunk for each thread
    const uint blockSize = (prevVoltages.size() - rem) / numThreads;
    LOG("Num threads %u, remainders %u", numThreads, rem);

    // Atomic value for concurrent multi-threaded access
    std::atomic<f64> maxErr(0.0);

    // Main loop
    for (u64 i = 0; i < maxIter; ++i)
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

    // Main loop
    for (u64 i = 0; i < maxIter; ++i)
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
            fprintf(file, "%u %u %f\n", x, y, grid.voltages[y * grid.lineLength + x]);
        }
        fprintf(file, "\n");
    }

    fclose(file);
    return true;
}

/// Writes the gnuplot data file for gradients (E-field), to be used
/// with WriteGnuplotGradientFile. Returns true on successful write.
/// stepSize ignores some of the data so as not to overcrowd the plot
/// with lines
bool
WriteGradientGridForGnuplot(const GradientGrid& grid,
                            const uint stepSize = 2,
                            const char* filename = "Plot/GradientGrid.dat")
{
#define GRADIENT_DEBUG
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    JasUnpack(grid, gradients, numLines, lineLength);

#ifdef GRADIENT_DEBUG
    auto VecNorm = [](const V2d& vec) -> f64
        {
            return std::sqrt(vec.x*vec.x + vec.y*vec.y);
        };

    f64 maxNorm = 0.0;
#endif

    for (uint y = 0; y < numLines; ++y)
    {
        if (y % stepSize == 0)
            continue;

        for (uint x = 0; x < lineLength; ++x)
        {
            if (x % stepSize == 0)
                continue;

#ifdef GRADIENT_DEBUG
            f64 norm = VecNorm(gradients[y * lineLength + x]);
            if (norm > maxNorm)
                maxNorm = norm;
#endif

            fprintf(file, "%u %u %f %f\n", x, y,
                    gradients[y * lineLength + x].x,
                    gradients[y * lineLength + x].y);
        }
        fprintf(file, "\n");
    }

    fclose(file);

#ifdef GRADIENT_DEBUG
    LOG("Max norm: %f", maxNorm);
#endif
    return true;
}

/// Prints a script config file for gnuplot, the makefile can run
/// this. Returns true on successful write
bool
WriteGnuplotColormapFile(const Grid& grid,
                         const char* gridDataFile = "Plot/Grid.dat",
                         const char* filename = "Plot/PlotFinal.gpi")
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    JasUnpack(grid, lineLength, numLines);

    fprintf(file,
            // "set terminal pngcairo size 2560,1440\n"
            // "set output \"Grid.png\"\n"
            "set terminal canvas rounded size 1280,720 enhanced mousing fsize 10 lw 1.6 fontscale 1 standalone\n"
            "set output \"Grid.html\"\n"
            "load 'Plot/MorelandColors.plt'\n"
            "set xlabel \"x\"\nset ylabel \"y\"\n"
            "set xrange [0:%u]; set yrange [0:%u]\n"
            "set size ratio %f\n"
            "set style data lines\n"
            "set title \"Stable Potential (V)\"\n"
            "plot \"%s\" with image title \"Numeric Solution\"",
            lineLength-1,
            numLines-1,
            (f64)numLines / (f64)lineLength,
            gridDataFile);

    fclose(file);
    return true;
}

// Prints a script config file for gnuplot to plot the contour map.
// Returns true on succesful write
bool
WriteGnuplotContourFile(const Grid& grid,
                        const char* gridDataFile = "Plot/Grid.dat",
                        const char* filename = "Plot/PlotContour.gpi")
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    JasUnpack(grid, lineLength, numLines);

    fprintf(file,
            // "set terminal pngcairo size 2560,1440\n"
            // "set output \"GridContour.png\"\n"
            "set terminal canvas rounded size 1280,720 enhanced mousing fsize 10 lw 1.6 fontscale 1 standalone\n"
            "set output \"GridContour.html\"\n"
            "load 'Plot/MorelandColors.plt'\n"
            "set key outside\n"
            "set view map\n"
            "unset surface\n"
            "set contour base\n"
            "set cntrparam bspline\n"
            "set cntrparam levels auto 20\n"
            "set xlabel \"x\"\nset ylabel \"y\"\n"
            "set xrange [0:%u]; set yrange [0:%u]\n"
            "set size ratio %f\n"
            "set style data lines\n"
            "set title \"Stable Potential (V)\"\n"
            "splot \"%s\" with lines title \"\"\n"
            "set key default\n",
            lineLength-1,
            numLines-1,
            (f64)numLines / (f64)lineLength,
            gridDataFile);

    return true;
}

// Prints a script config file for gnuplot to plot the E-field map.
// Returns true on succesful write. scaling scales the magnitude of
// the vectors
bool
WriteGnuplotGradientFile(const GradientGrid& grid,
                         const f64 scaling = 2.5,
                         const char* gridDataFile = "Plot/GradientGrid.dat",
                         const char* filename = "Plot/PlotGradient.gpi")
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return false;
    }

    JasUnpack(grid, lineLength, numLines);

    fprintf(file,
            // "set terminal pngcairo size 2560,1440\n"
            // "set output \"GradientGrid.png\"\n"
            "set terminal canvas rounded size 1280,720 enhanced mousing fsize 10 lw 1.6 fontscale 1 standalone\n"
            "set output \"GradientGrid.html\"\n"
            "load 'Plot/MorelandColors.plt'\n"
            "set xlabel \"x\"\nset ylabel \"y\"\n"
            "set xrange [0:%u]; set yrange [0:%u]\n"
            "set size ratio %f\n"
            "set style data lines\n"
            "set title \"E-field (V/m)\"\n"
            "scaling = %f\n"
            "plot \"%s\" using 1:2:($3*scaling):($4*scaling):(sqrt($3*$3+$4*$4)) with vectors"
            " filled lc palette title \"\"",
            lineLength-1,
            numLines-1,
            (f64)numLines / (f64)lineLength,
            scaling,
            gridDataFile);

    return true;
}

#if 0
struct CommandLineFlags
{
    bool lastMatrix;
    std::string infofilepath;
};


CommandLineFlags ParseArguments()
{
    try
    {
        // Aguments are in order: discription, seperation char, version number.
        TCLAP::CmdLine cmd("Solving the electric and the voltage fields of a electrostatic prblem ",
                           ' ', Version::Gridle);

        // Aguments are in order: '-' flag, "--" flag,
        // discription,add to an object(cmd line), default state.
        TCLAP::SwitchArg lastMatrix("m", "lastMatrix", "Runs with the last matrix used",
                                    cmd,false);

        // Aguments are in order: '-' flag, "--" flag,
        // discription, default state, default string value , path, add to an object(cmd line).
        TCLAP::ValueArg<std::string> infofile("i" , "infofile","holds the information about the matrix",
                                              true, "", "path", cmd);

        CommandLineFlags ret;

        ret.lastMatrix = lastMatrix.getValue();
        ret.infofilepath = infofile.getValue();

        if(lastMatrix)
        {
            //TODO, get the previous matrix
        }
        else
        {
            //TODO,
        }


    }
    catch(TCLAP::ArgException& ex)
    {
        LOG("Parsing error %s for arg %s", ex.error().c_str(), ex.argId().c_str());
    }
}
#endif

#ifndef CATCH_CONFIG_MAIN
int main(void)
{
    // This looks better and far more generic
    std::unordered_map<u32, Constraint> colorMap;
    colorMap.emplace(Color::Black, std::make_pair(ConstraintType::CONSTANT, 0.0));
    colorMap.emplace(Color::Red, std::make_pair(ConstraintType::CONSTANT, 10.0));
    colorMap.emplace(Color::PaintRed, std::make_pair(ConstraintType::CONSTANT, 10.0));
    colorMap.emplace(Color::Blue, std::make_pair(ConstraintType::CONSTANT, -10.0));
    colorMap.emplace(Color::Green, std::make_pair(ConstraintType::LERP_HORIZ, 0.0));
    // Grid grid("prob1.png", colorMap);
    Grid grid;
    grid.LoadFromImage("prob0.png", colorMap, 8);

    // NOTE(Chris): Staggered leapfrog seems to follow a 1/(x^2) convergence, we can exploit this...
    // SolveGridLaplacianZero(&grid, 0.0001, 10000);
    SolveGridLaplacianZero(&grid, 0.001, 10000);

    const f64 cellsToMeters = 100.0;
    GradientGrid grad;
    grad.CalculateNegGradient(grid, cellsToMeters);

    WriteGridForGnuplot(grid);
    WriteGnuplotColormapFile(grid);
    WriteGnuplotContourFile(grid);

    WriteGradientGridForGnuplot(grad);
    WriteGnuplotGradientFile(grad, 0.08);

    return 0;
}
#endif
