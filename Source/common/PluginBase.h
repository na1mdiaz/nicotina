#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

// ---------------------------------------------------------------- lo que cada plugin aporta
struct ParamSpec { const char* id; const char* name; float min, max, def; bool integer; };

struct IEngine
{
    virtual ~IEngine() = default;
    virtual void prepare (double fs, int maxBlock) = 0;
    virtual void setParams (const float* p) = 0;              // valores reales, en el orden de la lista de parámetros
    virtual void setTempo (double bpm, bool playing) { juce::ignoreUnused (bpm, playing); }
    virtual void process (float* L, float* R, int N) = 0;     // en el sitio
    virtual int latency() const { return 0; }
    virtual double tail() const { return 4.0; }
    virtual void fillViz (juce::DynamicObject& o) = 0;        // hilo de mensajes: solo lee atómicos
};

struct PluginInfo
{
    const char* name;
    std::vector<ParamSpec> params;
    std::function<std::unique_ptr<IEngine>()> makeEngine;
    const char* html; int htmlSize;
    int baseW, baseH;
    juce::uint32 bg;
};
const PluginInfo& thisPlugin();   // lo define cada plugin

// ---------------------------------------------------------------- procesador
class PepelusProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    PepelusProcessor();
    ~PepelusProcessor() override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return thisPlugin().name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return engine->tail(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return thisPlugin().name; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void setParamFromUI (const juce::String& id, float value);
    juce::Array<int> takeDirtyParams();
    float paramValue (int i) const { return raw[(size_t) i]->load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<IEngine> engine;
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostPlaying { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged (const juce::String& id, float) override;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> pv;
    std::unique_ptr<std::atomic<bool>[]> dirty;
    bool applyingFromUI = false;
    juce::AudioBuffer<float> scratch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PepelusProcessor)
};

// ---------------------------------------------------------------- editor (interfaz web)
class PepelusEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PepelusEditor (PepelusProcessor&);
    ~PepelusEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    void sendState();
    PepelusProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;
    bool ready = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PepelusEditor)
};
