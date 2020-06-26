#pragma once
#include "../common/SharedClientUtils.hpp"
#include "DataClient.hpp"
#include "NRTClient.hpp"
#include "data/FluidDataSet.hpp"
#include <sstream>
#include <string>

namespace fluid {
namespace client {

class DataSetClient : public FluidBaseClient, OfflineIn, OfflineOut,
  public DataClient<FluidDataSet<std::string, double, 1>> {
  enum { kName };

public:
  using string = std::string;
  using BufferPtr = std::shared_ptr<BufferAdaptor>;
  using DataSet = FluidDataSet<string, double, 1>;

  template <typename T> Result process(FluidContext &) { return {}; }
  FLUID_DECLARE_PARAMS(StringParam<Fixed<true>>("name", "DataSet"));

  DataSetClient(ParamSetViewType &p): mParams(p) {}

  MessageResult<void> addPoint(string id, BufferPtr data) {
    DataSet& dataset = mAlgorithm;
    if (!data)
      return Error(NoBuffer);
    BufferAdaptor::Access buf(data.get());
    if (!buf.exists())
      return Error(InvalidBuffer);
    if (buf.numFrames() == 0)
      return Error(EmptyBuffer);
    if (dataset.size() == 0) {
      if (dataset.dims() != buf.numFrames())
        dataset = DataSet(buf.numFrames());
    } else if (buf.numFrames() != dataset.dims())
      return Error(WrongPointSize);
    RealVector point(dataset.dims());
    point = buf.samps(0, dataset.dims(), 0);
    return dataset.add(id, point) ? OK() : Error(DuplicateLabel);
  }

  MessageResult<void> getPoint(string id, BufferPtr data) const {
    if (!data)
      return Error(NoBuffer);
    BufferAdaptor::Access buf(data.get());
    if (!buf.exists())
      return Error(InvalidBuffer);
    Result resizeResult = buf.resize(mAlgorithm.dims(), 1, buf.sampleRate());
    if (!resizeResult.ok())
      return {resizeResult.status(), resizeResult.message()};
    RealVector point(mAlgorithm.dims());
    point = buf.samps(0, mAlgorithm.dims(), 0);
    bool result = mAlgorithm.get(id, point);
    if (result) {
      buf.samps(0, mAlgorithm.dims(), 0) = point;
      return OK();
    } else {
      return Error(PointNotFound);
    }
  }

  MessageResult<void> updatePoint(string id, BufferPtr data) {
    if (!data)
      return Error(NoBuffer);
    BufferAdaptor::Access buf(data.get());
    if (!buf.exists())
      return Error(InvalidBuffer);
    if (buf.numFrames() < mAlgorithm.dims())
      return Error(WrongPointSize);
    RealVector point(mAlgorithm.dims());
    point = buf.samps(0, mAlgorithm.dims(), 0);
    return mAlgorithm.update(id, point) ? OK() : Error(PointNotFound);
  }

  MessageResult<void> deletePoint(string id) {
    return mAlgorithm.remove(id) ? OK() : Error(PointNotFound);
  }

  MessageResult<void> clear() {mAlgorithm = DataSet(0); return OK();}
  MessageResult<string> print() {return mAlgorithm.print();}
  const DataSet getDataSet() const { return mAlgorithm; }
  void setDataSet(DataSet ds) {mAlgorithm = ds;}

  FLUID_DECLARE_MESSAGES(makeMessage("addPoint", &DataSetClient::addPoint),
                         makeMessage("getPoint", &DataSetClient::getPoint),
                         makeMessage("updatePoint",
                                     &DataSetClient::updatePoint),
                         makeMessage("deletePoint",
                                     &DataSetClient::deletePoint),
                         makeMessage("dump", &DataSetClient::dump),
                         makeMessage("load", &DataSetClient::load),
                         makeMessage("print", &DataSetClient::print),
                         makeMessage("size", &DataSetClient::size),
                         makeMessage("cols", &DataSetClient::dims),
                         makeMessage("clear", &DataSetClient::clear),
                         makeMessage("write", &DataSetClient::write),
                         makeMessage("read", &DataSetClient::read));
};
using DataSetClientRef = SharedClientRef<DataSetClient>;
using NRTThreadedDataSetClient =
    NRTThreadingAdaptor<typename DataSetClientRef::SharedType>;
} // namespace client
} // namespace fluid
