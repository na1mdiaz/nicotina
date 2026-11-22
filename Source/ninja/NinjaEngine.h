#pragma once
// PEPELUS NINJA OPERATIONS · motor (traducción del dspBlock de la vista previa, v4: 12 estilos)
#include "../common/Dsp.h"

namespace pp {
enum NinjaFx { FX_NONE, FX_DUB, FX_ESC, FX_ABIS, FX_RITMO, FX_ESP, FX_NUBE };
struct NinjaPreset { bool pp; double lp, hp, wow, rnd, sat, diff; bool cry, lofi; int fx; double fb, wet; };
static const NinjaPreset NJ_PRESETS[12] = {
    { true,  7500, 220, 0,    0,   1.1, 0,    false, false, FX_NONE,  0.82, 0.75 },  // PING-PONG
    { false, 3400, 170, 1,    0.2, 2.2, 0.25, false, false, FX_NONE,  0.88, 0.8  },  // CINTA
    { false, 6000, 260, 0.15, 0,   1.3, 0,    false, false, FX_NONE,  0.38, 0.65 },  // SLAP
    { false, 2400, 380, 0.3,  1.4, 2.6, 0.62, false, false, FX_NONE,  0.86, 0.85 },  // ACEITE
    { true,  9000, 300, 0,    0,   1.2, 0.4,  true,  false, FX_NONE,  0.7,  0.8  },  // CRISTAL
    { true,  5200, 200, 0,    0,   1.6, 0,    false, true,  FX_NONE,  0.8,  0.8  },  // 8 BITS
    { true,  9000, 120, 0.2,  0,   1.8, 0.35, false, false, FX_DUB,   0.86, 0.85 },  // DUB
    { true,  8000, 250, 0,    0,   1.2, 0.2,  false, false, FX_ESC,   0.72, 0.8  },  // ESCALERA
    { false, 3000, 60,  0,    0,   1.6, 0.3,  false, false, FX_ABIS,  0.75, 0.85 },  // ABISMO
    { false, 7000, 200, 0,    0,   1.3, 0,    false, false, FX_RITMO, 0.3,  0.8  },  // RITMO
    { false, 7500, 200, 0,    0,   1.2, 0.15, false, false, FX_ESP,   0.8,  0.8  },  // ESPIRAL
    { false, 8500, 250, 0,    0,   1.1, 0.5,  false, false, FX_NUBE,  0.6,  0.9  },  // NUBE
};
static const double NJ_DIVS[6] = { 0.25, 0.5, 0.75, 1, 1.5, 2 };

struct NinjaEngine
{
    double k = 0.5; int div = 2, preset = 0; bool bypass = false; double bpm = 140;
    AtomF vIn, vOut, vEcho, vHold; std::atomic<int> vTicks { 0 }; std::atomic<bool> vFrozen { false };

    static constexpr int DL = 1 << 20; static constexpr int64_t DM = DL - 1;   // ~21 s a 48 kHz
    std::vector<float> B[2];
    struct AP { std::vector<float> b; int i = 0; };
    AP ap[2][3];
    BQ lpf[2], hpf[2];
    double fs = 48000, dT = 0, env = 0, ks = 0.5, ph = 0, echo = 0, hold = 0, rn = 0, rnG = 0, rnT = 0, rfb = 0, cph[2] { 0, 0.37 }, sph[2] { 0, 0.3 }, sh[2] { 0, 0 }, svf[2][2] { { 0, 0 }, { 0, 0 } }, gT = 0, inL = 0, outL = 0;
    int64_t w = 0, t = 0; int shc = 0, ticks = 0; bool frozen = false;
    struct Grain { double t, len, r, d0, pan; };
    std::vector<Grain> grains;
    Rng rng;

