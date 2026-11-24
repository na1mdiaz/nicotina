// =====================================================================================
//  PEPELUS FIRE OPERATIONS · plugin (VST3 / AU)
//  El sonido de los efectos lo fabrica la interfaz (el mismo motor que la vista previa).
//  Aquí: deja pasar el audio de la pista, la graba cuando se lo pides, abre archivos,
//  escucha el clip por la salida del DAW y lo arrastra (o lo guarda) como .wav.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "FireUI.h"

namespace
{
    constexpr double kRecSeconds = 30.0;   // lo máximo que graba de la pista
    constexpr double kFileSeconds = 45.0;  // lo máximo que lee de un archivo

    juce::String toB64 (const void* d, size_t n) { return juce::Base64::toBase64 (d, n); }
    bool fromB64 (const juce::String& s, juce::MemoryBlock& out)
    {
        juce::MemoryOutputStream mo (out, false);
        return juce::Base64::convertFromBase64 (mo, s);
    }
    // audio -> base64 de int16 intercalado (es lo que entiende la interfaz)
    juce::String bufferToB64 (const juce::AudioBuffer<float>& b)
    {
        const int n = b.getNumSamples(), ch = b.getNumChannels();
        std::vector<int16_t> v ((size_t) n * 2);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                const float x = juce::jlimit (-1.f, 1.f, b.getSample (juce::jmin (c, ch - 1), i));
                v[(size_t) i * 2 + (size_t) c] = (int16_t) std::lround (x * 32767.f);
            }
        return toB64 (v.data(), v.size() * sizeof (int16_t));
    }
    juce::AudioBuffer<float> b64ToBuffer (const juce::String& s)
    {
        juce::MemoryBlock mb; fromB64 (s, mb);
        const int n = (int) (mb.getSize() / 4);
        juce::AudioBuffer<float> b (2, juce::jmax (1, n)); b.clear();
        auto* p = static_cast<const int16_t*> (mb.getData());
        for (int i = 0; i < n; ++i) { b.setSample (0, i, p[i * 2] / 32768.f); b.setSample (1, i, p[i * 2 + 1] / 32768.f); }
        return b;
    }
    juce::AudioBuffer<float> resampleTo (const juce::AudioBuffer<float>& in, double fromSr, double toSr)
    {
        if (fromSr <= 0 || toSr <= 0 || std::abs (fromSr - toSr) < 0.5) return in;
        const double ratio = fromSr / toSr;
        const int outN = juce::jmax (1, (int) std::floor (in.getNumSamples() / ratio) - 4);
        juce::AudioBuffer<float> out (2, outN);
        for (int c = 0; c < 2; ++c)
        {
            juce::LagrangeInterpolator li;
            li.process (ratio, in.getReadPointer (juce::jmin (c, in.getNumChannels() - 1)), out.getWritePointer (c), outN);
        }
        return out;
    }
}

