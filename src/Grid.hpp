// -*- c++ -*-
#if !defined(GRID_H)
/* ==========================================================================
   $File: Grid.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define GRID_H
#include "GlobalDefines.hpp"
#include "Jasnah.hpp"
#include <memory>
#include <unordered_map>

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
    inline RGBA&
    operator=(const RGBA&) = default;
    inline RGBA&
    operator=(u32 other) { rgba = other; return *this; }
    /// Compare as u32
    inline bool
    operator==(RGBA other) const
    { return this->rgba == other.rgba; }
    inline bool
    operator<(RGBA other) const
    { return this->rgba < other.rgba; }
    inline bool
    operator>(RGBA other) const
    { return this->rgba > other.rgba; }
    inline bool
    operator>=(RGBA other) const
    { return this->rgba >= other.rgba; }
    inline bool
    operator<=(RGBA other) const
    { return this->rgba <= other.rgba; }
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
    LoadImage(const char* path, const uint desiredComponents);

    /// Returns an ImageInfo struct
    inline ImageInfo
    GetInfo() const
    {
        return ImageInfo(x, y, n, fileN);
    }

    /// Returns a pointer to the first byte of image data
    inline const u8*
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
inline Jasnah::Option<Image>
LoadImage(const char* path, const uint desiredComponents)
{
    Jasnah::Option<Image> im(Jasnah::ConstructInPlace);
    bool success = im->LoadImage(path, desiredComponents);

    if (!success)
        return Jasnah::None;

    return im;
}

// This is a placeholder until it's done with dynamic scripting - maybe not anymore
enum class ConstraintType
{
    CONSTANT, // Constant value
    OUTSIDE, // Ignore
    LERP_HORIZ, // Lerp along x
    LERP_VERTIC, // Lerp along y
};

typedef std::pair<ConstraintType, f64> Constraint;

/// Object representing a grid. This is the key object in this system.
class Grid
{
public:
    /// Stores the voltage for each cell
    // DoubleVec voltages;
    std::vector<f64> voltages;
    /// Width of the simulation area
    uint lineLength;
    /// Height of the simulation area
    uint numLines;
    /// Stores the indices and values of the fixed points
    std::unordered_map<MemIndex, f64> fixedPoints;
    // Enable horizontal and vertical zipping for the edges of the grid
    bool horizZip;
    bool verticZip;

    /// Default constructors and assignment operators to keep
    /// everything working
    Grid(bool hZip, bool vZip) :
        voltages(),
        lineLength(0),
        numLines(0),
        fixedPoints(),
        horizZip(hZip),
        verticZip(vZip)
    {}

    Grid(const Grid&) = default;
    Grid(Grid&&) = default;
    Grid& operator=(const Grid&) = default;
    Grid& operator=(Grid&&) = default;

    // TODO(Chris): Add scaling (super/sub-sampling)
    /// Initialise grid from image
    bool
    LoadFromImage(const char* imagePath,
                  const std::unordered_map<u32, Constraint>& colorMapping,
                  uint scaleFactor = 1);

    /// Sets the two boundary plates for the basic box
    void
    InitialiseBasicGrid(const f64 plusWall, const f64 minusWall);

    /// Prints the grid to stdout, useful for debugging at low res
    void
    Print();

    /// Fixes this grid's point x, y to val for the duration of this grid
    inline void
    AddFixedPoint(const uint x, const uint y, const f64 val)
    {
        const uint index = y * lineLength + x;
        auto found = fixedPoints.find(index);

        if (found != fixedPoints.end())
        {
            fixedPoints.erase(found);
        }

        voltages[index] = val;
        fixedPoints.emplace(std::make_pair(index, val));
    }
};

#endif
