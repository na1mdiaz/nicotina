// Convierte el estado "complejo" que manda la interfaz (JSON) al formato del motor.
#pragma once
#include <juce_core/juce_core.h>
#include "Engine.h"

namespace mg {

inline int paramIndex (const juce::String& id)
{
    for (int i = 0; i < NP; ++i) if (id == PINFO[i].id) return i;
    return -1;
}

inline void parseComplex (const juce::var& v, Complex& out)
{
    if (! v.isObject()) return;
    if (auto pat = v["PAT"]; pat.isObject())
    {
        const char* lanes[3] = { "t", "v", "f" };
        for (int l = 0; l < 3; ++l)
            if (auto* a = pat[lanes[l]].getArray())
                for (int i = 0; i < 16 && i < a->size(); ++i) out.pat[l][i] = juce::jlimit (0, 11, (int) (*a)[i]);
    }
    if (auto mods = v["MODS"]; mods.isObject())
    {
        const char* ks[2] = { "A", "B" };
        for (int k = 0; k < 2; ++k)
        {
            auto m = mods[ks[k]];
            if (! m.isObject()) continue;
            if (auto* nodes = m["nodes"].getArray())
            {
                std::vector<Node> nv;
                for (auto& n : *nodes) nv.push_back ({ (float) (double) n["x"], (float) (double) n["y"], (float) (double) n["c"] });
                std::sort (nv.begin(), nv.end(), [] (const Node& a, const Node& b) { return a.x < b.x; });
                if (nv.size() >= 2 && nv.size() <= 64) out.nodes[k] = std::move (nv);
            }
            out.msync[k] = (bool) m["sync"];
            out.mdiv[k] = juce::jlimit (0, 13, (int) m["div"]);
            out.mhz[k] = juce::jlimit (0.05f, 30.f, (float) (double) m["hz"]);
            out.hum[k] = juce::jlimit (0.f, 1.f, (float) (double) m["hum"]);
        }
        if (auto sg = mods["SEG"]; sg.isObject())
        {
            out.segAtk = (float) (double) sg["atk"]; out.segRel = (float) (double) sg["rel"]; out.segGain = (float) (double) sg["gain"];
        }
    }
    if (auto as = v["ASSIGN"]; as.isObject())
    {
        const char* ks[NSRC] = { "A", "B", "SEG", "X", "Y" };
        for (int s = 0; s < NSRC; ++s)
        {
            out.assign[s].clear();
            if (auto* o = as[ks[s]].getDynamicObject())
                for (auto& p : o->getProperties())
                {
                    const int idx = paramIndex (p.name.toString());
                    if (idx >= 0) out.assign[s].push_back ({ idx, juce::jlimit (-1.f, 1.f, (float) (double) p.value) });
                }
        }
    }
    if (auto* m = v["mask"].getArray())
    {
        out.maskN = 0;
        for (auto& x : *m) if (out.maskN < 12) out.mask[out.maskN++] = juce::jlimit (0, 11, (int) x);
        if (out.maskN == 0) { out.mask[0] = 0; out.maskN = 1; }
    }
    if (v.hasProperty ("quant")) out.quant = (bool) v["quant"];
}

} // namespace mg