// ================================================================ procesador
class FireProcessor : public juce::AudioProcessor
{
public:
    FireProcessor()
        : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        formats.registerBasicFormats();
    }

    const juce::String getName() const override { return "PEPELUS FIRE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "PEPELUS FIRE"; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
        if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
        return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
    }

    void prepareToPlay (double sampleRate, int) override
    {
        hostSr = sampleRate > 0 ? sampleRate : 48000.0;
        const juce::ScopedLock sl (recLock);
        recBuf.setSize (2, (int) (hostSr * kRecSeconds) + 16);
        recBuf.clear();
        recPos.store (0);
    }
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        const int N = buffer.getNumSamples(); if (N <= 0) return;
        const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
        for (int c = inCh; c < outCh; ++c) buffer.clear (c, 0, N);
        if (inCh == 1 && outCh > 1) buffer.copyFrom (1, 0, buffer, 0, 0, N);

        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm()) if (*b > 20 && *b < 400) hostBpm.store (*b);
                hostPlaying.store (pos->getIsPlaying());
            }

        const float* L = buffer.getReadPointer (0);
        const float* R = buffer.getReadPointer (outCh > 1 ? 1 : 0);
        float pk = 0.f; for (int i = 0; i < N; ++i) pk = juce::jmax (pk, std::abs (L[i]), std::abs (R[i]));
        inPeak.store (juce::jmax (pk, inPeak.load() * 0.9f));

        // grabar lo que suena en la pista
        if (recording.load())
        {
            const juce::ScopedTryLock tl (recLock);
            if (tl.isLocked())
            {
                const int pos = recPos.load(), cap = recBuf.getNumSamples(), n = juce::jmin (N, cap - pos);
                if (n > 0) { recBuf.copyFrom (0, pos, L, n); recBuf.copyFrom (1, pos, R, n); recPos.store (pos + n); }
                if (pos + n >= cap) { recording.store (false); recFull.store (true); }
            }
        }

        // escuchar el clip (o la palabra) por la salida del DAW, encima de lo que pasa por la pista
        const int which = playing.load();
        if (which != 0)
        {
            const juce::SpinLock::ScopedTryLockType tl (prevLock);
            if (tl.isLocked())
            {
                auto* src = which == 1 ? clipPrev.get() : selPrev.get();
                if (src == nullptr) playing.store (0);
                else
                {
                    const int len = src->getNumSamples(); int p = playPos;
                    for (int i = 0; i < N && p < len; ++i, ++p)
                        for (int c = 0; c < juce::jmin (2, outCh); ++c)
                            buffer.addSample (c, i, src->getSample (c, p) * 0.9f);
                    playPos = p;
                    if (p >= len) playing.store (0);
                }
            }
        }
        // red de seguridad: nunca un pitido en el DAW
        for (int c = 0; c < outCh; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < N; ++i) if (! std::isfinite (d[i])) { buffer.clear(); return; }
        }
    }

    void getStateInformation (juce::MemoryBlock& dest) override
    {
        juce::ValueTree t ("PEPELUSFIRE"); t.setProperty ("json", stateJson, nullptr);
        if (auto xml = t.createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size)) { auto t = juce::ValueTree::fromXml (*xml); if (t.isValid()) { stateJson = t["json"].toString(); stateDirty = true; } }
    }

    // ------------------------------------------------ desde la interfaz (hilo de mensajes)
    void startRec() { const juce::ScopedLock sl (recLock); recPos.store (0); recFull.store (false); recording.store (true); }
    juce::AudioBuffer<float> stopRec()
    {
        recording.store (false);
        const juce::ScopedLock sl (recLock);
        const int n = recPos.load();
        juce::AudioBuffer<float> out (2, juce::jmax (1, n));
        if (n > 0) { out.copyFrom (0, 0, recBuf, 0, 0, n); out.copyFrom (1, 0, recBuf, 1, 0, n); } else out.clear();
        return out;
    }
    void setPreview (bool isClip, juce::AudioBuffer<float> b, double sr)
    {
        auto nb = std::make_unique<juce::AudioBuffer<float>> (resampleTo (b, sr, hostSr));
        const juce::SpinLock::ScopedLockType sl (prevLock);
        if (playing.load() == (isClip ? 1 : 2)) playing.store (0);
        (isClip ? clipPrev : selPrev) = std::move (nb);
    }
    void play (bool isClip) { const juce::SpinLock::ScopedLockType sl (prevLock); playPos = 0; playing.store (isClip ? 1 : 2); }
    void stop() { playing.store (0); }

    juce::AudioFormatManager formats;
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<bool> hostPlaying { false }, recording { false }, recFull { false };
    std::atomic<int> recPos { 0 }, playing { 0 };
    std::atomic<float> inPeak { 0.f };
    double hostSr = 48000.0;
    juce::String stateJson; bool stateDirty = false;
    juce::MemoryBlock clipWav; juce::String clipName { "Pepelu_Fire.wav" };

private:
    juce::CriticalSection recLock;
    juce::AudioBuffer<float> recBuf;
    juce::SpinLock prevLock;
    std::unique_ptr<juce::AudioBuffer<float>> clipPrev, selPrev;
    int playPos = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FireProcessor)
};

