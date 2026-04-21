//------------------------------------------------------------------------
// Copyright(c) 2026 My Plug-in Company.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace MyCompanyName {
//------------------------------------------------------------------------
static const Steinberg::FUID kLA2A_CompressorProcessorUID (0x3E2117D2, 0x8482586B, 0x8A7C0F0A, 0xD3C3EDC3);
static const Steinberg::FUID kLA2A_CompressorControllerUID (0x776B35EC, 0x74FF5454, 0x9EC633A1, 0x62FF29BD);

#define LA2A_CompressorVST3Category "Fx"

enum ParamIDs : Steinberg::Vst::ParamID
{

	kBypassId,

	kInputGainId,   //  0–100% input gain, first knob being used
	kReductionId,    // 0–100% aka peak reduction
	kOutputGainId,         // 0–100%
	kToneId,     // Tone (0–1, tilt EQ)
	kMixId,	  // 0–100% wet/dry mix

	kCompressionTypeId, // Limit, or compress (0 or 1)

	kParamCount
};

//------------------------------------------------------------------------
} // namespace MyCompanyName
