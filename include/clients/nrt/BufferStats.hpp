#pragma once

#include <algorithms/public/Stats.hpp>
#include <clients/common/FluidBaseClient.hpp>
#include <clients/common/ParameterConstraints.hpp>
#include <clients/common/ParameterTypes.hpp>

namespace fluid {
namespace client {

enum BufferStatsParamIndex {
  kSource,
  kStats,
  kNumDerivatives,
  kLow,
  kMiddle,
  kHigh
};

auto constexpr BufferStatsParams = defineParameters(
    BufferParam("source", "Source Buffer"),
    BufferParam("stats", "Stats Buffer"),
    LongParam("numDerivatives", "Number of derivatives", 0, Min(0), Max(2)),
    FloatParam("low", "Low percentile", 0, Min(0), Max(100),
              UpperLimit<kMiddle>()),
    FloatParam("middle", "Middle percentile", 50, Min(0), Max(100),
              LowerLimit<kLow>(), UpperLimit<kHigh>()),
    FloatParam("high", "High percentile", 100, Min(0), Max(100),
              LowerLimit<kMiddle>()));

template <typename T>
class BufferStats
    : public FluidBaseClient<decltype(BufferStatsParams), BufferStatsParams>,
      public OfflineIn,
      public OfflineOut {

public:
  BufferStats(ParamSetViewType &p) : FluidBaseClient(p) {}

  Result process() {
    algorithm::Stats processor;


    if (!get<kSource>().get())
      return {Result::Status::kError, "No input buffer supplied"};

    if (!get<kStats>().get())
      return {Result::Status::kError, "No output buffer supplied"};

    BufferAdaptor::Access source(get<kSource>().get());
    BufferAdaptor::Access dest(get<kStats>().get());

    if (!source.exists())
      return {Result::Status::kError, "Input buffer not found"};

    if (!source.valid())
      return {Result::Status::kError, "Can't access input buffer"};

    if (!dest.exists())
      return {Result::Status::kError, "Output buffer not found"};

    /*if (!dest.valid())
      return {Result::Status::kError, "Can't access output buffer"};*/
    int numFrames = source.numFrames();
    int numChannels = source.numChans();
    int outputSize = processor.numStats() * (get<kNumDerivatives>() + 1);
    dest.resize(outputSize, numChannels, 1, source.sampleRate());

    processor.init(get<kNumDerivatives>(), get<kLow>(), get<kMiddle>(),
                   get<kHigh>());
    for (int i = 0; i < numChannels; i++) {
        auto sourceChannel = FluidTensor<double,1>(source.numFrames());
        auto destChannel = FluidTensor<double,1>(outputSize);
        for (int j = 0; j < numFrames; j++) sourceChannel(j) = source.samps(i)(j);
        processor.process(sourceChannel, destChannel);
        for (int j = 0; j < outputSize; j++)  dest.samps(i)(j) = destChannel(j);
    }

    return {Result::Status::kOk, ""};
  }
};
} // namespace client
} // namespace fluid