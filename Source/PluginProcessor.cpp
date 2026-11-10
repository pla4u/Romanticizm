#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace rmz;

static juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Romanticizm").getChildFile ("settings.txt");
}

RomanticizmProcessor::RomanticizmProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ROMANTICIZM_STATE", createParameterLayout())
{
    pBypass = rawParam (pid::bypass); pLimiter = rawParam (pid::limiter);
    pEchoTime = rawParam (pid::echoTime); pEchoMode = rawParam (pid::echoMode);
    auto f = settingsFile();
    if (f.existsAsFile()) themeIndex = juce::jlimit (0, 99, f.loadFileAsString().trim().getIntValue());
}

bool RomanticizmProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RomanticizmProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    formant.prepare (sr); eq.prepare (sr); comp.prepare (sr); wobbleFx.prepare (sr); tapeFx.prepare (sr);
    saturator.prepare (sr, kChunk); crusher.prepare (sr); doubler.prepare (sr); delayFx.prepare (sr);
    reverbFx.prepare (sr); petalShift.prepare (sr); widthFx.prepare (sr); limiterFx.prepare (sr);

    dryDelay = formant.getLatency() + wobbleFx.getLatency() + saturator.getLatency();   // wet path, before the limiter
    latency = dryDelay + limiterFx.getLatency();
    for (auto& d : dryLine) d.prepare (dryDelay + 8);
    setLatencySamples (latency);

    sIn.init (rawParam (pid::inGain), sr);     sOut.init (rawParam (pid::outGain), sr);   sMix.init (rawParam (pid::mix), sr);
    sBody.init (rawParam (pid::body), sr, 0.05); sClose.init (rawParam (pid::close), sr);   sVelvet.init (rawParam (pid::velvet), sr);
    sWarmth.init (rawParam (pid::warmth), sr); sBite.init (rawParam (pid::bite), sr);     sAir.init (rawParam (pid::air), sr);
    sPhone.init (rawParam (pid::phone), sr);   sDrive.init (rawParam (pid::drive), sr);   sWobble.init (rawParam (pid::wobble), sr);
    sHiss.init (rawParam (pid::hiss), sr);     sCrunch.init (rawParam (pid::crunch), sr); sTwin.init (rawParam (pid::twin), sr);
    sSpread.init (rawParam (pid::spread), sr); sEcho.init (rawParam (pid::echo), sr);     sEchoFb.init (rawParam (pid::echoFb), sr);
    sEchoTone.init (rawParam (pid::echoTone), sr); sHaze.init (rawParam (pid::haze), sr); sSize.init (rawParam (pid::size), sr, 0.08);
    sHazeTone.init (rawParam (pid::hazeTone), sr); sBloom.init (rawParam (pid::bloom), sr); sPetals.init (rawParam (pid::petals), sr);
    sGloss.init (rawParam (pid::gloss), sr);   sWidth.init (rawParam (pid::width), sr);   sDrift.init (rawParam (pid::drift), sr, 0.08);
    sMoodX.init (rawParam (pid::moodX), sr, 0.12); sMoodY.init (rawParam (pid::moodY), sr, 0.12);

    prevIn = juce::Decibels::decibelsToGain (sIn.raw->load());
    prevOut = juce::Decibels::decibelsToGain (sOut.raw->load());
    prevWet = pBypass->load() > 0.5f ? 0.0f : sMix.raw->load() * 0.01f;
    prevDly = prevRev = 0.0f; lastPhone = -1.0f; duckEnv = 0.0f; petalLp[0] = petalLp[1] = 0.0f;
}

static inline void rampGain (float* l, float* r, int n, float g0, float g1) noexcept
{
    const float step = (g1 - g0) / (float) n;
    float g = g0;
    for (int i = 0; i < n; ++i) { g += step; l[i] *= g; r[i] *= g; }
}

void RomanticizmProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, numSamples);

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0) bpm = *b;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    for (int pos = 0; pos < numSamples; pos += kChunk)
        processChunk (L + pos, R + pos, std::min (kChunk, numSamples - pos), bpm);
}

