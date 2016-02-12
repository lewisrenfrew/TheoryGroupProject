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
#include "Utility.hpp"

using Jasnah::Option;
using Jasnah::None;

typedef V2<uint> V2u;

// template <typename T, typename... Args>
// std::unique_ptr<T>
// make_unique(Args&&... args)
// {
//     return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
// }

struct BB
{
    V2u NW, SE;
    BB(V2u nw, V2u se) : NW(nw), SE(se) {}

    inline
    bool
    IsInside(V2u pt) const
    {
        return pt.x > NW.x
            && pt.x < SE.x
            && pt.y > NW.y
            && pt.y < SE.y;
    }

    inline
    uint
    MaxSize() const
    {
        if (NW.x - SE.x > NW.y - SE.y)
            return NW.x - SE.x;
        else
            return NW.y - SE.y;
    }

    inline
    uint
    MinSize() const
    {
        if (NW.x - SE.x > NW.y - SE.y)
            return NW.y - SE.y;
        else
            return NW.x - SE.x;
    }

    inline
    uint
    SizeX() const
    {
        return SE.x - NW.x;
    }

    inline
    uint
    SizeY() const
    {
        return SE.y - NW.y;
    }
};

enum QTOrder
{
    QT_NW = 0,
    QT_NE = 1,
    QT_SE = 2,
    QT_SW = 3,
};

class QuadTree
{
public:
    typedef std::array<std::unique_ptr<QuadTree>, 4> NodeArray;
    // NOTE(Chris): We store in order NW, NE, SE, SW i.e. CW
    Option<NodeArray> subNodes;
    const uint depth;
    const uint maxDepth;
    f64 nodeVal;
    const BB box;

    QuadTree(BB _box, uint _depth, uint _maxDepth)
        : subNodes(None),
          depth(_depth),
          maxDepth(_maxDepth),
          nodeVal(0.0),
          box(_box)
    {}

    // TODO(Chris): Currently only supporting power of 2 grids
    QuadTree(const Grid& base, uint _maxDepth)
        : subNodes(None),
          nodeVal(0.0),
          depth(0),
          maxDepth(_maxDepth),
          box(V2u(0,0), V2u(base.lineLength, base.numLines)),
          baseGrid(&base)
    {}

    void
    Clear()
    {
        subNodes = None;
    }

    void
    SubDivide()
    {
        if (box.MinSize() < 2 || depth == maxDepth)
            return; // We can't redivide this further

        const uint x1 = box.SizeX() / 2;
        const uint x2 = box.SizeX() - x1;
        const uint y1 = box.SizeY() / 2;
        const uint y2 = box.SizeY() - y1;

        NodeArray nodes;
        nodes[0] = make_unique<QuadTree>(BB(box.NW,
                                            V2u(box.NW.x + x1, box.NW.y + y1)),
                                         depth+1, maxDepth);
        nodes[1] = make_unique<QuadTree>(BB(V2u(box.NW.x + x1, box.NW.y),
                                            V2u(box.SE.x, box.NW.y + y1)),
                                         depth+1, maxDepth);
        nodes[2] = make_unique<QuadTree>(BB(V2u(box.NW.x + x1, box.NW.y + y1),
                                            box.SE),
                                         depth+1, maxDepth);
        nodes[3] = make_unique<QuadTree>(BB(V2u(box.NW.x, box.NW.y + y1),
                                            V2u(box.NW.x + x1, box.SE.y)),
                                         depth+1, maxDepth);

        subNodes = std::move(nodes);
    }

private:
    const Grid* baseGrid;
};

// NOTE(Chris): The following method is based on "Finite Approximate
// Schemes with Adaptive Grid Refinement" By Dai and Tsukerman 2008
// IEEE Transactions of Magnetics except we use a finite difference method on the grid
void
AMRFDM()
{

}
