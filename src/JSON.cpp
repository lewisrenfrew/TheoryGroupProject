/* ==========================================================================
   $File: JSON.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */
#include "JSON.hpp"
#include "Utility.hpp"
#include <rapidjson/reader.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <cmath>
#include <cstdio>
#include <unordered_map>

using namespace rapidjson;

static
Jasnah::Option<std::vector<char> >
ReadFileAndNullTerminate(const char* path)
{
    // Here we assume that we don't try to load a bigger json file
    // than we can parse. If we do then the vector will kill the
    // program with an exception

    FILE* in = fopen(path, "r");
    if (!in)
    {
        LOG("Unable to load %s", path);
        return Jasnah::None;
    }

    fseek(in, 0, SEEK_END);
    const uint len = ftell(in) + 1;
    rewind(in);
    std::vector<char> buf(len);
    const uint lengthRead = fread(buf.data(), 1, len, in);
    buf[lengthRead - 1] = '\0';
    fclose(in);

    return buf;
}

static
Jasnah::Option<std::unordered_map<u32, Constraint> >
ParseColorMap(const GenericValue<UTF8<> >& array)
{
    const uint size = array.Size();
    if (size == 0)
    {
        LOG("Provided empty array for ColorMap...");
        return Jasnah::None;
    }

    std::unordered_map<u32, Constraint> result;
    result.reserve(size);

    for (auto iter = array.Begin();
         iter != array.End();
        ++iter)
    {
        if (!iter->IsObject())
        {
            LOG("All inner ColorMap entries must be objects");
            return Jasnah::None;
        }

        if (!iter->HasMember("Type") || !iter->HasMember("Color"))
        {
            LOG("All inner ColorMap entries need Type and Color members");
            return Jasnah::None;
        }

        const auto type = iter->FindMember("Type");
        if (!type->value.IsString())
        {
            LOG("ColorMap type fields must be strings");
            return Jasnah::None;
        }

        Constraint innerResult;

        switch (StringHash(type->value.GetString()))
        {
        case StringHash("Constant"):
        {
            innerResult.first = ConstraintType::CONSTANT;
            if (!iter->HasMember("Value"))
            {
                LOG("Constant fields must be associated with a value");
                return Jasnah::None;
            }
            auto value = iter->FindMember("Value");
            if (!value->value.IsNumber())
            {
                LOG("Value field must refer to a double");
                return Jasnah::None;
            }
            innerResult.second = value->value.GetDouble();
        } break;

        case StringHash("Outside"):
        {
            innerResult.first = ConstraintType::OUTSIDE;
        } break;

        case StringHash("HorizontalLerp"):
        {
            innerResult.first = ConstraintType::LERP_HORIZ;
        } break;

        case StringHash("VerticalLerp"):
        {
            innerResult.first = ConstraintType::LERP_VERTIC;
        } break;

        // case StringHash("HorizontalZip"):
        // {
        //     innerResult.first = ConstraintType::ZIP_X;
        // } break;

        // case StringHash("VerticalZip"):
        // {
        //     innerResult.first = ConstraintType::ZIP_Y;
        // } break;

        default:
        {
            LOG("Unknown constraint type in ColorMap: \"%s\", ignoring", type->value.GetString());
            continue;
        }
        }

        const auto color = iter->FindMember("Color");
        u32 colorVal = 0;

        if (color->value.IsUint())
        {
            colorVal = color->value.GetUint();
        }
        else if (color->value.IsObject())
        {
            if (!color->value.HasMember("r")
                && !color->value.HasMember("g")
                && !color->value.HasMember("b"))
            {
                LOG("Color objects are required to have \"r\", \"g\" and \"b\" members (unsigned ints (0-255))");
                return Jasnah::None;
            }
            const auto red = color->value.FindMember("r");
            const auto green = color->value.FindMember("g");
            const auto blue = color->value.FindMember("b");

            if (!red->value.IsUint()
                || !green->value.IsUint()
                || !blue->value.IsUint())
            {
                LOG("Color objects are required to have \"r\", \"g\" and \"b\" members (unsigned ints (0-255))");
                return Jasnah::None;
            }

            auto CheckAssignColor = [](const u32 val) -> u8
            {
                if (val > 255)
                {
                    LOG("Color component values must be in the range 0-255 (inclusive)");
                    return 255;
                }
                return val;
            };

            u8 alpha = 255;
            if (color->value.HasMember("a"))
            {
                auto a = color->value.FindMember("a");
                if (!a->value.IsUint())
                {
                    LOG("Optional alpha (\"a\") component must also be an unsigned int (0-255)");
                    return Jasnah::None;
                }

                alpha = CheckAssignColor(a->value.GetUint());
            }

            const RGBA col(CheckAssignColor(red->value.GetUint()),
                           CheckAssignColor(green->value.GetUint()),
                           CheckAssignColor(blue->value.GetUint()), alpha);
            if (col.rgba == Color::White)
            {
                LOG("Cannot use White <r255, g255, b255, a255> as a custom colour, ignoring");
                continue;
            }
            colorVal = col.rgba;
        }
        else
        {
            LOG("Color types must be unsigned ints or an object");
            return Jasnah::None;
        }


        result.emplace(std::make_pair(colorVal, innerResult));
    }

    return result;
}

