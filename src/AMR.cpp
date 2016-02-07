/* ==========================================================================
   $File: AMR.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2016 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#include "AMR.hpp"
#include <array>
#include "Jasnah.hpp"
#include "Utility.hpp"
#include "Grid.hpp"

using Jasnah::Option;
using Jasnah::None;

typedef V2<uint> V2u;

template <typename T, typename... Args>
std::unique_ptr<T>
make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

struct BB
{
    V2u NW, SE;
    BB(V2u nw, V2u se) : NW(nw), SE(se) {}

    bool
    IsInside(V2u pt)
    {
        return pt.x > NW.x
            && pt.x < SE.x
            && pt.y > NW.y
            && pt.y < SE.y;
    }

    uint
    MaxSize()
    {
        if (NW.x - SE.x > NW.y - SE.y)
            return NW.x - SE.x;
        else
            return NW.y - SE.y;
    }

    uint
    MinSize()
    {
        if (NW.x - SE.x > NW.y - SE.y)
            return NW.y - SE.y;
        else
            return NW.x - SE.x;
    }

    uint
    SizeX()
    {
        return SE.x - NW.x;
    }

    uint
    SizeY()
    {
        return SE.y - NW.y;
    }
};

class QuadTree
{
public:
    QuadTree(BB box) : subNodes({nullptr}), nodeVal(0.0), box(box), subDivided(false), baseGrid(nullptr){}
    QuadTree(const Grid& base) : subNodes({nullptr}), nodeVal(0.0), box(V2u(0,0), V2u(base.lineLength, base.numLines)), subDivided(false), baseGrid(&base) {}
    Option<QuadTree*> GetNW()
    {
        if (subDivided)
            return subNodes[0].get();
        else
            return None;
    }

    Option<QuadTree*> GetNE()
    {
        if (subDivided)
            return subNodes[1].get();
        else
            return None;
    }

    Option<QuadTree*> GetSE()
    {
        if (subDivided)
            return subNodes[2].get();
        else
            return None;
    }

    Option<QuadTree*> GetSW()
    {
        if (subDivided)
            return subNodes[3].get();
        else
            return None;
    }

    // Option<std::array<std::unique_ptr<QuadTree>, 4> >
    // GetNodes()
    // {
    //     if (subDivided)
    //         return subNodes;
    //     else
    //         return None;
    // }

    void
    SubDivide()
    {
        if (box.MinSize() < 4)
            return; // We can't redivide this further

        const uint x1 = box.SizeX() / 2;
        const uint x2 = box.SizeX() - x1;
        const uint y1 = box.SizeY() / 2;
        const uint y2 = box.SizeY() - y1;

        subNodes[0] = make_unique<QuadTree>(BB(box.NW, V2u(box.NW.x + x1, box.NW.y + y1)));
        subNodes[1] = make_unique<QuadTree>(BB(V2u(box.NW.x + x1, box.NW.y), V2u(box.SE.x, box.NW.y + y1)));
        subNodes[2] = make_unique<QuadTree>(BB(V2u(box.NW.x + x1, box.NW.y + y1), box.SE));
        subNodes[3] = make_unique<QuadTree>(BB(V2u(box.NW.x, box.NW.y + y1), V2u(box.NW.x + x1, box.SE.y)));
    }

private:
    // NOTE(Chris): We store in order NW, NE, SE, SW i.e. CW
    std::array<std::unique_ptr<QuadTree>, 4> subNodes;
    f64 nodeVal;
    BB box;
    bool subDivided;
    const Grid* baseGrid;
};
