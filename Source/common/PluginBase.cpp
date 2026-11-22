#include "PluginBase.h"

// ================================================================ procesador
juce::AudioProcessorValueTreeState::ParameterLayout PepelusProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& d : thisPlugin().params)
    {
        if (d.integer)
            layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { d.id, 1 }, juce::String (juce::CharPointer_UTF8 (d.name)), (int) d.min, (int) d.max, (int) d.def));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, juce::String (juce::CharPointer_UTF8 (d.name)), juce::NormalisableRange<float> (d.min, d.max), d.def));
    }
    return layout;
}

PepelusProcessor::PepelusProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PEPELUS", makeLayout()),
      engine (thisPlugin().makeEngine())
{
    const auto& ps = thisPlugin().params;
    dirty.reset (new std::atomic<bool>[ps.size()]);
    for (size_t i = 0; i < ps.size(); ++i) { raw.push_back (apvts.getRawParameterValue (ps[i].id)); apvts.addParameterListener (ps[i].id, this); dirty[i].store (false); }
    pv.resize (ps.size());
    engine->prepare (44100.0, 512);
}

PepelusProcessor::~PepelusProcessor() { for (auto& d : thisPlugin().params) apvts.removeParameterListener (d.id, this); }

bool PepelusProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void PepelusProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine->prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine->latency());
    scratch.setSize (2, juce::jmax (1, samplesPerBlock));
}

void PepelusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int N = buffer.getNumSamples(); if (N <= 0) return;
    for (size_t i = 0; i < raw.size(); ++i) pv[i] = raw[i]->load (std::memory_order_relaxed);
    engine->setParams (pv.data());
    double bpm = hostBpm.load(); bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) if (*b > 20 && *b < 400) bpm = *b;
            playing = pos->getIsPlaying();
        }
    hostBpm.store (bpm); hostPlaying.store (playing);
    engine->setTempo (bpm, playing);
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if (scratch.getNumSamples() < N) scratch.setSize (2, N, false, false, true);
    for (int c = inCh; c < outCh; ++c) buffer.clear (c, 0, N);
    float* L = buffer.getWritePointer (0);
    float* R = outCh > 1 ? buffer.getWritePointer (1) : scratch.getWritePointer (1);
    if (inCh < 2 || outCh < 2) juce::FloatVectorOperations::copy (R, L, N);
    // en trozos de como mucho 2048 (los motores tienen búferes fijos)
    for (int o = 0; o < N; o += 2048) engine->process (L + o, R + o, juce::jmin (2048, N - o));
    // red de seguridad: si algo saliera mal (NaN/inf), silencio y motor limpio, nunca un pitido en FL
    bool bad = false;
    for (int i = 0; i < N && ! bad; ++i) bad = ! std::isfinite (L[i]) || ! std::isfinite (R[i]) || std::abs (L[i]) > 8.f || std::abs (R[i]) > 8.f;
    if (bad) { buffer.clear(); engine->prepare (getSampleRate() > 0 ? getSampleRate() : 48000.0, juce::jmax (N, 512)); }
}

void PepelusProcessor::setParamFromUI (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float nv = p->convertTo0to1 (value);
        if (std::abs (p->getValue() - nv) < 1e-7f) return;
        applyingFromUI = true;
        p->beginChangeGesture(); p->setValueNotifyingHost (nv); p->endChangeGesture();
        applyingFromUI = false;
    }
}

void PepelusProcessor::parameterChanged (const juce::String& id, float)
{
    if (applyingFromUI) return;
    const auto& ps = thisPlugin().params;
    for (size_t i = 0; i < ps.size(); ++i) if (id == ps[i].id) dirty[i].store (true);
}

juce::Array<int> PepelusProcessor::takeDirtyParams()
{
    juce::Array<int> out;
    for (size_t i = 0; i < thisPlugin().params.size(); ++i) if (dirty[i].exchange (false)) out.add ((int) i);
    return out;
}

void PepelusProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PepelusProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        apvts.replaceState (tree);
        for (size_t i = 0; i < thisPlugin().params.size(); ++i) dirty[i].store (true);
    }
}

juce::AudioProcessorEditor* PepelusProcessor::createEditor() { return new PepelusEditor (*this); }

// ================================================================ editor
PepelusEditor::PepelusEditor (PepelusProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    using WBC = juce::WebBrowserComponent;
    const auto& info = thisPlugin();
    setOpaque (true);
    auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("PEPELUS").getChildFile (info.name).getChildFile ("webview");
    auto opts = WBC::Options {}
        .withBackend (WBC::Options::Backend::webview2)
        .withWinWebView2Options (WBC::Options::WinWebView2 {}.withUserDataFolder (userData).withStatusBarDisabled().withBackgroundColour (juce::Colour (info.bg)))
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withResourceProvider ([] (const juce::String& url) -> std::optional<WBC::Resource>
        {
            const auto path = url.fromLastOccurrenceOf ("/", false, false);
            if (path.isEmpty() || path == "index.html")
            {
                const auto& in = thisPlugin();
                WBC::Resource r; r.data.resize ((size_t) in.htmlSize); std::memcpy (r.data.data(), in.html, (size_t) in.htmlSize); r.mimeType = "text/html";
                return r;
            }
            return std::nullopt;
        })
        .withEventListener ("ready", [this] (const juce::var&) { ready = true; sendState(); })
        .withEventListener ("setParams", [this] (const juce::var& v)
        {
            if (auto* o = v.getDynamicObject())
                for (auto& prop : o->getProperties()) proc.setParamFromUI (prop.name.toString(), (float) (double) prop.value);
        });
    web = std::make_unique<WBC> (opts);
    addAndMakeVisible (*web);
    setResizable (true, true);
    setResizeLimits (info.baseW * 6 / 10, info.baseH * 6 / 10, info.baseW * 2, info.baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) info.baseW / info.baseH);
    setSize (info.baseW, info.baseH);
    web->goToURL (WBC::getResourceProviderRoot());
    startTimerHz (30);
}

PepelusEditor::~PepelusEditor() { stopTimer(); }
void PepelusEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (thisPlugin().bg)); }
void PepelusEditor::resized() { if (web) web->setBounds (getLocalBounds()); }

void PepelusEditor::sendState()
{
    auto* o = new juce::DynamicObject();
    const auto& ps = thisPlugin().params;
    for (size_t i = 0; i < ps.size(); ++i) o->setProperty (ps[i].id, (double) proc.paramValue ((int) i));
    proc.takeDirtyParams();
    web->emitEventIfBrowserIsVisible ("params", juce::var (o));
}

void PepelusEditor::timerCallback()
{
    if (! ready || ! web) return;
    const auto dirty = proc.takeDirtyParams();
    if (! dirty.isEmpty())
    {
        auto* o = new juce::DynamicObject();
        const auto& ps = thisPlugin().params;
        for (int i : dirty) o->setProperty (ps[(size_t) i].id, (double) proc.paramValue (i));
        web->emitEventIfBrowserIsVisible ("params", juce::var (o));
    }
    auto* v = new juce::DynamicObject();
    proc.engine->fillViz (*v);
    v->setProperty ("bpm", proc.hostBpm.load()); v->setProperty ("playing", proc.hostPlaying.load());
    web->emitEventIfBrowserIsVisible ("viz", juce::var (v));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PepelusProcessor(); }
