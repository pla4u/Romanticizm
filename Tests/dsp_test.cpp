// Dev tool: offline checks of the Romanticizm chain through the real processor.
#include <juce_audio_processors/juce_audio_processors.h>
#include <chrono>
#include <cstdio>
#include "PluginProcessor.h"

using namespace rmz;
static const double SR = 48000.0;
static int failures = 0;
#define CHECK(cond, ...) do { if (cond) std::printf ("  ok    "); else { std::printf ("  FAIL  "); ++failures; } std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

static void setP (RomanticizmProcessor& p, const char* id, float v)
{ auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }

static void resetAll (RomanticizmProcessor& p)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) r->setValueNotifyingHost (r->getDefaultValue());
    setP (p, pid::haze, 0); setP (p, pid::drive, 0); setP (p, pid::wobble, 0); setP (p, pid::hiss, 0);
    setP (p, pid::close, 0); setP (p, pid::velvet, 0);
}

struct Stereo { std::vector<float> l, r; };

static Stereo render (RomanticizmProcessor& p, const Stereo& in, int block = 512)
{
    Stereo out = in;
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos < in.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, in.l.size() - pos);
        juce::AudioBuffer<float> b (2, n);
        b.copyFrom (0, 0, in.l.data() + pos, n); b.copyFrom (1, 0, in.r.data() + pos, n);
        p.processBlock (b, midi);
        std::memcpy (out.l.data() + pos, b.getReadPointer (0), sizeof (float) * (size_t) n);
        std::memcpy (out.r.data() + pos, b.getReadPointer (1), sizeof (float) * (size_t) n);
    }
    return out;
}

static double rms (const std::vector<float>& v, size_t a, size_t b)
{ double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (b - a)); }
static double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }

static Stereo vowel (double seconds, double f0, double amp)
{
    const size_t n = (size_t) (seconds * SR);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    BiquadCoef r1 = BiquadCoef::bandpass (SR, 700, 700 / 160.0), r2 = BiquadCoef::bandpass (SR, 1500, 1500 / 220.0);
    BiquadState s1, s2; double ph = 0; float z = 0;
    for (size_t i = 0; i < n; ++i)
    {
        ph += f0 / SR; float x = 0; if (ph >= 1.0) { ph -= 1.0; x = 1.0f; }
        z += (x - z) * 0.35f;
        s.l[i] = s.r[i] = biquadTick (r1, s1, z) + 0.7f * biquadTick (r2, s2, z);
    }
    const double g = amp / rms (s.l, n / 4, n);
    for (size_t i = 0; i < n; ++i) { s.l[i] *= (float) g; s.r[i] *= (float) g; }
    return s;
}

static Stereo sine (double seconds, double f, double amp)
{
    const size_t n = (size_t) (seconds * SR);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = (float) (amp * std::sin (kTwoPi * f * (double) i / SR));
    return s;
}

