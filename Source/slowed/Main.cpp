#include "../common/PluginBase.h"
#include "SlowedEngine.h"
#include "SlowedUI.h"

struct SlowedAdapter : IEngine
{
    pp::SlowedEngine e; std::vector<float> a, b;
    void prepare (double fs, int) override { e.prepare (fs); a.assign (2048, 0.f); b.assign (2048, 0.f); }
    void setParams (const float* p) override { e.k = p[0]; e.mix = p[1]; e.preset = (int) std::lround (p[2]); e.bypass = p[3] > 0.5f; }
    void setTempo (double bpm, bool) override { e.bpm = bpm; }
    void process (float* L, float* R, int N) override { std::copy (L, L + N, a.begin()); std::copy (R, R + N, b.begin()); e.process (a.data(), b.data(), L, R, N); }
    double tail() const override { return 6.0; }
    void fillViz (juce::DynamicObject& o) override
    {
        static const char* ST[5] = { "run", "stopping", "stopped", "starting", "xfade" };
        o.setProperty ("inL", e.vIn.get()); o.setProperty ("outL", e.vOut.get()); o.setProperty ("lv", e.vLv.get()); o.setProperty ("gr", e.vGr.get());
        o.setProperty ("tv", e.vTv.get()); o.setProperty ("st", e.vSt.get()); o.setProperty ("tape", ST[juce::jlimit (0, 4, e.vTape.load())]);
    }
};

const PluginInfo& thisPlugin()
{
    static const PluginInfo info {
        "PEPELUS SLOWED",
        { { "k", "SLOW", 0, 1, 0, false }, { "mix", "MEZCLA", 0, 1, 1, false }, { "preset", "MODO", 0, 2, 0, true }, { "bypass", "OFF", 0, 1, 0, true } },
        [] { return std::unique_ptr<IEngine> (new SlowedAdapter()); },
        SlowedUI::index_html, SlowedUI::index_htmlSize, 912, 576, 0xff050505 };
    return info;
}
