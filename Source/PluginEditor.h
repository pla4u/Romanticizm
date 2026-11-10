#pragma once
#include "PluginProcessor.h"
#include "UI/UI.h"

class RomanticizmEditor : public juce::AudioProcessorEditor
{
public:
    explicit RomanticizmEditor (RomanticizmProcessor&);
    ~RomanticizmEditor() override;
    void resized() override;
    void setTheme (int index);

    static constexpr int baseW = 1060, baseH = 640;

private:
    class Content;
    RomanticizmProcessor& proc;
    rmz::RmzLookAndFeel lf;
    juce::Component root;
    std::unique_ptr<juce::Component> backdrop, overlay;
    std::unique_ptr<Content> content;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomanticizmEditor)
};
