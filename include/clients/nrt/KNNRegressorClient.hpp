/*
Part of the Fluid Corpus Manipulation Project (http://www.flucoma.org/)
Copyright 2017-2019 University of Huddersfield.
Licensed under the BSD-3 License.
See license.md file in the project root for full license information.
This project has received funding from the European Research Council (ERC)
under the European Union’s Horizon 2020 research and innovation programme
(grant agreement No 725899).
*/

#pragma once

#include "DataSetClient.hpp"
#include "NRTClient.hpp"
#include "../../algorithms/public/KNNRegressor.hpp"

namespace fluid {
namespace client {
namespace knnregressor {

struct KNNRegressorData
{
  algorithm::KDTree                    tree{0};
  FluidDataSet<std::string, double, 1> target{1};
  index                                size() { return target.size(); }
  index                                dims() { return tree.dims(); }
  void                                 clear()
  {
    tree.clear();
    target = FluidDataSet<std::string, double, 1>();
  }
  bool initialized() const { return tree.initialized(); }
};

void to_json(nlohmann::json& j, const KNNRegressorData& data)
{
  j["tree"] = data.tree;
  j["target"] = data.target;
}

bool check_json(const nlohmann::json& j, const KNNRegressorData&)
{
  return fluid::check_json(j, {"tree", "target"},
                           {JSONTypes::OBJECT, JSONTypes::OBJECT});
}

void from_json(const nlohmann::json& j, KNNRegressorData& data)
{
  data.tree = j["tree"].get<algorithm::KDTree>();
  data.target = j["target"].get<FluidDataSet<std::string, double, 1>>();
}


constexpr auto KNNRegressorParams = defineParameters(
    StringParam<Fixed<true>>("name", "Name"),
    LongParam("numNeighbours", "Number of Nearest Neighbours", 3, Min(1)),
    EnumParam("weight", "Weight Neighbours by Distance", 1, "No", "Yes"));

class KNNRegressorClient : public FluidBaseClient,
                           OfflineIn,
                           OfflineOut,
                           ModelObject,
                           public DataClient<KNNRegressorData>
{
  enum { kName, kNumNeighbors, kWeight };

public:
  using string = std::string;
  using BufferPtr = std::shared_ptr<BufferAdaptor>;
  using DataSet = FluidDataSet<string, double, 1>;
  using StringVector = FluidTensor<string, 1>;

  using ParamDescType = decltype(KNNRegressorParams);

  using ParamSetViewType = ParameterSetView<ParamDescType>;
  std::reference_wrapper<ParamSetViewType> mParams;

  void setParams(ParamSetViewType& p) { mParams = p; }

  template <size_t N>
  auto& get() const
  {
    return mParams.get().template get<N>();
  }

  static constexpr auto& getParameterDescriptors()
  {
    return KNNRegressorParams;
  }

  KNNRegressorClient(ParamSetViewType& p) : mParams(p)
  {
    audioChannelsIn(1);
    controlChannelsOut({1, 1});
  }

  template <typename T>
  Result process(FluidContext&)
  {
    return {};
  }

  MessageResult<string> fit(DataSetClientRef datasetClient,
                            DataSetClientRef targetClient)
  {
    auto datasetClientPtr = datasetClient.get().lock();
    if (!datasetClientPtr) return Error<string>(NoDataSet);
    auto dataSet = datasetClientPtr->getDataSet();
    if (dataSet.size() == 0) return Error<string>(EmptyDataSet);
    auto targetClientPtr = targetClient.get().lock();
    if (!targetClientPtr) return Error<string>(NoDataSet);
    auto target = targetClientPtr->getDataSet();
    if (target.size() == 0) return Error<string>(EmptyDataSet);
    if (dataSet.size() != target.size()) return Error<string>(SizesDontMatch);
    mAlgorithm.tree = algorithm::KDTree{dataSet};
    mAlgorithm.target = target;
    mAlgorithm = {mAlgorithm.tree, mAlgorithm.target};
    return {};
  }

  MessageResult<double> predictPoint(BufferPtr data) const
  {
    index k = get<kNumNeighbors>();
    bool  weight = get<kWeight>() != 0;
    if (k == 0) return Error<double>(SmallK);
    if (mAlgorithm.tree.size() == 0) return Error<double>(NoDataFitted);
    if (mAlgorithm.tree.size() < k) return Error<double>(NotEnoughData);
    InBufferCheck bufCheck(mAlgorithm.tree.dims());
    if (!bufCheck.checkInputs(data.get()))
      return Error<double>(bufCheck.error());
    algorithm::KNNRegressor regressor;
    RealVector              point(mAlgorithm.tree.dims());
    point = BufferAdaptor::ReadAccess(data.get())
                .samps(0, mAlgorithm.tree.dims(), 0);
    double result =
        regressor.predict(mAlgorithm.tree, mAlgorithm.target, point, k, weight);
    return result;
  }

  MessageResult<void> predict(DataSetClientRef source,
                              DataSetClientRef dest) const
  {
    index k = get<kNumNeighbors>();
    bool  weight = get<kWeight>() != 0;
    auto  sourcePtr = source.get().lock();
    if (!sourcePtr) return Error(NoDataSet);
    auto dataSet = sourcePtr->getDataSet();
    if (dataSet.size() == 0) return Error(EmptyDataSet);
    auto destPtr = dest.get().lock();
    if (!destPtr) return Error(NoDataSet);
    if (dataSet.pointSize() != mAlgorithm.tree.dims())
      return Error(WrongPointSize);
    if (k == 0) return Error(SmallK);
    if (mAlgorithm.tree.size() == 0) return Error(NoDataFitted);
    if (mAlgorithm.tree.size() < k) return Error(NotEnoughData);

    algorithm::KNNRegressor regressor;
    auto                    ids = dataSet.getIds();
    auto                    data = dataSet.getData();
    DataSet                 result(1);
    for (index i = 0; i < dataSet.size(); i++)
    {
      RealVectorView point = data.row(i);
      RealVector     prediction = {regressor.predict(
          mAlgorithm.tree, mAlgorithm.target, point, k, weight)};
      result.add(ids(i), prediction);
    }
    destPtr->setDataSet(result);
    return OK();
  }

  static auto getMessageDescriptors()
  {
    return defineMessages(
        makeMessage("fit", &KNNRegressorClient::fit),
        makeMessage("predict", &KNNRegressorClient::predict),
        makeMessage("predictPoint", &KNNRegressorClient::predictPoint),
        makeMessage("cols", &KNNRegressorClient::dims),
        makeMessage("clear", &KNNRegressorClient::clear),
        makeMessage("size", &KNNRegressorClient::size),
        makeMessage("load", &KNNRegressorClient::load),
        makeMessage("dump", &KNNRegressorClient::dump),
        makeMessage("write", &KNNRegressorClient::write),
        makeMessage("read", &KNNRegressorClient::read));
  }

private:
  FluidInputTrigger mTrigger;
};

using KNNRegressorRef = SharedClientRef<KNNRegressorClient>;

constexpr auto KNNRegressorQueryParams = defineParameters(
    KNNRegressorRef::makeParam("model", "Source model"),
    LongParam("numNeighbours", "Number of Nearest Neighbours", 3, Min(1)),
    EnumParam("weight", "Weight Neighbours by Distance", 1, "No", "Yes"),
    BufferParam("inputPointBuffer", "Input Point Buffer"),
    BufferParam("predictionBuffer", "Prediction Buffer"));

class KNNRegressorQuery : public FluidBaseClient, ControlIn, ControlOut
{
  enum { kModel, kNumNeighbors, kWeight, kInputBuffer, kOutputBuffer };

public:
  using ParamDescType = decltype(KNNRegressorQueryParams);

