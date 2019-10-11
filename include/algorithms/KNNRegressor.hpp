#pragma once

#include "KDTree.hpp"
#include "algorithms/util/FluidEigenMappings.hpp"
#include "data/FluidDataset.hpp"
#include "data/FluidTensor.hpp"
#include "data/TensorTypes.hpp"
#include <string>


namespace fluid {
namespace algorithm {

class KNNRegressor {

public:

  using DataSet = FluidDataset<std::string, double, std::string, 1>;
  double predict(KDTree<std::string> tree, DataSet targets, RealVectorView point, int k){
    using namespace std;
    auto nearest = tree.kNearest(point, k);
    double prediction = 0;
    double weight = 1.0/k;
    for(int i = 0; i < k; i++){
      auto id = nearest.getIds()(i);
      auto point = FluidTensor<double, 1>(1);
      targets.get(id, point);
      prediction += weight * point(0);
    }
    return prediction;
  }

};
} // namespace algorithm
} // namespace fluid
