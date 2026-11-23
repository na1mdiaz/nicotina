#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MutagenEngine.h"

// MUTAGEN 変異 · semilla como instrumento VST3: entra MIDI (tus acordes), sale el sinte.
class SemillaProcessor : public juce::AudioProcessor
{
public:
    SemillaProcessor();
    const juce::String getName() const override { return "MUTAGEN SEMILLA"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "MUTAGEN"; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // desde la interfaz
    void applyEngJson (const juce::String& json);     // genoma + mandos (lo que suena)
    void setSavedJson (const juce::String& json) { const juce::SpinLock::ScopedLockType l (savedLock); saved = json; }
    juce::String getSavedJson() { const juce::SpinLock::ScopedLockType l (savedLock); return saved; }
    void queueNote (int m, bool on, float v);
    void queueAllOff() { allOff = true; }

    struct Viz { double level = 0; std::vector<int> notes, played; int voicesOn = 0; bool hasKey = false; int keyK = 0; bool keyMinor = false; double bpm = 140; };
    Viz getViz() { const juce::SpinLock::ScopedLockType l (vizLock); return viz; }

private:
    mg::Engine engine;
    juce::SpinLock pendLock; mg::Genome pendG; mg::Params pendP; int pendSeqOn = 0, pendSeqProg = 0; bool dirty = true;
    juce::SpinLock savedLock; juce::String saved, engSaved;
    juce::SpinLock noteLock; std::vector<std::array<int, 3>> uiNotes; std::atomic<bool> allOff { false };
    juce::SpinLock vizLock; Viz viz;
    std::vector<float> zeros; struct Ev { int pos, on, m; float v; }; std::vector<Ev> evs;
    double sr = 48000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SemillaProcessor)
};
