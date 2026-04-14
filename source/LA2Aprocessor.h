//------------------------------------------------------------------------
// Copyright(c) 2026 My Plug-in Company.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

namespace MyCompanyName {

//------------------------------------------------------------------------
//  LA2A_CompressorProcessor
//------------------------------------------------------------------------
class LA2A_CompressorProcessor : public Steinberg::Vst::AudioEffect
{
public:
	LA2A_CompressorProcessor ();
	~LA2A_CompressorProcessor () SMTG_OVERRIDE;

    // Create function
	static Steinberg::FUnknown* createInstance (void* /*context*/) 
	{ 
		return (Steinberg::Vst::IAudioProcessor*)new LA2A_CompressorProcessor; 
	}

	//--- ---------------------------------------------------------------------
	// AudioEffect overrides:
	//--- ---------------------------------------------------------------------
	/** Called at first after constructor */
	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	
	/** Called at the end before destructor */
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	
	/** Switch the Plug-in on/off */
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;

	/** Will be called before any process call */
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
	
	/** Asks if a given sample size is supported see SymbolicSampleSizes. */
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

	/** Here we go...the process call */
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
		
	/** For persistence */
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

//------------------------------------------------------------------------
protected:
	bool bypass = false;
	float inputGain = 0.25f; // 0 - 1f, 0–100% input gain, first knob being used
	float outputGain = 0.25f; // 0 - 1f, 0–100% output gain, second knob being used
	//Threshold is the level above which the compressor starts to work Valued 0 - 1, set as default
	float threshold = 0.5f; 
	// This will be a knob called "Mix" Valued 0 - 100%
	float mix = 0.50f;            // dry/wet
	// This will be a knob called "Tone" Valued 0 - 1
	float compressorEq = .5f;      // 0 = darkest, 0.5 = neutral, 1 = brightest
};

//------------------------------------------------------------------------
} // namespace MyCompanyName
