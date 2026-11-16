#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

namespace LogoMidi {
static const Steinberg::FUID kProcessorUid(
    0x73D654E1, 0xB1794A21, 0xA0C3B47E, 0x618D952F);
static const Steinberg::FUID kControllerUid(
    0xA44FC087, 0x5D264A58, 0x9B821F0E, 0xC7356D91);

enum ParameterId : Steinberg::Vst::ParamID {
  kPatternId = 100,
  kDawTempoSyncId = 101,
  kInternalTempoId = 102
};
constexpr int kMidiChannelCount = 16;
}  // namespace LogoMidi
