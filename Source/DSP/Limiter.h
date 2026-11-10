#pragma once
#include "Utils.h"
#include <atomic>

namespace rmz
{
/**
    Look-ahead brick-wall limiter (1.5 ms look-ahead, 60 ms release, -0.3 dBFS).
    When disabled the audio is only delayed (latency stays constant).
*/
class Limiter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        look = std::max (8, (int) std::lround (0.0015 * sr));
        delay[0].prepare (look + 8);
        delay[1].prepare (look + 8);
        gIn.assign ((size_t) look, 1.0f);
        gMin.assign ((size_t) look, 1.0f);
        relCoef = tauToCoef (0.06, sr);
        reset();
    }

    void reset()
    {
        delay[0].reset(); delay[1].reset();
        std::fill (gIn.begin(), gIn.end(), 1.0f);
        std::fill (gMin.begin(), gMin.end(), 1.0f);
        pos = 0; sum = (double) look; env = 1.0f; grMeter.store (0.0f);
    }

    int getLatency() const noexcept { return look - 1; }
    float getGainReduction() const noexcept { return grMeter.load(); }

    void process (float* l, float* r, int n, bool enabled) noexcept
    {
        const float ceilLin = 0.9659f;      // -0.3 dBFS
        float minG = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (std::abs (l[i]), std::abs (r[i]));
            float need = (enabled && pk > ceilLin) ? ceilLin / pk : 1.0f;

            // instant attack, exponential release
            env = need < env ? need : env + (1.0f - env) * relCoef;

            gIn[(size_t) pos] = env;
            float m = env;
            for (int k = 0; k < look; ++k) m = std::min (m, gIn[(size_t) k]);   // window of the last `look` values

            sum -= (double) gMin[(size_t) pos];
            gMin[(size_t) pos] = m;
            sum += (double) m;
            if (++pos >= look) pos = 0;
            const float g = (float) (sum / (double) look);

            const float dl = delay[0].readInt (look - 1);
            const float dr = delay[1].readInt (look - 1);
            delay[0].push (l[i]); delay[1].push (r[i]);
            l[i] = dl * g; r[i] = dr * g;
            minG = std::min (minG, g);
        }
        grMeter.store (-linToDb (minG));
    }

private:
    double sr = 48000.0, sum = 0.0;
    int look = 72, pos = 0;
    float relCoef = 0.001f, env = 1.0f;
    DelayLine delay[2];
    std::vector<float> gIn, gMin;
    std::atomic<float> grMeter { 0.0f };
};
} // namespace rmz
