#pragma once
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "Presets.h"
#include "DSP/Compressor.h"
#include "DSP/Doubler.h"
#include "DSP/Formant.h"
#include "DSP/Limiter.h"
#include "DSP/PitchShift.h"
#include "DSP/Reverb.h"
#include "DSP/Saturator.h"
#include "DSP/StereoDelay.h"
#include "DSP/Tape.h"
#include "DSP/VocalEQ.h"
#include "DSP/Width.h"

class RomanticizmProcessor : public juce::AudioProcessor
{
public:
    RomanticizmProcessor();
    ~RomanticizmProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Romanticizm"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return (int) rmz::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    int  getThemeIndex() const noexcept { return themeIndex.load(); }
    void setThemeIndex (int i);
    juce::String getPresetName() const;
    void setPresetName (const juce::String& n);
    static juce::File getUserPresetDirectory();
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, limGR { 0.0f };
    static constexpr int scopeSize = 4096;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopePos { 0 };

private:
    static constexpr int kChunk = 32;

    struct Sm
    {
        std::atomic<float>* raw = nullptr;
        juce::SmoothedValue<float> sv;
        void init (std::atomic<float>* r, double sr, double seconds = 0.03)
        { raw = r; sv.reset (sr, seconds); sv.setCurrentAndTargetValue (raw->load()); }
        float next (int n) { sv.setTargetValue (raw->load()); return sv.skip (n); }
    };

    void processChunk (float* L, float* R, int n, double bpm);
    std::atomic<float>* rawParam (const char* id) { return apvts.getRawParameterValue (id); }

    Sm sIn, sOut, sMix, sBody, sClose, sVelvet, sWarmth, sBite, sAir, sPhone, sDrive, sWobble, sHiss, sCrunch,
       sTwin, sSpread, sEcho, sEchoFb, sEchoTone, sHaze, sSize, sHazeTone, sBloom, sPetals, sGloss, sWidth, sDrift,
       sMoodX, sMoodY;
    std::atomic<float> *pBypass, *pLimiter, *pEchoTime, *pEchoMode;

    rmz::FormantShifter formant;
    rmz::VocalEQ eq;
    rmz::Compressor comp;
    rmz::TapeWobble wobbleFx;
    rmz::TapeColour tapeFx;
    rmz::Saturator saturator;
    rmz::Crusher crusher;
    rmz::Doubler doubler;
    rmz::StereoDelay delayFx;
    rmz::Reverb reverbFx;
    rmz::PitchShifter petalShift;
    rmz::Width widthFx;
    rmz::Limiter limiterFx;
    rmz::DelayLine dryLine[2];
    rmz::StereoBiquad phoneHp, phoneLp, phonePk;
    float lastPhone = -1.0f, petalLp[2] { 0, 0 }, duckEnv = 0.0f;

    double sr = 48000.0;
    int latency = 0, dryDelay = 0;
    float prevIn = 1.0f, prevOut = 1.0f, prevWet = 1.0f, prevDly = 0.0f, prevRev = 0.0f;
    std::array<float, kChunk> dryL {}, dryR {}, wdL {}, wdR {}, wrL {}, wrR {}, pgL {}, pgR {};

    std::atomic<int> themeIndex { 0 };
    int currentProgram = 0;
    juce::String presetName { "Init" };
    mutable juce::CriticalSection presetLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomanticizmProcessor)
};
