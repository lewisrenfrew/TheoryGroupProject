/* ==========================================================================
   $File: Grid.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "Grid.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STB_ONLY_PNG
#include "stb_image.h"
#include "Utility.hpp"

#include <algorithm>
#include <cstdio>

bool
Image::LoadImage(const char* path, const uint desiredComponents)
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

bool
Grid::LoadFromImage(const char* imagePath,
                    const std::unordered_map<u32, Constraint>& colorMapping,
                    uint scaleFactor)
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
                    std::vector<f64> lerp = LerpNPointsBetweenVoltages(voltages[yLoc * lineLength + xLoc - 1],
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
Grid::InitialiseBasicGrid(const f64 plusWall, const f64 minusWall)
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
Grid::Print()
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
