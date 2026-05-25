//------------------------------------------------------------------------
// Copyright(c) 2026 My Plug-in Company.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

namespace MyCompanyName {

	//------------------------------------------------------------------------
	// Biquad filter state + coefficients (Direct Form I)
	// Used for the Tone peaking EQ.  Reset coefficients to identity (b0=1,
	// all others 0) for passthrough; state variables x1/x2/y1/y2 carry the
	// filter memory between blocks.
	//------------------------------------------------------------------------
	struct BiquadFilter {
		float b0 = 1.f, b1 = 0.f, b2 = 0.f;   // numerator coefficients
		float a1 = 0.f, a2 = 0.f;               // denominator coefficients (a0 normalised out)
		float x1 = 0.f, x2 = 0.f;               // input  delay line
		float y1 = 0.f, y2 = 0.f;               // output delay line
	};

	//------------------------------------------------------------------------
	//  LA2A_CompressorProcessor
	//------------------------------------------------------------------------
	class LA2A_CompressorProcessor : public Steinberg::Vst::AudioEffect
	{
	public:
		LA2A_CompressorProcessor();
		~LA2A_CompressorProcessor() SMTG_OVERRIDE;

		static Steinberg::FUnknown* createInstance(void* /*context*/)
		{
			return (Steinberg::Vst::IAudioProcessor*)new LA2A_CompressorProcessor;
		}

		//--- AudioEffect overrides -------------------------------------------
		Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context)                     SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API terminate()                                                   SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state)                            SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& newSetup)      SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize)    SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data)                   SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state)                         SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state)                         SMTG_OVERRIDE;

	protected:

		//--------------------------------------------------------------------
		// Parameters (mirror of kXxxId values; updated from process())
		//--------------------------------------------------------------------
		bool  bypass = false;
		float inputGain = 0.25f;  // normalized 0–1; 0.25 = 0 dB unity
		float outputGain = 0.25f;  // normalized 0–1; 0.25 = 0 dB unity
		float threshold = 0.5f;   // normalized 0–1 → −60–0 dB
		float mix = 1.0f;   // 0 = 100% dry, 1 = 100% wet
		float tone = 0.5f;   // 0 = dark (600 Hz), 0.5 = flat, 1 = bright (2200 Hz)
		float compressionType = 1.0f;   // 0 = limit (hard knee), 1 = compress (soft knee)

		// kReductionId: three-position level offset applied at the input stage
		// 0 dB / +4 dB / +10 dB — decoded from normalized value in process()
		float reductionOffsetdB = 0.0f;

		//--------------------------------------------------------------------
		// Optical cell / timing constants
		//--------------------------------------------------------------------
		float attackTime = 10.0f;    // ms — cell brightening speed
		float initialReleaseTime = 60.0f;    // ms — fast release path
		float slowReleaseTime = 2500.0f;  // ms — slow release tail

		float opticalCurveExp = 2.2f;     // non-linear gain law exponent
		float releaseBlendRatio = 0.0f;     // 0 = fast only, 1 = slow only

		//--------------------------------------------------------------------
		// Running state — optical envelope follower (per channel, stereo-linked)
		//--------------------------------------------------------------------
		float opticalEnvelopeL = 0.f;
		float opticalEnvelopeR = 0.f;
		float releaseEnvFast = 0.f;
		float releaseEnvSlow = 0.f;

		//--------------------------------------------------------------------
		// Gain reduction state
		//--------------------------------------------------------------------
		float currentGainReduction = 0.f;   // dB applied this sample
		float gainReductionSmooth = 0.f;   // slow-smoothed version for the GR meter
		float grMeterLevel = 0.f;   // normalised 0–1 pushed to kVUId each block

		//--------------------------------------------------------------------
		// Sidechain HPF — removes deep bass from the detector path
		// Range: 45–350 Hz (kHighPassId 0–1)
		//--------------------------------------------------------------------
		float sidechainHPF_Hz = 45.0f;  // default = effectively off
		float sidechainHPF_coeff = 0.f;    // precomputed 1-pole coefficient
		float sidechainHPF_stateL = 0.f;
		float sidechainHPF_stateR = 0.f;

		//--------------------------------------------------------------------
		// Tone biquad EQ (per channel, rebuilt when kToneId changes)
		//--------------------------------------------------------------------
		BiquadFilter delayEqFilterL;
		BiquadFilter delayEqFilterR;

		//--------------------------------------------------------------------
		// Harmonic saturation / transformer colouring
		//--------------------------------------------------------------------
		float saturationAmount = 0.15f;    // 0 = none, 1 = heavy soft-clip
		float transformerDCOffset = 0.0002f;  // asymmetry → even harmonics

		//--------------------------------------------------------------------
		// Analog noise floor
		//--------------------------------------------------------------------
		float noiseFloorLevel = 0.00003f;   // ≈ −90 dBFS

		//--------------------------------------------------------------------
		// System
		//--------------------------------------------------------------------
		double sampleRate = 44100.0;

		//--------------------------------------------------------------------
		// Meter levels (optionally exposed to UI)
		//--------------------------------------------------------------------
		float inputMeterLevel = 0.f;
		float outputMeterLevel = 0.f;

		//--------------------------------------------------------------------
		// Internal helpers
		//--------------------------------------------------------------------

		// (Re)design the peaking biquad from the current tone value.
		// Must be called after tone changes and after setupProcessing().
		void updateToneFilter();
	};

} // namespace MyCompanyName