//------------------------------------------------------------------------
// Copyright(c) 2026 My Plug-in Company.
// LA-2A Optical Compressor Emulation - Processor Implementation
//------------------------------------------------------------------------

#include "LA2Aprocessor.h"
#include "LA2Acids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <vstgui/lib/cgraphicstransform.h>
using namespace MyCompanyName;

using namespace Steinberg;

namespace {

    //------------------------------------------------------------------------
    // Tube-style soft asymmetric saturation
    //------------------------------------------------------------------------
    inline float saturate(float x)
    {
        const float drive = 1.42f;
        float y = x * drive;
        return y / (1.0f + fabsf(y));
    }

    //------------------------------------------------------------------------
    // Convert milliseconds to a one-pole smoothing coefficient
    //------------------------------------------------------------------------
    inline float msToCoeff(float ms, double sampleRate)
    {
        if (ms <= 0.0f) return 0.0f;
        return 1.0f - expf(-1.0f / (static_cast<float>(sampleRate) * (ms / 1000.0f)));
    }

    //------------------------------------------------------------------------
    // Convert linear amplitude to dB
    //------------------------------------------------------------------------
    inline float linTodB(float x)
    {
        return 20.0f * log10f(std::max(x, 1e-9f));
    }

    //------------------------------------------------------------------------
    // Convert dB to linear amplitude
    //------------------------------------------------------------------------
    inline float dBToLin(float dB)
    {
        return powf(10.0f, dB / 20.0f);
    }

    //------------------------------------------------------------------------
    // LA-2A optical gain law
    //------------------------------------------------------------------------
    inline float opticalGainLaw(float overThresholddB, float opticalCurveExp)
    {
        if (overThresholddB <= 0.0f) return 0.0f;
        return powf(overThresholddB, opticalCurveExp) * 0.5f;
    }

    //------------------------------------------------------------------------
    // 1-pole high-pass filter (sidechain detection path)
    //------------------------------------------------------------------------
    inline float hpf1Pole(float x, float& z1, float coeff)
    {
        float y = x - z1;
        z1 = z1 + coeff * y;
        return y;
    }

    //------------------------------------------------------------------------
    // Apply a biquad filter (Direct Form I) to a single sample
    //------------------------------------------------------------------------
    inline float applyBiquad(float x, BiquadFilter& f)
    {
        float y = f.b0 * x + f.b1 * f.x1 + f.b2 * f.x2
            - f.a1 * f.y1 - f.a2 * f.y2;
        f.x2 = f.x1; f.x1 = x;
        f.y2 = f.y1; f.y1 = y;
        return y;
    }

} // anonymous namespace

namespace MyCompanyName {

    //------------------------------------------------------------------------
    // LA2A_CompressorProcessor
    //------------------------------------------------------------------------
    LA2A_CompressorProcessor::LA2A_CompressorProcessor()
    {
        setControllerClass(kLA2A_CompressorControllerUID);
    }

    //------------------------------------------------------------------------
    LA2A_CompressorProcessor::~LA2A_CompressorProcessor()
    {
    }

