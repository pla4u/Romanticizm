#pragma once
#include "Utils.h"

namespace rmz
{
/**
    Stereo widener.
      0 .. 1   narrows the side signal down to mono
      1        untouched
      1 .. 2   widens: boosts the side signal, keeps the bass mono and adds a
               decorrelated (all-pass) side signal derived from the centre, so a
               mono vocal really gets wider and still folds down to mono cleanly.
*/
class Width
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double sc = sr / 48000.0;
        static const int dA[3] = { 113, 271, 421 }, dB[3] = { 157, 311, 467 };
        for (int k = 0; k < 3; ++k)
        {
            apA[k].prepare ((int) (dA[k] * sc), 0.55f);
            apB[k].prepare ((int) (dB[k] * sc), -0.55f);
        }
        sideHp.c = BiquadCoef::highpass (sr, 140.0, 0.7);
        decHp.c  = BiquadCoef::highpass (sr, 260.0, 0.7);
        reset();
    }

    void reset()
    {
        for (int k = 0; k < 3; ++k) { apA[k].reset(); apB[k].reset(); }
        sideHp.reset(); decHp.reset();
    }

    void process (float* l, float* r, int n, float w) noexcept
    {
        const float widen = clampf (w - 1.0f, 0.0f, 1.0f);
        const float sGain = w <= 1.0f ? w : 1.0f + 0.6f * widen;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            const float s = 0.5f * (l[i] - r[i]);

            float a = m, b = m;
            for (int k = 0; k < 3; ++k) { a = apA[k].tick (a); b = apB[k].tick (b); }
            const float dec = decHp.tick (0, 0.5f * (a - b));
            const float sh = sideHp.tick (0, s);

            float s2 = s * sGain;
            if (widen > 0.0f)
            {
                s2 = lerpf (s2, sh * sGain, widen);               // bass stays mono
                s2 += widen * 0.95f * dec;                        // decorrelated width
            }
            l[i] = m + s2;
            r[i] = m - s2;
        }
    }

private:
    double sr = 48000.0;
    Allpass apA[3], apB[3];
    StereoBiquad sideHp, decHp;
};
} // namespace rmz
