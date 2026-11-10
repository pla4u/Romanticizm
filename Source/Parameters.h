#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace rmz
{
namespace pid
{
#define RMZ_ID(n) inline constexpr const char* n = #n;
    RMZ_ID (inGain) RMZ_ID (outGain) RMZ_ID (mix) RMZ_ID (bypass) RMZ_ID (limiter)
    // voice
    RMZ_ID (body) RMZ_ID (close) RMZ_ID (velvet)
    RMZ_ID (warmth) RMZ_ID (bite) RMZ_ID (air) RMZ_ID (phone)
    // tape
    RMZ_ID (drive) RMZ_ID (wobble) RMZ_ID (hiss) RMZ_ID (crunch)
    // echoes
    RMZ_ID (twin) RMZ_ID (spread) RMZ_ID (echo) RMZ_ID (echoTime) RMZ_ID (echoFb) RMZ_ID (echoTone) RMZ_ID (echoMode)
    // haze
    RMZ_ID (haze) RMZ_ID (size) RMZ_ID (hazeTone) RMZ_ID (bloom) RMZ_ID (petals)
    // gloss
    RMZ_ID (gloss) RMZ_ID (width) RMZ_ID (drift)
    // mood macro
    RMZ_ID (moodX) RMZ_ID (moodY)
#undef RMZ_ID
}

inline const juce::StringArray& echoTimeNames() { static const juce::StringArray s { "1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2" }; return s; }
inline const juce::StringArray& echoModeNames() { static const juce::StringArray s { "Tape", "Ping-Pong", "Stereo" }; return s; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto fmtDb  = [] (float v, int) { return String (v, 1) + " dB"; };
    auto fmtPct = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto fmtSt  = [] (float v, int) { return String (v, 1) + " st"; };
    auto fmtCt  = [] (float v, int) { return String (roundToInt (v)) + " ct"; };
    auto F = [&] (const char* id, const char* name, float lo, float hi, float def, std::function<String (float, int)> fmt, float step = 0.01f)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def, Attr().withStringFromValueFunction (fmt))); };
    auto B = [&] (const char* id, const char* name, bool def) { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto C = [&] (const char* id, const char* name, const StringArray& items, int def) { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    F (pid::inGain, "Input", -24.0f, 24.0f, 0.0f, fmtDb, 0.1f);
    F (pid::outGain, "Output", -24.0f, 12.0f, 0.0f, fmtDb, 0.1f);
    F (pid::mix, "Mix", 0.0f, 100.0f, 100.0f, fmtPct, 1.0f);
    B (pid::bypass, "Bypass", false);
    B (pid::limiter, "Limiter", true);

    F (pid::body, "Body", -12.0f, 12.0f, 0.0f, fmtSt, 0.1f);
    F (pid::close, "Closeness", 0.0f, 100.0f, 45.0f, fmtPct, 1.0f);
    F (pid::velvet, "Velvet", 0.0f, 100.0f, 40.0f, fmtPct, 1.0f);
    F (pid::warmth, "Warmth", -6.0f, 6.0f, 0.0f, fmtDb, 0.1f);
    F (pid::bite, "Bite", -6.0f, 6.0f, 0.0f, fmtDb, 0.1f);
    F (pid::air, "Air", -6.0f, 9.0f, 0.0f, fmtDb, 0.1f);
    F (pid::phone, "Phone", 0.0f, 100.0f, 0.0f, fmtPct, 1.0f);

    F (pid::drive, "Tape Drive", 0.0f, 100.0f, 25.0f, fmtPct, 1.0f);
    F (pid::wobble, "Wobble", 0.0f, 100.0f, 15.0f, fmtPct, 1.0f);
    F (pid::hiss, "Hiss", 0.0f, 100.0f, 10.0f, fmtPct, 1.0f);
    F (pid::crunch, "Crunch", 0.0f, 100.0f, 0.0f, fmtPct, 1.0f);

    F (pid::twin, "Twin", 0.0f, 100.0f, 0.0f, fmtPct, 1.0f);
    F (pid::spread, "Spread", 0.0f, 100.0f, 70.0f, fmtPct, 1.0f);
    F (pid::echo, "Echo", 0.0f, 100.0f, 0.0f, fmtPct, 1.0f);
    C (pid::echoTime, "Echo Time", echoTimeNames(), 2);
    F (pid::echoFb, "Echo Feedback", 0.0f, 95.0f, 35.0f, fmtPct, 1.0f);
    F (pid::echoTone, "Echo Tone", 0.0f, 100.0f, 45.0f, fmtPct, 1.0f);
    C (pid::echoMode, "Echo Mode", echoModeNames(), 0);

    F (pid::haze, "Haze", 0.0f, 100.0f, 15.0f, fmtPct, 1.0f);
    F (pid::size, "Size", 0.0f, 100.0f, 40.0f, fmtPct, 1.0f);
    F (pid::hazeTone, "Haze Tone", 0.0f, 100.0f, 45.0f, fmtPct, 1.0f);
    F (pid::bloom, "Bloom", 0.0f, 100.0f, 40.0f, fmtPct, 1.0f);
    F (pid::petals, "Petals", 0.0f, 100.0f, 0.0f, fmtPct, 1.0f);

    F (pid::gloss, "Gloss", 0.0f, 100.0f, 0.0f, fmtPct, 1.0f);
    F (pid::width, "Width", 0.0f, 200.0f, 100.0f, fmtPct, 1.0f);
    F (pid::drift, "Drift", 0.0f, 40.0f, 12.0f, fmtCt, 1.0f);

    F (pid::moodX, "Mood Distance", 0.0f, 100.0f, 50.0f, fmtPct, 1.0f);
    F (pid::moodY, "Mood Age", 0.0f, 100.0f, 50.0f, fmtPct, 1.0f);
    return { p.begin(), p.end() };
}
}