// ================================================================ editor (la interfaz web de siempre)
class FireEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FireEditor (FireProcessor& p) : AudioProcessorEditor (p), proc (p)
    {
        using WBC = juce::WebBrowserComponent;
        setOpaque (true);
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("PEPELUS").getChildFile ("FIRE").getChildFile ("webview");
        auto opts = WBC::Options {}
            .withBackend (WBC::Options::Backend::webview2)
            .withWinWebView2Options (WBC::Options::WinWebView2 {}.withUserDataFolder (userData).withStatusBarDisabled().withBackgroundColour (juce::Colour (0xff050505)))
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withNativeIntegrationEnabled()
            .withResourceProvider ([] (const juce::String& url) -> std::optional<WBC::Resource>
            {
                const auto path = url.fromLastOccurrenceOf ("/", false, false);
                if (path.isEmpty() || path == "index.html")
                {
                    WBC::Resource r; r.data.resize ((size_t) FireUI::index_htmlSize); std::memcpy (r.data.data(), FireUI::index_html, (size_t) FireUI::index_htmlSize); r.mimeType = "text/html";
                    return r;
                }
                return std::nullopt;
            })
            .withEventListener ("ready", [this] (const juce::var&) { ready = true; sendState(); })
            .withEventListener ("state", [this] (const juce::var& v) { proc.stateJson = v["json"].toString(); })
            .withEventListener ("rec", [this] (const juce::var& v)
            {
                if ((bool) v["on"]) proc.startRec();
                else
                {
                    auto b = proc.stopRec();
                    if (b.getNumSamples() < (int) (proc.hostSr * 0.2)) { emit ("toast", "GRABACIÓN DEMASIADO CORTA: DALE A PLAY EN EL DAW"); return; }
                    sendSource (b, proc.hostSr, "GRABACIÓN DE LA PISTA");
                }
            })
            .withEventListener ("open", [this] (const juce::var&) { openFile(); })
            .withEventListener ("clip", [this] (const juce::var& v)
            {
                juce::MemoryBlock mb; if (! fromB64 (v["wav"].toString(), mb) || mb.getSize() < 44) return;
                proc.clipWav = mb; proc.clipName = juce::File::createLegalFileName (v["name"].toString().isEmpty() ? "Pepelu_Fire.wav" : v["name"].toString());
                juce::WavAudioFormat wf;
                std::unique_ptr<juce::AudioFormatReader> rd (wf.createReaderFor (new juce::MemoryInputStream (mb, false), true));
                if (rd != nullptr)
                {
                    juce::AudioBuffer<float> b (2, (int) rd->lengthInSamples); rd->read (&b, 0, (int) rd->lengthInSamples, 0, true, true);
                    proc.setPreview (true, std::move (b), rd->sampleRate);
                }
            })
            .withEventListener ("play", [this] (const juce::var& v)
            {
                if (v["kind"].toString() == "sel") { proc.setPreview (false, b64ToBuffer (v["data"].toString()), (double) v["sr"]); proc.play (false); }
                else proc.play (true);
            })
            .withEventListener ("stop", [this] (const juce::var&) { proc.stop(); })
            .withEventListener ("drag", [this] (const juce::var&)
            {
                auto f = writeClip(); if (! f.existsAsFile()) return;
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
            })
            .withEventListener ("save", [this] (const juce::var&)
            {
                auto f = writeClip(); if (! f.existsAsFile()) return;
                f.revealToUser(); emit ("toast", "GUARDADO: " + f.getFileName());
            });
        web = std::make_unique<WBC> (opts);
        addAndMakeVisible (*web);
        setResizable (true, true);
        setResizeLimits (760 * 6 / 10, 520 * 6 / 10, 760 * 2, 520 * 2);
        if (auto* c = getConstrainer()) c->setFixedAspectRatio (760.0 / 520.0);
        setSize (912, 624);
        web->goToURL (WBC::getResourceProviderRoot());
        startTimerHz (20);
    }
    ~FireEditor() override { stopTimer(); proc.stop(); }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff050505)); }
    void resized() override { if (web) web->setBounds (getLocalBounds()); }

private:
    void emit (const char* name, const juce::var& v) { if (web) web->emitEventIfBrowserIsVisible (name, v); }
    void sendState()
    {
        auto* o = new juce::DynamicObject(); o->setProperty ("json", proc.stateJson);
        emit ("state", juce::var (o)); proc.stateDirty = false;
    }
    void sendSource (const juce::AudioBuffer<float>& b, double sr, const juce::String& name)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("sr", sr); o->setProperty ("n", b.getNumSamples()); o->setProperty ("name", name); o->setProperty ("data", bufferToB64 (b));
        emit ("source", juce::var (o));
    }
    void openFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Elige una voz", juce::File::getSpecialLocation (juce::File::userMusicDirectory), proc.formats.getWildcardForAllFormats());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult(); if (! f.existsAsFile()) return;
            std::unique_ptr<juce::AudioFormatReader> rd (proc.formats.createReaderFor (f));
            if (! rd) { emit ("toast", "NO PUEDO LEER ESE ARCHIVO"); return; }
            const int n = (int) juce::jmin<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * kFileSeconds));
            juce::AudioBuffer<float> b (2, juce::jmax (1, n)); b.clear();
            rd->read (&b, 0, n, 0, true, true);
            if (rd->numChannels == 1) b.copyFrom (1, 0, b, 0, 0, n);
            sendSource (b, rd->sampleRate, f.getFileName());
        });
    }
    // cada arrastre crea un archivo nuevo: los clips que ya están en el DAW no cambian nunca
    juce::File writeClip()
    {
        if (proc.clipWav.getSize() < 44) { emit ("toast", "PRIMERO HAZ UN CLIP"); return {}; }
        auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("PEPELUS FIRE");
        dir.createDirectory();
        auto f = dir.getChildFile (proc.clipName).getNonexistentSibling (false);
        if (! f.replaceWithData (proc.clipWav.getData(), proc.clipWav.getSize())) { emit ("toast", "NO PUEDO GUARDAR EL CLIP"); return {}; }
        return f;
    }
    void timerCallback() override
    {
        if (! ready || ! web) return;
        if (proc.stateDirty) sendState();
        auto* v = new juce::DynamicObject();
        v->setProperty ("bpm", proc.hostBpm.load()); v->setProperty ("playing", proc.hostPlaying.load());
        v->setProperty ("rec", proc.recording.load()); v->setProperty ("recSec", proc.recPos.load() / proc.hostSr);
        v->setProperty ("in", proc.inPeak.load()); v->setProperty ("prev", proc.playing.load());
        emit ("viz", juce::var (v));
        if (proc.recFull.exchange (false)) { auto b = proc.stopRec(); sendSource (b, proc.hostSr, "GRABACIÓN DE LA PISTA"); }
    }

    FireProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;
    std::unique_ptr<juce::FileChooser> chooser;
    bool ready = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FireEditor)
};

juce::AudioProcessorEditor* FireProcessor::createEditor() { return new FireEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FireProcessor(); }
