
#if !defined(GS_H)

#define GS_H
// Finite difference method implementations

#include "GlobalDefines.hpp"

class Grid;
namespace GaussSeidel
{
    // an ammended version of SolveGridLaplacianZero that uses GaussSeidel
    void
    SolveGridLaplacianZeroGaussSeidel(Grid* grid, const f64 zeroTol,
                           const u64 maxIter, bool parallel = true);
}
#endif
