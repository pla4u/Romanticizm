#pragma once
#include "Utils.h"

namespace rmz
{
/**
    Six static bands tuned for voices plus a dynamic "harsh" suppressor.

      LOW END  low shelf   80 Hz      LOW       bell  220 Hz
      MID      bell       700 Hz      MID HIGH  bell 2.2 kHz
      MID END  bell       4.0 kHz     AIR       high shelf 12 kHz

    HARSH watches the 3-9 kHz region against the full signal and dips a wide
    bell around 5.6 kHz only while that region is too loud (sibilance, bite).
*/
class VocalEQ
{
public:
    struct Params
    {
        float lowEnd = 0, low = 0, mid = 0, midHigh = 0, midEnd = 0, air = 0;   // dB
        float harsh = 0;                                                         // 0..1
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        scHp.c = BiquadCoef::highpass (sr, 3000.0, 0.8);
        scLp.c = BiquadCoef::lowpass  (sr, 9000.0, 0.8);
        scFull.c = BiquadCoef::highpass (sr, 120.0, 0.7);
        atkCoef = tauToCoef (0.0015, sr);
        relCoef = tauToCoef (0.045, sr);
        fullCoef = tauToCoef (0.060, sr);
        grCoef = tauToCoef (0.004, sr);
        reset();
        last = Params();
        lastDyn = 99.0f;
        for (int b = 0; b < kBands; ++b) { band[b].c = BiquadCoef(); }
        recompute (Params(), true);
    }

    void reset()
    {
        for (auto& b : band) b.reset();
        dyn.reset(); scHp.reset(); scLp.reset(); scFull.reset();
        bandEnv = fullEnv = 1.0e-6f; grDb = 0.0f;
    }

    float getHarshReduction() const noexcept { return grDb; }

    void process (float* l, float* r, int n, const Params& p) noexcept
    {
        recompute (p, false);

        // dynamic section -------------------------------------------------
        if (p.harsh > 0.002f)
        {
            float peakBand = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (l[i] + r[i]);
                const float b = scLp.tick (0, scHp.tick (0, m));
                const float f = scFull.tick (0, m);
                const float ab = std::abs (b), af = std::abs (f);
                bandEnv += (ab - bandEnv) * (ab > bandEnv ? atkCoef : relCoef);
                fullEnv += (af - fullEnv) * fullCoef;
                peakBand = bandEnv;
            }
            const float relDb = linToDb (peakBand + 1.0e-7f) - linToDb (fullEnv + 1.0e-7f);
            const float thr = -17.0f - 7.0f * p.harsh;
            const float maxRed = 3.0f + 10.0f * p.harsh;
            float red = std::max (0.0f, relDb - thr) * 0.9f;
            if (linToDb (fullEnv + 1.0e-7f) < -60.0f) red = 0.0f;
            red = std::min (red, maxRed);
            grDb += (red - grDb) * (1.0f - std::pow (1.0f - grCoef, (float) n));
        }
        else
            grDb += (0.0f - grDb) * (1.0f - std::pow (1.0f - grCoef, (float) n));

        if (std::abs (grDb - lastDyn) > 0.04f)
        {
            dyn.c = BiquadCoef::peak (sr, 5600.0, 0.9, -(double) grDb);
            lastDyn = grDb;
        }

        // filtering -------------------------------------------------------
        for (int b = 0; b < kBands; ++b)
        {
            if (! active[b]) continue;
            for (int i = 0; i < n; ++i) { l[i] = band[b].tick (0, l[i]); r[i] = band[b].tick (1, r[i]); }
        }
        if (grDb > 0.04f)
            for (int i = 0; i < n; ++i) { l[i] = dyn.tick (0, l[i]); r[i] = dyn.tick (1, r[i]); }
        else
            dyn.reset();
    }

private:
    static constexpr int kBands = 6;

    void recompute (const Params& p, bool force)
    {
        const float g[kBands] = { p.lowEnd, p.low, p.mid, p.midHigh, p.midEnd, p.air };
        const float old[kBands] = { last.lowEnd, last.low, last.mid, last.midHigh, last.midEnd, last.air };
        for (int b = 0; b < kBands; ++b)
        {
            active[b] = std::abs (g[b]) > 0.02f;
            if (! force && std::abs (g[b] - old[b]) < 1.0e-4f) continue;
            switch (b)
            {
                case 0: band[b].c = BiquadCoef::lowShelf  (sr,   80.0, 0.70, g[b]); break;
                case 1: band[b].c = BiquadCoef::peak      (sr,  220.0, 0.85, g[b]); break;
                case 2: band[b].c = BiquadCoef::peak      (sr,  700.0, 0.85, g[b]); break;
                case 3: band[b].c = BiquadCoef::peak      (sr, 2200.0, 0.85, g[b]); break;
                case 4: band[b].c = BiquadCoef::peak      (sr, 4000.0, 0.95, g[b]); break;
                case 5: band[b].c = BiquadCoef::highShelf (sr, std::min (12000.0, sr * 0.42), 0.70, g[b]); break;
            }
        }
        last = p;
    }

    double sr = 48000.0;
    StereoBiquad band[kBands], dyn;
    StereoBiquad scHp, scLp, scFull;
    bool active[kBands] = {};
    Params last;
    float lastDyn = 99.0f, bandEnv = 1.0e-6f, fullEnv = 1.0e-6f, grDb = 0.0f;
    float atkCoef = 0.1f, relCoef = 0.001f, fullCoef = 0.001f, grCoef = 0.01f;
};
} // namespace rmz
