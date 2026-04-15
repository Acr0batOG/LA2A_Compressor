//------------------------------------------------------------------------
// Copyright(c) 2026 My Plug-in Company.
//------------------------------------------------------------------------

#include "LA2Acontroller.h"
#include "LA2Acids.h"
#include "vstgui/plugin-bindings/vst3editor.h"
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
		// Here the Plug-in will be instantiated

		//---do not forget to call parent ------
		tresult result = EditControllerEx1::initialize(context);
		if (result != kResultOk)
		{
			return result;
		}

		// Here you could register some parameters

		return result;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::terminate()
	{
		// Here the Plug-in will be de-instantiated, last possibility to remove some memory!

		//---do not forget to call parent ------
		return EditControllerEx1::terminate();
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::setComponentState(IBStream* state)
	{
		// Here you get the state of the component (Processor part)
		if (!state)
			return kResultFalse;

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::setState(IBStream* state)
	{
		// Here you get the state of the controller

		return kResultTrue;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API LA2A_CompressorController::getState(IBStream* state)
	{
		// Here you are asked to deliver the state of the controller (if needed)
		// Note: the real state of your plug-in is saved in the processor

		return kResultTrue;
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

	tresult PLUGIN_API LA2A_CompressorController::setParamNormalized(
		Steinberg::Vst::ParamID tag,
		Steinberg::Vst::ParamValue value)
	{
		if (tag == kBypassId)
		{
			// maintain local copy for quick access
			//bypass = (value >= 0.5);

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

		//switch (tag)
		{
			//case kBypassId:
			//	snprintf(buf, sizeof(buf), "%s", (valueNormalized >= 0.5) ? "On" : "Off");
			//	break;

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
