#pragma once
#include "Utils.h"

namespace rmz
{
// ============================================================================
//  Two-tap crossfading delay-line pitch shifter (used for the Petals shimmer).
// ============================================================================
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        maxL = 0.075 * sr;
        for (auto& l : line) l.prepare ((int) maxL + 64);
        baseL = (float) (0.036 * sr);
        L = baseL; Tsm = 0.0f; period = 0.0f;
        smoothL = tauToCoef (0.25, sr);
        reset();
    }
    void reset() { for (auto& l : line) l.reset(); p = 0.25f; }

    int getLatency() const noexcept { return (int) (baseL * 0.5f); }

    /** period in samples of the voice (0 = unknown) */
    void setPeriod (float tSamples) noexcept { period = tSamples; }

    void process (float* l, float* r, int n, float ratio) noexcept
    {
        float Lt = baseL;
        if (period > 20.0f)
        {
            Tsm = Tsm <= 0.0f ? period : Tsm + (period - Tsm) * 0.15f;
            const int k = std::max (1, (int) std::lround ((double) baseL / (2.0 * Tsm)));
            Lt = std::min ((float) (2 * k) * Tsm, (float) maxL);
        }
        for (int i = 0; i < n; ++i)
        {
            L += (Lt - L) * smoothL * 0.1f;
            p += (1.0f - ratio) / L;
            p -= std::floor (p);
            float pB = p + 0.5f; if (pB >= 1.0f) pB -= 1.0f;
            const float gA = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * p);
            const float gB = 1.0f - gA;
            const float dA = p * L + 4.0f, dB = pB * L + 4.0f;
            const float inL = l[i], inR = r[i];
            l[i] = gA * line[0].read (dA) + gB * line[0].read (dB);
            r[i] = gA * line[1].read (dA) + gB * line[1].read (dB);
            line[0].push (inL); line[1].push (inR);
        }
    }

private:
    double sr = 48000.0, maxL = 3600.0;
    DelayLine line[2];
    float p = 0.25f, L = 1728.0f, baseL = 1728.0f, Tsm = 0.0f, period = 0.0f, smoothL = 0.001f;
};

} // namespace rmz
