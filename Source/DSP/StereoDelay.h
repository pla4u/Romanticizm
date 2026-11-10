#pragma once
#include "Utils.h"

namespace rmz
{
/**
    Tempo-synced stereo delay with three modes
      0 Stereo    two independent feedback lines
      1 PingPong  repeats bounce left <-> right
      2 Tape      stereo + wow/flutter, tanh saturation and a darker loop
    The feedback loop is high-passed (140 Hz) and low-passed (CUTOFF) so repeats
    sit behind the voice instead of piling up.
    process() only produces the wet signal.
*/
class StereoDelay
{
public:
    struct Params
    {
        float timeSamples = 12000.0f;
        float feedback = 0.35f;     // 0..0.95
        float cutoff = 0.7f;        // 0..1
        int   mode = 0;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxD = (int) (10.5 * sr);
        line[0].prepare (maxD);
        line[1].prepare (maxD);
        timeCoef = tauToCoef (0.09, sr);
        for (int c = 0; c < 2; ++c)
        {
            hp[c].c = BiquadCoef::highpass (sr, 140.0, 0.7);
            satLp[c].c = BiquadCoef::lowpass (sr, 5200.0, 0.7);
        }
        lastCut = -1.0f;
        timeCur = 0.0f;
        reset();
    }

    void reset()
    {
        line[0].reset(); line[1].reset();
        for (int c = 0; c < 2; ++c) { hp[c].reset(); lp[c].reset(); satLp[c].reset(); }
        wowPhase = flutPhase = 0.0f;
    }

    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n, const Params& p) noexcept
    {
        if (std::abs (p.cutoff - lastCut) > 0.002f)
        {
            const double f = 350.0 * std::pow (46.0, (double) p.cutoff);       // 350 Hz .. 16 kHz
            for (int c = 0; c < 2; ++c) lp[c].c = BiquadCoef::lowpass (sr, std::min (f, sr * 0.45), 0.7);
            lastCut = p.cutoff;
        }
        if (timeCur < 1.0f) timeCur = p.timeSamples;
        const float fb = clampf (p.feedback, 0.0f, 0.95f);
        const bool tape = p.mode == 2;
        const float wowDepth = (float) (0.0008 * sr), flutDepth = (float) (0.00005 * sr);

        for (int i = 0; i < n; ++i)
        {
            timeCur += (p.timeSamples - timeCur) * timeCoef;
            float mod = 0.0f;
            if (tape)
            {
                wowPhase  += 0.55f / (float) sr; if (wowPhase  >= 1.0f) wowPhase  -= 1.0f;
                flutPhase += 7.10f / (float) sr; if (flutPhase >= 1.0f) flutPhase -= 1.0f;
                mod = wowDepth * std::sin ((float) kTwoPi * wowPhase) + flutDepth * std::sin ((float) kTwoPi * flutPhase);
            }
            const float d = std::min (timeCur + mod, (float) line[0].size() - 16.0f);
            float dl = line[0].read (d);
            float dr = line[1].read (d);

            float fl = lp[0].tick (0, hp[0].tick (0, dl));
            float fr = lp[1].tick (0, hp[1].tick (0, dr));
            if (tape)
            {
                fl = satLp[0].tick (0, fastTanh (fl * 1.7f) * 0.62f);
                fr = satLp[1].tick (0, fastTanh (fr * 1.7f) * 0.62f);
            }
            else
            {
                fl = fastTanh (fl);
                fr = fastTanh (fr);
            }

            switch (p.mode)
            {
                case 1:  // ping-pong
                {
                    const float m = 0.5f * (inL[i] + inR[i]);
                    line[0].push (m + fb * fr);
                    line[1].push (fb * fl);
                    break;
                }
                case 2:  // tape
                    line[0].push (inL[i] + fb * (0.85f * fl + 0.15f * fr));
                    line[1].push (inR[i] + fb * (0.85f * fr + 0.15f * fl));
                    break;
                default: // stereo
                    line[0].push (inL[i] + fb * fl);
                    line[1].push (inR[i] + fb * fr);
                    break;
            }
            wetL[i] = tape ? fl * 1.6f : dl;
            wetR[i] = tape ? fr * 1.6f : dr;
            if (p.mode == 1) { wetL[i] = dl; wetR[i] = dr; }
        }
    }

private:
    double sr = 48000.0;
    DelayLine line[2];
    StereoBiquad hp[2], lp[2], satLp[2];
    float timeCoef = 0.001f, timeCur = 0.0f, lastCut = -1.0f, wowPhase = 0.0f, flutPhase = 0.0f;
};
} // namespace rmz
