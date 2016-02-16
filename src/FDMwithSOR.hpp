
#if !defined(FDMSOR_H)

#define FDMSOR_H
// Finite difference method implementations

#include "GlobalDefines.hpp"

class Grid;
namespace SOR
{
    // an ammended version of SolveGridLaplacianZero that uses succesive over relaxation
    void
    SolveGridLaplacianZero(Grid* grid, const f64 zeroTol,
                           const u64 maxIter, bool parallel = false);
}
#endif