static
Jasnah::Option<std::unordered_map<u32, Constraint> >
ParseColorMapFromFile(const char* path)
{
    auto file = ReadFileAndNullTerminate(path);
    if (!file)
        return Jasnah::None;

    Document doc;
    if(doc.ParseInsitu(file->data()).HasParseError())
    {
        LOG("JSON Error<%s:%u>: %s",
            path,
            (unsigned)doc.GetErrorOffset(),
            GetParseError_En(doc.GetParseError()));
        return Jasnah::None;
    }

    if (!doc.HasMember("ColorMap"))
    {
        LOG("ColorMap not found in provided ColorMapFile");
        return Jasnah::None;
    }

    auto cmap = doc.FindMember("ColorMap");
    if (!cmap->value.IsArray())
    {
        LOG("ColorMap value is not an array");
        return Jasnah::None;
    }

    return ParseColorMap(cmap->value);
}

static
Jasnah::Option<Cfg::GridConfigData>
ParseConfig(std::vector<char>* buf, const char* path)
{
    Document doc;
    if(doc.ParseInsitu(buf->data()).HasParseError())
    {
        LOG("JSON Error<%s:%u>: %s",
            path,
            (unsigned)doc.GetErrorOffset(),
            GetParseError_En(doc.GetParseError()));
        return Jasnah::None;
    }

    // NOTE(Chris): We require these members to be present
    // to understand the data
    if (!doc.HasMember("ImagePath"))
    {
        LOG("JSON Error: No ImagePath member.");
        return Jasnah::None;
    }
    // Can only have one of the two
    if (!doc.HasMember("ColorMap") && !doc.HasMember("ColorMapPath"))
    {
        LOG("JSON Error: No ColorMap/ColorMapPath member.");
        return Jasnah::None;
    }
    else if (doc.HasMember("ColorMap") && doc.HasMember("ColorMapPath"))
    {
        LOG("JSON Error: Cannot define both ColorMap and ColorMapPath. Please use just one");
        return Jasnah::None;
    }

    // NOTE(Chris): We will just default to 100 instead
    // if (!doc.HasMember("PixelsPerMeter"))
    // {
    //     LOG("JSON Error: No PixelsPerMeter member");
    //     return Jasnah::None;
    // }

    Cfg::GridConfigData result;

    for (auto iter = doc.MemberBegin();
         iter != doc.MemberEnd();
         ++iter)
    {
        switch (StringHash(iter->name.GetString()))
        {

        case StringHash("ImagePath"):
        {
            if (!iter->value.IsString())
            {
                LOG("ImagePath must be a path (string)");
                return Jasnah::None;
            }
            result.imagePath = iter->value.GetString();
        } break;

        case StringHash("ScaleFactor"):
        {
            if (!iter->value.IsUint())
            {
                LOG("ScaleFactor msut be an unsigned integer");
                return Jasnah::None;
            }
            result.scaleFactor = iter->value.GetUint();
        } break;

        case StringHash("ColorMap"):
        {
            if (!iter->value.IsArray())
            {
                LOG("ColorMap must be an array of Constraint obejcts "
                    "(containing a type string and a value for Constant types)");
                return Jasnah::None;
            }

            const auto constraints = ParseColorMap(iter->value);
            if (!constraints)
            {
                LOG("ColorMap array invalid");
                return Jasnah::None;
            }
            result.constraints = *constraints;

        } break;

        case StringHash("ColorMapPath"):
        {
            if (!iter->value.IsString())
            {
                LOG("ColorMapPath must be a path (string)");
                return Jasnah::None;
            }
            const auto constraints = ParseColorMapFromFile(iter->value.GetString());
            if (!constraints)
            {
                LOG("ColorMap array from loaded file invalid");
                return Jasnah::None;
            }
            result.constraints = *constraints;

        } break;

        case StringHash("PixelsPerMeter"):
        {
            if (!iter->value.IsNumber())
            {
                LOG("PixelsPerMeter member must be numeric");
                return Jasnah::None;
            }
            result.pixelsPerMeter = iter->value.GetDouble();
        } break;

        case StringHash("MaxIterations"):
        {
            if (!iter->value.IsUint64())
            {
                LOG("MaxIterations member must be an unsigned integer");
                return Jasnah::None;
            }
            result.maxIter = iter->value.GetUint64();
        } break;

        case StringHash("MaxRelErr"):
        {
            if (!iter->value.IsNumber())
            {
                LOG("RelativeZeroTolerance member must be a positive double");
                return Jasnah::None;
            }
            result.zeroTol = std::abs(iter->value.GetDouble());
        } break;

        case StringHash("HorizontalZip"):
        {
            if (!iter->value.IsBool())
            {
                LOG("HorizontaZip member must be a bool type");
                return Jasnah::None;
            }
            result.horizZip = iter->value.GetBool();
        } break;

        case StringHash("VerticalZip"):
        {
            if (!iter->value.IsBool())
            {
                LOG("VerticalZip member must be a bool type");
                return Jasnah::None;
            }
            result.verticZip = iter->value.GetBool();
        } break;

        case StringHash("AnalyticInnerRadius"):
        {
            if (!iter->value.IsNumber())
            {
                LOG("AnalyticInnerRadius member must be a number");
                return Jasnah::None;
            }
            result.analyticInner = iter->value.GetDouble();
        } break;

        case StringHash("AnalyticOuterRadius"):
        {
            if (!iter->value.IsNumber())
            {
                LOG("AnalyticOuterRadius member must be a number");
                return Jasnah::None;
            }
            result.analyticOuter = iter->value.GetDouble();
        } break;

        case StringHash("CalculationMode"):
        {
            if (!iter->value.IsString())
            {
                LOG("CalculationMode must be a string");
                return Jasnah::None;
            }

            switch (StringHash(iter->value.GetString()))
            {
            case StringHash("FiniteDiff"):
            {
                result.mode = Cfg::CalculationMode::FiniteDiff;
            } break;

            case StringHash("MatrixInversion"):
            {
                result.mode = Cfg::CalculationMode::MatrixInversion;
            } break;

            case StringHash("SOR"):
            {
                result.mode = Cfg::CalculationMode::SOR;
            } break;

            case StringHash("AMR"):
            {
                result.mode = Cfg::CalculationMode::AMR;
            } break;

            default:
            {
                LOG("Unknown CalculationMode, using default");
            }
            }
        } break;

        default:
            LOG("Unknown key \"%s\", ignoring", iter->name.GetString());
        }
    }

    return result;
}