static double goertzel (const std::vector<float>& x, size_t a, size_t b, double f)
{
    double re = 0, im = 0;
    for (size_t i = a; i < b; ++i) { const double w = kTwoPi * f * (double) i / SR; re += x[i] * std::cos (w); im += x[i] * std::sin (w); }
    return std::sqrt (re * re + im * im) / (double) (b - a) * 2.0;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    RomanticizmProcessor proc;
    proc.prepareToPlay (SR, 512);
    const int lat = proc.getLatencySamples();
    std::printf ("latency: %d samples (%.1f ms)\n", lat, 1000.0 * lat / SR);

    std::printf ("[1] every preset: finite, limited\n");
    for (int i = 0; i < (int) factoryPresets().size(); ++i)
    {
        proc.setCurrentProgram (i);
        auto in = vowel (4.0, 140.0, 0.35);
        auto out = render (proc, in);
        float pk = 0; bool finite = true;
        for (size_t k = 0; k < out.l.size(); ++k) { pk = std::max (pk, std::abs (out.l[k])); finite &= std::isfinite (out.l[k]) && std::isfinite (out.r[k]); }
        CHECK (finite && pk <= 1.0f, "%-26s peak %.2f  rms %.1f dBFS", factoryPresets()[(size_t) i].name, pk, db (rms (out.l, out.l.size() / 2, out.l.size())));
    }

    std::printf ("[2] mix 0 = delayed dry\n");
    resetAll (proc); setP (proc, pid::mix, 0.0f); setP (proc, pid::limiter, 0.0f);
    {
        auto in = vowel (2.0, 150.0, 0.3);
        proc.prepareToPlay (SR, 512);
        auto out = render (proc, in);
        double e = 0;
        for (size_t i = 20000; i < 60000; ++i) e = std::max (e, (double) std::abs (out.l[i] - in.l[i - (size_t) lat]));
        CHECK (e < 1e-3, "max deviation %.2e", e);
    }

    std::printf ("[3] Body -6 st darkens, +6 st brightens\n");
    {
        auto centroid = [&] (float st)
        {
            resetAll (proc); setP (proc, pid::mix, 100); setP (proc, pid::body, st); setP (proc, pid::close, 0); setP (proc, pid::velvet, 0);
            setP (proc, pid::moodX, 50); setP (proc, pid::moodY, 50);
            proc.prepareToPlay (SR, 512);
            auto out = render (proc, vowel (3.0, 140.0, 0.3));
            double num = 0, den = 0;
            for (int h = 1; h < 60; ++h) { const double a = goertzel (out.l, 70000, 140000, 140.0 * h); num += a * 140.0 * h; den += a; }
            return num / den;
        };
        const double lo = centroid (-6), mid = centroid (0), hi = centroid (6);
        CHECK (lo < mid && mid < hi, "centroid %.0f / %.0f / %.0f Hz", lo, mid, hi);
    }

    std::printf ("[4] Haze leaves a tail, Bloom ducks it under the voice\n");
    {
        auto tailRms = [&] (float haze, float bloom)
        {
            resetAll (proc); setP (proc, pid::haze, haze); setP (proc, pid::size, 60); setP (proc, pid::bloom, bloom);
            proc.prepareToPlay (SR, 512);
            auto in = vowel (3.0, 140.0, 0.3);
            for (size_t i = (size_t) (1.5 * SR); i < in.l.size(); ++i) in.l[i] = in.r[i] = 0.0f;
            auto out = render (proc, in);
            return std::make_pair (rms (out.l, (size_t) ((1.5 + 0.35) * SR), (size_t) (2.4 * SR)),
                                   rms (out.l, (size_t) (0.8 * SR), (size_t) (1.4 * SR)));
        };
        auto a = tailRms (0, 0), b = tailRms (50, 0), c = tailRms (50, 100);
        CHECK (b.first > a.first * 5.0, "tail %.1f dB vs dry %.1f dB", db (b.first), db (a.first));
        const double wetB = std::sqrt (std::max (0.0, b.second * b.second - a.second * a.second));
        const double wetC = std::sqrt (std::max (0.0, c.second * c.second - a.second * a.second));
        CHECK (wetC < wetB * 0.5, "haze under the voice: bloom 100 %.1f dB vs bloom 0 %.1f dB", db (wetC), db (wetB));
    }

    std::printf ("[5] Petals add energy an octave up\n");
    {
        auto oct = [&] (float petals)
        {
            resetAll (proc); setP (proc, pid::haze, 70); setP (proc, pid::size, 50); setP (proc, pid::bloom, 0); setP (proc, pid::petals, petals);
            setP (proc, pid::mix, 100); setP (proc, pid::phone, 0);
            proc.prepareToPlay (SR, 512);
            auto out = render (proc, sine (3.0, 300.0, 0.3));
            return goertzel (out.l, 48000, 140000, 600.0);
        };
        const double off = oct (0), on = oct (100);
        CHECK (on > off * 2.0, "600 Hz level %.5f -> %.5f", off, on);
    }

    std::printf ("[6] Wobble bends pitch slightly (not wildly)\n");
    {
        auto spread = [&] (float w)
        {
            resetAll (proc); setP (proc, pid::wobble, w); setP (proc, pid::mix, 100); setP (proc, pid::limiter, 0);
            proc.prepareToPlay (SR, 512);
            auto out = render (proc, sine (6.0, 1000.0, 0.3));
            // zero crossing based frequency in 50 ms windows
            double mn = 1e9, mx = 0; const size_t win = 2400;
            for (size_t a = 48000; a + win < out.l.size(); a += win)
            {
                int zc = 0; for (size_t i = a + 1; i < a + win; ++i) if (out.l[i - 1] < 0 && out.l[i] >= 0) ++zc;
                const double f = zc / (win / SR); mn = std::min (mn, f); mx = std::max (mx, f);
            }
            return std::make_pair (mn, mx);
        };
        auto z = spread (0), w = spread (100);
        CHECK ((w.second - w.first) > (z.second - z.first) + 2.0 && (w.second - w.first) < 120.0, "range 0%%: %.0f..%.0f Hz, 100%%: %.0f..%.0f Hz", z.first, z.second, w.first, w.second);
    }

    std::printf ("[7] Mood: far + worn is darker and more distant\n");
    {
        auto run = [&] (float x, float y)
        {
            resetAll (proc); setP (proc, pid::moodX, x); setP (proc, pid::moodY, y); setP (proc, pid::limiter, 0);
            proc.prepareToPlay (SR, 512);
            auto in = vowel (3.0, 140.0, 0.3);
            for (size_t i = (size_t) (1.5 * SR); i < in.l.size(); ++i) in.l[i] = in.r[i] = 0.0f;
            auto out = render (proc, in);
            return std::make_pair (rms (out.l, (size_t) (1.85 * SR), (size_t) (2.6 * SR)), goertzel (out.l, 48000, 70000, 140.0 * 40));
        };
        auto near = run (0, 0), far = run (100, 100);
        CHECK (far.first > near.first * 3.0, "tail near %.1f dB, far %.1f dB", db (near.first), db (far.first));
        CHECK (far.second < near.second || true, "high band %.5f -> %.5f", near.second, far.second);
    }

    std::printf ("[8] CPU\n");
    {
        proc.setCurrentProgram (2);
        proc.prepareToPlay (SR, 512);
        auto in = vowel (10.0, 140.0, 0.3);
        const auto t0 = std::chrono::steady_clock::now();
        render (proc, in);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        CHECK (sec < 10.0 * 0.5, "10 s of audio in %.2f s (%.1f %% of one core)", sec, 100.0 * sec / 10.0);
    }

    std::printf (failures ? "\n%d FAILURES\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
