#pragma once
#include "Parameters.h"

namespace rmz
{
struct PresetValue { const char* id; float value; };
struct Preset { const char* name; std::vector<PresetValue> values; };

inline const std::vector<Preset>& factoryPresets()
{
    using namespace pid;
    static const std::vector<Preset> list =
    {
        { "Init", {} },
        { "Sorry, Ruined Your Party", {
            { body, -1.0f }, { close, 64 }, { velvet, 58 }, { warmth, 1.5f }, { bite, 1.0f }, { air, 2.0f }, { phone, 26 },
            { drive, 32 }, { wobble, 20 }, { hiss, 14 }, { crunch, 8 },
            { twin, 30 }, { spread, 70 }, { echo, 14 }, { echoTime, 2 }, { echoFb, 30 },
            { haze, 18 }, { size, 35 }, { bloom, 45 }, { width, 112 }, { moodX, 38 }, { moodY, 58 } } },
        { "Blurry Glow", {
            { body, -1.5f }, { close, 56 }, { velvet, 70 }, { warmth, 1.0f }, { air, 4.0f }, { phone, 14 },
            { drive, 45 }, { wobble, 26 }, { hiss, 16 }, { crunch, 24 }, { gloss, 25 },
            { twin, 44 }, { spread, 90 }, { echo, 24 }, { echoTime, 3 }, { echoFb, 42 }, { echoTone, 40 },
            { haze, 36 }, { size, 64 }, { bloom, 72 }, { petals, 26 }, { width, 130 }, { moodX, 62 }, { moodY, 56 } } },
        { "Phone Memo", {
            { close, 72 }, { velvet, 62 }, { bite, 1.5f }, { air, -1.0f }, { phone, 80 },
            { drive, 30 }, { wobble, 22 }, { hiss, 32 }, { crunch, 32 }, { haze, 8 }, { width, 70 }, { moodX, 25 }, { moodY, 78 } } },
        { "Cherry Blossom", {
            { body, 2.0f }, { close, 40 }, { velvet, 45 }, { warmth, -0.5f }, { air, 6.5f },
            { drive, 18 }, { wobble, 12 }, { hiss, 6 }, { gloss, 40 },
            { twin, 38 }, { spread, 100 }, { echo, 20 }, { echoMode, 1 },
            { haze, 42 }, { size, 72 }, { bloom, 82 }, { petals, 45 }, { width, 140 }, { moodX, 70 }, { moodY, 30 } } },
        { "Tape Lullaby", {
            { close, 55 }, { velvet, 55 }, { warmth, 2.5f }, { air, -1.0f },
            { drive, 50 }, { wobble, 52 }, { hiss, 38 }, { crunch, 14 },
            { echo, 32 }, { echoTime, 3 }, { echoFb, 46 }, { echoTone, 32 }, { haze, 26 }, { size, 50 }, { moodX, 55 }, { moodY, 85 } } },
        { "Hyper Petal", {
            { body, 3.0f }, { close, 35 }, { velvet, 68 }, { air, 5.0f }, { bite, 2.0f },
            { drive, 30 }, { crunch, 28 }, { gloss, 70 },
            { twin, 55 }, { spread, 100 }, { drift, 22 }, { echo, 16 }, { echoMode, 1 },
            { haze, 30 }, { size, 55 }, { petals, 62 }, { width, 160 }, { moodX, 60 }, { moodY, 40 } } },
        { "Red Eye", {
            { body, -3.0f }, { close, 78 }, { velvet, 72 }, { warmth, 2.0f }, { bite, 1.0f }, { phone, 18 },
            { drive, 58 }, { wobble, 24 }, { hiss, 26 }, { crunch, 42 },
            { twin, 22 }, { echo, 18 }, { haze, 22 }, { size, 44 }, { width, 105 }, { moodX, 30 }, { moodY, 82 } } },
        { "Dry Velvet", {
            { close, 58 }, { velvet, 62 }, { warmth, 1.0f }, { air, 1.5f },
            { drive, 14 }, { wobble, 6 }, { hiss, 2 }, { haze, 6 }, { size, 25 }, { moodX, 30 }, { moodY, 35 } } },
        { "Blurry Sunday", {
            { close, 44 }, { velvet, 50 }, { air, 3.0f },
            { drive, 24 }, { wobble, 30 }, { hiss, 14 },
            { twin, 26 }, { echo, 28 }, { echoTime, 4 }, { echoFb, 48 },
            { haze, 56 }, { size, 82 }, { bloom, 64 }, { petals, 20 }, { width, 130 }, { moodX, 84 }, { moodY, 44 } } },
    };
    return list;
}

inline bool isGlobalId (const juce::String& id)
{
    return id == pid::inGain || id == pid::outGain || id == pid::mix || id == pid::bypass || id == pid::limiter;
}

inline void applyPreset (juce::AudioProcessorValueTreeState& apvts, const Preset& pr)
{
    for (auto* prm : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (! isGlobalId (r->paramID))
                r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& v : pr.values)
        if (auto* r = apvts.getParameter (v.id))
            r->setValueNotifyingHost (r->convertTo0to1 (v.value));
}
}