    void prepare (double sr)
    {
        fs = sr; B[0].assign (DL, 0.f); B[1].assign (DL, 0.f); dT = 0; w = 0;
        static const int AT[3] = { 142, 107, 379 };
        for (int c = 0; c < 2; ++c) for (int j = 0; j < 3; ++j) { ap[c][j].b.assign ((size_t) std::lround ((AT[j] + c * 13) * fs / 44100.0) + 1, 0.f); ap[c][j].i = 0; }
        grains.clear(); grains.reserve (32);
    }
    double apRun (int c, double x, double g)
    {
        for (auto& a : ap[c]) { const double bo = a.b[(size_t) a.i], y = -g * x + bo; a.b[(size_t) a.i] = (float) (x + g * y); if (++a.i >= (int) a.b.size()) a.i = 0; x = y; }
        return x;
    }
    inline double rd (int c, double dd) const
    {
        const double q = (double) w - dd, fi = std::floor (q); const int64_t qi = (int64_t) fi; const double f = q - fi;
        return B[c][(size_t) (qi & DM)] * (1 - f) + B[c][(size_t) ((qi + 1) & DM)] * f;
    }
    void fxTap (const NinjaPreset& pr, double& eL, double& eR)
    {
        const double d = dT;
        if (pr.fx == FX_ESC || pr.fx == FX_ABIS)
        {
            const double r = pr.fx == FX_ESC ? std::pow (2.0, 7.0 / 12) : 0.5, WIN = fs * 0.075; double out[2] = { 0, 0 };
            for (int c = 0; c < 2; ++c)
            {
                sph[c] += (1 - r) / WIN; sph[c] -= std::floor (sph[c]);
                for (double off : { 0.0, 0.5 }) { const double p2 = std::fmod (sph[c] + off, 1.0), wv = std::pow (std::sin (TAU / 2 * p2), 2); out[c] += rd (c, d + p2 * WIN + (c ? fs * 0.007 : 0)) * wv; }
            }
            eL = out[0]; eR = out[1]; return;
        }
        if (pr.fx == FX_DUB)
        {
            const double beat = fs * 60 / bpm, lfo = 0.5 - 0.5 * std::cos (TAU * t / (beat * 4)), fc = 280 * std::pow (12.0, lfo), f = 2 * std::sin (TAU / 2 * std::min (fc, fs / 6) / fs), q = 0.28; double out[2];
            for (int c = 0; c < 2; ++c) { const double xx = rd (c, d); double* st = svf[c]; const double hp = xx - st[0] - q * st[1]; st[1] += f * hp; st[0] += f * st[1]; out[c] = st[1] * 1.6 + xx * 0.25; }
            eL = out[0]; eR = out[1]; return;
        }
        if (pr.fx == FX_RITMO)
        {
            static const double TAPS[6][3] = { { 3, 1, -0.8 }, { 6, 0.8, 0.8 }, { 8, 0.7, -0.3 }, { 11, 0.55, 0.6 }, { 14, 0.45, -0.6 }, { 16, 0.35, 0.2 } };
            const double u = d / 4; double l = 0, r = 0;
            for (auto& tp : TAPS) { const double vv = rd (0, u * tp[0]) * 0.5 + rd (1, u * tp[0]) * 0.5; l += vv * tp[1] * (1 - tp[2]) * 0.6; r += vv * tp[1] * (1 + tp[2]) * 0.6; }
            rfb = (rd (0, u * 16) + rd (1, u * 16)) * 0.5; eL = l; eR = r; return;
        }
        if (pr.fx == FX_ESP)
        {
            const double beat = fs * 60 / bpm, m = 1 + 0.06 * std::sin (TAU * t / (beat * 2));
            eL = rd (0, d * m); eR = rd (1, d * m + fs * 0.004); return;
        }
        if (pr.fx == FX_NUBE)
        {
            if (--gT <= 0)
            {
                gT = fs * (0.018 + rng.next() * 0.02); const double len = fs * (0.06 + rng.next() * 0.12), rr2 = rng.next(), ratio = rr2 < 0.18 ? 2 : rr2 < 0.3 ? 0.5 : 1;
                grains.push_back ({ 0, len, ratio, d * (0.5 + rng.next() * 1.5) + (ratio > 1 ? len : 0) + 64, rng.next() * 2 - 1 });
                if (grains.size() > 14) grains.erase (grains.begin());
            }
            double l = 0, r = 0;
            for (auto& g : grains) { if (g.t >= g.len) continue; const double wv = std::pow (std::sin (TAU / 2 * g.t / g.len), 2), dd = g.d0 - (g.r - 1) * g.t, vv = (rd (0, dd) + rd (1, dd)) * 0.5 * wv; l += vv * (1 - g.pan) * 0.45; r += vv * (1 + g.pan) * 0.45; g.t++; }
            grains.erase (std::remove_if (grains.begin(), grains.end(), [] (const Grain& g) { return g.t >= g.len; }), grains.end());
            eL = l; eR = r; return;
        }
        eL = rd (0, d); eR = rd (1, d);
    }

