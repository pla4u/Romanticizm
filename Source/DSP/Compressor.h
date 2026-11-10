#pragma once
#include "Utils.h"
#include <atomic>

namespace rmz
{
/**
    One-knob vocal compressor with a soft knee, stereo-linked peak/RMS blended
    detector and program-dependent release (deeper reduction = slower release).

      amount  0..1 : moves threshold (-4 .. -34 dB) and ratio (1 .. 7:1) together
      gain    dB   : output trim on top of partial automatic make-up
*/
class Compressor
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        rmsCoef = tauToCoef (0.003, sr);
        atkCoef = tauToCoef (0.0065, sr);
        relFast = tauToCoef (0.07, sr);
        relSlow = tauToCoef (0.35, sr);
        reset();
    }
    void reset() { ms = 0.0f; gr = 0.0f; grMeter.store (0.0f); }

    float getGainReduction() const noexcept { return grMeter.load(); }

    void process (float* l, float* r, int n, float amount, float gainDb) noexcept
    {
        const float a = clampf (amount, 0.0f, 1.0f);
        const float thr = -4.0f - 30.0f * a;
        const float ratio = 1.0f + 6.0f * a;
        const float knee = 10.0f;
        const float slope = 1.0f / ratio - 1.0f;
        const float makeup = 0.5f * (-thr) * (1.0f - 1.0f / ratio) + gainDb;
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (std::abs (l[i]), std::abs (r[i]));
            ms += (pk * pk - ms) * rmsCoef;
            const float det = 0.5f * pk + 0.5f * std::sqrt (ms);
            const float lvl = linToDb (det);
            const float over = lvl - thr;

            float red = 0.0f;                                         // negative dB
            if (a > 0.0005f)
            {
                if (2.0f * over < -knee) red = 0.0f;
                else if (2.0f * std::abs (over) <= knee)
                {
                    const float x = over + knee * 0.5f;
                    red = slope * x * x / (2.0f * knee);
                }
                else red = slope * over;
            }
            const float target = -red;                                // positive reduction

            if (target > gr) gr += (target - gr) * atkCoef;
            else
            {
                gr += (target - gr) * lerpf (relFast, relSlow, clampf (gr / 12.0f, 0.0f, 1.0f));
            }

            const float g = dbToLin (makeup - gr);
            l[i] *= g; r[i] *= g;
            maxGr = std::max (maxGr, gr);
        }
        grMeter.store (maxGr);
    }

private:
    double sr = 48000.0;
    float rmsCoef = 0.01f, atkCoef = 0.01f, relFast = 0.001f, relSlow = 0.0003f, ms = 0.0f, gr = 0.0f;
    std::atomic<float> grMeter { 0.0f };
};
} // namespace rmz
