#include "PluginEditor.h"
#include "BinaryData.h"

static constexpr int BASE_W = 1200, BASE_H = 780;

SemillaEditor::SemillaEditor (SemillaProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setOpaque (true);
    web = std::make_unique<juce::WebBrowserComponent> (makeOptions());
    addAndMakeVisible (*web);
    setResizable (true, true);
    setResizeLimits (BASE_W * 6 / 10, BASE_H * 6 / 10, BASE_W * 16 / 10, BASE_H * 16 / 10);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) BASE_W / BASE_H);
    setSize (BASE_W, BASE_H);
    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz (30);
}
SemillaEditor::~SemillaEditor() { stopTimer(); }
void SemillaEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff010204)); }
void SemillaEditor::resized() { if (web) web->setBounds (getLocalBounds()); }

juce::WebBrowserComponent::Options SemillaEditor::makeOptions()
{
    using WBC = juce::WebBrowserComponent;
    auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("MUTAGEN SEMILLA").getChildFile ("webview");
    return WBC::Options {}
        .withBackend (WBC::Options::Backend::webview2)
        .withWinWebView2Options (WBC::Options::WinWebView2 {}
                                     .withUserDataFolder (userData)
                                     .withStatusBarDisabled()
                                     .withBackgroundColour (juce::Colour (0xff010204)))
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        .withEventListener ("ready", [this] (const juce::var&)
        {
            ready = true;
            auto* o = new juce::DynamicObject(); o->setProperty ("json", proc.getSavedJson());
            web->emitEventIfBrowserIsVisible ("restore", juce::var (o));
        })
        .withEventListener ("eng", [this] (const juce::var& v) { proc.applyEngJson (v["json"].toString()); })
        .withEventListener ("save", [this] (const juce::var& v) { proc.setSavedJson (v["json"].toString()); })
        .withEventListener ("note", [this] (const juce::var& v) { proc.queueNote ((int) v["m"], (int) v["on"] != 0, (float) (double) v["v"]); })
        .withEventListener ("allOff", [this] (const juce::var&) { proc.queueAllOff(); });
}

std::optional<juce::WebBrowserComponent::Resource> SemillaEditor::getResource (const juce::String& url)
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

void SemillaEditor::timerCallback()
{
    if (! ready) return;
    const auto v = proc.getViz();
    auto* o = new juce::DynamicObject();
    o->setProperty ("level", v.level);
    juce::Array<juce::var> ns, ps; for (int n : v.notes) ns.add (n); for (int n : v.played) ps.add (n);
    o->setProperty ("notes", ns); o->setProperty ("played", ps); o->setProperty ("voicesOn", v.voicesOn); o->setProperty ("bpm", v.bpm);
    if (v.hasKey) { juce::Array<juce::var> k; k.add (v.keyK); k.add (v.keyMinor ? 1 : 0); o->setProperty ("key", k); }
    web->emitEventIfBrowserIsVisible ("viz", juce::var (o));
}