void RomanticizmProcessor::processChunk (float* L, float* R, int n, double bpm)
{
    float inPk = 0.0f;
    for (int i = 0; i < n; ++i) inPk = std::max (inPk, std::max (std::abs (L[i]), std::abs (R[i])));
    if (inPk > inLevel.load()) inLevel.store (inPk);

    for (int i = 0; i < n; ++i)
    {
        dryL[(size_t) i] = dryLine[0].readInt (dryDelay);
        dryR[(size_t) i] = dryLine[1].readInt (dryDelay);
        dryLine[0].push (L[i]); dryLine[1].push (R[i]);
    }

    // ---- Mood macro: Distance (x) and Age (y) bend several sections at once
    const float dx = (sMoodX.next (n) - 50.0f) / 50.0f;      // -1 near ... +1 far
    const float dy = (sMoodY.next (n) - 50.0f) / 50.0f;      // -1 fresh ... +1 worn
    auto pc = [] (float v) { return clampf (v, 0.0f, 100.0f); };

    const float close  = pc (sClose.next (n) - dx * 30.0f);
    const float velvet = sVelvet.next (n);
    const float warmth = sWarmth.next (n), bite = sBite.next (n);
    const float air    = sAir.next (n) - 3.0f * std::max (0.0f, dy) + 1.5f * std::max (0.0f, -dy);
    const float phone  = pc (sPhone.next (n) + dy * 25.0f);
    const float drive  = pc (sDrive.next (n) + dy * 25.0f);
    const float wobble = pc (sWobble.next (n) + dy * 30.0f);
    const float hiss   = pc (sHiss.next (n) + dy * 35.0f);
    const float crunch = pc (sCrunch.next (n) + dy * 30.0f);
    const float haze   = pc (sHaze.next (n) + dx * 30.0f);
    const float echo   = pc (sEcho.next (n) + dx * 12.0f);
    const float age    = clampf (0.4f + 0.45f * dy, 0.0f, 1.0f);

    // ---- input
    const float inG = dbToLin (sIn.next (n));
    rampGain (L, R, n, prevIn, inG); prevIn = inG;

    // ---- Body (formants)
    formant.setShiftSemitones (sBody.next (n));
    formant.process (0, L, n); formant.process (1, R, n);

    // ---- Closeness + Lens EQ
    {
        VocalEQ::Params p;
        p.lowEnd  = close * 0.055f;
        p.low     = warmth + close * 0.018f;
        p.mid     = -close * 0.01f;
        p.midHigh = bite - warmth * 0.3f;
        p.midEnd  = bite * 0.6f - close * 0.012f;
        p.air     = air;
        p.harsh   = 0.30f + velvet * 0.004f;
        eq.process (L, R, n, p);
    }

    // ---- Phone lens
    if (phone > 0.5f || lastPhone > 0.5f)
    {
        if (std::abs (phone - lastPhone) > 0.3f)
        {
            const double k = phone * 0.01;
            phoneHp.c = BiquadCoef::highpass (sr, 40.0 + 340.0 * k, 0.7);
            phoneLp.c = BiquadCoef::lowpass (sr, 19000.0 * std::pow (0.34, k), 0.7);
            phonePk.c = BiquadCoef::peak (sr, 1700.0, 0.8, 4.5 * k);
            lastPhone = phone;
        }
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                float& x = (c == 0 ? L : R)[i];
                x = phoneLp.tick (c, phonePk.tick (c, phoneHp.tick (c, x)));
            }
    }

    // ---- Velvet (compression)
    comp.process (L, R, n, velvet * 0.0085f, 0.0f);

    // ---- Tape
    wobbleFx.process (L, R, n, wobble * 0.01f);
    tapeFx.process (L, R, n, age, hiss * 0.01f, std::max (0.0f, hiss * 0.01f - 0.45f) * 1.8f);
    {
        Saturator::Params sp;
        sp.sat = drive * 0.01f;
        sp.hyper = sGloss.next (n) * 0.01f;
        saturator.process (L, R, n, sp);
    }
    crusher.process (L, R, n, crunch * 0.01f);

    // ---- Twin
    {
        Doubler::Params p;
        p.mix = sTwin.next (n) * 0.01f;
        p.width = sSpread.next (n) * 0.01f;
        p.panAuto = 0.3f;
        p.detuneCents = sDrift.next (n);
        p.timeMs = 24.0f;
        p.type = 0;
        doubler.process (L, R, n, p);
    }

    // ---- Echo + Haze sends
    {
        static const float beats[6] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
        const int ti = juce::jlimit (0, 5, (int) (pEchoTime->load() + 0.5f));
        const float secs = juce::jlimit (0.01f, 9.5f, beats[ti] * 60.0f / (float) bpm);
        static const int modeMap[3] = { 2, 1, 0 };
        StereoDelay::Params dp;
        dp.timeSamples = secs * (float) sr;
        dp.feedback = sEchoFb.next (n) * 0.01f;
        dp.cutoff = sEchoTone.next (n) * 0.01f;
        dp.mode = modeMap[juce::jlimit (0, 2, (int) (pEchoMode->load() + 0.5f))];
        delayFx.process (L, R, wdL.data(), wdR.data(), n, dp);

        // Petals: an octave-up veil that only lives inside the reverb
        const float petals = sPetals.next (n) * 0.01f;
        for (int i = 0; i < n; ++i) { pgL[(size_t) i] = 0.5f * (L[i] + R[i]); pgR[(size_t) i] = pgL[(size_t) i]; }
        petalShift.process (pgL.data(), pgR.data(), n, 2.0f);
        float rinL[kChunk], rinR[kChunk];
        float envPk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            petalLp[0] += (pgL[(size_t) i] - petalLp[0]) * 0.45f;
            const float v = petalLp[0] * petals * 0.7f;
            rinL[i] = L[i] + v; rinR[i] = R[i] - v * 0.6f;
            envPk = std::max (envPk, std::abs (L[i]));
        }

        Reverb::Params rp;
        rp.decay = 0.18f + sSize.next (n) * 0.0078f;
        rp.tone = sHazeTone.next (n) * 0.01f;
        rp.preMs = 32.0f;
        reverbFx.process (rinL, rinR, wrL.data(), wrR.data(), n, rp);

        // Bloom: the haze swells into the gaps and ducks while the voice is singing
        const float coef = envPk > duckEnv ? 0.35f : 0.02f * ((float) n / 32.0f);
        duckEnv += (envPk - duckEnv) * coef;
        const float bloom = sBloom.next (n) * 0.01f;
        const float duck = 1.0f - bloom * 0.85f * clampf (duckEnv * 7.0f, 0.0f, 1.0f);

        const float dG = echo * 0.01f;
        const float rG = haze * 0.01f * 1.6f * duck;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            const float gd = prevDly + (dG - prevDly) * t, gr = prevRev + (rG - prevRev) * t;
            L[i] += wdL[(size_t) i] * gd + wrL[(size_t) i] * gr;
            R[i] += wdR[(size_t) i] * gd + wrR[(size_t) i] * gr;
        }
        prevDly = dG; prevRev = rG;
    }

    widthFx.process (L, R, n, sWidth.next (n) * 0.01f);

    // ---- dry/wet + bypass
    {
        const float wet = pBypass->load() > 0.5f ? 0.0f : sMix.next (n) * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            const float g = prevWet + (wet - prevWet) * t;
            L[i] = dryL[(size_t) i] + (L[i] - dryL[(size_t) i]) * g;
            R[i] = dryR[(size_t) i] + (R[i] - dryR[(size_t) i]) * g;
        }
        prevWet = wet;
    }

    limiterFx.process (L, R, n, pLimiter->load() > 0.5f);
    if (limiterFx.getGainReduction() > limGR.load()) limGR.store (limiterFx.getGainReduction());

    const float outG = dbToLin (sOut.next (n));
    rampGain (L, R, n, prevOut, outG); prevOut = outG;

    float outPk = 0.0f;
    int sp = scopePos.load();
    for (int i = 0; i < n; ++i)
    {
        outPk = std::max (outPk, std::max (std::abs (L[i]), std::abs (R[i])));
        scope[(size_t) sp] = 0.5f * (L[i] + R[i]);
        sp = (sp + 1) % scopeSize;
    }
    scopePos.store (sp);
    if (outPk > outLevel.load()) outLevel.store (outPk);
}

