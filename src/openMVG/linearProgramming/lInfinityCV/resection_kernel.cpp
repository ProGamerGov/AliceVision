// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include "aliceVision/linearProgramming/linearProgrammingInterface.hpp"
#include "aliceVision/linearProgramming/linearProgrammingOSI_X.hpp"
#include "aliceVision/linearProgramming/bisectionLP.hpp"

#include "aliceVision/linearProgramming/lInfinityCV/resection.hpp"
#include "aliceVision/linearProgramming/lInfinityCV/resection_kernel.hpp"

#include <cassert>

namespace aliceVision {
namespace lInfinityCV {
namespace kernel {

using namespace std;

void translate(const Mat3X & X, const Vec3 & vecTranslation,
               Mat3X * XPoints) {
  XPoints->resize(X.rows(), X.cols());
  for (size_t i=0; i<X.cols(); ++i)  {
    XPoints->col(i) = X.col(i) + vecTranslation;
  }
}

void l1SixPointResectionSolver::Solve(const Mat &pt2D, const Mat &pt3d, vector<Mat34> *Ps) {
  assert(2 == pt2D.rows());
  assert(3 == pt3d.rows());
  assert(6 <= pt2D.cols());
  assert(pt2D.cols() == pt3d.cols());

  //-- Translate 3D points in order to have X0 = (0,0,0,1).
  Vec3 vecTranslation = - pt3d.col(0);
  Mat4 translationMatrix = Mat4::Identity();
  translationMatrix.block<3,1>(0,3) = vecTranslation;

  Mat3X XPoints;
  translate(pt3d, vecTranslation, &XPoints);

  std::vector<double> vec_solution(11);
  OSI_CLP_SolverWrapper wrapperLpSolve(vec_solution.size());
  Resection_L1_ConstraintBuilder cstBuilder(pt2D, XPoints);
  if(
    (BisectionLP<Resection_L1_ConstraintBuilder, LP_Constraints_Sparse>(
    wrapperLpSolve,
    cstBuilder,
    &vec_solution,
    1.0,
    0.0))
    )
  {
    // Move computed value to dataset for residual estimation.
    Mat34 P;
    P << vec_solution[0], vec_solution[1], vec_solution[2], vec_solution[3],
         vec_solution[4], vec_solution[5], vec_solution[6], vec_solution[7],
         vec_solution[8], vec_solution[9], vec_solution[10], 1.0;
    P = P * translationMatrix;
    Ps->push_back(P);
  }
}

}  // namespace kernel
}  // namespace lInfinityCV
}  // namespace aliceVision
