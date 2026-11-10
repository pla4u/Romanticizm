#pragma once
#include "Utils.h"

namespace rmz
{
/**
    Stereo feedback-delay-network reverb.
    * 8 modulated delay lines, Householder feedback matrix (lossless mixing)
    * per-line RT60 gain + one-pole high-frequency damping inside the loop
    * input diffusion (series all-pass) and pre-delay
    * high-pass / low-pass on the return so the tail stays out of the way
    process() produces the wet signal only.
*/
class Reverb
{
public:
    struct Params
    {
        float decay = 0.4f;     // 0..1  -> RT60 0.4 .. 10 s
        float tone = 0.6f;      // 0..1  -> darker .. brighter
        float preMs = 20.0f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double scale = sr / 44100.0;
        static const int base[N] = { 1117, 1327, 1559, 1787, 2053, 2339, 2677, 3011 };
        for (int i = 0; i < N; ++i)
        {
            len[i] = (float) (base[i] * scale);
            line[i].prepare ((int) len[i] + 64);
            lfoPhase[i] = 0.125f * (float) i;
            lfoRate[i] = 0.07f + 0.045f * (float) i;
            lp[i] = 0.0f;
        }
        static const int apL[3] = { 142, 107, 379 }, apR[3] = { 157, 113, 403 };
        for (int k = 0; k < 3; ++k)
        {
            diffL[k].prepare ((int) (apL[k] * scale), 0.68f);
            diffR[k].prepare ((int) (apR[k] * scale), 0.68f);
        }
        pre[0].prepare ((int) (0.35 * sr));
        pre[1].prepare ((int) (0.35 * sr));
        modDepth = (float) (6.0 * scale);
        paramCoef = tauToCoef (0.05, sr);
        outHp[0].c = outHp[1].c = BiquadCoef::highpass (sr, 160.0, 0.7);
        lastTone = -1.0f; decaySm = -1.0f; preSm = -1.0f;
        reset();
    }

    void reset()
    {
        for (int i = 0; i < N; ++i) { line[i].reset(); lp[i] = 0.0f; }
        for (int k = 0; k < 3; ++k) { diffL[k].reset(); diffR[k].reset(); }
        pre[0].reset(); pre[1].reset();
        for (int c = 0; c < 2; ++c) { outHp[c].reset(); outLp[c].reset(); }
    }

    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n, const Params& p) noexcept
    {
        if (decaySm < 0.0f) { decaySm = p.decay; preSm = p.preMs; }

        if (std::abs (p.tone - lastTone) > 0.002f)
        {
            const double fc = 1800.0 * std::pow (8.0, (double) p.tone);                // 1.8k .. 14.4k
            dampA = (float) std::exp (-kTwoPi * std::min (fc, sr * 0.45) / sr);
            const double fo = std::min (sr * 0.45, fc * 1.5);
            outLp[0].c = outLp[1].c = BiquadCoef::lowpass (sr, fo, 0.7);
            lastTone = p.tone;
        }

        for (int s = 0; s < n; ++s)
        {
            decaySm += (p.decay - decaySm) * paramCoef;
            preSm   += (p.preMs - preSm) * paramCoef;

            // decay -> RT60 -> per-line gain (recomputed cheaply every 16 samples)
            if ((s & 15) == 0)
            {
                const float rt60 = 0.4f * std::pow (25.0f, decaySm);
                for (int i = 0; i < N; ++i)
                    gain[i] = std::pow (10.0f, -3.0f * len[i] / ((float) sr * rt60));
            }

            // pre-delay + diffusion
            pre[0].push (inL[s]); pre[1].push (inR[s]);
            const float pd = std::max (3.0f, preSm * 0.001f * (float) sr);
            float xl = pre[0].read (pd), xr = pre[1].read (pd);
            for (int k = 0; k < 3; ++k) { xl = diffL[k].tick (xl); xr = diffR[k].tick (xr); }

            float tap[N], sum = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                lfoPhase[i] += lfoRate[i] / (float) sr;
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                const float d = len[i] + modDepth * std::sin ((float) kTwoPi * lfoPhase[i]);
                float t = line[i].read (d);
                lp[i] = t + (lp[i] - t) * dampA;                    // one-pole low-pass (damping)
                tap[i] = lp[i] * gain[i];
                sum += tap[i];
            }
            const float k2 = sum * (2.0f / (float) N);
            for (int i = 0; i < N; ++i)
            {
                const float inj = (i & 1) ? xr : xl;
                line[i].push (tap[i] - k2 + inj * 0.35f);
            }

            const float oL = tap[0] + tap[1] - tap[2] - tap[3] + tap[4] + tap[5] - tap[6] - tap[7];
            const float oR = tap[0] - tap[1] - tap[2] + tap[3] + tap[4] - tap[5] - tap[6] + tap[7];
            wetL[s] = outLp[0].tick (0, outHp[0].tick (0, oL)) * 0.42f;
            wetR[s] = outLp[1].tick (0, outHp[1].tick (0, oR)) * 0.42f;
        }
    }

private:
    static constexpr int N = 8;
    double sr = 48000.0;
    float len[N] = {}, gain[N] = {}, lp[N] = {}, lfoPhase[N] = {}, lfoRate[N] = {};
    DelayLine line[N], pre[2];
    Allpass diffL[3], diffR[3];
    StereoBiquad outHp[2], outLp[2];
    float modDepth = 6.0f, dampA = 0.3f, paramCoef = 0.001f, lastTone = -1.0f, decaySm = -1.0f, preSm = 20.0f;
};
} // namespace rmz