juce::AudioProcessorEditor* RomanticizmProcessor::createEditor() { return new RomanticizmEditor (*this); }

void RomanticizmProcessor::setCurrentProgram (int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    applyPreset (apvts, list[(size_t) index]);
    setPresetName (list[(size_t) index].name);
}

const juce::String RomanticizmProcessor::getProgramName (int index)
{
    const auto& list = factoryPresets();
    return (index >= 0 && index < (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
}

void RomanticizmProcessor::setThemeIndex (int i)
{
    themeIndex = i;
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::String (i));
}

juce::String RomanticizmProcessor::getPresetName() const { const juce::ScopedLock sl (presetLock); return presetName; }
void RomanticizmProcessor::setPresetName (const juce::String& n) { const juce::ScopedLock sl (presetLock); presetName = n; }

juce::File RomanticizmProcessor::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Romanticizm").getChildFile ("Presets");
}

bool RomanticizmProcessor::saveUserPreset (const juce::File& file)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        file.getParentDirectory().createDirectory();
        if (xml->writeTo (file)) { setPresetName (file.getFileNameWithoutExtension()); return true; }
    }
    return false;
}

bool RomanticizmProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            for (auto id : { pid::inGain, pid::outGain, pid::mix, pid::bypass, pid::limiter })
            {
                auto cur = apvts.getParameter (id);
                for (auto child : vt)
                    if (child.getProperty ("id").toString() == id)
                        child.setProperty ("value", cur->convertFrom0to1 (cur->getValue()), nullptr);
            }
            apvts.replaceState (vt);
            setPresetName (file.getFileNameWithoutExtension());
            return true;
        }
    return false;
}

void RomanticizmProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("theme", themeIndex.load(), nullptr);
    state.setProperty ("preset", getPresetName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void RomanticizmProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            if (vt.hasProperty ("theme"))  themeIndex = (int) vt.getProperty ("theme");
            if (vt.hasProperty ("preset")) setPresetName (vt.getProperty ("preset").toString());
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RomanticizmProcessor(); }