  using ParamSetViewType = ParameterSetView<ParamDescType>;
  std::reference_wrapper<ParamSetViewType> mParams;

  void setParams(ParamSetViewType& p) { mParams = p; }

  template <size_t N>
  auto& get() const
  {
    return mParams.get().template get<N>();
  }

  static constexpr auto& getParameterDescriptors()
  {
    return KNNRegressorQueryParams;
  }

  KNNRegressorQuery(ParamSetViewType& p) : mParams(p)
  {
    controlChannelsIn(1);
    controlChannelsOut({1, 1});
  }


  template <typename T>
  void process(std::vector<FluidTensorView<T, 1>>& input,
               std::vector<FluidTensorView<T, 1>>& output, FluidContext&)
  {
    output[0] = input[0];
    if (input[0](0) > 0)
    {
      auto knnPtr = get<kModel>().get().lock();
      if (!knnPtr)
      {
        // report error?
        return;
      }
      const KNNRegressorData& algorithm = knnPtr->algorithm();
      index             k = get<kNumNeighbors>();
      bool              weight = get<kWeight>() != 0;
      if (k == 0 || algorithm.tree.size() == 0 || algorithm.tree.size() < k)
        return;
      InOutBuffersCheck bufCheck(algorithm.tree.dims());
      if (!bufCheck.checkInputs(get<kInputBuffer>().get(),
                                get<kOutputBuffer>().get()))
        return;
      auto outBuf = BufferAdaptor::Access(get<kOutputBuffer>().get());
      if (outBuf.samps(0).size() != 1) return;

      algorithm::KNNRegressor regressor;

      RealVector point(algorithm.tree.dims());
      point = BufferAdaptor::ReadAccess(get<kInputBuffer>().get())
                  .samps(0, algorithm.tree.dims(), 0);
                  
      // mTrigger.process(input, output, [&]() {
        double result = regressor.predict(algorithm.tree, algorithm.target,
                                          point, k, weight);
        outBuf.samps(0)[0] = result;
      // });
    }
  }

  index latency() { return 0; }
};

} // namespace knnregressor

using NRTThreadedKNNRegressorClient =
    NRTThreadingAdaptor<typename knnregressor::KNNRegressorRef::SharedType>;

using RTKNNRegressorQueryClient =
    ClientWrapper<knnregressor::KNNRegressorQuery>;

} // namespace client
} // namespace fluid
