#pragma once
// PEPELUS SLOWED OPERATIONS · motor (traducción del dspBlock de la vista previa, v7)
#include "../common/Dsp.h"

namespace pp {
struct SlowedPreset { double st, verb, size, lp, wow, pre; };
static const SlowedPreset SLW_PRESETS[3] = {
    { -4, 0.55, 0.86, 9500,  0.12, 0.025 },   // EMPANAO
    { -7, 0.68, 0.90, 6000,  0.20, 0.035 },   // ENTERRAO
    {  0, 0.85, 0.92, 11000, 0.0,  0.045 },   // ECO
};
inline double herm (const float* b, int64_t M, double p)
{
    const double fi = std::floor (p); const int64_t i = (int64_t) fi; const double f = p - fi;
    const double y0 = b[(i - 1) & M], y1 = b[i & M], y2 = b[(i + 1) & M], y3 = b[(i + 2) & M];
    const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

struct SlowedEngine
{
    double k = 0, mix = 1; int preset = 0; bool bypass = false; double bpm = 140;
    AtomF vIn, vOut, vLv, vGr, vTv, vSt; std::atomic<int> vTape { 0 };

    enum { RUN, STOPPING, STOPPED, STARTING, XFADE };
    struct Chan { BQ lp, lp2, rhp, rhp2, rdip, mud, sub, rlp; double env = 0; std::vector<float> pre; int pw = 0; Rev rev; };
    Chan ch[2];
    double fs = 48000, rIn = 1e-4, rOut = 1e-4, ag = 1, gr = 1, inL = 0, outL = 0, lv = 0, ks = -1, ms = -1; int64_t t = 0;
    // cinta
    static constexpr int TN = 1 << 19; static constexpr int64_t TM = TN - 1;
    std::vector<float> tb[2]; int64_t wp = 0; double rp = 0, v = 1, u = 0, xf = 1, tlp[2] { 0, 0 }; int state = RUN;
    std::vector<float> vArr, xArr, tin[2], ps[2];
    // desplazador de tono (SOLA)
    static constexpr int PN = 1 << 15; static constexpr int64_t PM = PN - 1;
    std::vector<float> pb[2], pm; int64_t pw = 0; double d = 0, d2 = 0, pxf = 1; bool pinit = false;
    int dmin = 0, span = 0, srch = 0, L = 0, XF = 0;

    void prepare (double sr)
    {
        fs = sr;
        for (int c = 0; c < 2; ++c)
        {
            ch[c] = Chan();
            ch[c].rhp.hp (240, 0.7, fs); ch[c].rhp2.hp (240, 0.7, fs); ch[c].rdip.pk (420, 0.9, -4.5, fs); ch[c].sub.hp (28, 0.7, fs);
            ch[c].pre.assign ((size_t) std::ceil (fs * 0.06), 0.f); ch[c].rev.make (fs, c * 23, 1.3);
            tb[c].assign (TN, 0.f); pb[c].assign (PN, 0.f); tin[c].assign (4096, 0.f); ps[c].assign (4096, 0.f);
        }
        pm.assign (PN, 0.f); vArr.assign (4096, 0.f); xArr.assign (4096, 0.f);
        wp = 0; rp = 0; v = 1; u = 0; xf = 1; state = RUN; tlp[0] = tlp[1] = 0;
        pw = 0; d = d2 = 0; pxf = 1; pinit = false; t = 0; ag = 1; rIn = rOut = 1e-4; ks = ms = -1;
        dmin = (int) std::lround (fs * 0.032); span = (int) std::lround (fs * 0.06); srch = (int) std::lround (fs * 0.012); L = (int) std::lround (fs * 0.01); XF = (int) std::lround (fs * 0.014);
    }

    void tapeBlock (float* in0, float* in1, int N)
    {
        float* ins[2] = { in0, in1 };
        const double beat = 60.0 / bpm;
        if (k >= 0.93 && (state == RUN || state == STARTING || state == XFADE)) { if (state == RUN || state == XFADE) { rp = (double) (wp - 1); tlp[0] = ins[0][0]; tlp[1] = ins[1][0]; } u = 1 - std::pow (v, 1 / 1.3); state = STOPPING; }
        if (k < 0.9 && (state == STOPPING || state == STOPPED)) { if (state == STOPPED) rp = (double) (wp - 64); u = v; state = STARTING; }
        const double incStop = 1 / (2 * beat * fs), incUp = 1 / (0.35 * fs), incX = 1 / (0.03 * fs);
        for (int n = 0; n < N; ++n)
        {
            if (state == STOPPING) { u += incStop; if (u >= 1) { u = 1; state = STOPPED; } v = std::pow (1 - u, 1.3); }
            else if (state == STARTING) { u += incUp; v = std::min (1.0, u); if (u >= 1) { state = XFADE; xf = 0; } }
            else if (state == XFADE) { xf += incX; v = 1; if (xf >= 1) { xf = 1; state = RUN; } }
            else if (state == STOPPED) v = 0;
            else v = 1;
            vArr[(size_t) n] = (float) v; xArr[(size_t) n] = (float) (state == XFADE ? xf : state == RUN ? 1 : 0);
        }
        if (state == RUN && xArr[0] == 1.f) { for (int c = 0; c < 2; ++c) for (int n = 0; n < N; ++n) tb[c][(size_t) ((wp + n) & TM)] = ins[c][n]; wp += N; rp = (double) wp; return; }
        const double rp0 = rp; double rpEnd = rp0;
        for (int c = 0; c < 2; ++c)
        {
            double r = rp0, lp = tlp[c];
            for (int n = 0; n < N; ++n)
            {
                tb[c][(size_t) ((wp + n) & TM)] = ins[c][n];
                const double vv = vArr[(size_t) n]; r += vv;
                const double fi = std::floor (r); const int64_t i = (int64_t) fi; const double f = r - fi;
                double y = tb[c][(size_t) (i & TM)] * (1 - f) + tb[c][(size_t) ((i + 1) & TM)] * f;
                const double fc = 120 + 17000 * vv * vv, a = 1 - std::exp (-TAU * fc / fs); lp += (y - lp) * a; y = lp * std::min (1.0, vv * 6);
                ins[c][n] = (float) (y + (ins[c][n] - y) * xArr[(size_t) n]);
            }
            tlp[c] = lp; rpEnd = r;
        }
        rp = rpEnd; wp += N;
        if ((double) wp - rp > TN - 8192) rp = (double) (wp - (TN - 8192));
    }

    int psBest (double dOld, double r)
    {
        int best = dmin; double bv = -1e9;
        for (int dn = dmin; dn <= dmin + srch; dn += 2)
        {
            double xy = 0, yy = 1e-9;
            for (int j = 0; j < L; j += 3) { const int64_t rj = (int64_t) std::llround (j * r); const double a = pm[(size_t) ((pw - (int64_t) std::llround (dOld) + rj) & PM)], b = pm[(size_t) ((pw - dn + rj) & PM)]; xy += a * b; yy += b * b; }
            const double vv = xy / std::sqrt (yy); if (vv > bv) { bv = vv; best = dn; }
        }
        return best;
    }
    // ojo: Math.round de JS redondea .5 hacia arriba; llround se aleja de cero. Para j*r >= 0 es igual.

    void psProcess (double x0, double x1, float& y0, float& y1, double r)
    {
        pb[0][(size_t) (pw & PM)] = (float) x0; pb[1][(size_t) (pw & PM)] = (float) x1; pm[(size_t) (pw & PM)] = (float) (x0 + x1);
        if (! pinit) { d = dmin; pinit = true; }
        if (pxf >= 1 && d > dmin + span) { d2 = d; d = psBest (d, r); pxf = 0; }
        if (d < dmin * 0.5) d = dmin;
        double a = herm (pb[0].data(), PM, (double) pw - d), b = herm (pb[1].data(), PM, (double) pw - d);
        if (pxf < 1) { const double g = 0.5 - 0.5 * std::cos (TAU / 2 * pxf), a2 = herm (pb[0].data(), PM, (double) pw - d2), b2 = herm (pb[1].data(), PM, (double) pw - d2); a = a2 + (a - a2) * g; b = b2 + (b - b2) * g; pxf += 1.0 / XF; d2 += 1 - r; }
        d += 1 - r; pw++;
        y0 = (float) a; y1 = (float) b;
    }

    void process (const float* inLp, const float* inRp, float* oL, float* oR, int N)
    {
        const SlowedPreset& p = SLW_PRESETS[std::clamp (preset, 0, 2)];
        ks = ks < 0 ? k : ks + (k - ks) * 0.2; ms = ms < 0 ? mix : ms + (mix - ms) * 0.2;
        const double kk = ks, mx = ms;
        std::copy (inLp, inLp + N, tin[0].begin()); std::copy (inRp, inRp + N, tin[1].begin());
        tapeBlock (tin[0].data(), tin[1].data(), N);
        const double st = p.st * std::pow (kk, 0.85), shA = clampd (std::abs (st) / 0.35, 0, 1);
        const double verb = p.verb * std::pow (kk, 0.8), fb = 0.72 + (p.size - 0.72) * std::min (1.0, kk * 1.3), damp = 0.22 + 0.3 * kk;
        const double fc = std::exp (lerpd (std::log (19000.0), std::log (p.lp), std::pow (kk, 0.9))); const int preN = (int) std::lround (fs * p.pre * kk);
        { int64_t tt = t; for (int n = 0; n < N; ++n, ++tt)
            { const double wow = p.wow * kk * (std::sin (TAU * 0.55 * tt / fs) * 0.7 + std::sin (TAU * 3.1 * tt / fs) * 0.15);
              psProcess (tin[0][(size_t) n], tin[1][(size_t) n], ps[0][(size_t) n], ps[1][(size_t) n], std::min (1.0, std::pow (2.0, (st + wow) / 12))); } }
        const double duck = 0.55 * kk, ea = 1 - std::exp (-1 / (fs * 0.006)), er = 1 - std::exp (-1 / (fs * 0.28));
        double sIn = 0, sOut = 0; float* outs[2] = { oL, oR };
        for (int c = 0; c < 2; ++c)
        {
            Chan& C = ch[c];
            C.lp.lp (fc, 0.6, fs); C.lp2.lp (fc * 1.4, 0.55, fs); C.rlp.lp (std::min (fc * 1.1, 8500.0), 0.7, fs); C.mud.pk (330, 0.8, -3.5 * shA * kk, fs);
            const int PL = (int) C.pre.size();
            for (int n = 0; n < N; ++n)
            {
                const double dry = tin[c][(size_t) n]; sIn += dry * dry;
                double y = dry + (ps[c][(size_t) n] - dry) * shA;
                y = C.lp2.run (C.lp.run (y)); y = C.sub.run (C.mud.run (y));
                C.pre[(size_t) C.pw] = (float) y; double ri = C.pre[(size_t) ((C.pw - preN + PL) % PL)]; if (++C.pw >= PL) C.pw = 0;
                ri = C.rhp2.run (C.rhp.run (ri));
                double rv = C.rdip.run (C.rlp.run (C.rev.run (ri * 0.03, fb, damp)));
                const double av = std::abs (y); C.env += (av - C.env) * (av > C.env ? ea : er); rv *= 1 / (1 + duck * std::min (4.0, C.env * 6));
                const double wet = y * (1 - 0.35 * verb) + rv * verb * 2.3;
                outs[c][n] = (float) (dry + (wet - dry) * mx);
            }
        }
        t += N;
        for (int c = 0; c < 2; ++c) for (int n = 0; n < N; ++n) { const double vv = outs[c][n]; sOut += vv * vv; }
        const double a = std::exp (-N / (fs * 0.6));
        rIn = rIn * a + (sIn / (2 * N)) * (1 - a); rOut = rOut * a + (sOut / (2 * N)) * (1 - a);
        const double target = rIn < 1e-7 ? ag : clampd (std::sqrt (rIn / std::max (1e-9, rOut)), 0.5, 1.6);
        double pk = 0, pin = 0, gmin = 1;
        for (int n = 0; n < N; ++n)
        {
            ag += (target - ag) * 0.0005;
            for (int c = 0; c < 2; ++c)
            {
                const double orig = c ? inRp[n] : inLp[n];
                double vv = outs[c][n] * ag; const double av = std::abs (vv);
                if (av > 0.89) { const double nv = 0.89 + 0.09 * std::tanh ((av - 0.89) / 0.09); gmin = std::min (gmin, nv / av); vv = sgn (vv) * nv; }
                if (bypass) vv = orig;
                outs[c][n] = (float) vv; pk = std::max (pk, std::abs (vv)); pin = std::max (pin, std::abs (orig));
            }
        }
        gr = std::min ({ 1.0, gr * 0.98 + 0.02, gmin });
        outL = std::max (pk, outL * 0.9); inL = std::max (pin, inL * 0.9); lv = lv * 0.6 + std::sqrt (sOut / (2 * N)) * ag * 0.4 * 3;
        vIn.set (inL); vOut.set (outL); vLv.set (lv); vGr.set (gr); vTv.set (v); vSt.set (st); vTape.store (state);
    }
};
}
