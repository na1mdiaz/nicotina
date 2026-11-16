#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class MutagenProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    MutagenProcessor();
    ~MutagenProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MUTAGEN"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "MUTAGEN"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- puente con la interfaz
    void setComplexJson (const juce::String& json);          // desde la interfaz (hilo de mensajes)
    juce::String getComplexJson() const { const juce::ScopedLock sl (jsonLock); return complexJson; }
    bool isFresh() const { return fresh; }
    void setParamFromUI (int index, float rawValue);
    juce::Array<int> takeDirtyParams();                       // cambios del host (automatización)

    juce::AudioProcessorValueTreeState apvts;
    mg::Engine engine;
    std::atomic<bool> hostPlaying { false };
    std::atomic<double> hostBpm { 0.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged (const juce::String& id, float) override;

    std::atomic<float>* raw[mg::NP] {};
    mg::Complex pending;
    bool hasPending = false;
    juce::SpinLock pendingLock;
    juce::CriticalSection jsonLock;
    juce::String complexJson;
    bool fresh = true;
    bool applyingFromUI = false;
    std::atomic<bool> dirty[mg::NP] {};
    juce::AudioBuffer<float> monoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutagenProcessor)
};
