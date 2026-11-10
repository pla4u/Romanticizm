#pragma once
#include "Utils.h"

namespace rmz
{
// ============================================================================
//  Tape transport: wow (slow), flutter (fast) and random drift, done with a
//  modulated delay around a fixed 3 ms centre (constant latency).
// ============================================================================
class TapeWobble
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        centre = (float) (0.0035 * sr);
        line[0].prepare ((int) (0.012 * sr));
        line[1].prepare ((int) (0.012 * sr));
        driftCoef = tauToCoef (0.35, sr);
        reset();
    }
    void reset()
    {
        line[0].reset(); line[1].reset();
        wow = 0.0f; flut = 0.0f; drift = 0.0f; target = 0.0f; rng = 22222u; cnt = 0;
    }
    int getLatency() const noexcept { return (int) centre; }

    void process (float* l, float* r, int n, float depth) noexcept
    {
        const float wowAmp   = depth * (float) (0.0016 * sr);   // samples
        const float flutAmp  = depth * (float) (0.00007 * sr);
        const float driftAmp = depth * (float) (0.0007 * sr);
        const float dw = (float) (kTwoPi * 0.62 / sr), df = (float) (kTwoPi * 6.4 / sr);
        for (int i = 0; i < n; ++i)
        {
            if ((cnt++ & 2047) == 0) target = nextRand() * 2.0f - 1.0f;
            drift += (target - drift) * driftCoef * 4.0f;
            wow += dw; if (wow > (float) kTwoPi) wow -= (float) kTwoPi;
            flut += df; if (flut > (float) kTwoPi) flut -= (float) kTwoPi;
            const float d = centre + wowAmp * std::sin (wow) + flutAmp * std::sin (flut + 1.3f * std::sin (wow))
                          + driftAmp * drift;
            const float inL = l[i], inR = r[i];
            l[i] = line[0].read (d);
            r[i] = line[1].read (d);
            line[0].push (inL); line[1].push (inR);
        }
    }

    float nextRand() noexcept { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) * (1.0f / 16777216.0f); }

private:
    double sr = 48000.0;
    float centre = 168.0f, wow = 0, flut = 0, drift = 0, target = 0, driftCoef = 0.001f;
    uint32_t rng = 1; uint32_t cnt = 0;
    DelayLine line[2];
};

// ============================================================================
//  Tape colour: head bump, HF loss with age, hiss and dust.
// ============================================================================
class TapeColour
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            hissHp[c].c = BiquadCoef::highpass (sr, 1500.0, 0.7);
            hissLp[c].c = BiquadCoef::lowpass (sr, 10500.0, 0.7);
        }
        lastAge = -1.0f;
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) { bump[c].reset(); lp[c].reset(); lp2[c].reset(); hissHp[c].reset(); hissLp[c].reset(); }
        dustEnv[0] = dustEnv[1] = 0.0f; rng = 987654u;
    }

    void process (float* l, float* r, int n, float age, float hiss, float dust) noexcept
    {
        if (std::abs (age - lastAge) > 0.003f)
        {
            lastAge = age;
            const double fc = 17000.0 * std::pow (0.36, (double) age);              // 17 k -> 6 k
            for (int c = 0; c < 2; ++c)
            {
                bump[c].c = BiquadCoef::lowShelf (sr, 95.0, 0.8, 0.5 + 2.6 * (double) age);
                lp[c].c   = BiquadCoef::lowpass (sr, std::min (fc, sr * 0.45), 0.6);
                lp2[c].c  = BiquadCoef::highpass (sr, 30.0 + 40.0 * (double) age, 0.7);
            }
        }
        const float hissG = hiss > 0.001f ? dbToLin (-76.0f + 36.0f * hiss) : 0.0f;
        const float dustRate = dust * dust * 55.0f / (float) sr;                    // events / sample
        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                x = biquadTick (bump[c].c, bump[c].s, x);
                x = biquadTick (lp[c].c, lp[c].s, x);
                x = biquadTick (lp2[c].c, lp2[c].s, x);
                if (hissG > 0.0f)
                {
                    const float nz = (rand01() * 2.0f - 1.0f) * hissG;
                    x += biquadTick (hissLp[c].c, hissLp[c].s, biquadTick (hissHp[c].c, hissHp[c].s, nz)) * 2.4f;
                }
                if (dustRate > 0.0f)
                {
                    if (rand01() < dustRate) dustEnv[c] = (rand01() * 2.0f - 1.0f) * (0.01f + 0.05f * dust);
                    x += dustEnv[c];
                    dustEnv[c] *= 0.82f;
                }
                ch[c][i] = x;
            }
    }

private:
    float rand01() noexcept { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) * (1.0f / 16777216.0f); }
    struct BS { BiquadCoef c; BiquadState s; void reset() { s.reset(); } };
    double sr = 48000.0;
    BS bump[2], lp[2], lp2[2], hissHp[2], hissLp[2];
    float lastAge = -1.0f, dustEnv[2] { 0, 0 };
    uint32_t rng = 1;
};

// ============================================================================
//  Crunch: sample-rate / bit-depth reduction blended in parallel, soft and dirty
// ============================================================================
class Crusher
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { held[0] = held[1] = 0.0f; ctr = 0.0f; sm[0] = sm[1] = 0.0f; }

    void process (float* l, float* r, int n, float amt) noexcept
    {
        if (amt < 0.002f) return;
        const float hold = 1.0f + amt * amt * 9.0f;
        const float bits = 15.0f - amt * 7.5f;
        const float levels = std::exp2 (bits - 1.0f);
        const float inv = 1.0f / levels;
        const float mixA = std::min (1.0f, amt * 1.5f) * 0.65f;
        const float lpc = clampf (0.9f / hold + 0.08f, 0.1f, 1.0f);
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            ctr += 1.0f;
            const bool take = ctr >= hold;
            if (take) ctr -= hold;
            for (int c = 0; c < 2; ++c)
            {
                if (take) held[c] = std::round (ch[c][i] * levels) * inv;
                sm[c] += (held[c] - sm[c]) * lpc;
                ch[c][i] += (sm[c] - ch[c][i]) * mixA;
            }
        }
    }

private:
    double sr = 48000.0;
    float held[2] { 0, 0 }, sm[2] { 0, 0 }, ctr = 0.0f;
};
} // namespace rmz
