#include "../common/PluginBase.h"
#include "SaturaEngine.h"
#include "SaturaUI.h"

struct SaturaAdapter : IEngine
{
    pp::SaturaEngine e; std::vector<float> a, b;
    void prepare (double fs, int) override { e.prepare (fs); a.assign (2048, 0.f); b.assign (2048, 0.f); }
    void setParams (const float* p) override { e.k = p[0]; e.mix = p[1]; e.preset = (int) std::lround (p[2]); e.bypass = p[3] > 0.5f; }
    void process (float* L, float* R, int N) override { std::copy (L, L + N, a.begin()); std::copy (R, R + N, b.begin()); e.process (a.data(), b.data(), L, R, N); }
    int latency() const override { return e.latency(); }
    double tail() const override { return 0.1; }
    void fillViz (juce::DynamicObject& o) override { o.setProperty ("inL", e.vIn.get()); o.setProperty ("outL", e.vOut.get()); o.setProperty ("lv", e.vLv.get()); o.setProperty ("gr", e.vGr.get()); o.setProperty ("louder", e.vLouder.get()); }
};

const PluginInfo& thisPlugin()
{
    static const PluginInfo info {
        "PEPELUS SATURA",
        { { "k", "SATURA", 0, 1, 0, false }, { "mix", "MEZCLA", 0, 1, 0.85f, false }, { "preset", "MODO", 0, 3, 0, true }, { "bypass", "OFF", 0, 1, 0, true } },
        [] { return std::unique_ptr<IEngine> (new SaturaAdapter()); },
        SaturaUI::index_html, SaturaUI::index_htmlSize, 912, 576, 0xff050505 };
    return info;
}