    //------------------------------------------------------------------------
    tresult PLUGIN_API LA2A_CompressorProcessor::initialize(FUnknown* context)
    {
        tresult result = AudioEffect::initialize(context);
        if (result != kResultOk)
            return result;

        addAudioInput(STR16("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);
        addEventInput(STR16("Event In"), 1);

        return kResultOk;
    }

    //------------------------------------------------------------------------
    tresult PLUGIN_API LA2A_CompressorProcessor::terminate()
    {
        return AudioEffect::terminate();
    }

    //------------------------------------------------------------------------
    tresult PLUGIN_API LA2A_CompressorProcessor::setActive(TBool state)
    {
        return AudioEffect::setActive(state);
    }

    //------------------------------------------------------------------------
    tresult PLUGIN_API LA2A_CompressorProcessor::setupProcessing(Vst::ProcessSetup& newSetup)
    {
        sampleRate = newSetup.sampleRate;

        // Precompute sidechain HPF coefficient (default 20 Hz = effectively off)
        sidechainHPF_coeff = expf(-2.0f * static_cast<float>(M_PI) * sidechainHPF_Hz
            / static_cast<float>(sampleRate));

        // Reset all state variables on setup
        opticalEnvelopeL = 0.0f;
        opticalEnvelopeR = 0.0f;
        currentGainReduction = 0.0f;
        releaseEnvFast = 0.0f;
        releaseEnvSlow = 0.0f;
        sidechainHPF_stateL = 0.0f;
        sidechainHPF_stateR = 0.0f;
        grMeterLevel = 0.0f;

        // Reset tone biquad state
        delayEqFilterL = {};
        delayEqFilterR = {};

        return AudioEffect::setupProcessing(newSetup);
    }

    //------------------------------------------------------------------------
    tresult PLUGIN_API LA2A_CompressorProcessor::canProcessSampleSize(int32 symbolicSampleSize)
    {
        if (symbolicSampleSize == Vst::kSample32)
            return kResultTrue;
        return kResultFalse;
    }

    //------------------------------------------------------------------------
    // Helper: (re)design the peaking EQ biquad from the current tone value.
    // Called whenever the tone parameter changes.
    // tone is normalized [0..1]; 0.5 = flat, <0.5 = low boost, >0.5 = high boost.
    //------------------------------------------------------------------------
    void LA2A_CompressorProcessor::updateToneFilter()
    {
        // Map tone 0..1 → eqAmount -1..+1
        const float eqAmount = (tone - 0.5f) * 2.0f;

        // Dead zone — flat EQ
        if (fabsf(eqAmount) < 0.01f)
        {
            delayEqFilterL = {};   // zeroed struct = identity (passthrough) if b0=0,
            delayEqFilterR = {};   // so set b0=1 explicitly
            delayEqFilterL.b0 = 1.0f;
            delayEqFilterR.b0 = 1.0f;
            return;
        }

        const float fs = static_cast<float>(sampleRate);
        const float lowBoost = 600.0f;
        const float highBoost = 2200.0f;

        // Positive eqAmount → high-frequency boost; negative → low-frequency boost
        float freq = (eqAmount > 0.0f) ? highBoost : lowBoost;
        float gainDb = fabsf(eqAmount) * 5.5f;   // max ±5.5 dB

        float A = powf(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * 3.14159265f * freq / fs;
        float beta = sinf(w0) / (2.0f * 0.8f);   // Q = 0.8
        float cosw = cosf(w0);

        // Peaking EQ coefficients (boost)
        float b0 = 1.0f + beta * A;
        float b1 = -2.0f * cosw;
        float b2 = 1.0f - beta * A;
        float a1 = -2.0f * cosw;
        float a2 = 1.0f - beta / A;

        // Normalize by a0 = 1 + beta/A
        float norm = 1.0f / (1.0f + beta / A);
        b0 *= norm;
        b1 *= norm;
        b2 *= norm;
        a1 *= norm;
        a2 *= norm;

        // Apply to both channel filters (preserve existing state x1/x2/y1/y2)
        delayEqFilterL.b0 = b0; delayEqFilterL.b1 = b1; delayEqFilterL.b2 = b2;
        delayEqFilterL.a1 = a1; delayEqFilterL.a2 = a2;

        delayEqFilterR.b0 = b0; delayEqFilterR.b1 = b1; delayEqFilterR.b2 = b2;
        delayEqFilterR.a1 = a1; delayEqFilterR.a2 = a2;
    }

    //------------------------------------------------------------------------
    tresult PLUGIN_API LA2A_CompressorProcessor::process(Vst::ProcessData& data)
    {
        //--------------------------------------------------------------------
        // 1. Read incoming parameter changes from the host / UI knobs
        //--------------------------------------------------------------------
        bool toneChanged = false;

        if (data.inputParameterChanges)
        {
            int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
            for (int32 index = 0; index < numParamsChanged; index++)
            {
                if (auto* paramQueue = data.inputParameterChanges->getParameterData(index))
                {
                    Vst::ParamValue value;
                    int32 sampleOffset;
                    int32 numPoints = paramQueue->getPointCount();
                    if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
                    {
                        switch (paramQueue->getParameterId())
                        {
                        case kInputGainId:
                            inputGain = static_cast<float>(value);   // 0–1
                            break;

                        case kReductionId:
                            // Three-position switch: 0 = 0 dB, 0.5 = +4 dB, 1.0 = +10 dB
                            // Map continuous 0–1 → nearest step
                        {
                            float v = static_cast<float>(value);
                            if (v < 0.25f)
                                reductionOffsetdB = 0.0f;
                            else if (v < 0.75f)
                                reductionOffsetdB = 4.0f;
                            else
                                reductionOffsetdB = 10.0f;
                        }
                        break;

                        case kOutputGainId:
                            outputGain = static_cast<float>(value);  // 0–1
                            break;

                        case kToneId:
                            tone = static_cast<float>(value);        // 0–1
                            toneChanged = true;
                            break;

                        case kMixId:
                            mix = static_cast<float>(value);         // 0–1: 1.0 = full wet, 0.0 = full dry
                            break;

                        case kCompressionTypeId:
                            compressionType = static_cast<float>(value); // 0 = limit, 1 = compress
                            break;

                        case kBypassId:
                            bypass = (value > 0.5);
                            break;

                        case kHighPassId:
                            // Map 0–1 → 20–200 Hz
                            sidechainHPF_Hz = 45.0f + static_cast<float>(value) * (350.0f - 45.0f);
                            sidechainHPF_coeff = expf(-2.0f * static_cast<float>(M_PI) * sidechainHPF_Hz
                                / static_cast<float>(sampleRate));
                            break;

                        case kVUId:
                            // VU meter is read-only from the user's perspective; ignore incoming writes.
                            break;

                        default:
                            break;
                        }
                    }
                }
            }
        }

        // Rebuild tone biquad if the tone knob moved
        if (toneChanged)
            updateToneFilter();

        //--------------------------------------------------------------------
        // 2. Guard: nothing to process
        //--------------------------------------------------------------------
        if (data.numSamples <= 0 || data.numInputs == 0 || data.numOutputs == 0)
            return kResultOk;

        //--------------------------------------------------------------------
        // 3. Bypass — hard copy input → output and exit
        //--------------------------------------------------------------------
        if (bypass)
        {
            int32 minBus = std::min(data.numInputs, data.numOutputs);
            for (int32 i = 0; i < minBus; i++)
            {
                int32 minChan = std::min(data.inputs[i].numChannels, data.outputs[i].numChannels);
                for (int32 c = 0; c < minChan; c++)
                {
                    if (data.outputs[i].channelBuffers32[c] != data.inputs[i].channelBuffers32[c])
                        memcpy(data.outputs[i].channelBuffers32[c],
                            data.inputs[i].channelBuffers32[c],
                            data.numSamples * sizeof(Vst::Sample32));
                }
            }
            return kResultOk;
        }

        //--------------------------------------------------------------------
        // 4. Grab channel pointers (stereo: L=ch0, R=ch1)
        //--------------------------------------------------------------------
        int32 numInChannels = data.inputs[0].numChannels;
        int32 numOutChannels = data.outputs[0].numChannels;

        Vst::Sample32* inL = data.inputs[0].channelBuffers32[0];
        Vst::Sample32* inR = (numInChannels > 1)
            ? data.inputs[0].channelBuffers32[1] : inL;

        Vst::Sample32* outL = data.outputs[0].channelBuffers32[0];
        Vst::Sample32* outR = (numOutChannels > 1)
            ? data.outputs[0].channelBuffers32[1] : outL;

        //--------------------------------------------------------------------
        // 5. Precompute per-block coefficients
        //--------------------------------------------------------------------

        // Input gain: 0–1 → dB range. 0.25 = 0 dB unity, 0.0 = –∞, 1.0 = +18 dB
        // reductionOffsetdB adds the selected 0 / +4 / +10 dB level offset
        const float inputGainLin = dBToLin((inputGain - 0.25f) * 72.0f + reductionOffsetdB);
        const float outputGainLin = dBToLin((outputGain - 0.25f) * 72.0f);

        // Threshold: 0–1 → –60 dB to 0 dB
        const float thresholddB = (threshold - 1.0f) * 60.0f;

        // Optical cell timing coefficients
        const float attackCoeff = msToCoeff(attackTime, sampleRate);
        const float fastRelCoeff = msToCoeff(initialReleaseTime, sampleRate);
        const float slowRelCoeff = msToCoeff(slowReleaseTime, sampleRate);

        // compressionType: 0 = limit (higher ratio / harder knee), 1 = compress (softer)
        // Modulate the optical curve exponent: limit → sharper (lower exp), compress → softer
        const float effectiveCurveExp = (compressionType < 0.5f)
            ? opticalCurveExp * 0.6f   // limit: sharper knee
            : opticalCurveExp;          // compress: normal soft knee

        //--------------------------------------------------------------------
        // 6. Sample loop — the full LA-2A chain
        //--------------------------------------------------------------------
        for (int32 n = 0; n < data.numSamples; n++)
        {
            //----------------------------------------------------------------
            // A. Input gain stage (includes Reduction level offset)
            //----------------------------------------------------------------
            float dryL = inL[n];
            float dryR = inR[n];

            float sigL = dryL * inputGainLin;
            float sigR = dryR * inputGainLin;

            //----------------------------------------------------------------
            // B. Sidechain path: HPF removes deep bass from the detector.
            //    Range 20–200 Hz set by kHighPassId (0–100%).
            //----------------------------------------------------------------
            float scL = hpf1Pole(sigL, sidechainHPF_stateL, sidechainHPF_coeff);
            float scR = hpf1Pole(sigR, sidechainHPF_stateR, sidechainHPF_coeff);

            // Sum to mono (LA-2A uses a mono side-chain)
            float scMono = (scL + scR) * 0.5f;

            //----------------------------------------------------------------
            // C. Optical envelope follower — models the T4B electroluminescent cell
            //----------------------------------------------------------------
            float scAbs = fabsf(scMono);

            if (scAbs > opticalEnvelopeL)
            {
                // Attack: cell brightens as signal rises
                opticalEnvelopeL += attackCoeff * (scAbs - opticalEnvelopeL);
            }
            else
            {
                //------------------------------------------------------------
                // D. Program-dependent dual release:
                //    Fast path decays first; heavy GR blends in the slow tail.
                //------------------------------------------------------------
                releaseEnvFast += fastRelCoeff * (scAbs - releaseEnvFast);
                releaseEnvSlow += slowRelCoeff * (scAbs - releaseEnvSlow);

                float grNorm = std::min(currentGainReduction / 20.0f, 1.0f);
                releaseBlendRatio = grNorm;

                float blendedRelease = (1.0f - releaseBlendRatio) * releaseEnvFast
                    + releaseBlendRatio * releaseEnvSlow;

                opticalEnvelopeL = blendedRelease;
            }
            // Stereo-linked: both channels share the same gain reduction
            opticalEnvelopeR = opticalEnvelopeL;

            //----------------------------------------------------------------
            // E. Gain computer — optical gain law with compression type shaping
            //----------------------------------------------------------------
            float envelopedB = linTodB(opticalEnvelopeL + 1e-9f);
            float overThreshdB = envelopedB - thresholddB;

            float gainReductiondB = opticalGainLaw(overThreshdB, effectiveCurveExp);

            // Smooth gain reduction to avoid clicks
            currentGainReduction += 0.01f * (gainReductiondB - currentGainReduction);
            gainReductionSmooth += 0.001f * (currentGainReduction - gainReductionSmooth);
            grMeterLevel = gainReductionSmooth; // exposed to UI via VU output

            float gainLinear = dBToLin(-currentGainReduction);

            //----------------------------------------------------------------
            // F. Apply gain reduction to main signal path
            //----------------------------------------------------------------
            sigL *= gainLinear;
            sigR *= gainLinear;

            //----------------------------------------------------------------
            // G. Harmonic saturation — transformer / tube coloring
            //----------------------------------------------------------------
            sigL += transformerDCOffset;
            sigR -= transformerDCOffset;

            float satL = saturate(sigL);
            float satR = saturate(sigR);
            sigL = sigL + saturationAmount * (satL - sigL);
            sigR = sigR + saturationAmount * (satR - sigR);

            sigL -= transformerDCOffset;
            sigR += transformerDCOffset;

            //----------------------------------------------------------------
            // H. Tone / Emphasis EQ — peaking biquad filter
            //    tone 0..1 → eqAmount –1..+1
            //    Dead zone at centre (|eqAmount| < 0.01): passthrough (b0=1, rest 0)
            //    Positive: +5.5 dB peak at 2200 Hz
            //    Negative: +5.5 dB peak at 600 Hz
            //----------------------------------------------------------------
            sigL = applyBiquad(sigL, delayEqFilterL);
            sigR = applyBiquad(sigR, delayEqFilterR);

            //----------------------------------------------------------------
            // I. Output gain stage
            //----------------------------------------------------------------
            sigL *= outputGainLin;
            sigR *= outputGainLin;

            //----------------------------------------------------------------
            // J. Analog noise floor
            //----------------------------------------------------------------
            float noise = noiseFloorLevel * (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f);
            sigL += noise;
            sigR += noise * 0.97f;

            //----------------------------------------------------------------
            // K. Dry/wet mix
            //    1.0 = 100% compressed (wet only)
            //    0.0 = 100% dry (no compression)
            //    Dry reference is taken before input gain so the blend is clean.
            //----------------------------------------------------------------
            outL[n] = mix * sigL + (1.0f - mix) * dryL;
            outR[n] = mix * sigR + (1.0f - mix) * dryR;
        }

        // Mark output as not silent
        data.outputs[0].silenceFlags = 0;

        //--------------------------------------------------------------------
        // 7. Push VU meter level back to the host / UI (read-only parameter)
        //--------------------------------------------------------------------
        if (data.outputParameterChanges)
        {
            int32 queueIndex = 0;
            if (auto* vuQueue = data.outputParameterChanges->addParameterData(kVUId, queueIndex))
            {
                // Normalize GR (0–20 dB range) to 0–1 for the parameter
                float vuNorm = std::min(grMeterLevel / 20.0f, 1.0f);
                vuQueue->addPoint(data.numSamples - 1, static_cast<Vst::ParamValue>(vuNorm), queueIndex);
            }
        }

        return kResultOk;
    }

    //------------------------------------------------------------------------
    tresult PLUGIN_API LA2A_CompressorProcessor::setState(IBStream* state)
    {
        IBStreamer streamer(state, kLittleEndian);

        float fVal; int32 iVal;

        if (streamer.readFloat(fVal) == false) return kResultFalse; inputGain = fVal;
        if (streamer.readFloat(fVal) == false) return kResultFalse; outputGain = fVal;
        if (streamer.readFloat(fVal) == false) return kResultFalse; threshold = fVal;
        if (streamer.readFloat(fVal) == false) return kResultFalse; mix = fVal;
        if (streamer.readFloat(fVal) == false) return kResultFalse; tone = fVal;
        if (streamer.readFloat(fVal) == false) return kResultFalse; compressionType = fVal;
        if (streamer.readFloat(fVal) == false) return kResultFalse; sidechainHPF_Hz = fVal;
        if (streamer.readInt32(iVal) == false) return kResultFalse; bypass = iVal != 0;

        // Recompute derived state after load
        if (sampleRate > 0.0)
        {
            sidechainHPF_coeff = expf(-2.0f * static_cast<float>(M_PI) * sidechainHPF_Hz
                / static_cast<float>(sampleRate));
            updateToneFilter();
        }

        return kResultOk;
    }

    //------------------------------------------------------------------------
    tresult PLUGIN_API LA2A_CompressorProcessor::getState(IBStream* state)
    {
        IBStreamer streamer(state, kLittleEndian);

        streamer.writeFloat(inputGain);
        streamer.writeFloat(outputGain);
        streamer.writeFloat(threshold);
        streamer.writeFloat(mix);
        streamer.writeFloat(tone);
        streamer.writeFloat(compressionType);
        streamer.writeFloat(sidechainHPF_Hz);
        streamer.writeInt32(bypass ? 1 : 0);

        return kResultOk;
    }

    //------------------------------------------------------------------------
} // namespace MyCompanyName