#pragma once
// PEPELUS SATURA OPERATIONS · motor (traducción del dspBlock de la vista previa)
#include "../common/Dsp.h"
#include <string>

namespace pp {
struct SaturaPreset { int drive; int shape; double emF, emG, subClean, lp, mix; bool deess; double louder, ceil; bool soft; double air; bool phone; };
// shape: 0 tube · 1 tape · 2 fat · 3 phone
static const SaturaPreset SAT_PRESETS[4] = {
    { 20, 0, 2600, 5, 150, 16000, 0.85, true,  0, -0.3, false, 3, false },   // RESACA · voces
    { 14, 1, 220,  2, 90,  19000, 1.0,  false, 7, -0.5, true,  1, false },   // GARRAFÓN · máster
    { 30, 2, 140,  6, 90,  11000, 1.0,  false, 0, -0.3, false, 0, false },   // CALIMOCHO · 808
    { 26, 3, 1700, 7, 0,   3400,  1.0,  false, 0, -0.3, false, 0, true  },   // CABINA · teléfono
};
static const double BWQ[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };
static const double TB = std::tanh (0.3), TBN = 1 / (1 - TB * TB);

inline double satShape (int kind, double v, double k)
{
    if (kind == 0) return (std::tanh (v + 0.3) - TB) * TBN;
    if (kind == 3) { const double a = (std::tanh (1.6 * v + 0.3) - TB) * 0.8; return clampd (a, -0.85, 0.85); }
    if (kind == 1) { const double a = v / (1 + std::abs (v) * 0.6); return 0.5 * std::tanh (v) + 0.5 * a / (1 / 1.6) * 0.625; }
    if (kind == 2) { const double a = std::tanh (v), f = std::max (0.0, k - 0.7) / 0.3 * 0.6; return (1 - f) * (a + 0.45 * k * std::abs (std::tanh (0.8 * v))) + f * std::sin (1.3 * v) / 1.3; }
    const double f = 0.7 * k; return clampd ((1 - f) * std::tanh (v) + f * std::sin (1.3 * v) / 1.3, -1, 1);
}

struct SaturaEngine
{
    // parámetros
    double k = 0, mix = 0.85; int preset = 0; bool bypass = false;
    // pantalla
    AtomF vIn, vOut, vLv, vGr, vLouder;

    struct Chan { BQ up[4], dn[4], pre, de, post, air, ph[4], php[2], lpS[2], lpD[2]; double dsHp = 0, dsX = 0, dsEnv = 0, dsGain = 1, dcx = 0, dcy = 0; int pk = -1; };
    Chan ch[2];
    double fs = 48000, rIn = 1e-4, rOut = 1e-4, ag = 1, lg = 1, gr = 1, lastLouder = 0, inL = 0, outL = 0, lv = 0, ks = -1, ms = -1;
    std::vector<double> la, dl, dr; int lw = 0;
    static constexpr int QN = 8192; std::vector<double> qv = std::vector<double> (QN); std::vector<int64_t> qi = std::vector<int64_t> (QN); int qh = 0, qn = 0; int64_t cnt = 0;

    void prepare (double sr)
    {
        fs = sr;
        for (auto& c : ch)
        {
            c = Chan();
            for (int i = 0; i < 4; ++i) { c.up[i].lp (fs * 0.45, BWQ[i], fs * 4, false); c.dn[i].lp (fs * 0.45, BWQ[i], fs * 4, false); }
            for (auto& b : c.lpS) b.lp (90, 0.7071, fs, false);
            for (auto& b : c.lpD) b.lp (90, 0.7071, fs, false);
        }
        const int LA = (int) std::lround (fs * 0.0013);
        la.assign ((size_t) LA, 1.0); dl.assign ((size_t) LA, 0.0); dr.assign ((size_t) LA, 0.0); lw = 0; lg = 1; ag = 1; rIn = rOut = 1e-4; ks = ms = -1; qh = qn = 0; cnt = 0;
    }
    int latency() const { return (int) la.size(); }

