// =============================================================================
//  MUTAGEN 変異 — motor de audio (port 1:1 del motor de la vista previa web)
//  gross (tiempo/volumen/química) → granos → destrozo → filtro → rack → eco/reverb → máster
// =============================================================================
#pragma once
#include "Params.h"
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <array>

namespace mg {

static constexpr double PI = 3.14159265358979323846;
inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline double clampd (double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }

inline float toN (int id, float v)
{
    const auto& d = PINFO[id];
    if (d.curve == 1) return std::log (v / d.min) / std::log (d.max / d.min);
    return (v - d.min) / (d.max - d.min);
}
inline float fromN (int id, float n)
{
    const auto& d = PINFO[id];
    n = clampf (n, 0.f, 1.f);
    float v = d.curve == 1 ? d.min * std::pow (d.max / d.min, n) : d.min + n * (d.max - d.min);
    return d.curve == 2 ? std::round (v) : v;
}

// ------------------------------------------------------------------ tablas
static const double GSTEP[3] = { 0.125, 0.25, 0.5 };
static const double DIVS[12] = { 1.0 / 24, 1.0 / 16, 1.0 / 12, 1.0 / 8, 1.0 / 6, 1.0 / 4, 1.0 / 3, 1.0 / 2, 2.0 / 3, 1, 2, 4 };
static const double DSYNC[7] = { 0, 1, 0.75, 0.5, 1.0 / 3, 0.25, 1.5 };
static const double RETRIG[5] = { 0, 1, 2, 4, 8 };
static const double GDS[6] = { 0, 0.25, 0.5, 0.75, 1, 2 };
static const double MDIVS[14] = { 1.0 / 24, 1.0 / 16, 1.0 / 8, 1.0 / 6, 1.0 / 4, 1.0 / 3, 1.0 / 2, 2.0 / 3, 1, 2, 4, 8, 16, 32 };
static const float VOWELS[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };

// ------------------------------------------------------------------ estado "complejo" (lo que no son mandos)
struct Node { float x, y, c; };
enum Src { S_A, S_B, S_SEG, S_X, S_Y, NSRC };
struct Complex
{
    int pat[3][16] {};                     // tiempo, volumen, química
    std::vector<Node> nodes[2];            // MOV A / MOV B
    bool msync[2] { true, true };
    int mdiv[2] { 10, 9 };
    float mhz[2] { 1.f, 1.f };
    float hum[2] { 0.f, 0.f };
    float segAtk = 0.01f, segRel = 0.25f, segGain = 0.5f;
    std::vector<std::pair<int, float>> assign[NSRC];
    int mask[12] { 0, 2, 3, 5, 7, 8, 10 };
    int maskN = 7;
    bool quant = true;
    Complex()
    {
        nodes[0] = { { 0, .5f, -.5f }, { .25f, 1, .5f }, { .5f, .5f, -.5f }, { .75f, 0, .5f }, { 1, .5f, 0 } };
        nodes[1] = { { 0, 0, 0 }, { .5f, 1, 0 }, { 1, 0, 0 } };
        assign[S_X] = { { p_drive, 0.45f }, { p_low, 0.15f } };
        assign[S_Y] = { { p_glevel, 0.6f }, { p_rmix, 0.35f }, { p_size, 0.25f } };
    }
};

inline float envAt (const std::vector<Node>& nodes, float ph, const float* jit)
{
    const int n = (int) nodes.size();
    if (n < 2) return n ? nodes[0].y : 0.f;
    int i = 0; while (i < n - 2 && ph >= nodes[(size_t) i + 1].x) ++i;
    const auto& a = nodes[(size_t) i]; const auto& b = nodes[(size_t) i + 1];
    const float t = b.x > a.x ? clampf ((ph - a.x) / (b.x - a.x), 0.f, 1.f) : 1.f, c = a.c;
    const float tt = c >= 0 ? std::pow (t, 1 + c * 4) : 1 - std::pow (1 - t, 1 - c * 4);
    const float ya = a.y + (jit ? jit[i] : 0.f), yb = b.y + (jit ? jit[i + 1] : 0.f);
    return ya + (yb - ya) * tt;
}

// ------------------------------------------------------------------ utilidades DSP
struct SV { float ic1 = 0, ic2 = 0; };
struct SC { float a1 = 0, a2 = 0, a3 = 0, k = 0; };
inline SC svfCoef (float fc, float k, float sr)
{
    const float g = (float) std::tan (PI * clampf (fc, 20.f, sr * 0.45f) / sr);
    const float a1 = 1.f / (1.f + g * (g + k));
    return { a1, g * a1, g * g * a1, k };
}
struct SO { float lp, bp, hp; };
inline SO svfRun (SV& f, const SC& c, float x)
{
    const float v3 = x - f.ic2, v1 = c.a1 * f.ic1 + c.a2 * v3, v2 = f.ic2 + c.a2 * f.ic1 + c.a3 * v3;
    f.ic1 = 2 * v1 - f.ic1; f.ic2 = 2 * v2 - f.ic2;
    return { v2, v1, x - c.k * v1 - v2 };
}
inline float svfLP (SV& f, const SC& c, float x) { return svfRun (f, c, x).lp; }
struct BQ { float z1 = 0, z2 = 0; };
inline float bqRun (BQ& s, const float* c, float x) { const float y = c[0] * x + s.z1; s.z1 = c[1] * x - c[3] * y + s.z2; s.z2 = c[2] * x - c[4] * y; return y; }

inline float rd (const std::vector<float>& buf, double pos)
{
    const int64_t B = (int64_t) buf.size();
    const double fl = std::floor (pos); const float f = (float) (pos - fl);
    int64_t a = ((int64_t) fl % B + B) % B, b = (a + 1) % B;
    return buf[(size_t) a] + (buf[(size_t) b] - buf[(size_t) a]) * f;
}
inline float tri (float v) { const float u = v * 0.25f + 0.25f; return 1 - 4 * std::abs (u - std::floor (u) - 0.5f); }
inline float shapeFn (float v, int ty)
{
    switch (ty)
    {
        case 0: return std::tanh (v);
        case 1: { const float y = std::tanh (2 * v) * 1.25f; return clampf (y, -0.92f, 0.92f); }
        case 2: return std::sin (v * 1.3f);
        case 3: return tri (v);
        case 4: { const float y = std::tanh (v); return 1.4f * std::abs (y) - 0.25f * y; }
        case 5: { const float c = clampf (v * 0.8f, -1.5f, 1.5f); return c - c * c * c / 6.75f; }
        case 6: return std::tanh (v * 0.8f);
        default: { const float y = tri (std::sin (v * 1.6f) * 2.4f); return clampf (y, -0.9f, 0.9f); }
    }
}
inline float win (float t, float s)
{
    const float hann = 0.5f - 0.5f * std::cos (6.2832f * t);
    if (s < 0.5f) { const float perc = t < 0.03f ? t / 0.03f : std::exp (-(t - 0.03f) * 5); return lerpf (perc, hann, s * 2); }
    const float a = 0.08f, flat = t < a ? t / a : (t > 1 - a ? (1 - t) / a : 1.f);
    return lerpf (hann, flat, (s - 0.5f) * 2);
}

struct Rng { uint32_t s = 0x9E3779B9u; float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.0f / 16777216.0f); } };

