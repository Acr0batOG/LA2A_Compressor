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
	float compressorEq = .5f;      // 0 = darkest, 0.5 = neutral, 1 = brightest Which I would like to be user editable through a UI and   

	float attackTime = 10.0f;        // ms — how fast the compressor reacts to increases in level

	float initialReleaseTime = 60.0f; // ms — how fast the compressor reacts to decreases in level (fast path)

	float slowReleaseTime = 2500.0f;   // ms — how fast the compressor reacts to decreases in level (slow path)

	float opticalEnvelope = 0.0f;      // Running level of the optical element (0–1)
	float opticalAttack = 10.0f;       // ms — how fast the cell brightens
	float opticalDecay = 60.0f;        // ms — how fast the cell dims (separate from audio release)
	float opticalCurveExp = 2.2f;      // Exponent shaping the non-linear gain law
	float currentGainReduction = 0.0f; // dB of reduction being applied right now
	float gainReductionSmooth = 0.0f;  // Smoothed version for the GR meter

	float releaseBlendRatio = 0.0f;    // 0 = fast only, 1 = slow only (set by signal history)
	float releaseEnvFast = 0.0f;       // Fast-path envelope follower state
	float releaseEnvSlow = 0.0f;       // Slow-path envelope follower state

	float sidechainHPF_Hz = 90.0f;     // High-pass cutoff on the detection path (~80–120 Hz)
	float sidechainHPF_state = 0.0f;   // Filter state variable (one per channel)

	float saturationAmount = 0.15f;    // 0 = none, 1 = heavy soft-clip
	float evenHarmonicBias = 0.3f;     // Ratio of 2nd harmonic vs 3rd (LA-2A is warm/even)
	float transformerDCOffset = 0.0002f; // Tiny asymmetry to generate even harmonics

	double sampleRate = 44100.0;       // Set from prepareToPlay()
	int oversampleFactor = 2;          // 2x or 4x to reduce aliasing from saturation

	float noiseFloorLevel = 0.00003f;  // ~−90 dBFS analog hiss

	float grMeterLevel = 0.0f;         // Gain reduction in dB for display
	float inputMeterLevel = 0.0f;
	float outputMeterLevel = 0.0f;

	// Filter / envelope states (per channel)
	float opticalEnvelopeL = 0.f, opticalEnvelopeR = 0.f;
	float sidechainHPF_stateL = 0.f, sidechainHPF_stateR = 0.f;
	float sidechainHPF_coeff = 0.f;
	float toneStateL = 0.f, toneStateR = 0.f;

};

//------------------------------------------------------------------------
} // namespace MyCompanyName
