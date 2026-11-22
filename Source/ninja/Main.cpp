#include "../common/PluginBase.h"
#include "NinjaEngine.h"
#include "NinjaUI.h"

struct NinjaAdapter : IEngine
{
    pp::NinjaEngine e; std::vector<float> a, b;
    void prepare (double fs, int) override { e.prepare (fs); a.assign (2048, 0.f); b.assign (2048, 0.f); }
    void setParams (const float* p) override { e.k = p[0]; e.div = (int) std::lround (p[1]); e.preset = (int) std::lround (p[2]); e.bypass = p[3] > 0.5f; }
    void setTempo (double bpm, bool) override { e.bpm = bpm; }
    void process (float* L, float* R, int N) override { std::copy (L, L + N, a.begin()); std::copy (R, R + N, b.begin()); e.process (a.data(), b.data(), L, R, N); }
    double tail() const override { return 10.0; }
    void fillViz (juce::DynamicObject& o) override
    {
        o.setProperty ("inL", e.vIn.get()); o.setProperty ("outL", e.vOut.get()); o.setProperty ("echo", e.vEcho.get()); o.setProperty ("hold", e.vHold.get());
        o.setProperty ("ticks", e.vTicks.load()); o.setProperty ("frozen", e.vFrozen.load());
    }
};

const PluginInfo& thisPlugin()
{
    static const PluginInfo info {
        "PEPELUS NINJA",
        { { "k", "ECOS", 0, 1, 0.5f, false }, { "div", "TIEMPO", 0, 5, 2, true }, { "preset", "ESTILO", 0, 11, 0, true }, { "bypass", "OFF", 0, 1, 0, true } },
        [] { return std::unique_ptr<IEngine> (new NinjaAdapter()); },
        NinjaUI::index_html, NinjaUI::index_htmlSize, 912, 576, 0xff070707 };
    return info;
}
