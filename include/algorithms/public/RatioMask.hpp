#pragma once

#include "../../data/FluidTensor.hpp"
#include "../../data/TensorTypes.hpp"
#include "../util/FluidEigenMappings.hpp"
#include <Eigen/Dense>

namespace fluid {
namespace algorithm {

using _impl::asEigen;
using _impl::asFluid;

using Eigen::ArrayXXcd;
using Eigen::ArrayXXd;
using Eigen::Map;
using Eigen::Array;

class RatioMask {
  const double epsilon = std::numeric_limits<double>::epsilon();

public:
  RatioMask(RealMatrixView denominator, int exponent) : mExponent(exponent) {
    mMultiplier = (1 / asEigen<Array>(denominator).max(epsilon));
  }

  void process(const ComplexMatrixView &mixture, RealMatrixView targetMag,
               ComplexMatrixView result) {
    assert(mixture.cols() == targetMag.cols());
    assert(mixture.rows() == targetMag.rows());
    // ComplexMatrixView result(mixture.extent(0), mixture.extent(1));
    ArrayXXcd tmp =
        asEigen<Array>(mixture) *
        (asEigen<Array>(targetMag).pow(mExponent) * mMultiplier.pow(mExponent))
            .min(1.0);
    result = asFluid(tmp);
  }

private:
  ArrayXXd mMultiplier;
  int mExponent;
};

} // namespace algorithm
} // namespace fluid