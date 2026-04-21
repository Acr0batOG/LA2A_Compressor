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
		{
			return result;
		}

		// --- Register parameters here ---

		// Bypass: discrete 0/1
		parameters.addParameter(
			STR16("Bypass"),
			nullptr,
			2,          // two steps: off/on
			0.0,        // default = OFF
			Steinberg::Vst::ParameterInfo::kCanAutomate |
			Steinberg::Vst::ParameterInfo::kIsBypass,
			kBypassId
		);

		
		parameters.addParameter(
			STR16("Input Gain"),
			STR16("dB"),
			0, // continuous
			(400.0f - 50.0f) / (1000.0f - 50.0f),
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kInputGainId
		);

		parameters.addParameter(
			STR16("Reduction"),
			STR16("dB"),
			0, // continuous
			(400.0f - 50.0f) / (1000.0f - 50.0f),
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kReductionId
		);

		

		// Feedback: stored normalized (0..1) maps to actual feedback = normalized * 0.95
		// default actual ~24% -> store normalized = 0.24 / 0.95
		parameters.addParameter(
			STR16("Output Gain"),
			STR16("dB"),
			0,
			0.24f / 0.95f,
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kOutputGainId
		);

		// Tone: normalized [0..1], controller default 0.5 (center)
		parameters.addParameter(
			STR16("Tone"),
			nullptr,
			0,
			0.5,
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kToneId
		);

		// Mix: 0..1
		parameters.addParameter(
			STR16("Mix"),
			nullptr,
			0,
			0.50, // default (50%)
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kMixId
		);
		
		parameters.addParameter(
			STR16("Compression Type"),
			nullptr,
			0,
			1.0, // Compress
			Steinberg::Vst::ParameterInfo::kCanAutomate,
			kCompressionTypeId
		);

		// apply normalized defaults into parameter container
		setParamNormalized(kBypassId, 0.0);
		setParamNormalized(kInputGainId, (400.f - 50.f) / (1000.f - 50.f));
		setParamNormalized(kReductionId, (400.f - 50.f) / (1000.f - 50.f));
		setParamNormalized(kMixId, 0.50);
		setParamNormalized(kOutputGainId, 0.24f / 0.95f);
		setParamNormalized(kToneId, 0.5);
		setParamNormalized(kCompressionTypeId, 1.0);

		parameters.getParameter(kBypassId)->setNormalized(0.0);
		parameters.getParameter(kInputGainId)->setNormalized(
			(400.f - 50.f) / (1000.f - 50.f)
		);
		parameters.getParameter(kReductionId)->setNormalized(
			(400.f - 50.f) / (1000.f - 50.f)
		);
		parameters.getParameter(kMixId)->setNormalized(0.50);
		parameters.getParameter(kOutputGainId)->setNormalized(0.24f / 0.95f);
		parameters.getParameter(kToneId)->setNormalized(0.5);
		parameters.getParameter(kCompressionTypeId)->setNormalized(1.0);
		

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::terminate()
	{
		// Here the Plug-in will be de-instantiated, last possibility to remove some memory!

		//---do not forget to call parent ------
		return EditControllerEx1::terminate();
	}

	//------------------------------------------------------------------------
	

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::setState(IBStream* state)
	{
		// Here you get the state of the controller

		return kResultTrue;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::getState(IBStream* state)
	{
		
			// Called by the host to store the controller state (we write all parameter normalized values)
			if (!state)
				return kResultFalse;

			IBStreamer streamer(state, kLittleEndian);

			// write number of params, then pairs (id, float normalized)
			const int32 numParams = kParamCount;
			if (!streamer.writeInt32(numParams))
				return kResultFalse;

			auto writeParam = [&](Vst::ParamID id, float normalized) -> bool {
				if (!streamer.writeInt32(static_cast<int32>(id)))
					return false;
				if (!streamer.writeFloat(normalized))
					return false;
				return true;
				};

			// fetch normalized values from parameter objects so we persist exactly what's in controller
			if (auto* p = parameters.getParameter(kBypassId))
			{
				writeParam(kBypassId, static_cast<float>(p->getNormalized()));
			}
			else writeParam(kBypassId, bypass ? 1.0f : 0.0f);

			//if (auto* p = parameters.getParameter(kDelayTimeId))
			//{
			//	writeParam(kDelayTimeId, static_cast<float>(p->getNormalized()));
			//}
			//if (auto* p = parameters.getParameter(kMixId))
			//{
			//	writeParam(kMixId, static_cast<float>(p->getNormalized()));
			//}
			//if (auto* p = parameters.getParameter(kFeedbackId))
			//{
			//	writeParam(kFeedbackId, static_cast<float>(p->getNormalized()));
			//}
			//if (auto* p = parameters.getParameter(kToneId))
			//{
			//	writeParam(kToneId, static_cast<float>(p->getNormalized()));
			//}
			//if (auto* p = parameters.getParameter(kStereoWidthId))
			//{
			//	writeParam(kStereoWidthId, static_cast<float>(p->getNormalized()));
			//}

			return kResultOk;
		
	}

	//------------------------------------------------------------------------
	IPlugView* PLUGIN_API LA2A_CompressorController::createView(FIDString name)
	{
		// Here the Host wants to open your editor (if you have one)
		if (FIDStringsEqual(name, Vst::ViewType::kEditor))
		{
			// create your editor here and return a IPlugView ptr of it
			auto* view = new VSTGUI::VST3Editor(this, "view", "LA2Aeditor.uidesc");
			return view;
		}
		return nullptr;
	}
	tresult PLUGIN_API LA2A_CompressorController::setComponentState(IBStream* state)
	{
		// Host may call this to restore component/processor state into the controller (UI)
		if (!state)
			return kResultFalse;

		IBStreamer streamer(state, kLittleEndian);

		int32 firstInt = 0;
		if (!streamer.readInt32(firstInt))
			return kResultFalse;

		// If old format (single savedBypass written by older code), firstInt will be 0/1.
		// If new format, firstInt == number of parameter entries.
		if (firstInt == kParamCount)
		{
			const int32 numParams = firstInt;
			for (int32 i = 0; i < numParams; ++i)
			{
				int32 id = 0;
				float val = 0.f;
				if (!streamer.readInt32(id))
					continue;
				if (!streamer.readFloat(val))
					continue;

				// Update controller parameter (this will notify UI)
				setParamNormalized(static_cast<Vst::ParamID>(id), static_cast<Vst::ParamValue>(val));
			}
			return kResultOk;
		}
		else
		{
			// Legacy: treat firstInt as savedBypass
			int32 savedBypass = firstInt;
			bypass = (savedBypass != 0);
			setParamNormalized(kBypassId, bypass ? 1.0 : 0.0);
			return kResultOk;
		}
	}

	tresult PLUGIN_API LA2A_CompressorController::setParamNormalized(
		Steinberg::Vst::ParamID tag,
		Steinberg::Vst::ParamValue value)
	{
		if (tag == kBypassId)
		{
			// maintain local copy for quick access
			bypass = (value >= 0.5);

			// Let the base class actually set the parameter value/store it and notify listeners.
			return EditControllerEx1::setParamNormalized(tag, value);
		}

		return EditControllerEx1::setParamNormalized(tag, value);
	}

	tresult PLUGIN_API LA2A_CompressorController::getParamStringByValue(Vst::ParamID tag,
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

			//case kDelayTimeId:
			//{
			//	// Map normalized -> ms using same mapping as processor: 50..1000 ms
			//	const float ms = 50.0f + static_cast<float>(valueNormalized) * (1000.0f - 50.0f);
			//	// Show integer ms
			//	snprintf(buf, sizeof(buf), "%.0f ms", std::round(ms));
			//	break;
			//}

			//case kMixId:
			//{
			//	// Mix is 0..1 wet gain relative to dry. Display percent 0..100%
			//	int pct = static_cast<int>(std::round(valueNormalized * 100.0));
			//	snprintf(buf, sizeof(buf), "%d%%", pct);
			//	break;
			//}

			//case kFeedbackId:
			//{
			//	// stored normalized maps to actual feedback = value * 0.9
			//	float fb = static_cast<float>(valueNormalized) * 1.0f;
			//	int pct = static_cast<int>(std::round(fb * 100.0f));
			//	snprintf(buf, sizeof(buf), "%d%%", pct);
			//	break;
			//}

			//case kToneId:
			//{
			//	// Tone 0-20% Darker, 21-40% Dark, 41-60% Neutral, 61-80% Bright, 81-100% Brighter
			//	int pct = static_cast<int>(std::round(valueNormalized * 100.0));
			//	int idx = 0;
			//	const char* labels[5] = { "Darker", "Dark", "Neutral", "Bright", "Brighter" };
			//	if (pct <= 20)
			//		idx = 0;
			//	else if (pct <= 40)
			//		idx = 1;
			//	else if (pct <= 60)
			//		idx = 2;
			//	else if (pct <= 80)
			//		idx = 3;
			//	else
			//		idx = 4;
			//	snprintf(buf, sizeof(buf), "%s", labels[idx]);
			//	break;
			//}

			//case kStereoWidthId:
			//{
			//	// Tone 0-20% Darker, 21-40% Dark, 41-60% Neutral, 61-80% Bright, 81-100% Brighter
			//	int pct = static_cast<int>(std::round(valueNormalized * 100.0));
			//	int idx = 0;
			//	const char* labels[4] = { "Mono", "Narrow", "Stereo", "Wide" };
			//	if (pct <= 10.0f)
			//		idx = 0;
			//	else if (pct <= 36.0f)
			//		idx = 1;
			//	else if (pct <= 80.0f)
			//		idx = 2;
			//	else
			//		idx = 3;
			//	snprintf(buf, sizeof(buf), "%s", labels[idx]);
			//	break;
			//}

			//default:
			//	// fall back to base class formatting (numbers, etc.)
			//	return EditControllerEx1::getParamStringByValue(tag, valueNormalized, string);
			//}


			//// Use UString128 to convert ASCII/UTF-8 into SDK TChar buffer safely
			//Steinberg::UString128 ustr;
			//ustr.fromAscii(buf);
			//ustr.copyTo(string, 128);

			return kResultTrue;
		}

		//------------------------------------------------------------------------
	} // namespace MyCompanyName
}
