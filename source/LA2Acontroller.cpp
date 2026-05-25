//------------------------------------------------------------------------
// Copyright(c) 2026 My Plug-in Company.
//------------------------------------------------------------------------

#include "LA2Acontroller.h"
#include "LA2Acids.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "pluginterfaces/base/ustring.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/futils.h"
#include <cstdio>
#include <cmath>

using namespace Steinberg;

namespace MyCompanyName {

	//------------------------------------------------------------------------
	// LA2A_CompressorController Implementation
	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::initialize(FUnknown* context)
	{
		tresult result = EditControllerEx1::initialize(context);
		if (result != kResultOk)
			return result;

		// --- Register parameters ---

		// Bypass: discrete 0/1
		parameters.addParameter(
			STR16("Bypass"),
			nullptr,
			2,          // two steps: off/on
			0.0,        // default = No Bypass (Effect On)
			Steinberg::Vst::ParameterInfo::kCanAutomate |
			Steinberg::Vst::ParameterInfo::kIsBypass,
			kBypassId
		);

		parameters.addParameter(
			STR16("Input Gain"),
			STR16("dB"),
			0,
			0.25,
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kInputGainId
		);

		// Reduction: three-position (0 = 0dB, ~0.5 = +4dB, 1.0 = +10dB)
		parameters.addParameter(
			STR16("Reduction"),
			STR16("dB"),
			0,
			0.0,
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kReductionId
		);

		parameters.addParameter(
			STR16("Output Gain"),
			STR16("dB"),
			0,
			0.25,
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kOutputGainId
		);

		// Tone: 0 = dark (600 Hz boost), 0.5 = flat, 1 = bright (2200 Hz boost)
		parameters.addParameter(
			STR16("Tone"),
			nullptr,
			0,
			0.5,
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kToneId
		);

		// Mix: 0 = 100% dry, 1 = 100% wet (full compression)
		parameters.addParameter(
			STR16("Mix"),
			nullptr,
			0,
			1.0,
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kMixId
		);

		// Compression Type: 0 = Limit, 1 = Compress
		parameters.addParameter(
			STR16("Compression Type"),
			nullptr,
			0,
			1.0,
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kCompressionTypeId
		);

		// VU Meter: read-only, driven by processor via outputParameterChanges
		parameters.addParameter(
			STR16("VU Meter"),
			STR16("dB"),
			0,
			0.0,
			Steinberg::Vst::ParameterInfo::kIsReadOnly,  // user cannot change this
			kVUId
		);

		// High Pass Filter: 0–1 maps to 20–200 Hz on the sidechain detector
		parameters.addParameter(
			STR16("High Pass Filter"),
			STR16("Hz"),
			0,
			0.0,
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kHighPassId
		);

		// Apply normalized defaults
		setParamNormalized(kBypassId, 0.0);
		setParamNormalized(kInputGainId, 0.25);
		setParamNormalized(kReductionId, 0.0);
		setParamNormalized(kMixId, 1.0);
		setParamNormalized(kOutputGainId, 0.25);
		setParamNormalized(kToneId, 0.5);
		setParamNormalized(kCompressionTypeId, 1.0);
		setParamNormalized(kVUId, 0.0);
		setParamNormalized(kHighPassId, 0.0);

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::terminate()
	{
		return EditControllerEx1::terminate();
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::setState(IBStream* state)
	{
		return kResultTrue;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::getState(IBStream* state)
	{
		if (!state)
			return kResultFalse;

		IBStreamer streamer(state, kLittleEndian);

		const int32 numParams = kParamCount;
		if (!streamer.writeInt32(numParams))
			return kResultFalse;

		auto writeParam = [&](Vst::ParamID id, float fallback) -> bool {
			if (!streamer.writeInt32(static_cast<int32>(id)))
				return false;
			float val = fallback;
			if (auto* p = parameters.getParameter(id))
				val = static_cast<float>(p->getNormalized());
			return streamer.writeFloat(val);
			};

		writeParam(kBypassId, bypass ? 1.0f : 0.0f);
		writeParam(kInputGainId, 0.25f);
		writeParam(kMixId, 1.0f);
		writeParam(kReductionId, 0.0f);
		writeParam(kOutputGainId, 0.25f);
		writeParam(kToneId, 0.5f);
		writeParam(kCompressionTypeId, 1.0f);
		writeParam(kVUId, 0.0f);
		writeParam(kHighPassId, 0.0f);

		return kResultOk;
	}

	//------------------------------------------------------------------------
	IPlugView* PLUGIN_API LA2A_CompressorController::createView(FIDString name)
	{
		if (FIDStringsEqual(name, Vst::ViewType::kEditor))
		{
			auto* view = new VSTGUI::VST3Editor(this, "view", "LA2Aeditor.uidesc");
			return view;
		}
		return nullptr;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::setComponentState(IBStream* state)
	{
		if (!state)
			return kResultFalse;

		IBStreamer streamer(state, kLittleEndian);

		int32 firstInt = 0;
		if (!streamer.readInt32(firstInt))
			return kResultFalse;

		if (firstInt == kParamCount)
		{
			// New format: (id, float) pairs
			for (int32 i = 0; i < firstInt; ++i)
			{
				int32 id = 0;
				float val = 0.f;
				if (!streamer.readInt32(id))   continue;
				if (!streamer.readFloat(val))  continue;
				setParamNormalized(static_cast<Vst::ParamID>(id),
					static_cast<Vst::ParamValue>(val));
			}
		}
		else
		{
			// Legacy format: firstInt is bypass value
			bypass = (firstInt != 0);
			setParamNormalized(kBypassId, bypass ? 1.0 : 0.0);
		}

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::setParamNormalized(
		Steinberg::Vst::ParamID tag,
		Steinberg::Vst::ParamValue value)
	{
		if (tag == kBypassId)
			bypass = (value >= 0.5);

		return EditControllerEx1::setParamNormalized(tag, value);
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::getParamStringByValue(
		Vst::ParamID tag,
		Vst::ParamValue valueNormalized,
		Vst::String128 string)
	{
		if (!string)
			return kResultFalse;

		char buf[128] = { 0 };

		switch (tag)
		{
		case kBypassId:
			snprintf(buf, sizeof(buf), "%s", (valueNormalized >= 0.5) ? "On" : "Off");
			break;

		case kInputGainId:
		{
			// Map normalized 0–1 to dB: 0.25 = 0 dB, range ±18 dB
			float dB = (static_cast<float>(valueNormalized) - 0.25f) * 72.0f;
			snprintf(buf, sizeof(buf), "%.1f dB", dB);
			break;
		}

		case kReductionId:
		{
			// Three-position: 0–0.25 = 0dB, 0.25–0.75 = +4dB, 0.75–1.0 = +10dB
			float v = static_cast<float>(valueNormalized);
			if (v < 0.25f)
				snprintf(buf, sizeof(buf), "0 dB");
			else if (v < 0.75f)
				snprintf(buf, sizeof(buf), "+4 dB");
			else
				snprintf(buf, sizeof(buf), "+10 dB");
			break;
		}

		case kOutputGainId:
		{
			float dB = (static_cast<float>(valueNormalized) - 0.25f) * 72.0f;
			snprintf(buf, sizeof(buf), "%.1f dB", dB);
			break;
		}

		case kMixId:
		{
			int pct = static_cast<int>(std::round(valueNormalized * 100.0));
			snprintf(buf, sizeof(buf), "%d%%", pct);
			break;
		}

		case kToneId:
		{
			// Match the three zones used by the biquad EQ
			float v = static_cast<float>(valueNormalized);
			if (v < 0.49f)
				snprintf(buf, sizeof(buf), "Dark");
			else if (v <= 0.51f)
				snprintf(buf, sizeof(buf), "Neutral");
			else
				snprintf(buf, sizeof(buf), "Bright");
			break;
		}

		case kHighPassId:
		{
			// Map 0–1 → 20–200 Hz (matches processor)
			float hz = 20.0f + static_cast<float>(valueNormalized) * (200.0f - 20.0f);
			snprintf(buf, sizeof(buf), "%.0f Hz", hz);
			break;
		}

		case kVUId:
		{
			// Display gain reduction level in dB (0–1 normalized = 0–20 dB GR)
			float grDB = static_cast<float>(valueNormalized) * 20.0f;
			snprintf(buf, sizeof(buf), "%.1f dB", grDB);
			break;
		}

		case kCompressionTypeId:
			snprintf(buf, sizeof(buf), "%s", (valueNormalized >= 0.5) ? "Compress" : "Limit");
			break;

		default:
			return EditControllerEx1::getParamStringByValue(tag, valueNormalized, string);
		}

		// Convert ASCII into the SDK's TChar (UTF-16) output buffer
		Steinberg::UString128 ustr;
		ustr.fromAscii(buf);
		ustr.copyTo(string, 128);

		return kResultTrue;
	}

} // namespace MyCompanyName