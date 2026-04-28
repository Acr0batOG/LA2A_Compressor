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

using namespace Steinberg;

namespace {

    //------------------------------------------------------------------------
    // Tube-style soft asymmetric saturation (your existing function)
    //------------------------------------------------------------------------
    inline float saturate(float x)
    {
        const float drive = 1.42f;
        float y = x * drive;
        return y / (1.0f + fabsf(y));
    }

    //------------------------------------------------------------------------
    // Convert milliseconds to a one-pole smoothing coefficient
    // alpha close to 1.0 = slow (long time), close to 0.0 = fast (short time)
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
    // LA-2A optical gain law:
    //   The T4B cell has a non-linear, program-dependent response.
    //   We model it with a soft-knee curve using the optical exponent.
    //   inputdB = signal level above threshold (positive = over threshold)
    //   Returns gain reduction in dB (positive number = amount of reduction)
    //------------------------------------------------------------------------
    inline float opticalGainLaw(float overThresholddB, float opticalCurveExp)
    {
        if (overThresholddB <= 0.0f) return 0.0f;
        // Non-linear power law — higher exponent = softer knee / more gradual onset
        return powf(overThresholddB, opticalCurveExp) * 0.5f;
    }

    //------------------------------------------------------------------------
    // 1-pole high-pass filter (used on the sidechain detection path)
    // Removes deep bass so the optical cell doesn't over-react to kick/bass
    //------------------------------------------------------------------------
    inline float hpf1Pole(float x, float& z1, float coeff)
    {
        // coeff = 1 - (2*pi*fc / sampleRate), precomputed in prepareToPlay
        float y = x - z1;
        z1 = z1 + coeff * y; // leaky integrator for the low-frequency state
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

        // Precompute sidechain HPF coefficient
        // HPF coefficient: how much low-end to remove from the detection signal
        // sidechainHPF_Hz ~ 90 Hz cuts rumble and fundamental bass from the detector
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
    tresult PLUGIN_API LA2A_CompressorProcessor::process(Vst::ProcessData& data)
    {

        
        //--------------------------------------------------------------------
        // 1. Read incoming parameter changes from the host / UI knobs
        //--------------------------------------------------------------------
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
                    // Take the last value in the queue for this block
                    if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
                    {
                        switch (paramQueue->getParameterId())
                        {
                        case kInputGainId:
                            inputGain = static_cast<float>(value);   // 0–1
                            break;
                        case kReductionId:
                            threshold = static_cast<float>(value);   // 0–1
							break;
                        case kOutputGainId:
                            outputGain = static_cast<float>(value);  // 0–1
                            break;
                        case kToneId:
                            tone = static_cast<float>(value);   // 0–1
                            break;
                        case kMixId:
                            mix = static_cast<float>(value);         // 0–1
                            break;
                        case kCompressionTypeId:
                            compressionType = static_cast<float>(value); // 0–1
                            break;
                        case kBypassId:
                            bypass = (value > 0.5);
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }

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
            ? data.inputs[0].channelBuffers32[1]
            : inL;

        Vst::Sample32* outL = data.outputs[0].channelBuffers32[0];
        Vst::Sample32* outR = (numOutChannels > 1)
            ? data.outputs[0].channelBuffers32[1]
            : outL;

        //--------------------------------------------------------------------
        // 5. Precompute per-block coefficients from timing parameters
        //    (cheaper than computing per-sample)
        //--------------------------------------------------------------------

        // Map inputGain / outputGain from 0–1 to a useful dB range.
        // 0.25 = unity (0 dB), 0.0 = –∞, 1.0 = +18 dB
        // Simple linear-to-dB mapping: gain = 10^((value - 0.25) * 72 / 20)
        const float inputGainLin = dBToLin((inputGain - 0.25f) * 72.0f);
        const float outputGainLin = dBToLin((outputGain - 0.25f) * 72.0f);

        // Map threshold 0–1 → –60 dB to 0 dB
        const float thresholddB = (threshold - 1.0f) * 60.0f;

        // Optical cell attack/decay coefficients
        const float attackCoeff = msToCoeff(attackTime, sampleRate);
        const float fastRelCoeff = msToCoeff(initialReleaseTime, sampleRate);
        const float slowRelCoeff = msToCoeff(slowReleaseTime, sampleRate);

        // Tone EQ: compressorEq 0–1 maps to a shelf gain on the output.
        // 0 = darkest (–3 dB HF), 0.5 = neutral, 1 = brightest (+3 dB HF)
        // We implement this as a simple 1-pole IIR tone shelf on the output.
        // Positive toneGain brightens, negative darkens.
        const float toneGainLin = (tone - 0.5f) * 2.0f; // –1 to +1
        // 1-pole shelf coeff for ~3 kHz crossover
        const float toneCoeff = expf(-2.0f * static_cast<float>(M_PI) * 3000.0f
            / static_cast<float>(sampleRate));

        //--------------------------------------------------------------------
        // 6. Sample loop — the full LA-2A chain
        //--------------------------------------------------------------------
        for (int32 n = 0; n < data.numSamples; n++)
        {
            //----------------------------------------------------------------
            // A. Input gain stage
            //----------------------------------------------------------------
            float dryL = inL[n];
            float dryR = inR[n];

            float sigL = dryL * inputGainLin;
            float sigR = dryR * inputGainLin;

            //----------------------------------------------------------------
            // B. Sidechain path: high-pass filter to remove deep bass
            //    The optical cell of the LA-2A is less sensitive to
            //    sub-bass, preventing kick drums from pumping the compressor.
            //----------------------------------------------------------------
            float scL = hpf1Pole(sigL, sidechainHPF_stateL, sidechainHPF_coeff);
            float scR = hpf1Pole(sigR, sidechainHPF_stateR, sidechainHPF_coeff);

            // Sum to mono for the detector (LA-2A is a mono side-chain device)
            float scMono = (scL + scR) * 0.5f;

            //----------------------------------------------------------------
            // C. Optical envelope follower
            //    Models the electroluminescent cell in the T4B attenuator.
            //    The cell responds to the RMS-like brightness of the signal,
            //    with a non-linear attack much slower than digital attack.
            //----------------------------------------------------------------
            float scAbs = fabsf(scMono);

            if (scAbs > opticalEnvelopeL)
                // Attack: cell brightens as signal rises
                opticalEnvelopeL += attackCoeff * (scAbs - opticalEnvelopeL);
            else
            {
                //----------------------------------------------------------------
                // D. Program-dependent dual release:
                //    Fast path decays first; as signal drops further, the slow
                //    path takes over. The blend is driven by how much gain
                //    reduction is currently happening — heavy GR = more slow tail.
                //----------------------------------------------------------------
                releaseEnvFast += fastRelCoeff * (scAbs - releaseEnvFast);
                releaseEnvSlow += slowRelCoeff * (scAbs - releaseEnvSlow);

                // Blend ratio: more GR → more slow release character
                float grNorm = std::min(currentGainReduction / 20.0f, 1.0f); // 0–1
                releaseBlendRatio = grNorm;

                float blendedRelease = (1.0f - releaseBlendRatio) * releaseEnvFast
                    + releaseBlendRatio * releaseEnvSlow;

                opticalEnvelopeL = blendedRelease;
            }
            // Mirror envelope to R (stereo-linked: both channels use same GR)
            opticalEnvelopeR = opticalEnvelopeL;

            //----------------------------------------------------------------
            // E. Gain computer — optical gain law
            //    The T4B cell has a soft, progressive gain curve.
            //    We compute how far above threshold the envelope is,
            //    then apply the non-linear power law.
            //----------------------------------------------------------------
            float envelopedB = linTodB(opticalEnvelopeL + 1e-9f);
            float overThreshdB = envelopedB - thresholddB;

            float gainReductiondB = opticalGainLaw(overThreshdB, opticalCurveExp);

            // Smooth the gain reduction signal to avoid clicks
            currentGainReduction += 0.01f * (gainReductiondB - currentGainReduction);
            gainReductionSmooth += 0.001f * (currentGainReduction - gainReductionSmooth);
            grMeterLevel = gainReductionSmooth; // expose to UI

            float gainLinear = dBToLin(-currentGainReduction);

            //----------------------------------------------------------------
            // F. Apply gain reduction to main signal path
            //----------------------------------------------------------------
            sigL *= gainLinear;
            sigR *= gainLinear;
             
            //----------------------------------------------------------------
            // G. Harmonic saturation — transformer / tube coloring
            //    The LA-2A's transformer and tube output stage add warmth
            //    via soft clipping and even-order harmonics.
            //    We inject a tiny DC offset to bias the saturator asymmetrically,
            //    which generates more 2nd harmonic (even) character.
            //----------------------------------------------------------------
            sigL += transformerDCOffset;
            sigR -= transformerDCOffset; // opposite polarity on R for stereo width

            // Blend dry and saturated signal by saturationAmount
            float satL = saturate(sigL);
            float satR = saturate(sigR);
            sigL = sigL + saturationAmount * (satL - sigL);
            sigR = sigR + saturationAmount * (satR - sigR);

            // Remove DC offset after saturation
            sigL -= transformerDCOffset;
            sigR += transformerDCOffset;

            //----------------------------------------------------------------
            // H. Tone / Emphasis EQ
            //    Simple 1-pole high-shelf:
            //    toneGainLin > 0 brightens, < 0 darkens.
            //    The LA-2A's emphasis control was essentially a treble lift
            //    applied after the optical attenuator.
            //    toneStateL/R track the low-frequency content to subtract/add.
            //----------------------------------------------------------------
            toneStateL = toneCoeff * toneStateL + (1.0f - toneCoeff) * sigL;
            toneStateR = toneCoeff * toneStateR + (1.0f - toneCoeff) * sigR;

            float hfL = sigL - toneStateL; // high-frequency content
            float hfR = sigR - toneStateR;

            sigL = sigL + toneGainLin * hfL;
            sigR = sigR + toneGainLin * hfR;

            //----------------------------------------------------------------
            // I. Output gain stage
            //----------------------------------------------------------------
            sigL *= outputGainLin;
            sigR *= outputGainLin;

            //----------------------------------------------------------------
            // J. Analog noise floor
            //    Tiny broadband noise adds the subtle hiss of real analog gear.
            //    rand() is not ideal for production — replace with a proper
            //    PRNG (e.g. xorshift) if CPU overhead matters.
            //----------------------------------------------------------------
            float noise = noiseFloorLevel * (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f);
            sigL += noise;
            sigR += noise * 0.97f; // slight decorrelation between channels

            //----------------------------------------------------------------
            // K. Dry/wet mix
            //    Parallel compression: blend the compressed/processed signal
            //    with the unprocessed dry signal at input gain level.
            //----------------------------------------------------------------
            outL[n] = mix * sigL + (1.0f - mix) * dryL;
            outR[n] = mix * sigR + (1.0f - mix) * dryR;
        }

        // Mark output as not silent
        data.outputs[0].silenceFlags = 0;

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
        if (streamer.readInt32(iVal) == false) return kResultFalse; bypass = iVal != 0;

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
        streamer.writeInt32(bypass ? 1 : 0);

        return kResultOk;
    }

    //------------------------------------------------------------------------
} // namespace MyCompanyName