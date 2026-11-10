#pragma once
#include "Utils.h"

namespace rmz
{
/**
    Formant shifter.

    Short-time Fourier transform with 75 % overlap. For every frame the spectral
    envelope (the vocal-tract "formants") is estimated with a "true envelope"
    cepstral smoother, the envelope is warped along the frequency axis and the
    original fine structure (pitch harmonics + phase) is kept untouched. The
    result is a change of the vocal character (deeper / brighter voice) without
    changing the sung pitch.

    ratio < 1  ->  formants move down (deeper, "bigger" voice)
    ratio > 1  ->  formants move up.

    Latency is exactly fftSize samples.
*/
class FormantShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        order = sr > 100000.0 ? 13 : (sr > 50000.0 ? 12 : 11);
        N = 1 << order;
        H = N / 2;
        hop = N / 4;
        fft = std::make_unique<FFT> (order);

        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (kTwoPi * i / N));

        lifterLen = std::max (24, (int) (sr / 450.0));
        lifterLen = std::min (lifterLen, N / 6);
        lifter.assign ((size_t) (lifterLen + 1), 1.0f);
        const int taperStart = (int) (lifterLen * 0.75);
        for (int q = taperStart; q <= lifterLen; ++q)
            lifter[(size_t) q] = (float) (0.5 * (1.0 + std::cos (kPi * (q - taperStart) / (double) (lifterLen - taperStart + 1))));

        for (auto& c : ch)
        {
            c.in.assign ((size_t) N, 0.0f);
            c.acc.assign ((size_t) N, 0.0f);
            c.inPos = c.outPos = c.hopCount = 0;
        }
        spec.assign ((size_t) (2 * N), 0.0f);
        tmp.assign ((size_t) (2 * N), 0.0f);
        frame.assign ((size_t) N, 0.0f);
        logMag.assign ((size_t) (H + 1), 0.0f);
        env.assign ((size_t) (H + 1), 0.0f);
        target.assign ((size_t) (H + 1), 0.0f);
        ratio = 1.0f;
    }

    void reset()
    {
        for (auto& c : ch)
        {
            std::fill (c.in.begin(), c.in.end(), 0.0f);
            std::fill (c.acc.begin(), c.acc.end(), 0.0f);
            c.inPos = c.outPos = c.hopCount = 0;
        }
    }

    int getLatency() const noexcept { return N; }

    void setShiftSemitones (float st) noexcept { ratio = std::exp2 (st / 12.0f); }

    void process (int channel, float* x, int n) noexcept
    {
        auto& c = ch[(size_t) channel];
        const int mask = N - 1;
        for (int i = 0; i < n; ++i)
        {
            c.in[(size_t) c.inPos] = x[i];
            c.inPos = (c.inPos + 1) & mask;

            const float out = c.acc[(size_t) c.outPos];
            c.acc[(size_t) c.outPos] = 0.0f;
            c.outPos = (c.outPos + 1) & mask;

            if (++c.hopCount >= hop)
            {
                c.hopCount = 0;
                processFrame (c);
            }
            x[i] = out;
        }
    }

private:
    struct Chan
    {
        std::vector<float> in, acc;
        int inPos = 0, outPos = 0, hopCount = 0;
    };

    // smoothed log spectrum of a (symmetric) real log-magnitude array via cepstral liftering
    void smoothLog (const float* src, float* dst) noexcept
    {
        float* t = tmp.data();
        for (int k = 0; k <= H; ++k) { t[2 * k] = src[k]; t[2 * k + 1] = 0.0f; }
        for (int k = 1; k < H; ++k)  { t[2 * (N - k)] = src[k]; t[2 * (N - k) + 1] = 0.0f; }
        fft->transform (t, false);

        const float inv = 1.0f / (float) N;
        // keep low quefrencies only (symmetric)
        for (int q = lifterLen + 1; q < N - lifterLen; ++q) { t[2 * q] = 0.0f; t[2 * q + 1] = 0.0f; }
        for (int q = 0; q <= lifterLen; ++q)
        {
            t[2 * q] *= inv * lifter[(size_t) q];
            t[2 * q + 1] = 0.0f;
        }
        for (int q = 1; q <= lifterLen; ++q)
        {
            t[2 * (N - q)] *= inv * lifter[(size_t) q];
            t[2 * (N - q) + 1] = 0.0f;
        }
        fft->transform (t, false);
        for (int k = 0; k <= H; ++k) dst[k] = t[2 * k];
    }

    void processFrame (Chan& c) noexcept
    {
        const int mask = N - 1;
        const bool identity = std::abs (ratio - 1.0f) < 0.0007f;
        const float norm = 1.0f / 1.5f;               // sum of hann^2 at 75 % overlap

        for (int k = 0; k < N; ++k)
            frame[(size_t) k] = c.in[(size_t) ((c.inPos + k) & mask)] * window[(size_t) k];

        if (! identity)
        {
            float* s = spec.data();
            for (int k = 0; k < N; ++k) { s[2 * k] = frame[(size_t) k]; s[2 * k + 1] = 0.0f; }
            fft->transform (s, false);

            for (int k = 0; k <= H; ++k)
                logMag[(size_t) k] = 0.5f * std::log (s[2 * k] * s[2 * k] + s[2 * k + 1] * s[2 * k + 1] + 1.0e-12f);

            // true-envelope estimation (3 smoothing passes)
            smoothLog (logMag.data(), env.data());
            for (int it = 0; it < 2; ++it)
            {
                for (int k = 0; k <= H; ++k) target[(size_t) k] = std::max (logMag[(size_t) k], env[(size_t) k]);
                smoothLog (target.data(), env.data());
            }

            // warp envelope along frequency, keep fine structure
            const float inv = 1.0f / ratio;
            for (int k = 0; k <= H; ++k)
            {
                const float p = (float) k * inv;
                float e;
                if (p >= (float) H) e = env[(size_t) H];
                else
                {
                    const int i0 = (int) p;
                    const float f = p - (float) i0;
                    e = env[(size_t) i0] * (1.0f - f) + env[(size_t) (i0 + 1)] * f;
                }
                const float d = clampf (e - env[(size_t) k], -3.5f, 3.5f);
                const float g = std::exp (d);
                s[2 * k] *= g; s[2 * k + 1] *= g;
            }
            s[1] = 0.0f; s[2 * H + 1] = 0.0f;
            for (int k = 1; k < H; ++k) { s[2 * (N - k)] = s[2 * k]; s[2 * (N - k) + 1] = -s[2 * k + 1]; }

            fft->transform (s, true);
            const float inN = 1.0f / (float) N;
            for (int k = 0; k < N; ++k) frame[(size_t) k] = s[2 * k] * inN * window[(size_t) k] * norm;
        }
        else
        {
            for (int k = 0; k < N; ++k) frame[(size_t) k] *= window[(size_t) k] * norm;
        }

        for (int k = 0; k < N; ++k)
            c.acc[(size_t) ((c.outPos + k) & mask)] += frame[(size_t) k];
    }

    double sr = 48000.0;
    int order = 11, N = 2048, H = 1024, hop = 512, lifterLen = 100;
    float ratio = 1.0f;
    std::unique_ptr<FFT> fft;
    std::vector<float> window, lifter, spec, tmp, frame, logMag, env, target;
    Chan ch[2];
};
} // namespace rmz
