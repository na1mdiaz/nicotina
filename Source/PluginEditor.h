#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class MutagenEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MutagenEditor (MutagenProcessor&);
    ~MutagenEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::WebBrowserComponent::Options makeOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    void sendState();

    MutagenProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;
    bool ready = false;
    juce::String view = "cultivo";
    uint32_t geRead = 0;
    int tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutagenEditor)
};