struct Grain { double start; float len, ph, rate, amp, gl, gr, shape; };
struct GrainEvt { float posN, st, len, pan; int rev; };
struct Comb { std::vector<float> b; size_t i = 0; float s = 0; };
struct FxSlot
{
    std::vector<float> bl, br; size_t w = 0; double ph = 0, ph2 = 0; float apl[6] {}, apr[6] {}; float fl = 0, fr = 0, k = 0; int cnt = 0; float hp = 0, hl = 0, hr = 0;
    void init (double sr) { bl.assign ((size_t) (sr * 0.05), 0.f); br.assign ((size_t) (sr * 0.05), 0.f); }
};

// ------------------------------------------------------------------ visualización (motor → interfaz)
struct Viz
{
    std::atomic<float> level { 0 }, inRms { 0 }, outRms { 0 }, mk { 1 }, gr { 0 }, lim { 1 }, stepFrac { 0 };
    std::atomic<int> step { -1 }, activeGrains { 0 };
    std::atomic<float> mv[NSRC] {};
    std::atomic<float> mph[2] {};
    std::atomic<float> q[NP] {};
    static constexpr int SCN = 1024;
    float scIn[SCN] {}, scOut[SCN] {};
    std::atomic<int> scI { 0 };
    static constexpr int GE = 256;
    GrainEvt ge[GE] {};
    std::atomic<uint32_t> geW { 0 };
};

// ------------------------------------------------------------------ motor
class Engine
{
public:
    float P[NP] {}, Q[NP] {};
    Complex cx;
    Viz viz;
    double t = 0;                // posición en muestras (la fija el host si está sonando)
    double bpm = 140;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (int i = 0; i < NP; ++i) { P[i] = Q[i] = PINFO[i].def; }
        B = (size_t) (sr * 4); GB = (size_t) (sr * 12);
        bufL.assign (B, 0.f); bufR.assign (B, 0.f); gL.assign (GB, 0.f); gR.assign (GB, 0.f);
        gdl.assign ((size_t) (sr * 4.2), 0.f); gdr.assign ((size_t) (sr * 4.2), 0.f);
        combL.assign ((size_t) (sr / 25), 0.f); combR.assign ((size_t) (sr / 25), 0.f);
        dl.assign ((size_t) (sr * 3), 0.f); dr.assign ((size_t) (sr * 3), 0.f);
        const int cm[4] = { 1116, 1188, 1277, 1356 }, ap[2] = { 556, 441 };
        for (int i = 0; i < 4; ++i) { cL[i].b.assign ((size_t) std::lround (cm[i] * sr / 44100.0), 0.f); cR[i].b.assign ((size_t) std::lround (cm[i] * sr / 44100.0) + 23, 0.f); cL[i].i = cR[i].i = 0; cL[i].s = cR[i].s = 0; }
        for (int i = 0; i < 2; ++i) { aL[i].b.assign ((size_t) std::lround (ap[i] * sr / 44100.0), 0.f); aR[i].b.assign ((size_t) std::lround (ap[i] * sr / 44100.0) + 23, 0.f); aL[i].i = aR[i].i = 0; }
        const double Qs[2] = { 0.5412, 1.3066 };
        for (int j = 0; j < 2; ++j) { const double w0 = 2 * PI * 0.45 / 2, c = std::cos (w0), al = std::sin (w0) / (2 * Qs[j]), a0 = 1 + al;
            osC[j][0] = (float) ((1 - c) / 2 / a0); osC[j][1] = (float) ((1 - c) / a0); osC[j][2] = osC[j][0]; osC[j][3] = (float) (-2 * c / a0); osC[j][4] = (float) ((1 - al) / a0); }
        fx1.init (sr); fx2.init (sr);
        grains.clear(); grains.reserve (160);
        reset();
    }
    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.f); std::fill (bufR.begin(), bufR.end(), 0.f);
        std::fill (gL.begin(), gL.end(), 0.f); std::fill (gR.begin(), gR.end(), 0.f);
        std::fill (dl.begin(), dl.end(), 0.f); std::fill (dr.begin(), dr.end(), 0.f);
        std::fill (gdl.begin(), gdl.end(), 0.f); std::fill (gdr.begin(), gdr.end(), 0.f);
        for (auto* c : { &cL[0], &cL[1], &cL[2], &cL[3], &cR[0], &cR[1], &cR[2], &cR[3], &aL[0], &aL[1], &aR[0], &aR[1] }) { std::fill (c->b.begin(), c->b.end(), 0.f); c->s = 0; }
        grains.clear(); w = gw = 0; lastStep = -1; xf = 0; lp = 0; lr = 1; vs = 1; lim = 1; mk = 0.2f; rIn = 1e-4f; rOut = 1e-2f; rN = 0.004f;
        dT = 0.33f * sr; fxCut = 20000; fxC = svfCoef (19000, 0.9f, sr);
    }

    // procesa un bloque (cualquier tamaño): modulación cada 64 muestras
    void process (const float* inL, const float* inR, float* oL, float* oR, int N)
    {
        for (int o = 0; o < N; o += 64)
        {
            const int n = std::min (64, N - o);
            updateMods (inL + o, inR + o, n);
            core (inL + o, inR + o, oL + o, oR + o, n);
        }
        for (int i = 0; i < NP; ++i) viz.q[i].store (Q[i], std::memory_order_relaxed);
        for (int k = 0; k < NSRC; ++k) viz.mv[k].store (MV[k], std::memory_order_relaxed);
        viz.mph[0].store (mph[0], std::memory_order_relaxed); viz.mph[1].store (mph[1], std::memory_order_relaxed);
    }

    // datos de lectura para el visor de granos
    const std::vector<float>& grainBuffer() const { return bufL; }
    size_t grainWrite() const { return w; }
    float sampleRate() const { return sr; }

