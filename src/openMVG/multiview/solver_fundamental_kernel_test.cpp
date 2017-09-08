// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include "aliceVision/multiview/solver_fundamental_kernel.hpp"
#include "aliceVision/multiview/projection.hpp"
#include "testing/testing.h"

using namespace aliceVision;
using namespace std;

// Check that sin(angle(a, b)) < tolerance.
template<typename A, typename B>
bool Colinear(const A &a, const B &b, double tolerance) {
  bool dims_match = (a.rows() == b.rows()) && (a.cols() == b.cols());
  if (!dims_match) {
    return false;
  }
  double c = CosinusBetweenMatrices(a, b);
  if (c * c < 1) {
    double s = sqrt(1 - c * c);
    return fabs(s) < tolerance;
  }
  return false;
}

// Check the properties of a fundamental matrix:
//
//   1. The determinant is 0 (rank deficient)
//   2. The condition x'T*F*x = 0 is satisfied to precision.
//
template<typename TMat>
bool ExpectFundamentalProperties(const TMat &F,
                                 const Mat &ptsA,
                                 const Mat &ptsB,
                                 double precision) {
  bool bOk = true;
  bOk &= F.determinant() < precision;
  assert(ptsA.cols() == ptsB.cols());
  Mat hptsA, hptsB;
  EuclideanToHomogeneous(ptsA, &hptsA);
  EuclideanToHomogeneous(ptsB, &hptsB);
  for (int i = 0; i < ptsA.cols(); ++i) {
    double residual = hptsB.col(i).dot(F * hptsA.col(i));
    bOk &= residual < precision;
  }
  return bOk;
}

// Check the fundamental fitting:
//
//   1. Estimate the fundamental matrix
//   2. Check epipolar distance.
//
template <class Kernel>
bool ExpectKernelProperties(const Mat &x1,
                              const Mat &x2,
                              Mat3 *F_expected = nullptr) {
  bool bOk = true;
  Kernel kernel(x1, x2);
  vector<size_t> samples;
  for (size_t i = 0; i < x1.cols(); ++i) {
    samples.push_back(i);
  }
  vector<Mat3> Fs;
  kernel.Fit(samples, &Fs);

  bOk &= (!Fs.empty());
  for (int i = 0; i < Fs.size(); ++i) {
    bOk &= ExpectFundamentalProperties(Fs[i], x1, x2, 1e-8);
    if (F_expected) {
      bOk &= Colinear(Fs[i], *F_expected, 1e-6);
    }
  }
  return bOk;
}

TEST(SevenPointTest, EasyCase) {
  Mat x1(2, 7), x2(2, 7);
  x1 << 0, 0, 0, 1, 1, 1, 2,
        0, 1, 2, 0, 1, 2, 0;
  x2 << 0, 0, 0, 1, 1, 1, 2,
        1, 2, 3, 1, 2, 3, 1;
  typedef fundamental::kernel::SevenPointKernel Kernel;
  EXPECT_TRUE(ExpectKernelProperties<Kernel>(x1, x2));
}

TEST(SevenPointTest_Normalized, EasyCase) {
  Mat x1(2, 7), x2(2, 7);
  x1 << 0, 0, 0, 1, 1, 1, 2,
    0, 1, 2, 0, 1, 2, 0;
  x2 << 0, 0, 0, 1, 1, 1, 2,
    1, 2, 3, 1, 2, 3, 1;
  typedef fundamental::kernel::NormalizedSevenPointKernel Kernel;
  EXPECT_TRUE(ExpectKernelProperties<Kernel>(x1, x2));
}

TEST(EightPointTest, EasyCase) {
  Mat x1(2, 8), x2(2, 8);
  x1 << 0, 0, 0, 1, 1, 1, 2, 2,
        0, 1, 2, 0, 1, 2, 0, 1;
  x2 << 0, 0, 0, 1, 1, 1, 2, 2,
        1, 2, 3, 1, 2, 3, 1, 2;
  typedef fundamental::kernel::EightPointKernel Kernel;
  EXPECT_TRUE(ExpectKernelProperties<Kernel>(x1, x2));
}

TEST(EightPointTest_Normalized, EasyCase) {
  Mat x1(2, 8), x2(2, 8);
  x1 << 0, 0, 0, 1, 1, 1, 2, 2,
    0, 1, 2, 0, 1, 2, 0, 1;
  x2 << 0, 0, 0, 1, 1, 1, 2, 2,
    1, 2, 3, 1, 2, 3, 1, 2;
  typedef fundamental::kernel::NormalizedEightPointKernel Kernel;
  EXPECT_TRUE(ExpectKernelProperties<Kernel>(x1, x2));
}

/* ************************************************************************* */
int main() { TestResult tr; return TestRegistry::runAllTests(tr);}
/* ************************************************************************* */
