#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class SemillaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SemillaEditor (SemillaProcessor&);
    ~SemillaEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    juce::WebBrowserComponent::Options makeOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    SemillaProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;
    bool ready = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SemillaEditor)
};