    void process (const float* inLp, const float* inRp, float* oL, float* oR, int n)
    {
        const NinjaPreset& pr = NJ_PRESETS[std::clamp (preset, 0, 11)];
        ks += (k - ks) * 0.2; const double kk = ks; frozen = k >= 0.96 && ! bypass;
        const double target = fs * 60 / bpm * NJ_DIVS[std::clamp (div, 0, 5)]; if (dT <= 0) dT = target;
        const double wet = pr.wet * smoothd (0, 0.5, kk) * (0.75 + 0.25 * kk), fb = 0.12 + (pr.fb - 0.12) * std::pow (std::min (kk, 0.95) / 0.95, 1.2), duckA = 0.65 * smoothd (0.2, 1, kk) * (frozen ? 0.3 : 1);
        const double ea = 1 - std::exp (-1 / (fs * 0.005)), er = 1 - std::exp (-1 / (fs * 0.25)), sat = pr.sat;
        for (int c = 0; c < 2; ++c) { lpf[c].lp (pr.lp, 0.6, fs); hpf[c].hp (pr.hp, 0.6, fs); }
        double pk = 0, pin = 0, sE = 0;
        const int hold_ = std::max (1, (int) std::lround (fs / 11025)); const double bits = 64;
        auto proc = [&] (int c, double vv) { vv = lpf[c].run (hpf[c].run (vv)); if (pr.diff > 0) vv = apRun (c, vv, pr.diff * 0.7); return std::tanh (sat * vv) / sat; };
        for (int i = 0; i < n; ++i, ++t)
        {
            dT += (target - dT) * 0.00025;
            if (--rnT <= 0) { rnT = fs * 0.05; rnG = rng.next() * 2 - 1; } rn += (rnG - rn) * 0.0004;
            const double mod = fs * 0.0022 * (pr.wow * (std::sin (TAU * 0.55 * t / fs) + 0.3 * std::sin (TAU * 4.7 * t / fs)) + pr.rnd * rn);
            double eL, eR;
            if (pr.cry && ! frozen)
            {
                const double G = dT, sp = 2;
                auto head = [&] (int c) { double y = 0; for (double off : { 0.0, 0.5 }) { const double p2 = std::fmod (cph[c] + off, 1.0), wv = std::pow (std::sin (TAU / 2 * p2), 2); y += rd (c, G * 0.25 + p2 * G * (1 + sp)) * wv; } return y; };
                eL = head (0); eR = head (1); cph[0] = std::fmod (cph[0] + 1 / G, 1.0); cph[1] = std::fmod (cph[1] + 1 / G, 1.0);
            }
            else if (pr.fx != FX_NONE && ! frozen) fxTap (pr, eL, eR);
            else { eL = rd (0, dT + mod); eR = rd (1, dT + mod + (pr.pp ? 0 : fs * 0.009)); }
            if (pr.lofi && ! frozen) { if (++shc >= hold_) { shc = 0; sh[0] = std::round (eL * bits) / bits; sh[1] = std::round (eR * bits) / bits; } eL = sh[0]; eR = sh[1]; }
            const double L = inLp[i], R = inRp[i], x = (L + R) * 0.5, ax = std::abs (x); pin = std::max ({ pin, std::abs (L), std::abs (R) });
            env += (ax - env) * (ax > env ? ea : er);
            const size_t wi = (size_t) (w & DM);
            if (frozen) { const double fL = rd (0, dT), fR = rd (1, dT + (pr.pp ? 0 : fs * 0.009)); if (pr.pp) { B[0][wi] = (float) fR; B[1][wi] = (float) fL; } else { B[0][wi] = (float) fL; B[1][wi] = (float) fR; } }
            else if (pr.fx == FX_ESP) { const double cc = std::cos (1.05), sn = std::sin (1.05); B[0][wi] = (float) proc (0, L + (cc * eL - sn * eR) * fb); B[1][wi] = (float) proc (1, R + (sn * eL + cc * eR) * fb); }
            else if (pr.fx == FX_RITMO) { B[0][wi] = (float) proc (0, L + rfb * fb); B[1][wi] = (float) proc (1, R + rfb * fb); }
            else if (pr.pp) { B[0][wi] = (float) proc (0, x + eR * fb); B[1][wi] = (float) proc (1, eL * fb); }
            else { B[0][wi] = (float) proc (0, L + eL * fb); B[1][wi] = (float) proc (1, R + eR * fb); }
            ++w;
            const double duck = 1 / (1 + duckA * std::min (4.0, env * 6)), g = wet * duck;
            const double yl = L + eL * g, yr = R + eR * g;
            sE += (eL * eL + eR * eR) * g * g;
            double outv[2] = { yl, yr };
            for (int c = 0; c < 2; ++c) { double uu = outv[c]; const double au = std::abs (uu); if (au > 0.89) uu = sgn (uu) * (0.89 + 0.09 * std::tanh ((au - 0.89) / 0.09)); if (bypass) uu = c ? R : L; (c ? oR : oL)[i] = (float) uu; pk = std::max (pk, std::abs (uu)); }
            ph += 1 / dT; if (ph >= 1) { ph -= 1; ++ticks; }
        }
        echo = echo * 0.7 + std::sqrt (sE / n) * 0.3;
        hold = std::max (echo, hold * std::exp (-n / (fs * 1.2)));
        outL = std::max (pk, outL * 0.9); inL = std::max (pin, inL * 0.9);
        vIn.set (inL); vOut.set (outL); vEcho.set (echo); vHold.set (hold); vTicks.store (ticks); vFrozen.store (frozen);
    }
};
}
