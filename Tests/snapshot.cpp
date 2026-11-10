// Dev tool: renders the editor in every theme to PNG files (run under xvfb-run).
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

static void feed (RomanticizmProcessor& p, int blocks)
{
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    static double phase = 0.0, lfo = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            double s = 0.0;
            for (int h = 1; h < 30; ++h)
            {
                const double f = 170.0 * h;
                s += (std::exp (-std::pow (std::log (f / 800.0), 2.0) * 1.2) + 0.5 * std::exp (-std::pow (std::log (f / 2600.0), 2.0) * 2.0)) * std::sin (phase * h) / h * 1.6;
            }
            phase += 2.0 * juce::MathConstants<double>::pi * 170.0 / 48000.0;
            lfo += 2.0 * juce::MathConstants<double>::pi * 1.3 / 48000.0;
            const double env = 0.55 + 0.45 * std::sin (lfo);
            buf.setSample (0, i, (float) (0.3 * s * env)); buf.setSample (1, i, (float) (0.3 * s * env));
        }
        p.processBlock (buf, midi);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? argv[1] : ".";
    const int only = argc > 2 ? juce::String (argv[2]).getIntValue() : -1;
    RomanticizmProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    proc.setCurrentProgram (1);
    feed (proc, 300);
    for (int t = 0; t < (int) rmz::themes().size(); ++t)
    {
        if (only >= 0 && t != only) continue;
        proc.setThemeIndex (t);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (RomanticizmEditor::baseW, RomanticizmEditor::baseH);
        for (int k = 0; k < 8; ++k) { feed (proc, 20); juce::MessageManager::getInstance()->runDispatchLoopUntil (70); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (outDir + "/theme_" + juce::String (t) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
    }
    return 0;
}
