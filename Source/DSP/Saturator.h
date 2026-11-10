#pragma once
#include "Utils.h"
#include <juce_dsp/juce_dsp.h>

namespace rmz
{
/**
    Saturation / distortion / "hyper" exciter stack, run at 4x oversampling.

      SAT     tape-like soft saturation with pre/de-emphasis and a touch of bias
      DIST    asymmetric hard-ish clipper with a bass-safe pre filter and a TONE
              low-pass (the crunchy, forward vocal distortion)
      HYPER   high-band harmonic exciter ("sparkle")
*/
class Saturator
{
public:
    struct Params { float sat = 0, dist = 0, tone = 0.5f, hyper = 0; };

    void prepare (double sampleRate, int maxBlock)
    {
        using OS = juce::dsp::Oversampling<float>;
        os = std::make_unique<OS> (2, 2, OS::filterHalfBandPolyphaseIIR, true, true);
        os->initProcessing ((size_t) maxBlock);
        osr = sampleRate * 4.0;

        for (int c = 0; c < 2; ++c)
        {
            preEmph[c].c  = BiquadCoef::highShelf (osr, 2200.0, 0.7, +4.5);
            postEmph[c].c = BiquadCoef::highShelf (osr, 2200.0, 0.7, -4.5);
            dcBlock[c].c  = BiquadCoef::highpass (osr, 18.0, 0.7);
            distHp[c].c   = BiquadCoef::highpass (osr, 150.0, 0.7);
            hypHp1[c].c   = BiquadCoef::highpass (osr, 4200.0, 0.7);
            hypHp2[c].c   = BiquadCoef::highpass (osr, 4200.0, 0.7);
            hypHp3[c].c   = BiquadCoef::highpass (osr, 4200.0, 0.7);
            distPres[c].c = BiquadCoef::peak (osr, 2600.0, 0.8, 2.5);
        }
        lastTone = -1.0f;
        reset();
    }

    void reset()
    {
        if (os) os->reset();
        for (int c = 0; c < 2; ++c)
        {
            preEmph[c].reset(); postEmph[c].reset(); dcBlock[c].reset(); distHp[c].reset();
            distLp1[c].reset(); distLp2[c].reset(); hypHp1[c].reset(); hypHp2[c].reset();
            hypHp3[c].reset(); distPres[c].reset();
        }
    }

    int getLatency() const noexcept { return os ? (int) std::lround (os->getLatencyInSamples()) : 0; }

    void process (float* l, float* r, int n, const Params& p) noexcept
    {
        float* chans[2] = { l, r };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto up = os->processSamplesUp (block);
        const int m = (int) up.getNumSamples();
        float* u[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };

        const bool doSat = p.sat > 0.001f, doDist = p.dist > 0.001f, doHyper = p.hyper > 0.001f;

        if (doDist && std::abs (p.tone - lastTone) > 0.002f)
        {
            const double f = std::min (18000.0, 1400.0 * std::pow (12.0, (double) p.tone));   // 1.4k .. 16.8k
            for (int c = 0; c < 2; ++c)
            {
                distLp1[c].c = BiquadCoef::lowpass (osr, f, 0.6);
                distLp2[c].c = BiquadCoef::lowpass (osr, f, 0.9);
            }
            lastTone = p.tone;
        }

        // sat constants
        const float s = p.sat;
        const float satDrive = dbToLin (21.0f * s);
        const float satBias  = 0.16f * s;
        const float satBiasT = fastTanh (satBias);
        const float satComp  = 1.0f / std::pow (satDrive, 0.68f);
        const float satMix   = std::min (1.0f, s * 2.0f);
        // dist constants
        const float d = p.dist;
        const float distDrive = dbToLin (8.0f + 34.0f * d);
        const float distMix = 0.18f + 0.82f * std::pow (d, 0.7f);
        const float distOut = 0.42f / std::pow (distDrive, 0.18f);
        // hyper
        const float hy = p.hyper;

        for (int c = 0; c < 2; ++c)
        {
            float* x = u[c];
            for (int i = 0; i < m; ++i)
            {
                float v = x[i];

                if (doSat)
                {
                    const float e = biquadTick (preEmph[c].c, preEmph[c].s[0], v);
                    float y = (fastTanh (satDrive * e + satBias) - satBiasT) * satComp;
                    y = biquadTick (postEmph[c].c, postEmph[c].s[0], y);
                    y = biquadTick (dcBlock[c].c, dcBlock[c].s[0], y);
                    v += (y - v) * satMix;
                }
                if (doDist)
                {
                    float a = biquadTick (distHp[c].c, distHp[c].s[0], v) * distDrive;
                    float y = a >= 0.0f ? fastTanh (a) : -1.15f * fastTanh (-a * 0.55f);
                    y = biquadTick (distLp1[c].c, distLp1[c].s[0], y);
                    y = biquadTick (distLp2[c].c, distLp2[c].s[0], y);
                    y = biquadTick (distPres[c].c, distPres[c].s[0], y) * distOut;
                    v += (y - v) * distMix;
                }
                if (doHyper)
                {
                    const float h = biquadTick (hypHp1[c].c, hypHp1[c].s[0], v);
                    float e = fastTanh (h * 7.0f) * 0.30f;
                    e = biquadTick (hypHp2[c].c, hypHp2[c].s[0], e);
                    e = biquadTick (hypHp3[c].c, hypHp3[c].s[0], e);
                    v += hy * 1.1f * e;
                }
                x[i] = v;
            }
        }
        os->processSamplesDown (block);
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    double osr = 192000.0;
    float lastTone = -1.0f;
    StereoBiquad preEmph[2], postEmph[2], dcBlock[2], distHp[2], distLp1[2], distLp2[2], distPres[2],
                 hypHp1[2], hypHp2[2], hypHp3[2];
};
} // namespace rmz
