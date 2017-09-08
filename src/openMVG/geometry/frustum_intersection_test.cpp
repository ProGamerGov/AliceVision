// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include "aliceVision/geometry/half_space_intersection.hpp"
#include "aliceVision/geometry/frustum.hpp"

#include "aliceVision/multiview/test_data_sets.hpp"
#include "aliceVision/multiview/projection.hpp"

#include "CppUnitLite/TestHarness.h"
#include "testing/testing.h"
#include <iostream>

using namespace aliceVision;
using namespace aliceVision::geometry;
using namespace aliceVision::geometry::halfPlane;
using namespace std;

//--
// Camera frustum intersection unit test
//--

TEST(frustum, intersection)
{
  const int focal = 1000;
  const int principal_Point = 500;
  //-- Setup a circular camera rig or "cardioid".
  const int iNviews = 4;
  const int iNbPoints = 6;
  const NViewDataSet d =
    NRealisticCamerasRing(
    iNviews, iNbPoints,
    nViewDatasetConfigurator(focal, focal, principal_Point, principal_Point, 5, 0));

  // Test with infinite Frustum for each camera
  {
    std::vector<Frustum> vec_frustum;
    for (int i=0; i < iNviews; ++i)
    {
      vec_frustum.push_back(
        Frustum(principal_Point*2, principal_Point*2, d._K[i], d._R[i], d._C[i]));
      EXPECT_TRUE(vec_frustum[i].isInfinite());
    }

    // Check that frustums have an overlap
    for (int i = 0; i < iNviews; ++i)
      for (int j = 0; j < iNviews; ++j)
        EXPECT_TRUE(vec_frustum[i].intersect(vec_frustum[j]));
  }

  // Test with truncated frustum
  {
    // Build frustum with near and far plane defined by min/max depth per camera
    std::vector<Frustum> vec_frustum;
    for (int i=0; i < iNviews; ++i)
    {
      double minDepth = std::numeric_limits<double>::max();
      double maxDepth = std::numeric_limits<double>::min();
      for (int j=0; j < iNbPoints; ++j)
      {
        const double depth = Depth(d._R[i], d._t[i], d._X.col(j));
        if (depth < minDepth)
          minDepth = depth;
        if (depth > maxDepth)
          maxDepth = depth;
      }
      vec_frustum.push_back(
        Frustum(principal_Point*2, principal_Point*2,
          d._K[i], d._R[i], d._C[i], minDepth, maxDepth));
      EXPECT_TRUE(vec_frustum[i].isTruncated());
    }

    // Check that frustums have an overlap
    for (int i = 0; i < iNviews; ++i)
      for (int j = 0; j < iNviews; ++j)
        EXPECT_TRUE(vec_frustum[i].intersect(vec_frustum[j]));
  }
}

TEST(frustum, empty_intersection)
{
  // Create infinite frustum that do not share any space
  //--
  // 4 cameras on a circle that look to the same direction
  // Apply a 180° rotation to the rotation matrix in order to make the cameras
  //  don't share any visual hull
  //--

  const int focal = 1000;
  const int principal_Point = 500;
  const int iNviews = 4;
  const int iNbPoints = 6;
  const NViewDataSet d =
    NRealisticCamerasRing(
    iNviews, iNbPoints,
    nViewDatasetConfigurator(focal, focal, principal_Point, principal_Point, 5, 0));

  // Test with infinite Frustum for each camera
  {
    std::vector<Frustum> vec_frustum;
    for (int i=0; i < iNviews; ++i)
    {
      const Mat3 flipMatrix = RotationAroundY(D2R(180));
      vec_frustum.push_back(
        Frustum(principal_Point*2, principal_Point*2, d._K[i], d._R[i]*flipMatrix, d._C[i]));
      EXPECT_TRUE(vec_frustum[i].isInfinite());
    }

    // Test if the frustum have an overlap
    for (int i=0; i < iNviews; ++i)
    {
      for (int j=0; j < iNviews; ++j)
      {
        if (i == j) // Same frustum (intersection must exist)
        {
          EXPECT_TRUE(vec_frustum[i].intersect(vec_frustum[j]));
        }
        else // different frustum
        {
          EXPECT_FALSE(vec_frustum[i].intersect(vec_frustum[j]));
        }
      }
    }
  }

  // Test but with truncated frustum
  {
    // Build frustum with near and far plane defined by min/max depth per camera
    std::vector<Frustum> vec_frustum;
    for (int i=0; i < iNviews; ++i)
    {
      double minDepth = std::numeric_limits<double>::max();
      double maxDepth = std::numeric_limits<double>::min();
      for (int j=0; j < iNbPoints; ++j)
      {
        const double depth = Depth(d._R[i], d._t[i], d._X.col(j));
        if (depth < minDepth)
          minDepth = depth;
        if (depth > maxDepth)
          maxDepth = depth;
      }
      const Mat3 flipMatrix = RotationAroundY(D2R(180));
      vec_frustum.push_back(
        Frustum(principal_Point*2, principal_Point*2,
          d._K[i], d._R[i]*flipMatrix, d._C[i], minDepth, maxDepth));
      EXPECT_TRUE(vec_frustum[i].isTruncated());
    }

    // Test if the frustum have an overlap
    for (int i=0; i < iNviews; ++i)
    {
      for (int j=0; j < iNviews; ++j)
      {
        if (i == j) // Same frustum (intersection must exist)
        {
          EXPECT_TRUE(vec_frustum[i].intersect(vec_frustum[j]));
        }
        else // different frustum
        {
          EXPECT_FALSE(vec_frustum[i].intersect(vec_frustum[j]));
        }
      }
    }
  }
}

/* ************************************************************************* */
int main() { TestResult tr; return TestRegistry::runAllTests(tr);}
/* ************************************************************************* */
