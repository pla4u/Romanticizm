#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace rmz
{
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

inline float dbToLin (float db) noexcept   { return std::exp (db * 0.11512925464970229f); }
inline float linToDb (float x) noexcept    { return 8.685889638065035f * std::log (std::max (x, 1.0e-9f)); }
inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf (float a, float b, float t) noexcept   { return a + (b - a) * t; }

/** Fast, accurate tanh (Lambert continued fraction), exact enough for audio waveshaping. */
inline float fastTanh (float x) noexcept
{
    if (x > 4.97f)  return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

/** One-pole smoothing coefficient for a time constant in seconds. */
inline float tauToCoef (double tauSeconds, double sampleRate) noexcept
{
    if (tauSeconds <= 0.0) return 1.0f;
    return (float) (1.0 - std::exp (-1.0 / (tauSeconds * sampleRate)));
}

// ============================================================================
//  Biquad (RBJ cookbook), double precision state
// ============================================================================
struct BiquadCoef
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static BiquadCoef make (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        BiquadCoef c;
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0;
        c.a1 = a1 / a0; c.a2 = a2 / a0;
        return c;
    }

    static double clampFreq (double fs, double f) noexcept { return std::min (std::max (f, 10.0), fs * 0.49); }

    static BiquadCoef lowpass (double fs, double f, double q = 0.70710678) noexcept
    {
        const double w = kTwoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        return make ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoef highpass (double fs, double f, double q = 0.70710678) noexcept
    {
        const double w = kTwoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        return make ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoef bandpass (double fs, double f, double q) noexcept
    {
        const double w = kTwoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        return make (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoef allpass (double fs, double f, double q) noexcept
    {
        const double w = kTwoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        return make (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoef peak (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = kTwoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        return make (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    static BiquadCoef lowShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        const double w = kTwoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        return make (A * ((A + 1) - (A - 1) * c + 2 * sA * al),
                     2 * A * ((A - 1) - (A + 1) * c),
                     A * ((A + 1) - (A - 1) * c - 2 * sA * al),
                     (A + 1) + (A - 1) * c + 2 * sA * al,
                     -2 * ((A - 1) + (A + 1) * c),
                     (A + 1) + (A - 1) * c - 2 * sA * al);
    }
    static BiquadCoef highShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        const double w = kTwoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        return make (A * ((A + 1) + (A - 1) * c + 2 * sA * al),
                     -2 * A * ((A - 1) + (A + 1) * c),
                     A * ((A + 1) + (A - 1) * c - 2 * sA * al),
                     (A + 1) - (A - 1) * c + 2 * sA * al,
                     2 * ((A - 1) - (A + 1) * c),
                     (A + 1) - (A - 1) * c - 2 * sA * al);
    }
};

struct BiquadState
{
    double z1 = 0, z2 = 0;
    void reset() noexcept { z1 = z2 = 0; }
};

inline float biquadTick (const BiquadCoef& c, BiquadState& s, float x) noexcept
{
    const double in  = (double) x;
    const double out = c.b0 * in + s.z1;
    s.z1 = c.b1 * in - c.a1 * out + s.z2;
    s.z2 = c.b2 * in - c.a2 * out;
    return (float) out;
}

/** Biquad with independent state for L/R sharing one coefficient set. */
struct StereoBiquad
{
    BiquadCoef c;
    BiquadState s[2];
    void reset() noexcept { s[0].reset(); s[1].reset(); }
    float tick (int ch, float x) noexcept { return biquadTick (c, s[ch], x); }
};

// ============================================================================
//  Delay line with Hermite interpolation. Semantics: read(d) before push(x[n])
//  returns x[n-d].
// ============================================================================
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int sz = 8;
        while (sz < maxDelaySamples + 8) sz <<= 1;
        buf.assign ((size_t) sz, 0.0f);
        mask = sz - 1;
        w = 0;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    float readInt (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }

    float read (float d) const noexcept
    {
        d = std::max (d, 3.0f);
        const float p = (float) w - d;
        const int   i = (int) std::floor (p);
        const float f = p - (float) i;
        const float y0 = buf[(size_t) ((i - 1) & mask)];
        const float y1 = buf[(size_t) ((i)     & mask)];
        const float y2 = buf[(size_t) ((i + 1) & mask)];
        const float y3 = buf[(size_t) ((i + 2) & mask)];
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
    int size() const noexcept { return (int) buf.size(); }

private:
    std::vector<float> buf;
    int mask = 7, w = 0;
};

/** Schroeder all-pass:  H(z) = (-g + z^-N) / (1 - g z^-N) */
class Allpass
{
public:
    void prepare (int len, float gain)
    {
        buf.assign ((size_t) std::max (1, len), 0.0f);
        pos = 0; g = gain;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    float tick (float x) noexcept
    {
        const float d = buf[(size_t) pos];
        const float v = x + g * d;
        buf[(size_t) pos] = v;
        if (++pos >= (int) buf.size()) pos = 0;
        return d - g * v;
    }
private:
    std::vector<float> buf;
    int pos = 0;
    float g = 0.5f;
};

// ============================================================================
//  Radix-2 complex FFT (unscaled both directions), platform independent.
// ============================================================================
class FFT
{
public:
    explicit FFT (int order) : n (1 << order)
    {
        tw.resize ((size_t) n);           // interleaved cos / -sin
        for (int k = 0; k < n / 2; ++k)
        {
            tw[(size_t) (2 * k)]     = (float) std::cos (kTwoPi * k / n);
            tw[(size_t) (2 * k + 1)] = (float) -std::sin (kTwoPi * k / n);
        }
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b) r |= ((i >> b) & 1) << (order - 1 - b);
            rev[(size_t) i] = r;
        }
    }

    int size() const noexcept { return n; }

    /** data is interleaved re,im (2*n floats). */
    void transform (float* d, bool inverse) const noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (i < j) { std::swap (d[2 * i], d[2 * j]); std::swap (d[2 * i + 1], d[2 * j + 1]); }
        }
        const float sgn = inverse ? -1.0f : 1.0f;
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int j = 0; j < half; ++j)
                {
                    const float wr = tw[(size_t) (2 * j * step)];
                    const float wi = sgn * tw[(size_t) (2 * j * step + 1)];
                    float* a = d + 2 * (i + j);
                    float* b = d + 2 * (i + j + half);
                    const float tr = b[0] * wr - b[1] * wi;
                    const float ti = b[0] * wi + b[1] * wr;
                    b[0] = a[0] - tr; b[1] = a[1] - ti;
                    a[0] += tr;       a[1] += ti;
                }
            }
        }
    }

private:
    int n;
    std::vector<float> tw;
    std::vector<int>   rev;
};

} // namespace rmz
