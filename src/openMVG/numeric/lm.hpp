// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#ifndef OPENMVG_NUMERIC_LM_H_
#define OPENMVG_NUMERIC_LM_H_

#include "aliceVision/numeric/numeric.h"
// Levenberg Marquardt Non Linear Optimization
#include <unsupported/Eigen/NonLinearOptimization>
#include <unsupported/Eigen/NumericalDiff>

namespace aliceVision
{
  using namespace Eigen;
// Generic functor Levenberg-Marquardt minimization
template<typename _Scalar, int NX=Dynamic, int NY=Dynamic>
struct Functor
{
  typedef _Scalar Scalar;
  enum {
    InputsAtCompileTime = NX,
    ValuesAtCompileTime = NY
  };
  typedef Matrix<Scalar,InputsAtCompileTime,1> InputType;
  typedef Matrix<Scalar,ValuesAtCompileTime,1> ValueType;
  typedef Matrix<Scalar,ValuesAtCompileTime,InputsAtCompileTime> JacobianType;

  const int m_inputs, m_values;

  Functor() : m_inputs(InputsAtCompileTime), m_values(ValuesAtCompileTime) {}
  Functor(int inputs, int values) : m_inputs(inputs), m_values(values) {}

  int inputs() const { return m_inputs; }
  int values() const { return m_values; }

  // you should define that in the subclass :
  //  void operator() (const InputType& x, ValueType* v, JacobianType* _j=0) const;
};

}; // namespace aliceVision

#endif // OPENMVG_NUMERIC_LM_H_
