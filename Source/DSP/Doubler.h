#pragma once
#include "Utils.h"

namespace rmz
{
/**
    Vocal doubler in the spirit of a classic "Doubler" plug-in:
    short delayed copies of the voice, each with its own slow pitch drift
    (modulated delay = detune), band-limited and panned around the stereo field.

      Dual      two voices, hard left / right
      Quad      four voices, two wide + two inner
      PingPong  three taps that bounce left / right / left
*/
class Doubler
{
public:
    struct Params
    {
        float mix = 0.0f;      // 0..1
        float width = 0.7f;    // 0..1
        float panAuto = 0.0f;  // 0..1
        float detuneCents = 12.0f;
        float timeMs = 24.0f;
        int   type = 0;        // 0 dual, 1 quad, 2 ping-pong
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        line.prepare ((int) (0.35 * sr));
        smoothCoef = tauToCoef (0.04, sr);
        const double rates[kVoices]  = { 0.31, 0.43, 0.27, 0.53 };
        for (int v = 0; v < kVoices; ++v)
        {
            auto& vo = voice[v];
            vo = Voice();
            vo.rateA = (float) rates[v];
            vo.rateB = (float) (rates[v] * 1.37);
            vo.phaseA = 0.17f + 0.23f * (float) v;
            vo.phaseB = 0.61f + 0.19f * (float) v;
            vo.panPhase = 0.25f * (float) v;
            vo.hp.c = BiquadCoef::highpass (sr, 140.0);
            vo.lp.c = BiquadCoef::lowpass (sr, 8500.0);
            vo.hp2.c = BiquadCoef::highpass (sr, 140.0);
            vo.lp2.c = BiquadCoef::lowpass (sr, 8500.0);
            vo.ratio = 1.0f; vo.pan = 0.0f; vo.level = 0.0f;
        }
        mixSm = 0.0f;
        reset();
    }

    void reset()
    {
        line.reset();
        for (auto& v : voice) { v.hp.reset(); v.lp.reset(); v.hp2.reset(); v.lp2.reset(); }
    }

    void process (float* l, float* r, int n, const Params& p) noexcept
    {
        // voice layout for the selected type
        static const float ratioT[3][kVoices]  = { { 1.00f, 1.31f, 1.0f, 1.0f },  { 1.00f, 1.27f, 0.79f, 1.58f }, { 1.0f, 2.0f, 3.0f, 3.0f } };
        static const float panT[3][kVoices]    = { { -1.0f, 1.0f, 0.0f, 0.0f },   { -1.0f, 1.0f, -0.42f, 0.42f }, { -1.0f, 1.0f, -0.5f, 0.0f } };
        static const float levelT[3][kVoices]  = { { 0.85f, 0.85f, 0.0f, 0.0f },  { 0.62f, 0.62f, 0.52f, 0.52f }, { 0.9f, 0.7f, 0.5f, 0.0f } };
        const int t = std::min (2, std::max (0, p.type));

        const float r2c = (float) (p.detuneCents * 0.000578);       // ratio deviation
        const float timeSamples = (float) (p.timeMs * 0.001 * sr);
        const float pi4 = (float) (kPi * 0.25);
        const float sqrt2 = 1.41421356f;

        for (int i = 0; i < n; ++i)
        {
            mixSm += (p.mix - mixSm) * smoothCoef;
            const float m = 0.5f * (l[i] + r[i]);
            float oL = 0.0f, oR = 0.0f;

            for (int v = 0; v < kVoices; ++v)
            {
                auto& vo = voice[v];
                vo.ratio += (ratioT[t][v] - vo.ratio) * smoothCoef;
                vo.pan   += (panT[t][v]   - vo.pan)   * smoothCoef;
                vo.level += (levelT[t][v] - vo.level) * smoothCoef;
                if (vo.level < 1.0e-4f && levelT[t][v] == 0.0f) continue;

                vo.dly += (timeSamples * vo.ratio - vo.dly) * smoothCoef;
                vo.phaseA += vo.rateA / (float) sr; if (vo.phaseA >= 1.0f) vo.phaseA -= 1.0f;
                vo.phaseB += vo.rateB / (float) sr; if (vo.phaseB >= 1.0f) vo.phaseB -= 1.0f;
                vo.panPhase += 0.23f / (float) sr; if (vo.panPhase >= 1.0f) vo.panPhase -= 1.0f;

                const float signV = (v & 1) ? -1.0f : 1.0f;
                const float depth = r2c * (float) sr / (float) (kTwoPi * vo.rateA);
                const float mod = depth * signV * (0.65f * std::sin ((float) kTwoPi * vo.phaseA)
                                                 + 0.35f * std::sin ((float) kTwoPi * vo.phaseB));
                float s = line.read (std::max (8.0f, vo.dly + mod));
                s = vo.lp.tick (0, vo.hp.tick (0, s));
                s = vo.lp2.tick (0, vo.hp2.tick (0, s));

                float pan = vo.pan * p.width + p.panAuto * std::sin ((float) kTwoPi * vo.panPhase + 1.9f * (float) v);
                pan = clampf (pan, -1.0f, 1.0f);
                const float ang = (pan + 1.0f) * pi4;
                const float g = vo.level * sqrt2;
                oL += s * std::cos (ang) * g;
                oR += s * std::sin (ang) * g;
            }
            line.push (m);
            l[i] += oL * mixSm;
            r[i] += oR * mixSm;
        }
    }

private:
    static constexpr int kVoices = 4;
    struct Voice
    {
        float rateA = 0.3f, rateB = 0.4f, phaseA = 0, phaseB = 0, panPhase = 0;
        float ratio = 1, pan = 0, level = 0, dly = 0;
        StereoBiquad hp, lp, hp2, lp2;
    };
    double sr = 48000.0;
    float smoothCoef = 0.001f, mixSm = 0.0f;
    DelayLine line;
    Voice voice[kVoices];
};
} // namespace rmz
