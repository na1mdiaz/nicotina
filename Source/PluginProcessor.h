#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Presets.h"

namespace ParamID
{
    inline constexpr auto crush  = "crush";
    inline constexpr auto bits   = "bits";
    inline constexpr auto rate   = "rate";
    inline constexpr auto mix    = "mix";
    inline constexpr auto output = "output";
    inline constexpr auto mode   = "mode";
    inline constexpr auto bypass = "bypass";
}

class NicotinaProcessor : public juce::AudioProcessor
{
public:
    NicotinaProcessor();
    ~NicotinaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NICOTINA"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // los presets aparecen también en el menú de presets del DAW
    int getNumPrograms() override { return (int) kPresets.size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void loadPreset (int index);
    void setParam (const char* id, float value);

    juce::AudioProcessorValueTreeState apvts;
    int currentPreset = 0;

    // ajustes de la interfaz que se guardan con el proyecto
    int  getPalette() const;
    void setPalette (int p);
    bool getPower() const;
    void setPower (bool on);

    // valores para la interfaz
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<int>   transientCount { 0 };

private:
    struct ChannelState
    {
        float held = 0.0f, phase = 1.0f;
        float dcIn = 0.0f, dcOut = 0.0f;
        float lp1 = 0.0f, lp2 = 0.0f, hpIn = 0.0f, hpOut = 0.0f;
    };
    std::array<ChannelState, 8> channels {};
    juce::Random rng;

    juce::SmoothedValue<float> crushS, bitsS, rateS, mixS, gainS;
    std::atomic<float>* crushP = nullptr; std::atomic<float>* bitsP = nullptr; std::atomic<float>* rateP = nullptr;
    std::atomic<float>* mixP = nullptr; std::atomic<float>* outputP = nullptr; std::atomic<float>* modeP = nullptr;
    std::atomic<float>* bypassP = nullptr;

    double sr = 44100.0;
    float envFast = 0.0f, envSlow = 0.0f;
    float attackCoef = 0.0f, releaseFast = 0.0f, releaseSlow = 0.0f, attackSlow = 0.0f;
    int transientHold = 0, holdSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NicotinaProcessor)
};