    void process (const float* inLp, const float* inRp, float* oL, float* oR, int N)
    {
        const SaturaPreset& p = SAT_PRESETS[std::clamp (preset, 0, 3)];
        // perillas sin saltos (automatización)
        const double sm = 1 - std::exp (-N / (fs * 0.02));
        ks = ks < 0 ? k : ks + (k - ks) * sm; ms = ms < 0 ? mix : ms + (mix - ms) * sm;
        const double kk = ks, g = dB (p.drive * std::pow (kk, 1.2)), wet = std::min (1.0, kk * 6);
        double sIn = 0, sOut = 0;
        const float* ins[2] = { inLp, inRp }; float* outs[2] = { oL, oR };
        const double aHp = std::exp (-TAU * 6000 / fs), atk = 1 - std::exp (-1 / (0.0015 * fs)), rel = 1 - std::exp (-1 / (0.06 * fs));
        for (int c = 0; c < 2; ++c)
        {
            Chan& C = ch[c];
            if (C.pk != preset)
            {
                C.pre.pk (p.emF, p.phone ? 1.6 : 0.8, p.emG, fs); C.de.pk (p.emF, 0.8, p.phone ? 0 : -p.emG, fs); C.post.lp (p.lp, 0.7071, fs, false); C.air.pk (9500, 0.6, p.air, fs);
                if (p.subClean > 0) { for (auto& b : C.lpS) b.lp (p.subClean, 0.7071, fs, false); for (auto& b : C.lpD) b.lp (p.subClean, 0.7071, fs, false); }
                if (p.phone) { C.ph[0].hp (380, 0.7, fs); C.ph[1].hp (380, 1.1, fs); C.ph[2].lp (3200, 0.7, fs, false); C.ph[3].lp (3200, 1.2, fs, false); C.php[0].hp (320, 0.8, fs); C.php[1].lp (3400, 0.9, fs, false); }
                C.pk = preset;
            }
            for (int n = 0; n < N; ++n)
            {
                const double dry = ins[c][n]; sIn += dry * dry;
                double src = dry;
                if (p.deess)
                {
                    const double hp = aHp * (C.dsHp + dry - C.dsX); C.dsX = dry; C.dsHp = hp;
                    const double e = std::abs (hp); C.dsEnv += (e - C.dsEnv) * (e > C.dsEnv ? atk : rel);
                    const double lvl = std::abs (dry) * 0.5 + C.dsEnv * 0.5, over = (C.dsEnv > 0.03 && C.dsEnv > lvl * 0.7) ? (lvl * 0.7) / C.dsEnv : 1;
                    C.dsGain += (std::max (0.45, over) - C.dsGain) * 0.01; src = dry - hp * (1 - C.dsGain);
                }
                if (p.phone) for (auto& b : C.ph) src = b.run (src);
                const double xp = C.pre.run (src); double y = 0;
                for (int j = 0; j < 4; ++j)
                {
                    double u = j == 0 ? xp * 4 : 0;
                    for (auto& b : C.up) u = b.run (u);
                    double s = satShape (p.shape, u * g, kk);
                    for (auto& b : C.dn) s = b.run (s);
                    if (j == 3) y = s;
                }
                double s = C.de.run (y);
                if (p.subClean > 0) { double a = s, d = src; for (auto& b : C.lpS) a = b.run (a); for (auto& b : C.lpD) d = b.run (d); s = s - a + d; }
                const double dc = s - C.dcx + 0.9995 * C.dcy; C.dcx = s; C.dcy = dc; s = C.air.run (C.post.run (dc)); if (p.phone) for (auto& b : C.php) s = b.run (s);
                outs[c][n] = (float) (dry + (s - dry) * wet * ms);
            }
        }
        for (int c = 0; c < 2; ++c) for (int n = 0; n < N; ++n) { const double v = outs[c][n]; sOut += v * v; }
        const double a = std::exp (-N / (fs * 0.35));
        rIn = rIn * a + (sIn / (2 * N)) * (1 - a); rOut = rOut * a + (sOut / (2 * N)) * (1 - a);
        const double louder = p.louder * std::pow (kk, 0.9);
        const double target = (rIn < 1e-7 ? ag / dB (lastLouder) : clampd (std::sqrt (rIn / std::max (1e-9, rOut)), 0.05, 2)) * dB (louder); lastLouder = louder;
        const double ceil = dB (p.ceil), relL = 1 - std::exp (-1 / (0.08 * fs)); const int LA = (int) la.size();
        double pk = 0, pin = 0;
        for (int n = 0; n < N; ++n)
        {
            ag += (target - ag) * 0.0008;
            double l = outs[0][n] * ag, r = outs[1][n] * ag;
            if (p.soft) { const double th = ceil * 0.62, rg = ceil * 1.02 - th; double aa = std::abs (l); if (aa > th) l = sgn (l) * (th + rg * std::tanh ((aa - th) / rg)); aa = std::abs (r); if (aa > th) r = sgn (r) * (th + rg * std::tanh ((aa - th) / rg)); }
            const double need = std::min (1.0, ceil / std::max ({ 1e-9, std::abs (l), std::abs (r) }));
            la[(size_t) lw] = need; dl[(size_t) lw] = l; dr[(size_t) lw] = r; lw = (lw + 1) % LA;
            // mínimo de la ventana de anticipación con una cola monótona (mismo resultado que recorrerla entera, mucho más barato)
            const int64_t idx = cnt++;
            while (qn > 0 && qv[(size_t) ((qh + qn - 1) % QN)] >= need) --qn;
            qv[(size_t) ((qh + qn) % QN)] = need; qi[(size_t) ((qh + qn) % QN)] = idx; ++qn;
            while (qi[(size_t) qh] <= idx - LA) { qh = (qh + 1) % QN; --qn; }
            const double mn = qn > 0 ? qv[(size_t) qh] : 1.0;
            lg += (mn - lg) * (mn < lg ? 0.2 : relL);
            double vl = dl[(size_t) lw] * lg, vr = dr[(size_t) lw] * lg;
            vl = clampd (vl, -ceil, ceil); vr = clampd (vr, -ceil, ceil);
            gr = std::min (gr * 0.9995 + 0.0005, lg);
            if (bypass) { vl = inLp[n]; vr = inRp[n]; }
            pin = std::max ({ pin, (double) std::abs (inLp[n]), (double) std::abs (inRp[n]) });
            oL[n] = (float) vl; oR[n] = (float) vr; pk = std::max ({ pk, std::abs (vl), std::abs (vr) });
        }
        outL = std::max (pk, outL * 0.9); inL = std::max (pin, inL * 0.9); lv = lv * 0.6 + std::sqrt (sOut / (2 * N)) * ag * 0.4 * 3;
        vIn.set (inL); vOut.set (outL); vLv.set (lv); vGr.set (gr); vLouder.set (louder);
    }
};
}