namespace Cfg
{
    // TODO(Chris): Add a mode switch to vary the reqd args
    Jasnah::Option<GridConfigData>
    LoadGridConfigFile(const char* path)
    {
        auto in = ReadFileAndNullTerminate(path);
        if (!in)
            return Jasnah::None;

        return ParseConfig(&*in, path);
    }

    Jasnah::Option<GridConfigData>
    LoadGridConfigString(const std::string& data)
    {
        std::vector<char> buf(data.begin(), data.end());
        buf.push_back('\0');
        return ParseConfig(&buf, "stdin");
    }

    bool
    WriteJSONPreprocFile(const JSONPreprocConfigVars& vars)
    {
        StringBuffer sb;
        PrettyWriter<StringBuffer> writer(sb);

        // NOTE(Chris): This behaves very similarly to the old fixed-function OpenGL pipeline
        writer.StartObject();
        writer.String("ImagePath");
        writer.String(vars.imgPath.c_str());

        writer.String("ColorMap");
        writer.StartArray();
        for (const auto col : vars.colorMap)
        {
            writer.StartObject();
            writer.String("r");
            writer.Uint(col.r);
            writer.String("g");
            writer.Uint(col.g);
            writer.String("b");
            writer.Uint(col.b);
            writer.EndObject();
        }
        writer.EndArray();

        writer.String("PixelsPerMeter");
        writer.Double(100.0);

        writer.String("MaxIterations");
        writer.Uint64(1000000);

        writer.String("MaxRelErr");
        writer.Double(0.00001);

        // TODO(Chris): We could check and output the correct zip info
        writer.String("HorizontalZip");
        writer.Bool(false);
        writer.String("VerticalZip");
        writer.Bool(false);

        writer.EndObject();

        // std::string fileName(tmpnam(nullptr));
        // FILE* tempFile = fopen(fileName.c_str(), "w");
        // if (!tempFile)
        // {
        //     fclose(tempFile);
        //     return false;
        // }

        // fputs(sb.GetString(), tempFile);
        // fputs(fileName.c_str(), stderr);

        // fclose(tempFile);
        puts("**JSON_START**");
        puts(sb.GetString());
        puts("**EOF**");
        return true;
    }
}
