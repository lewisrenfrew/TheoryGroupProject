/* ==========================================================================
   $File: tests.cpp $
   $Version: 1.0 $
   $Notice: (C) Copyright 2015 Chris Osborne. All Rights Reserved. $
   $License: MIT: http://opensource.org/licenses/MIT $
   ========================================================================== */

#define CATCH_CONFIG_MAIN
#include "../src/catch.hpp"
#include "../src/grid.cpp"

unsigned int Factorial( unsigned int number ) 
    {
    return number <= 1 ? number : Factorial(number-1)*number;
    }

TEST_CASE( "Factorials are computed", "[factorial]" )
    {
    REQUIRE( Factorial(1) == 1);
    REQUIRE( Factorial(2) == 2);
    REQUIRE( Factorial(3) == 6);
    REQUIRE( Factorial(10) == 3628800);
    }

TEST_CASE( "Linear Interpolation", "[LerpNPointsBetweenVoltages]")
    {
  
     std::vector<f64> vec = {0.0, 0.0};
     REQUIRE( LerpNPointsBetweenVoltages(0,0,2) == vec);

     std::vector<f64> vec1 = {0,0,0};
     REQUIRE( LerpNPointsBetweenVoltages(0, 0, 1) == vec1);

     std::vector<f64> vec2 = {1,1,1};
     REQUIRE( LerpNPointsBetweenVoltages(1, 1, 1) == vec2);

     std::vector<f64> vec3 = {2,2,2};
     REQUIRE( LerpNPointsBetweenVoltages(2, 2, 1) == vec3);

     std::vector<f64> vec4 = {1,2,3};
     REQUIRE( LerpNPointsBetweenVoltages(1, 3, 1) == vec4);

     std::vector<f64> vec5 = {-1,0,1};
     REQUIRE( LerpNPointsBetweenVoltages(-1, 1, 1) == vec5);

    }
