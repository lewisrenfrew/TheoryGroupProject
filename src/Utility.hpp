// -*- c++ -*-
#if !defined(UTILITY_H)
/* ==========================================================================
   $File: Utility.hpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define UTILITY_H
#include "GlobalDefines.hpp"
#include <vector>
#include <memory>

#ifdef GOMP
#include <omp.h>
#else
extern "C" inline int omp_get_max_threads() { return 1; } // Glorious hack for clang not supporting OpenMP in mainline yet
extern "C" inline void omp_set_num_threads(int num) { (void)num; }
#endif

// NOTE(Chris): Macros from linux kernel for branch optimization
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

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

template <typename T>
using RemRefT = typename std::remove_reference<T>::type;

template <typename T, typename... Args>
inline std::unique_ptr<T>
make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

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
    inline const V2
    operator+(const V2& other) const
    {
        return V2(x+other.x, y+other.y);
    }
    // Subtraction operator
    inline const V2
    operator-(const V2& other) const
    {
        return V2(x-other.x, y-other.y);
    }
};

typedef V2<f64> V2d;

/// Returns a vector of numPoints points linearly interpolated between
/// v1 and v2, useful for some boundary conditions (top and bottom of
/// finite lengths in problem 1)
inline std::vector<f64>
LerpNPointsBetweenVoltages(const f64 v1, const f64 v2, const uint numPoints)
{
    // Set x0 as first point, field has potential v1 here, and v2 at
    // xn. We can interpolate them with the equation of a line:
    // a*xi+b = vi but we let xi = 0 then WLOG b = v1, a = (v2-v1)/(numPts-1)

    // We need at least 2 points between 2 voltages
    if (numPoints < 2)
        return std::vector<f64>();

    const f64 a = (v2 - v1)/(f64)(numPoints-1);
    const f64 b = v1;

    std::vector<f64> result;
    result.reserve(numPoints);

    for (uint i = 0; i < numPoints; ++i)
        result.push_back(a*i + b);

    return result;
}

typedef u32 HashedName;


namespace Impl
{
    constexpr HashedName CreateHash(HashedName prevHash, const char* ch)
    {
        return *ch == '\0' ? prevHash :
            CreateHash(*ch + (prevHash << 6) + (prevHash << 16) - prevHash, ch+1);

    }
}

/// Create 32bit hash from unsigned 8-bit values and initial
/// hash using the SDBM hash method
/// Methods are constexpr so can be used in switches
/// Same hash function as used by Urho3D (just recursive here).
constexpr HashedName StringHash(const char* str)
{
    return Impl::CreateHash(0, str);
}

#endif
