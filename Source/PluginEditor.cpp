#include "PluginEditor.h"
#include "StateIO.h"
#include "BinaryData.h"

using namespace mg;

static constexpr int BASE_W = 1200, BASE_H = 780;

MutagenEditor::MutagenEditor (MutagenProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setOpaque (true);
    web = std::make_unique<juce::WebBrowserComponent> (makeOptions());
    addAndMakeVisible (*web);
    setResizable (true, true);
    setResizeLimits (BASE_W * 6 / 10, BASE_H * 6 / 10, BASE_W * 16 / 10, BASE_H * 16 / 10);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) BASE_W / BASE_H);
    setSize (BASE_W, BASE_H);
    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    geRead = proc.engine.viz.geW.load();
    startTimerHz (30);
}

MutagenEditor::~MutagenEditor() { stopTimer(); }

void MutagenEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff010204)); }
void MutagenEditor::resized() { if (web) web->setBounds (getLocalBounds()); }

juce::WebBrowserComponent::Options MutagenEditor::makeOptions()
{
    using WBC = juce::WebBrowserComponent;
    auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("MUTAGEN").getChildFile ("webview");
    return WBC::Options {}
        .withBackend (WBC::Options::Backend::webview2)
        .withWinWebView2Options (WBC::Options::WinWebView2 {}
                                     .withUserDataFolder (userData)
                                     .withStatusBarDisabled()
                                     .withBackgroundColour (juce::Colour (0xff010204)))
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        .withEventListener ("ready", [this] (const juce::var&) { ready = true; sendState(); })
        .withEventListener ("view", [this] (const juce::var& v) { view = v["tab"].toString(); })
        .withEventListener ("setParams", [this] (const juce::var& v)
        {
            if (auto* o = v.getDynamicObject())
                for (auto& prop : o->getProperties())
                    proc.setParamFromUI (paramIndex (prop.name.toString()), (float) (double) prop.value);
        })
        .withEventListener ("setState", [this] (const juce::var& v) { proc.setComplexJson (v["json"].toString()); });
}

std::optional<juce::WebBrowserComponent::Resource> MutagenEditor::getResource (const juce::String& url)
{
    const auto path = url.fromLastOccurrenceOf ("/", false, false);
    if (path.isEmpty() || path == "index.html")
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::index_htmlSize);
        std::memcpy (r.data.data(), BinaryData::index_html, (size_t) BinaryData::index_htmlSize);
        r.mimeType = "text/html";
        return r;
    }
    return std::nullopt;
}

void MutagenEditor::sendState()
{
    auto* o = new juce::DynamicObject();
    auto* pp = new juce::DynamicObject();
    for (int i = 0; i < NP; ++i) pp->setProperty (PINFO[i].id, (double) proc.apvts.getRawParameterValue (PINFO[i].id)->load());
    o->setProperty ("P", juce::var (pp));
    o->setProperty ("complex", proc.getComplexJson());
    o->setProperty ("fresh", proc.isFresh());
    o->setProperty ("bpm", proc.hostBpm.load());
    web->emitEventIfBrowserIsVisible ("state", juce::var (o));
    proc.takeDirtyParams();
}

void MutagenEditor::timerCallback()
{
    if (! ready || ! web) return;
    ++tick;
    auto& vz = proc.engine.viz;

    // cambios que vienen del host (automatización, cargar proyecto)
    const auto dirty = proc.takeDirtyParams();
    if (! dirty.isEmpty())
    {
        auto* pp = new juce::DynamicObject();
        for (int i : dirty) pp->setProperty (PINFO[i].id, (double) proc.apvts.getRawParameterValue (PINFO[i].id)->load());
        web->emitEventIfBrowserIsVisible ("params", juce::var (pp));
    }

    auto* o = new juce::DynamicObject();
    o->setProperty ("level", vz.level.load()); o->setProperty ("inRms", vz.inRms.load()); o->setProperty ("outRms", vz.outRms.load());
    o->setProperty ("mk", vz.mk.load()); o->setProperty ("gr", vz.gr.load()); o->setProperty ("lim", vz.lim.load());
    o->setProperty ("step", vz.step.load()); o->setProperty ("stepFrac", vz.stepFrac.load()); o->setProperty ("ag", vz.activeGrains.load());
    o->setProperty ("playing", proc.hostPlaying.load()); o->setProperty ("bpm", proc.hostBpm.load());
    {
        auto* mv = new juce::DynamicObject();
        const char* ks[NSRC] = { "A", "B", "SEG", "X", "Y" };
        for (int k = 0; k < NSRC; ++k) mv->setProperty (ks[k], vz.mv[k].load());
        o->setProperty ("mv", juce::var (mv));
        o->setProperty ("ph", juce::Array<juce::var> { vz.mph[0].load(), vz.mph[1].load() });
        auto* q = new juce::DynamicObject();
        for (int i = 0; i < NP; ++i)
        {
            const float qv = vz.q[i].load(), pv = proc.apvts.getRawParameterValue (PINFO[i].id)->load();
            if (std::abs (qv - pv) > 1e-6f * (1 + std::abs (pv))) q->setProperty (PINFO[i].id, qv);
        }
        o->setProperty ("q", juce::var (q));
    }
    // granos nuevos
    {
        juce::Array<juce::var> ge;
        const uint32_t wi = vz.geW.load (std::memory_order_acquire);
        if (wi - geRead > (uint32_t) Viz::GE) geRead = wi - Viz::GE;
        for (; geRead != wi; ++geRead)
        {
            const auto& g = vz.ge[geRead % Viz::GE];
            ge.add (juce::Array<juce::var> { g.posN, g.st, g.len, g.pan, g.rev });
        }
        o->setProperty ("ge", ge);
    }
    // osciloscopio (solo en DESTROZO)
    if (view == "destrozo")
    {
        juce::Array<juce::var> si, so;
        const int start = vz.scI.load();
        for (int k = 0; k < Viz::SCN; ++k) { const int i = (start + k) % Viz::SCN; si.add (vz.scIn[i]); so.add (vz.scOut[i]); }
        o->setProperty ("scIn", si); o->setProperty ("scOut", so);
    }
    // visor de granos (solo en LABORATORIO)
    if (view == "lab" && (tick & 1) == 0)
    {
        const auto& buf = proc.engine.grainBuffer();
        const auto B = (int64_t) buf.size();
        if (B > 0)
        {
            juce::Array<juce::var> env;
            const int W = 380; const double span = 2.0 * proc.engine.sampleRate(), step = span / W;
            const int64_t w0 = (int64_t) proc.engine.grainWrite();
            for (int i = 0; i < W; ++i)
            {
                float mx = 0; const double s0 = (double) w0 - span + i * step;
                for (double kk = 0; kk < step; kk += 24) { const int64_t idx = (((int64_t) (s0 + kk) % B) + B) % B; mx = std::max (mx, std::abs (buf[(size_t) idx])); }
                env.add (mx);
            }
            o->setProperty ("env", env);
        }
    }
    web->emitEventIfBrowserIsVisible ("viz", juce::var (o));
}