private:
    float sr = 44100;
    size_t B = 0, GB = 0;
    std::vector<float> bufL, bufR, gL, gR, gdl, gdr, combL, combR, dl, dr;
    std::vector<Grain> grains;
    Rng rng;
    float MV[NSRC] {};
    // modulación
    double macc[2] {}; int64_t mcyc[2] { -1, -1 }; float jit[2][64] {}; float mph[2] {}; float segEnv = 0;
    // gross
    size_t gw = 0, w = 0; int64_t lastStep = -1; int tcS = 0, vcS = 0, fcS = 0, tr0 = 0, tn = 1, vr0 = 0, vn = 1;
    double lp = 0, lr = 1, xp = 0, xr = 1, anchor = 0; int xf = 0, tcPrev = 0; double shPh = 0; float vs = 1;
    float mDrive = 0, mCrush = 0, mGrain = 0, fxCut = 20000, throwS = 0, throwHold = 0; SC fxC {};
    double tNext = 0, beatPos = 0, sback = 0; float fbL = 0, fbR = 0, fbLp = 0, fbRp = 0; size_t gdw = 0;
    // destrozo
    float hpL = 0, hpR = 0, hxL = 0, hxR = 0, osPL = 0, osPR = 0; BQ os[2][2]; float osC[2][5] {};
    float holdL = 0, holdR = 0, holdPh = 0; SV tn2[2]; float dcxL = 0, dcyL = 0, dcxR = 0, dcyR = 0, rIn = 1e-4f, rOut = 1e-2f, mk = 0.2f, rN = 0.004f;
    // filtro
    SV f[2], f2[2], f3[2], fxs[2]; float cutS = 2400; size_t cw = 0;
    // espacio
    size_t dw = 0; float dT = 0; Comb cL[4], cR[4], aL[2], aR[2];
    FxSlot fx1, fx2; float fo0 = 0, fo1 = 0;
    SV mh[2], ml[2]; float cenv = 0, cgr = 0, lim = 1, env = 0;
    int scI = 0;

    float snapScale (float st) const
    {
        const float base = std::floor (st / 12) * 12, r = st - base;
        float best = 0, bd = 99;
        for (int i = 0; i < cx.maskN; ++i) for (int o : { -12, 0, 12 }) { const float d = std::abs (r - (cx.mask[i] + o)); if (d < bd) { bd = d; best = (float) (cx.mask[i] + o); } }
        return base + best;
    }
    static void runInfo (const int* lane, int i, int& r0, int& n)
    {
        int a = i; while (a > 0 && lane[a - 1] == lane[i]) --a;
        int b = i; while (b < 15 && lane[b + 1] == lane[i]) ++b;
        r0 = a; n = b - a + 1;
    }

    void updateMods (const float* inL, const float* inR, int n)
    {
        const double beatLen = sr * 60.0 / bpm;
        for (int k = 0; k < 2; ++k)
        {
            double ph; int64_t cyc;
            if (cx.msync[k]) { const double c = std::max (0.0, t) / (beatLen * MDIVS[std::clamp (cx.mdiv[k], 0, 13)]); ph = c - std::floor (c); cyc = (int64_t) std::floor (c); }
            else { macc[k] += cx.mhz[k] * n / sr; ph = macc[k] - std::floor (macc[k]); cyc = (int64_t) std::floor (macc[k]); }
            const auto& nodes = cx.nodes[k];
            if (cyc != mcyc[k]) { mcyc[k] = cyc; for (size_t i = 0; i < nodes.size() && i < 64; ++i) jit[k][i] = (rng.next() * 2 - 1) * cx.hum[k] * 0.45f; }
            mph[k] = (float) ph;
            MV[k] = clampf (envAt (nodes, (float) ph, jit[k]), 0.f, 1.f);
        }
        float pk = 0; for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (inL[i]), std::abs (inR[i])));
        const float co = pk > segEnv ? 1 - std::exp (-n / (std::max (0.001f, cx.segAtk) * sr)) : 1 - std::exp (-n / (std::max (0.01f, cx.segRel) * sr));
        segEnv += (pk - segEnv) * co;
        MV[S_SEG] = clampf (segEnv * (0.6f + cx.segGain * 9), 0.f, 1.f);
        MV[S_X] = P[p_xyx] * 2 - 1; MV[S_Y] = P[p_xyy] * 2 - 1;
        std::memcpy (Q, P, sizeof (P));
        float acc[NP] {}; bool touched[NP] {};
        for (int s = 0; s < NSRC; ++s) for (auto& a : cx.assign[s]) if (a.first >= 0 && a.first < NP) { acc[a.first] += a.second * MV[s]; touched[a.first] = true; }
        for (int id = 0; id < NP; ++id) if (touched[id] && PINFO[id].curve != 2) Q[id] = fromN (id, toN (id, P[id]) + acc[id]);
    }

    void fxRun (FxSlot& st, int type, float a, float b, float mix, float xl, float xr, double beatLen)
    {
        float wl = xl, wr = xr; const double BL = (double) st.bl.size();
        switch (type)
        {
            case 1: {
                st.ph += 0.1 * std::pow (30.0, a) / sr; if (st.ph > 1) st.ph -= 1;
                const float s1 = (float) std::sin (st.ph * 2 * PI), c1 = (float) std::cos (st.ph * 2 * PI);
                st.bl[st.w] = xl; st.br[st.w] = xr;
                const double d1 = (0.012 + 0.008 * b * s1) * sr, d2 = (0.012 + 0.008 * b * c1) * sr;
                wl = rd (st.bl, (double) st.w - d1); wr = rd (st.br, (double) st.w - d2); st.w = (st.w + 1) % st.bl.size();
                wl = (xl + wl) * 0.75f; wr = (xr + wr) * 0.75f; break; }
            case 2: {
                st.ph += 0.05 * std::pow (40.0, a) / sr; if (st.ph > 1) st.ph -= 1;
                const float lf = 0.5f + 0.5f * (float) std::sin (st.ph * 2 * PI);
                if ((st.cnt++ & 7) == 0) { const float fc = 180 * std::pow (2.f, lf * 5.5f), tt = (float) std::tan (PI * std::min (fc, sr * 0.45f) / sr); st.k = (tt - 1) / (tt + 1); }
                const float k = st.k, fb = b * 0.85f;
                float vl = xl + st.fl * fb, vr = xr + st.fr * fb;
                for (int i = 0; i < 6; ++i) { const float y1 = k * vl + st.apl[i]; st.apl[i] = vl - k * y1; vl = y1; const float y2 = k * vr + st.apr[i]; st.apr[i] = vr - k * y2; vr = y2; }
                st.fl = vl; st.fr = vr; wl = (xl + vl) * 0.6f; wr = (xr + vr) * 0.6f; break; }
            case 3: {
                st.ph += 0.05 * std::pow (20.0, a) / sr; if (st.ph > 1) st.ph -= 1;
                const float lf = 0.5f + 0.5f * (float) std::sin (st.ph * 2 * PI), lf2 = 0.5f + 0.5f * (float) std::cos (st.ph * 2 * PI);
                const double d1 = (0.0005 + 0.004 * lf) * sr, d2 = (0.0005 + 0.004 * lf2) * sr; const float fb = b * 0.9f;
                const float o1 = rd (st.bl, (double) st.w - d1), o2 = rd (st.br, (double) st.w - d2);
                st.bl[st.w] = xl + o1 * fb; st.br[st.w] = xr + o2 * fb; st.w = (st.w + 1) % st.bl.size();
                wl = (xl + o1) * 0.6f; wr = (xr + o2) * 0.6f; break; }
            case 4: {
                st.ph2 += 0.3 / sr; if (st.ph2 > 1) st.ph2 -= 1;
                const double fq = 30 * std::pow (100.0, a) * (1 + b * 0.5 * std::sin (st.ph2 * 2 * PI));
                st.ph += fq / sr; if (st.ph > 1) st.ph -= 1; const float c = (float) std::sin (st.ph * 2 * PI);
                wl = xl * c; wr = xr * c; break; }
            case 5: {
                static const double dv[6] = { 0.25, 0.5, 1, 2, 4, 8 };
                const double L = beatLen * dv[std::min (5, (int) std::floor (a * 5.99f))];
                const double ph = std::fmod (std::max (0.0, t), L) / L; const float sn = (float) std::sin (ph * 2 * PI);
                wl = xl * (1 - b * (0.5f + 0.5f * sn)) * 1.2f; wr = xr * (1 - b * (0.5f - 0.5f * sn)) * 1.2f; break; }
            case 6: {
                st.hp += 1 / (1 + b * b * 30); if (st.hp >= 1) { st.hp -= 1; const float lv = std::pow (2.f, 15 - 13 * a); st.hl = std::round (xl * lv) / lv; st.hr = std::round (xr * lv) / lv; }
                wl = st.hl; wr = st.hr; break; }
            default: break;
        }
        (void) BL;
        fo0 = xl + (wl - xl) * mix; fo1 = xr + (wr - xr) * mix;
    }

    void core (const float* inL, const float* inR, float* oL, float* oR, int N)
    {
        const float* q = Q;
        const double beatLen = sr * 60.0 / bpm, SL = beatLen * GSTEP[std::clamp ((int) std::lround (q[p_gstep]), 0, 2)];
        const bool gon = q[p_gon] > 0.5f; const float gmix = q[p_gmix], vdepth = q[p_vdepth]; const int XFN = std::max (8, (int) std::lround (q[p_xfade] * sr / 1000));
        const bool sync = q[p_sync] > 0.5f; const double gstepS = beatLen * DIVS[std::clamp ((int) std::lround (q[p_div]), 0, 11)];
        const double densBase = sync ? sr / gstepS : clampd (q[p_density], 0.5, 120);
        const double sizeS = clampd (q[p_size], 0.005, 1.0) * sr;
        const int voices = std::clamp ((int) std::lround (q[p_count]), 1, 8); const double speed = 1 - 0.95 * q[p_stretch], rtLen = beatLen * RETRIG[std::clamp ((int) std::lround (q[p_retrig]), 0, 4)];
        const int gdi = std::clamp ((int) std::lround (q[p_gdsync]), 0, 5); const bool gdOn = gdi > 0; const double gdT = beatLen * GDS[gdi];
        const bool don = q[p_don] > 0.5f, autog = q[p_autog] > 0.5f; const int dty = std::clamp ((int) std::lround (q[p_dtype]), 0, 7);
        const float hpA = std::exp (-2 * (float) PI * q[p_low] / sr); const SC toneC = svfCoef (q[p_tone], 1.3f, sr); const float asym = q[p_asym] * 0.7f;
        const float ak = 1 - std::exp (-1 / (0.15f * sr)), mkk = 1 - std::exp (-1 / (0.3f * sr)), mkf = 1 - std::exp (-1 / (0.008f * sr)), akN = 1 - std::exp (-1 / (1.2f * sr));
        const int ft = std::clamp ((int) std::lround (q[p_ftype]), 0, 7); const float k = 2 - 1.96f * q[p_reso];
        const int dsi = std::clamp ((int) std::lround (q[p_dsync]), 0, 6); const float dTarget = (float) (dsi > 0 ? beatLen * DSYNC[dsi] : q[p_dtime] * sr);
        const float rfb = 0.7f + 0.28f * q[p_rsize], damp = 0.25f;
        const float width = q[p_width] * 2, mix = q[p_mix], outG = std::pow (10.f, q[p_output] / 20), ing = std::pow (10.f, q[p_ingain] / 20);
        const float envA = std::exp (-1 / (0.005f * sr)), envR = std::exp (-1 / (0.15f * sr));
        const float vk = 1 - std::exp (-1 / (0.0015f * sr)), mk2 = 1 - std::exp (-1 / (0.003f * sr)), thD = std::exp (-1 / (1.8f * sr));
        const int fx1t = std::clamp ((int) std::lround (q[p_fx1t]), 0, 6), fx2t = std::clamp ((int) std::lround (q[p_fx2t]), 0, 6);
        const bool mhpOn = q[p_mhp] > 21, mlpOn = q[p_mlp] < 19800; const SC mhpC = svfCoef (q[p_mhp], 1.41f, sr), mlpC = svfCoef (q[p_mlp], 1.41f, sr);
        const bool compOn = q[p_comp] > 0.01f; const float cThr = -8 - 26 * q[p_comp], cRat = 1 + 9 * q[p_comp], cMk = std::pow (10.f, (-cThr * (1 - 1 / cRat) * 0.45f) / 20);
        const float compA = std::exp (-1 / (0.003f * sr)), compR = std::exp (-1 / (0.15f * sr)), limR = 1 - std::exp (-1 / (0.08f * sr));
        const bool rfz = q[p_rfreeze] > 0.5f, frz = q[p_freeze] > 0.5f, byp = q[p_bypass] > 0.5f;
        SC fc1 {}, fc2 {}, fc3 {}; bool haveC = false; double combLen = 100; float combFb = 0;
        float peak = 0, inSq = 0, outSq = 0;

        for (int n = 0; n < N; ++n)
        {
            const float xL = inL[n], xR = inR[n];
            const float a_L = xL * ing, a_R = xR * ing, ax = std::max (std::abs (a_L), std::abs (a_R));
            inSq += xL * xL;
            env = ax > env ? envA * env + (1 - envA) * ax : envR * env + (1 - envR) * ax;

            // ================= GROSS
            gL[gw] = a_L; gR[gw] = a_R;
            const int64_t stepAbs = (int64_t) std::floor (t / SL); const int si = (int) (((stepAbs % 16) + 16) % 16); const double ds = t - stepAbs * SL;
            if (stepAbs != lastStep)
            {
                lastStep = stepAbs;
                tcS = cx.pat[0][si]; vcS = cx.pat[1][si]; fcS = cx.pat[2][si];
                runInfo (cx.pat[0], si, tr0, tn); runInfo (cx.pat[1], si, vr0, vn);
                if (tcS == 1 && (si == 0 || cx.pat[0][si - 1] != 1)) anchor = (double) gw - ds - SL;
                viz.step.store (si, std::memory_order_relaxed);
            }
            const int tc = gon ? tcS : 0;
            const double dRun = (si - tr0) * SL + ds, LRun = tn * SL, uRun = dRun / LRun, uStep = ds / SL;
            const double gwd = (double) gw;
            double pos = gwd, rate = 1; bool shifting = false; float sL, sR, freno = 1;
            switch (tc)
            {
                case 1: pos = anchor + std::fmod (dRun, SL); break;
                case 2: pos = gwd - 2 * dRun; break;
                case 3: pos = gwd - dRun * 0.5; break;
                case 4: pos = gwd - dRun * uRun * 0.5; freno = (float) (1 - uRun * uRun); break;
                case 5: { const double A = 0.14 * SL; pos = gwd - A * (1 - std::cos (2 * PI * 2 * uStep)); break; }
                case 6: pos = gwd - 3 * dRun * uRun; break;
                case 7: pos = gwd - ds + std::fmod (ds, SL / 2); break;
                case 8: pos = gwd - ds + std::fmod (ds, SL / 4); break;
                case 9: case 10: case 11: shifting = true; break;
                default: break;
            }
            if (shifting)
            {
                const double ratio = tc == 9 ? 2 : tc == 10 ? 0.5 : 1.4983, Wd = (ratio > 1 ? 0.035 : 0.06) * sr;
                shPh += std::abs (1 - ratio) / Wd; if (shPh >= 1) shPh -= 1;
                const double ph1 = shPh, ph2 = std::fmod (ph1 + 0.5, 1.0);
                const double lag1 = ratio > 1 ? Wd * (1 - ph1) + 2 : Wd * ph1 + 2, lag2 = ratio > 1 ? Wd * (1 - ph2) + 2 : Wd * ph2 + 2;
                const float g1 = (float) std::pow (std::sin (PI * ph1), 2), g2 = (float) std::pow (std::sin (PI * ph2), 2);
                sL = rd (gL, gwd - lag1) * g1 + rd (gL, gwd - lag2) * g2;
                sR = rd (gR, gwd - lag1) * g1 + rd (gR, gwd - lag2) * g2;
                pos = gwd - (g1 > g2 ? lag1 : lag2); rate = ratio;
            }
            else
            {
                pos = std::max (gwd - (double) GB + 8, std::min (gwd, pos));
                sL = rd (gL, pos); sR = rd (gR, pos);
            }
            const bool jumped = tc != tcPrev || (! shifting && std::abs (pos - (lp + lr)) > 2);
            if (jumped) { xf = XFN; xp = lp + lr; xr = clampd (lr, -4, 4); }
            if (! shifting) rate = jumped ? lr : pos - lp;
            if (xf > 0)
            {
                const float a = (float) xf / XFN; const double xpp = std::min (xp, gwd);
                sL = sL * (1 - a) + rd (gL, xpp) * a; sR = sR * (1 - a) + rd (gR, xpp) * a;
                xp += xr; --xf;
            }
            lp = pos; lr = rate; tcPrev = tc;
            if (gmix < 1) { sL = a_L + (sL * freno - a_L) * gmix; sR = a_R + (sR * freno - a_R) * gmix; } else { sL *= freno; sR *= freno; }
            float vv = 1;
            if (gon)
            {
                const double uv = ((si - vr0) * SL + ds) / (vn * SL);
                switch (vcS)
                {
                    case 1: vv = 0; break;
                    case 2: { const double uq = ((si % 4) * SL + ds) / (4 * SL); vv = (float) std::min (1.0, uq / 0.5); vv *= vv; break; }
                    case 3: vv = std::fmod (ds, SL / 4) < SL / 8 ? 1.f : 0.f; break;
                    case 4: vv = ds < SL * 0.4 ? 1.f : 0.f; break;
                    case 5: vv = (float) std::pow (clampd (uv, 0, 1), 1.5); break;
                    case 6: vv = (float) std::pow (clampd (1 - uv, 0, 1), 1.5); break;
                    default: break;
                }
                vv = 1 - vdepth * gmix * (1 - vv);
            }
            vs += (vv - vs) * vk;
            const int fcode = gon ? fcS : 0;
            mDrive += ((fcode == 1 ? 0.45f : 0.f) - mDrive) * mk2;
            mCrush += ((fcode == 2 ? 0.9f : 0.f) - mCrush) * mk2;
            mGrain += ((fcode == 5 ? 1.f : 0.f) - mGrain) * mk2;
            const float thrOn = fcode == 6 ? 1.f : 0.f;
            throwS += (thrOn - throwS) * mk2;
            throwHold = thrOn > 0 ? 0.85f : throwHold * thD;
            if ((n & 15) == 0)
            {
                const float tgt = fcode == 3 ? 14000 * (float) std::pow (2.0, -6.5 * uStep) : fcode == 4 ? 160 * (float) std::pow (2.0, 6.5 * uStep) : 20000.f;
                fxCut += (tgt - fxCut) * 0.5f;
                fxC = svfCoef (std::min (fxCut, sr * 0.45f), 0.9f, sr);
            }
            gw = (gw + 1) % GB; t += 1;

            // ================= GRANOS
            const float glev = std::min (1.f, q[p_glevel] + mGrain);
            if (q[p_stretch] > 0.01f) { sback += 1 - speed; if (rtLen > 0 && std::fmod (t, rtLen) < 1) sback = 0; if (sback > 3.2 * sr) sback = 0; } else sback = 0;
            float fbl = fbL, fbr = fbR;
            if (gdOn) { const double rp2 = (double) gdw - gdT; fbl = rd (gdl, rp2); fbr = rd (gdr, rp2); gdl[gdw] = fbL; gdr[gdw] = fbR; gdw = (gdw + 1) % gdl.size(); }
            fbLp += (fbl - fbLp) * 0.35f; fbRp += (fbr - fbRp) * 0.35f;
            if (! frz) { bufL[w] = sL + std::tanh (fbLp * q[p_feedback]); bufR[w] = sR + std::tanh (fbRp * q[p_feedback]); w = (w + 1) % B; }
            float wl = 0, wr = 0;
            if (glev > 0.002f || ! grains.empty())
            {
                bool fire = false;
                if (sync) { beatPos += 1; if (beatPos >= gstepS) { beatPos -= gstepS; fire = true; } }
                else if (--tNext <= 0) { tNext += sr / (densBase * (1 + mGrain)) * (0.65 + rng.next() * 0.7); fire = true; }
                if (fire && glev > 0.002f && grains.size() < 128)
                    for (int vo = 0; vo < voices; ++vo)
                    {
                        float st = std::round (q[p_pitch]) + q[p_spread] * (rng.next() * 2 - 1);
                        if (cx.quant) st = snapScale (st);
                        if (rng.next() < q[p_octave]) st += rng.next() < 0.6f ? 12.f : -12.f;
                        const float rate2 = std::pow (2.f, (st + q[p_detune] * (rng.next() * 2 - 1) / 100) / 12);
                        const bool rev = rng.next() < q[p_reverse];
                        const float posN = clampf (q[p_position] + q[p_spray] * (rng.next() * 2 - 1) * 0.5f, 0, 1);
                        const double lenRaw = clampd (sizeS * (sync ? 1 : 0.85 + rng.next() * 0.3) * (1 + q[p_sizernd] * (rng.next() * 1.8 - 0.9)), 0.004 * sr, sr);
                        const double back = std::min (posN * 2.0 * sr + sback + vo * q[p_offset] * lenRaw * 0.5, B * 0.9);
                        const double len = std::min (lenRaw, ((double) B - back - 16) / std::max (1.f, rate2));
                        if (len < 64) continue;
                        const double start = rev ? (double) w - back - 4 : (double) w - back - len * std::max (1.f, rate2) - 4;
                        const float pan = clampf ((rng.next() * 2 - 1) * q[p_width], -1, 1); const float pa = (pan + 1) * (float) PI / 4;
                        const float ampBase = 1.5f / (float) std::sqrt (1 + densBase * sizeS / sr * 0.7 * voices);
                        grains.push_back ({ start, (float) len, 0.f, rev ? -rate2 : rate2, ampBase * (sync ? 1.3f : 1.f), std::cos (pa), std::sin (pa), q[p_shape] });
                        if (vo == 0)
                        {
                            const uint32_t wi = viz.geW.load (std::memory_order_relaxed);
                            viz.ge[wi % Viz::GE] = { posN, st, (float) (len / sr), pan, rev ? 1 : 0 };
                            viz.geW.store (wi + 1, std::memory_order_release);
                        }
                    }
                for (size_t i = grains.size(); i-- > 0;)
                {
                    auto& g = grains[i];
                    const float wv = win (g.ph / g.len, g.shape) * g.amp; const double gp = g.start + (double) g.ph * g.rate;
                    wl += rd (bufL, gp) * wv * g.gl; wr += rd (bufR, gp) * wv * g.gr;
                    if (++g.ph >= g.len) { g = grains.back(); grains.pop_back(); }
                }
                fbL = wl; fbR = wr;
                const float dk = 1 - q[p_duck] * clampf (env * 4, 0, 1);
                wl *= glev * dk; wr *= glev * dk;
            }
            float yl = sL + wl, yr = sR + wr;

            // ================= DESTROZO
            const float crush = std::max (dty == 6 && don ? q[p_drive] : (dty == 7 && don ? 0.55f : 0.f), mCrush);
            if (don || mDrive > 0.01f || crush > 0.01f)
            {
                const float mL = yl, mR = yr;
                hpL = hpA * (hpL + yl - hxL); hxL = yl; hpR = hpA * (hpR + yr - hxR); hxR = yr;
                const float drv = clampf ((don ? q[p_drive] : 0.f) + mDrive, 0, 1);
                const float pn = autog ? clampf (0.125f / std::sqrt (rN + 1e-9f), 0.5f, 10) : 1.f;
                const float kk = std::pow (10.f, drv * 2.3f) * (dty == 7 ? 1.6f : 1.f) * pn; const int ty = don ? dty : 0;
                float ol, orr;
                if (don || mDrive > 0.01f)
                {
                    const float iL = hpL, iR = hpR;
                    float a1 = shapeFn (((osPL + iL) * 0.5f) * kk + asym, ty), a2 = shapeFn (iL * kk + asym, ty);
                    a1 = bqRun (os[0][1], osC[1], bqRun (os[0][0], osC[0], a1)); a2 = bqRun (os[0][1], osC[1], bqRun (os[0][0], osC[0], a2));
                    float b1 = shapeFn (((osPR + iR) * 0.5f) * kk + asym, ty), b2 = shapeFn (iR * kk + asym, ty);
                    b1 = bqRun (os[1][1], osC[1], bqRun (os[1][0], osC[0], b1)); b2 = bqRun (os[1][1], osC[1], bqRun (os[1][0], osC[0], b2));
                    (void) a1; (void) b1;
                    osPL = iL; osPR = iR; ol = a2; orr = b2;
                }
                else { ol = yl; orr = yr; }
                if (crush > 0.01f)
                {
                    holdPh += 1 / (1 + crush * crush * 22);
                    if (holdPh >= 1) { holdPh -= 1; const float lv = std::pow (2.f, 13 - 11 * crush); holdL = std::round (ol * lv) / lv; holdR = std::round (orr * lv) / lv; }
                    ol = holdL; orr = holdR;
                }
                ol = svfLP (tn2[0], toneC, ol); orr = svfLP (tn2[1], toneC, orr);
                const float yL2 = ol - dcxL + 0.995f * dcyL; dcxL = ol; dcyL = yL2; const float yR2 = orr - dcxR + 0.995f * dcyR; dcxR = orr; dcyR = yR2;
                const float pin = mL * mL + mR * mR;
                if (pin > 2e-6f) { rIn += (pin - rIn) * ak; rOut += (yL2 * yL2 + yR2 * yR2 - rOut) * ak; rN += (pin * 0.5f - rN) * akN; }
                const float tg = autog ? clampf (std::sqrt ((rIn + 1e-7f) / (rOut + 1e-7f)), 0.05f, 3) : 0.6f / (1 + drv);
                mk += (tg - mk) * (tg < mk ? mkf : mkk);
                const float dw2 = don ? std::max ({ q[p_dwet], mDrive * 2, crush > 0.3f ? 1.f : 0.f }) : 1.f;
                const float dwc = std::min (1.f, dw2);
                yl = mL + (yL2 * mk - mL) * dwc; yr = mR + (yR2 * mk - mR) * dwc;
            }

            // ================= FILTRO
            if (ft > 0)
            {
                if ((n & 31) == 0 || ! haveC)
                {
                    haveC = true;
                    const float envMod = std::pow (2.f, q[p_fenv] * 6 * clampf (env * 3, 0, 1));
                    cutS += (clampf (q[p_cutoff] * envMod, 30, sr * 0.45f) - cutS) * 0.35f;
                    if (ft == 7)
                    {
                        const float vp = clampf (toN (p_cutoff, clampf (cutS, 40, 18000)), 0, 1) * 4; const int vi = std::min (3, (int) std::floor (vp)); const float vf = vp - vi, kq = 0.5f - 0.42f * q[p_reso];
                        fc1 = svfCoef (lerpf (VOWELS[vi][0], VOWELS[vi + 1][0], vf), kq, sr); fc2 = svfCoef (lerpf (VOWELS[vi][1], VOWELS[vi + 1][1], vf), kq, sr); fc3 = svfCoef (lerpf (VOWELS[vi][2], VOWELS[vi + 1][2], vf), kq, sr);
                    }
                    else if (ft == 6) { combLen = clampd (sr / cutS, 2, (double) combL.size() - 2); combFb = 0.5f + 0.48f * q[p_reso]; }
                    else if (ft == 2) { fc1 = svfCoef (cutS, 1.85f, sr); fc2 = svfCoef (cutS, 2 - 1.98f * q[p_reso], sr); }
                    else fc1 = svfCoef (cutS, k, sr);
                }
                float fl, fr;
                if (ft == 1) { fl = svfRun (f[0], fc1, yl).lp; fr = svfRun (f[1], fc1, yr).lp; }
                else if (ft == 2) { fl = svfLP (f2[0], fc2, svfLP (f[0], fc1, yl)); fr = svfLP (f2[1], fc2, svfLP (f[1], fc1, yr)); fl = std::tanh (fl * 1.2f) / 1.1f; fr = std::tanh (fr * 1.2f) / 1.1f; }
                else if (ft == 3) { fl = svfRun (f[0], fc1, yl).hp; fr = svfRun (f[1], fc1, yr).hp; }
                else if (ft == 4) { const float kb = std::sqrt (k) * 1.4f; fl = svfRun (f[0], fc1, yl).bp * kb; fr = svfRun (f[1], fc1, yr).bp * kb; }
                else if (ft == 5) { const auto a = svfRun (f[0], fc1, yl), b = svfRun (f[1], fc1, yr); fl = yl - a.bp * k; fr = yr - b.bp * k; }
                else if (ft == 6)
                {
                    const float dlL = rd (combL, (double) cw - combLen), dlR = rd (combR, (double) cw - combLen);
                    combL[cw] = std::tanh (yl + dlL * combFb); combR[cw] = std::tanh (yr + dlR * combFb); cw = (cw + 1) % combL.size();
                    fl = (yl + dlL * combFb) * (1 - combFb * 0.55f); fr = (yr + dlR * combFb) * (1 - combFb * 0.55f);
                }
                else
                {
                    const float a = svfRun (f[0], fc1, yl).bp, b = svfRun (f2[0], fc2, yl).bp, c = svfRun (f3[0], fc3, yl).bp;
                    const float d = svfRun (f[1], fc1, yr).bp, e2 = svfRun (f2[1], fc2, yr).bp, g2 = svfRun (f3[1], fc3, yr).bp;
                    fl = (a + b * 0.75f + c * 0.4f) * 0.8f; fr = (d + e2 * 0.75f + g2 * 0.4f) * 0.8f;
                }
                yl = fl; yr = fr;
            }
            if (fxCut < 19000) { yl = svfLP (fxs[0], fxC, yl); yr = svfLP (fxs[1], fxC, yr); }
            else { svfLP (fxs[0], fxC, yl); svfLP (fxs[1], fxC, yr); }

            if (fx1t) { fxRun (fx1, fx1t, q[p_fx1a], q[p_fx1b], q[p_fx1m], yl, yr, beatLen); yl = fo0; yr = fo1; }
            if (fx2t) { fxRun (fx2, fx2t, q[p_fx2a], q[p_fx2b], q[p_fx2m], yl, yr, beatLen); yl = fo0; yr = fo1; }
            yl *= vs; yr *= vs;

            // ================= ECO + REVERB
            dT += (dTarget - dT) * 0.0006f;
            const double rp = (double) dw - dT; const float dlo = rd (dl, rp), dro = rd (dr, rp);
            const float send = q[p_dmix] > 0.01f ? 1.f : throwS;
            dl[dw] = yl * send + dro * q[p_dfb]; dr[dw] = yr * send + dlo * q[p_dfb]; dw = (dw + 1) % dl.size();
            const float dg = std::max (q[p_dmix], throwHold);
            yl += dlo * dg; yr += dro * dg;
            if (q[p_rmix] > 0.005f || rfz)
            {
                const float rin = rfz ? 0.f : (yl + yr) * 0.03f, rfb2 = rfz ? 1.f : rfb, dmp = rfz ? 0.f : damp; float rl = 0, rr = 0;
                for (auto& c : cL) { const float o = c.b[c.i]; c.s = o * (1 - dmp) + c.s * dmp; c.b[c.i] = rin + c.s * rfb2; c.i = (c.i + 1) % c.b.size(); rl += o; }
                for (auto& c : cR) { const float o = c.b[c.i]; c.s = o * (1 - dmp) + c.s * dmp; c.b[c.i] = rin + c.s * rfb2; c.i = (c.i + 1) % c.b.size(); rr += o; }
                for (auto& a : aL) { const float bo = a.b[a.i], o = -rl + bo; a.b[a.i] = rl + bo * 0.5f; a.i = (a.i + 1) % a.b.size(); rl = o; }
                for (auto& a : aR) { const float bo = a.b[a.i], o = -rr + bo; a.b[a.i] = rr + bo * 0.5f; a.i = (a.i + 1) % a.b.size(); rr = o; }
                const float rg = std::max (q[p_rmix], rfz ? 0.5f : 0.f) * 1.4f; yl += rl * rg; yr += rr * rg;
            }
            const float m = (yl + yr) * 0.5f, sd = (yl - yr) * 0.5f * width; yl = m + sd; yr = m - sd;
            if (mhpOn) { yl = svfRun (mh[0], mhpC, yl).hp; yr = svfRun (mh[1], mhpC, yr).hp; }
            if (mlpOn) { yl = svfLP (ml[0], mlpC, yl); yr = svfLP (ml[1], mlpC, yr); }
            if (compOn)
            {
                const float lv = std::max (std::abs (yl), std::abs (yr)); cenv = lv > cenv ? compA * cenv + (1 - compA) * lv : compR * cenv + (1 - compR) * lv;
                const float ldb = 20 * std::log10 (cenv + 1e-9f), grd = ldb > cThr ? (ldb - cThr) * (1 - 1 / cRat) : 0.f; cgr = grd;
                const float cg = std::pow (10.f, -grd / 20) * cMk; yl *= cg; yr *= cg;
            }
            else cgr = 0;
            float ol = (xL * (1 - mix) + yl * mix) * outG, orr = (xR * (1 - mix) + yr * mix) * outG;
            if (! std::isfinite (ol) || ! std::isfinite (orr)) { ol = 0; orr = 0; resetIfBroken(); }
            const float lp2 = std::max (std::abs (ol), std::abs (orr)), lt = lp2 > 0.97f ? 0.97f / lp2 : 1.f;
            lim = lt < lim ? lt : lim + (1 - lim) * limR; ol *= lim; orr *= lim;
            ol = clampf (ol, -1, 1); orr = clampf (orr, -1, 1);
            if (byp) { ol = xL; orr = xR; }
            oL[n] = ol; oR[n] = orr;
            peak = std::max (peak, std::max (std::abs (ol), std::abs (orr))); outSq += ol * ol;
            if ((n & 1) == 0) { viz.scIn[scI] = a_L; viz.scOut[scI] = ol; scI = (scI + 1) % Viz::SCN; }
        }
        viz.scI.store (scI, std::memory_order_relaxed);
        viz.level.store (std::max (peak, viz.level.load (std::memory_order_relaxed) * 0.99f), std::memory_order_relaxed);
        viz.inRms.store (viz.inRms.load (std::memory_order_relaxed) * 0.985f + 0.015f * std::sqrt (inSq / N), std::memory_order_relaxed);
        viz.outRms.store (viz.outRms.load (std::memory_order_relaxed) * 0.985f + 0.015f * std::sqrt (outSq / N), std::memory_order_relaxed);
        viz.mk.store (mk, std::memory_order_relaxed); viz.gr.store (cgr, std::memory_order_relaxed); viz.lim.store (lim, std::memory_order_relaxed);
        viz.activeGrains.store ((int) grains.size(), std::memory_order_relaxed);
        viz.stepFrac.store ((float) std::fmod (t / SL, 16.0), std::memory_order_relaxed);
    }

    // si algo explota (NaN), limpia los filtros para que no se quede mudo
    void resetIfBroken()
    {
        for (auto* s : { &f[0], &f[1], &f2[0], &f2[1], &f3[0], &f3[1], &fxs[0], &fxs[1], &tn2[0], &tn2[1], &mh[0], &mh[1], &ml[0], &ml[1] }) *s = {};
        for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) os[i][j] = {};
        std::fill (dl.begin(), dl.end(), 0.f); std::fill (dr.begin(), dr.end(), 0.f);
        std::fill (combL.begin(), combL.end(), 0.f); std::fill (combR.begin(), combR.end(), 0.f);
        for (auto* c : { &cL[0], &cL[1], &cL[2], &cL[3], &cR[0], &cR[1], &cR[2], &cR[3], &aL[0], &aL[1], &aR[0], &aR[1] }) { std::fill (c->b.begin(), c->b.end(), 0.f); c->s = 0; }
        fbL = fbR = fbLp = fbRp = 0; cenv = 0; mk = 0.2f;
    }
};

} // namespace mg
