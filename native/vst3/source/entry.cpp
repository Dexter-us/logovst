#include "controller.hpp"
#include "ids.hpp"
#include "processor.hpp"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF(stringCompanyName, stringCompanyWeb, stringCompanyEmail)

DEF_CLASS2(INLINE_UID_FROM_FUID(LogoMidi::kProcessorUid),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           stringPluginName,
           kDistributable,
           "Fx",
           FULL_VERSION_STR,
           kVstVersionString,
           LogoMidi::Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(LogoMidi::kControllerUid),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           stringPluginName " Controller",
           0,
           "",
           FULL_VERSION_STR,
           kVstVersionString,
           LogoMidi::Controller::createInstance)

END_FACTORY
