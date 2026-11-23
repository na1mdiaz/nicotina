// =====================================================================================
//  MUTAGEN 変異 · semilla — motor de sonido (traducción línea a línea de la vista previa web 6.0)
//  Instrumento MIDI: tus acordes entran, MUTAGEN los toca con su sonido o inventa melodía encima.
// =====================================================================================
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include <map>
#include <functional>
#include <climits>

namespace mg {
static constexpr double PI = 3.14159265358979323846, TWO_PI = 2 * PI;
inline double clampd (double v, double a, double b) { return std::max (a, std::min (b, v)); }
inline double lerp (double a, double b, double t) { return a + (b - a) * t; }
inline double jsSign (double x) { return x > 0 ? 1.0 : x < 0 ? -1.0 : 0.0; }
inline double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); }
inline int pick (double n, int k) { return std::min (k - 1, (int) std::floor (n * k * 0.9999)); }
inline int imod (int a, int m) { return ((a % m) + m) % m; }

// generador aleatorio (mulberry32), el mismo en la prueba de la vista previa
struct Rand { uint32_t a = 0x9E3779B9u; double operator()() { a += 0x6D2B79F5u; uint32_t t = a; t = (t ^ (t >> 15)) * (1u | t); t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t; return (double) (t ^ (t >> 14)) / 4294967296.0; } };
// rng32 de la vista previa (para el motivo de INVENTAR): idéntico bit a bit
struct Rng32 {
    uint32_t a = 0;
    double operator()() { a += 0x6D2B79F5u; uint32_t t = (a ^ (a >> 15)) * (1u | a); t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t; return (double) (t ^ (t >> 14)) / 4294967296.0; }
};
// ---------------- el genoma (mismo orden que GENE en la vista previa)
enum Gi { waveA, pwA, waveB, intB, detB, mixB, sub, uni, fm, fmR, fmEnv, metal, noise, soplo, vocal, vowel, cut, res, fenv, fdec, drive, crush,
          att, dec, sus, rel, vib, vibR, drift, glide, motion, motionR, mArp, mRate, mRhy, mPat, mHarm, mOct, mOrn, mEcho, mSeed, mPad, mBass, G_COUNT };
static const char* GNAMES[G_COUNT] = { "waveA","pwA","waveB","intB","detB","mixB","sub","uni","fm","fmR","fmEnv","metal","noise","soplo","vocal","vowel","cut","res","fenv","fdec","drive","crush",
          "att","dec","sus","rel","vib","vibR","drift","glide","motion","motionR","mArp","mRate","mRhy","mPat","mHarm","mOct","mOrn","mEcho","mSeed","mPad","mBass" };
static const double GDEF[G_COUNT] = { 0.66,0.5,0.66,0,0.2,0.35,0,0,0,0.2,0.4,0,0,0,0,0.3,0.55,0.2,0.3,0.4,0.1,0,0.05,0.5,0.7,0.35,0,0.45,0.15,0,0,0.35,0,0.35,0,0,0,0,0,0,0.2,0.45,0 };
using Genome = std::array<double, G_COUNT>;
inline Genome defGenome() { Genome g; for (int i = 0; i < G_COUNT; ++i) g[(size_t) i] = GDEF[i]; return g; }

// mandos generales que usa el motor
struct Params {
    double blend = 0.85, amb = 0.35, oct = 0, follow = 0.6, bright = 0, attOff = 0, relOff = 0, move = 0, out = 0, poly = 4, bpm = 140,
           sync = 0, toca = 0, vari = 0.4, flUp = 0, flDown = 0, flFifth = 0;
    double pgGain = 1, pgTame = 1;   // volumen igualado y anti-chirrido (los calcula la interfaz, como en la vista previa)
    bool bypass = false;
};
static const double INTS[8] = { 0, 12, -12, 7, 19, 24, 5, -5 };
static const double FMR[7] = { 0.5, 1, 2, 3, 1, 2, 3.5 };
static const double VOW[5][3] = { {730,1090,2440},{530,1840,2480},{270,2290,3010},{570,840,2410},{300,870,2240} };
static const double KMAJ[12] = { 6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88 }, KMIN[12] = { 6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17 };
static const int SC_MAJ[7] = { 0,2,4,5,7,9,11 }, SC_MIN8[8] = { 0,2,3,5,7,8,10,11 }, SC_NMIN[7] = { 0,2,3,5,7,8,10 };
static const double SYNCB[7] = { 0, 2, 1, 0.5, 1.0 / 3, 0.25, 1.0 / 6 };
static const char* RHYP[6] = { "1001001010010010","1000100100101000","1010101010101010","1011101110111011","0010010000100100","1000000010001000" };
static const double ARPR[4] = { 0.5, 0.25, 1.0 / 6, 0.125 };
static const int PROGS[6][4][2] = { {{9,0},{5,1},{2,0},{4,1}}, {{0,0},{8,1},{5,0},{7,1}}, {{2,0},{10,1},{5,1},{0,1}}, {{4,0},{0,1},{7,1},{2,1}}, {{5,1},{7,1},{4,0},{9,0}}, {{5,0},{1,1},{8,1},{3,1}} };
static constexpr int MIDI_LO = 24, MIDI_HI = 96, HOP = 512, NFFT = 4096;

struct D_t {
    double waveA, pw, waveB, intB, detB, mixB, sub; int uni; double spread, fm, fmR, fmEnv, metal, noise, cutMul, res, fenv, fdec, drive, att, dec, sus, rel,
           vib, vibR, drift, glide, soplo, vocal, vowel, crush, motion, motionR, flU, flD, fl5, alive, syncInc = 0, syncPh0 = 0;
};
inline D_t decode (const Genome& g, const Params& M)
{
    D_t d;
    d.waveA = g[waveA]; d.pw = 0.05 + 0.45 * g[pwA]; d.waveB = g[waveB]; d.intB = INTS[pick (g[intB], 8)]; d.detB = g[detB] * 35; d.mixB = g[mixB]; d.sub = g[sub];
    d.uni = 1 + (int) std::floor (g[uni] * 2.999); d.spread = g[uni] * 28;
    d.fm = g[fm] * 2.4; d.fmR = FMR[pick (g[fmR], 7)]; d.fmEnv = g[fmEnv]; d.metal = g[metal] * 0.35; d.noise = g[noise] * 0.22;
    d.cutMul = std::pow (2.0, g[cut] * 4.5 - 0.5 + M.bright * 1.5); d.res = g[res] * 0.68; d.fenv = g[fenv] * 3.5; d.fdec = 0.02 * std::pow (100.0, g[fdec]); d.drive = g[drive] * 0.6;
    d.att = 0.003 * std::pow (500.0, clampd (g[att] + M.attOff * 0.35, 0, 1)); d.dec = 0.03 * std::pow (100.0, g[dec]); d.sus = g[sus]; d.rel = 0.02 * std::pow (150.0, clampd (g[rel] + M.relOff * 0.35, 0, 1));
    d.vib = g[vib] * 40 + M.move * 18; d.vibR = 1.5 + 6.5 * g[vibR]; d.drift = g[drift] + M.move * 0.5; d.glide = g[glide] * 0.25;
    d.soplo = g[soplo] * 0.7; d.vocal = g[vocal] * 0.85; d.vowel = g[vowel]; d.crush = g[crush]; d.motion = g[motion] * 1.6; d.motionR = 0.12 * std::pow (60.0, g[motionR]);
    d.flU = M.flUp; d.flD = M.flDown; d.fl5 = M.flFifth; d.alive = M.move;
    return d;
}
inline double blep (double t, double dt) { if (t < dt) { t /= dt; return t + t - t * t - 1; } if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; } return 0; }
inline double osc (double ph, double dt, double wave, double pw)
{
    const double sine = std::sin (TWO_PI * ph);
    if (wave < 0.333) { const double tri = 1 - 4 * std::abs (ph - std::floor (ph + 0.75) + 0.25); return lerp (sine, tri, wave * 3); }
    const double saw = 2 * ph - 1 - blep (ph, dt);
    if (wave < 0.666) { const double tri = 1 - 4 * std::abs (ph - std::floor (ph + 0.75) + 0.25); return lerp (tri, saw, (wave - 0.333) * 3); }
    double p2 = ph + (1 - pw); p2 -= std::floor (p2);
    const double sq = (ph < pw ? 1 : -1) + blep (ph, dt) - blep (p2, dt) - (2 * pw - 1);
    return lerp (saw, sq, (wave - 0.666) * 3);
}

struct Voice {
    int note = -1; bool on = false; double env = 0; int stage = 0; double fenv = 0; double age = 0, freq = 440, target = 440, amp = 0, ampT = 0;
    double ph[8] {}, phB[8] {}, phS = 0, phF = 0, phM = 0, phU = 0, phD = 0, ph5 = 0, cj = 1, dj = 0;
    std::vector<float> comb = std::vector<float> (2400, 0.f); int cw = 0; double cLp = 0, mPh = 0;
    double foA[3] {}, foB[3] {}; double hold = 0, holdPh = 0, ic1 = 0, ic2 = 0, ic3 = 0, ic4 = 0, drift = 0, driftT = 0, vibPh = 0, pan = 0, seed = 0;
    double fmLevel = 1; bool hasG = false, hasFc = false; double gA1 = 0, gA2 = 0, gA3 = 0, gC = 0; double fc[3][3] {};
};

struct Engine {
    double sr = 48000; Rand rnd; Genome G = defGenome(); Params P;
    std::vector<Voice> voices;
    // estado MIDI (como MIDI/e.mst de la vista previa)
    uint8_t mOn[128] {}, mTrig[128] {}; float mVel[128] {};
    uint8_t sOn[128] {}, sTrig[128] {}; double sAmp[128] {}, chroma[12] {}; double peakAmp = 1;
    bool hasKey = false; int keyK = 0; bool keyMinor = false; double keyC = 0;
    // reloj y generador
    double clock = 0; int hopC = 0; int genStep = -1;
    struct Orn { double until; int grace; int oct; bool first, done, use; };
    std::map<int, Orn> orn;
    struct EchoQ { double at; int m; double amp; int n; };
    std::vector<EchoQ> echoQ;
    // INVENTAR
    struct Cmp {
        int seed = -1; double gk = -999; bool gkSet = false; std::array<std::array<int, 16>, 3> rh {}; std::array<std::array<int, 16>, 3> mot {}; int vary = 1; Rng32 r;
        std::vector<int> ck; bool ckSet = false, chordTrig = false; std::vector<int> voicing; int root = 0; long long step = LLONG_MIN;
        bool hasCur = false; int cur = 0; double curEnd = 0, curAmp = 1; bool mTrig = false; bool hasLast = false; int last = 0;
        bool hasGrace = false; int graceM = 0; double graceUntil = 0; bool graceDone = false;
        struct E { double at; int m; double until; int n; }; std::vector<E> echo; long long arpStep = LLONG_MIN;
    } cmp;
    // secuenciador de acordes de prueba
    bool seqOn = false; int seqProg = 0; std::vector<int> seqNotes;
    // efectos
    std::vector<float> ensL, ensR, dl, dr, pre, dryL, dryR; int eW = 0, dw = 0, pI = 0, dI = 0; double eP = 0;
    struct Line { std::vector<float> b; int i = 0; double s = 0; }; std::vector<Line> cL, cR, aL, aR;
    double lim = 1, rLp = 0, hpL = 0, hpR = 0, hxL = 0, hxR = 0, shL = 0, shR = 0, cEnv = 0;
    std::vector<float> vL, vR;
    // lo que ve la interfaz
    double level = 0, inLevel = 0; int voicesOn = 0; std::vector<int> played, notesNow; double hopTick = 0;

    void prepare (double fs, uint32_t seed = 0x9E3779B9u)
    {
        sr = fs; rnd.a = seed; voices.clear();
        for (int i = 0; i < 8; ++i) { Voice v; v.mPh = rnd(); v.vibPh = rnd(); v.seed = rnd(); voices.push_back (std::move (v)); }   // mismo orden que makeVoice
        const int comb[8] = { 1116,1188,1277,1356,1422,1491,1557,1617 }, ap[4] = { 556,441,341,225 };
        auto mk = [] (int n) { Line l; l.b.assign ((size_t) n, 0.f); return l; };
        cL.clear(); cR.clear(); aL.clear(); aR.clear();
        for (int c : comb) { const int n = (int) std::lround (c * fs / 44100); cL.push_back (mk (n)); cR.push_back (mk (n + 23)); }
        for (int a : ap) { const int n = (int) std::lround (a * fs / 44100); aL.push_back (mk (n)); aR.push_back (mk (n + 23)); }
        ensL.assign ((size_t) std::floor (fs * 0.05), 0.f); ensR = ensL; dl.assign ((size_t) std::floor (fs * 2), 0.f); dr = dl; pre.assign ((size_t) std::floor (fs * 0.025), 0.f);
        dryL.assign (NFFT / 2 + 1, 0.f); dryR = dryL; vL.assign (8192, 0.f); vR = vL;
        eW = dw = pI = dI = 0; eP = 0; lim = 1; rLp = hpL = hpR = hxL = hxR = shL = shR = cEnv = 0; clock = 0; hopC = 0; genStep = -1;
        std::fill (std::begin (mOn), std::end (mOn), 0); std::fill (std::begin (mTrig), std::end (mTrig), 0); std::fill (std::begin (sOn), std::end (sOn), 0); std::fill (std::begin (sTrig), std::end (sTrig), 0);
        std::fill (std::begin (sAmp), std::end (sAmp), 0.0); std::fill (std::begin (chroma), std::end (chroma), 0.0); hasKey = false; orn.clear(); echoQ.clear(); cmp = Cmp(); seqNotes.clear();
    }
    void noteOn (int m, double v = 0.8) { if (m < 0 || m > 127) return; mOn[m] = 1; mTrig[m] = 1; mVel[m] = (float) v; }
    void noteOff (int m) { if (m >= 0 && m < 128) mOn[m] = 0; }
    void allNotesOff() { std::fill (std::begin (mOn), std::end (mOn), 0); }

    double randn() { double u = 0, v = 0; while (u == 0) u = rnd(); while (v == 0) v = rnd(); return std::sqrt (-2 * std::log (u)) * std::cos (2 * PI * v); }
    void trig (Voice& v)
    {
        v.stage = 0; v.fenv = 1; v.fmLevel = 1; v.age = 0; if (v.env > 0.6) v.env = 0.6;
        const double life = 0.25 + P.move; v.cj = std::pow (2.0, randn() * 0.1 * life); v.dj = randn() * 3 * life;
    }
    // ---------------------------------------------------------------- una voz
    void voiceRender (Voice& v, const D_t& D, float* oL, float* oR, int n0, int n1, double follow)
    {
        if (v.stage == 4 && v.env < 1e-5) return;
        const double attK = 1 / std::max (1.0, D.att * sr), decK = 1 - std::exp (-1 / std::max (1.0, D.dec * sr * 0.3)), relK = 1 - std::exp (-1 / std::max (1.0, D.rel * sr * 0.3));
        const double fdK = std::exp (-1 / std::max (1.0, D.fdec * sr * 0.3)), glideK = D.glide > 0.001 ? 1 - std::exp (-1 / (D.glide * sr * 0.3)) : 1;
        const double fmDecay = std::exp (-1 / std::max (1.0, (0.05 + (1 - D.fmEnv) * 2) * sr * 0.3));
        const int uniN = D.uni; const double gl = std::cos ((v.pan + 1) * PI / 4), gr = std::sin ((v.pan + 1) * PI / 4);
        const double ratioB = std::pow (2.0, D.intB / 12), detBm = std::pow (2.0, D.detB / 1200);
        const double k = 2 - 2 * D.res, drv = 1 + D.drive * 8 * std::sqrt (P.pgTame), drvN = 1 / std::sqrt (drv);
        const double keyG = v.freq > 700 ? std::pow (700 / v.freq, 0.4) : 1;
        double fmL = v.fmLevel;
        static const double FW[3] = { 1, 0.7, 0.35 };
        for (int n = n0; n < n1; ++n)
        {
            if (v.stage == 0) { v.env += attK; if (v.env >= 1) { v.env = 1; v.stage = 1; } }
            else if (v.stage == 1) { v.env += (D.sus - v.env) * decK; if (std::abs (v.env - D.sus) < 1e-3) v.stage = 2; }
            else if (v.stage == 4) { v.env -= v.env * relK; }
            v.fenv *= fdK; fmL *= fmDecay;
            v.freq += (v.target - v.freq) * glideK;
            v.amp += (v.ampT - v.amp) * 0.002;
            v.vibPh += D.vibR / sr; if (v.vibPh > 1) v.vibPh -= 1;
            v.mPh += D.motionR / sr; if (v.mPh > 1) v.mPh -= 1;
            const double lfo = D.syncInc != 0 ? std::sin (TWO_PI * (D.syncPh0 + (n - n0) * D.syncInc)) : std::sin (TWO_PI * v.mPh);
            if ((n & 255) == 0) v.driftT = (rnd() * 2 - 1) * D.drift * 12;
            v.drift += (v.driftT - v.drift) * 0.0004;
            const double f = v.freq * std::pow (2.0, (std::sin (TWO_PI * v.vibPh) * D.vib * std::min (1.0, v.age / (0.25 * sr)) + v.drift + v.dj) / 1200);
            double s = 0;
            const double fmIdx = D.fm * (D.fmEnv > 0.02 ? (0.25 + 0.75 * fmL) : 1) * std::min (1.0, std::pow (330 / f, 0.7));
            double mod = 0;
            if (D.fm > 0.01) { v.phM += f * D.fmR / sr; v.phM -= std::floor (v.phM); mod = std::sin (TWO_PI * v.phM) * fmIdx / TWO_PI; }
            for (int u = 0; u < uniN; ++u)
            {
                const double det = uniN == 1 ? 1 : std::pow (2.0, (((double) u / (uniN - 1)) * 2 - 1) * D.spread / 1200);
                const double fa = f * det, dt = fa / sr;
                v.ph[u] += dt; v.ph[u] -= std::floor (v.ph[u]);
                double pa = v.ph[u] + mod; pa -= std::floor (pa);
                s += osc (pa, dt, D.waveA, D.pw);
                if (D.mixB > 0.01) { const double fb = fa * ratioB * detBm, dtb = fb / sr; v.phB[u] += dtb; v.phB[u] -= std::floor (v.phB[u]); s += osc (v.phB[u], dtb, D.waveB, 0.5) * D.mixB; }
            }
            s /= std::sqrt ((double) uniN);
            if (D.flU > 0.01) { const double fu = f * 2, du = fu / sr; v.phU += du; v.phU -= std::floor (v.phU); s += osc (v.phU, du, D.waveA, D.pw) * D.flU * 0.6; }
            if (D.flD > 0.01) { const double fd = f * 0.5, dd = fd / sr; v.phD += dd; v.phD -= std::floor (v.phD); s += osc (v.phD, dd, D.waveA, D.pw) * D.flD * 0.75; }
            if (D.fl5 > 0.01) { const double f5 = f * 1.4983, d5 = f5 / sr; v.ph5 += d5; v.ph5 -= std::floor (v.ph5); s += osc (v.ph5, d5, D.waveA, D.pw) * D.fl5 * 0.55; }
            if (D.sub > 0.01) { v.phS += f * 0.5 / sr; v.phS -= std::floor (v.phS); s += std::sin (TWO_PI * v.phS) * D.sub; }
            if (D.metal > 0.01) { v.phF += f * 2.76 / sr; v.phF -= std::floor (v.phF); s = lerp (s, s * std::sin (TWO_PI * v.phF) * 1.6, D.metal); }
            if (D.noise > 0.005) s += (rnd() * 2 - 1) * D.noise * (0.4 + 0.6 * v.env);
            if (D.soplo > 0.01)
            {
                const int CL = (int) v.comb.size(); const double dlen = clampd (sr / f, 2, CL - 2), rp = v.cw - dlen; const int ri = (int) std::floor (rp); const double rf = rp - ri; const int a0 = imod (ri, CL);
                const double y = v.comb[(size_t) a0] + (v.comb[(size_t) ((a0 + 1) % CL)] - v.comb[(size_t) a0]) * rf;
                v.cLp += (y - v.cLp) * 0.5;
                const double exc = (rnd() * 2 - 1) * (0.05 + 0.6 * v.fenv) * 0.5;
                v.comb[(size_t) v.cw] = (float) (exc + v.cLp * 0.985); v.cw = (v.cw + 1) % CL;
                s = lerp (s, y * 1.6, D.soplo);
            }
            if ((n & 7) == 0 || ! v.hasG)
            {
                const double wob = D.motion > 0.01 ? std::pow (2.0, D.motion * (D.syncInc != 0 ? 1.7 : 1.2) * (0.5 * lfo - 0.5)) : 1;
                const double fc = clampd (std::min (f * D.cutMul * v.cj * wob * std::pow (2.0, D.fenv * v.fenv), 6500 + f) * P.pgTame, 30, sr * 0.45);
                const double g = std::tan (PI * fc / sr); v.gA1 = 1 / (1 + g * (g + k)); v.gA2 = g * v.gA1; v.gA3 = g * v.gA2; v.gC = g; v.hasG = true;
            }
            double x = s;
            double v3 = x - v.ic2, v1 = v.gA1 * v.ic1 + v.gA2 * v3, v2 = v.ic2 + v.gA2 * v.ic1 + v.gA3 * v3; v.ic1 = 2 * v1 - v.ic1; v.ic2 = 2 * v2 - v.ic2; x = v2;
            v3 = x - v.ic4; v1 = v.gA1 * v.ic3 + v.gA2 * v3; v2 = v.ic4 + v.gA2 * v.ic3 + v.gA3 * v3; v.ic3 = 2 * v1 - v.ic3; v.ic4 = 2 * v2 - v.ic4; x = v2;
            if (D.drive > 0.02) x = std::tanh (x * drv) * drvN * 1.2;
            if (D.vocal > 0.01)
            {
                if ((n & 15) == 0 || ! v.hasFc)
                {
                    const double vp = clampd (D.vowel + (D.motion > 0.01 ? lfo * D.motion * 0.3 : 0), 0, 1) * 3.999; const int vi = (int) std::floor (vp); const double vf = vp - vi;
                    for (int j = 0; j < 3; ++j) { const double F0 = lerp (VOW[vi][j], VOW[vi + 1][j], vf), g = std::tan (PI * std::min (F0, sr * 0.45) / sr), kq = 0.18; const double a1 = 1 / (1 + g * (g + kq)); v.fc[j][0] = a1; v.fc[j][1] = g * a1; v.fc[j][2] = g * g * a1; }
                    v.hasFc = true;
                }
                double y = 0;
                for (int j = 0; j < 3; ++j) { const double* c = v.fc[j]; const double w3 = x - v.foB[j], w1 = c[0] * v.foA[j] + c[1] * w3, w2 = v.foB[j] + c[1] * v.foA[j] + c[2] * w3; v.foA[j] = 2 * w1 - v.foA[j]; v.foB[j] = 2 * w2 - v.foB[j]; y += w1 * FW[j]; }
                x = lerp (x, y * 0.9, D.vocal);
            }
            if (D.crush > 0.02) { const double cr = D.crush * std::sqrt (P.pgTame); v.holdPh += 1 / (1 + cr * cr * 14); if (v.holdPh >= 1) { v.holdPh -= 1; const double lv = std::pow (2.0, 12 - 8 * cr); v.hold = std::round (x * lv) / lv; } x = lerp (x, v.hold, std::min (1.0, cr * 1.5)); }
            const double dyn = lerp (1, v.amp, follow);
            const double out = x * v.env * dyn * 0.32 * P.pgGain * keyG;
            oL[n] += (float) (out * gl); oR[n] += (float) (out * gr);
            v.age++;
        }
        v.fmLevel = fmL;
    }
    // ---------------------------------------------------------------- tonalidad
    void findKey()
    {
        double tot = 0; for (double c : chroma) tot += c; if (tot < 2) return;
        auto corr = [&] (const double* prof, int kk) { double mx = 0, my = 0; for (int i = 0; i < 12; ++i) { mx += chroma[(i + kk) % 12]; my += prof[i]; } mx /= 12; my /= 12; double nn = 0, dx = 0, dy = 0; for (int i = 0; i < 12; ++i) { const double a = chroma[(i + kk) % 12] - mx, b = prof[i] - my; nn += a * b; dx += a * a; dy += b * b; } return nn / std::sqrt (dx * dy + 1e-12); };
        bool have = false; int bk = 0; bool bm = false; double bc = 0;
        for (int kk = 0; kk < 12; ++kk) for (int t = 0; t < 2; ++t) { const double c = corr (t ? KMIN : KMAJ, kk); if (! have || c > bc) { have = true; bk = kk; bm = t == 1; bc = c; } }
        if (bc < 0.55) { hasKey = false; return; }
        hasKey = true; keyK = bk; keyMinor = bm; keyC = bc;
    }
    int scaleStep (int m, int steps) const
    {
        const int root = hasKey ? keyK : imod (m, 12); const bool maj = hasKey && ! keyMinor;
        int deg[7]; for (int i = 0; i < 7; ++i) deg[i] = ((maj ? SC_MAJ[i] : SC_NMIN[i]) + root) % 12;
        auto inS = [&] (int x) { const int pc = imod (x, 12); for (int d : deg) if (d == pc) return true; return false; };
        int x = m, dir = steps > 0 ? 1 : steps < 0 ? -1 : 0, left = std::abs (steps), guard = 0;
        while (! inS (x) && guard++ < 12) x -= 1;
        while (left > 0 && guard++ < 60) { x += dir; if (inS (x)) left--; }
        return x;
    }
    // ---------------------------------------------------------------- INVENTAR
    struct Out { int m; bool trig; double amp; };
    void buildMotif()
    {
        const Genome& g = G; Cmp& c = cmp;
        const int seed = (int) std::floor (g[mSeed] * 31.999); Rng32 r; r.a = (uint32_t) (seed * 7919 + 101); const double dens = 0.18 + 0.62 * g[mRate], sync = g[mRhy];
        auto rhythm = [&] (int typ) {   // 0=A 1=B 2=C
            std::array<int, 16> on {};
            for (int s = 0; s < 16; ++s) {
                double w = s % 4 == 0 ? 1 : s % 2 == 0 ? 0.55 : 0.28;
                if (sync > 0.01) w = lerp (w, s % 4 == 0 ? 0.35 : s % 4 == 3 ? 0.9 : (s % 2) ? 0.55 : 0.7, sync);
                on[(size_t) s] = r() < clampd (w * dens * 1.7, 0.03, 0.95) ? 1 : 0;
            }
            if (typ == 2) { for (int s = 9; s < 16; ++s) on[(size_t) s] = 0; on[8] = 1; }
            if (sync < 0.5 || typ != 1) on[0] = 1; else on[2] = 1;
            return on;
        };
        auto contour = [&]() {
            std::array<int, 16> m {}; int cc = 0; static const int ST[8] = { 1,-1,1,-1,2,-2,0,1 };
            for (int i = 1; i < 16; ++i) { int st = ST[(int) std::floor (r() * 8)]; if (r() < g[mOct] * 0.45) st = (r() < 0.5 ? -1 : 1) * (3 + (int) std::floor (r() * 3)); cc = (int) clampd (cc + st, -4, 7); m[(size_t) i] = cc; }
            return m;
        };
        const auto A = contour(); const auto rA = rhythm (0); const auto rB = rhythm (1); const auto rC = rhythm (2);
        std::array<int, 16> B {}, Cc {};
        for (int i = 0; i < 16; ++i) { if (i < 4) B[(size_t) i] = A[(size_t) i] + 2; else { const auto ct = contour(); B[(size_t) i] = ct[(size_t) i]; } }
        for (int i = 0; i < 16; ++i) Cc[(size_t) i] = A[(size_t) i] - 1;
        c.seed = seed; c.rh = { rA, rB, rC }; c.mot = { A, B, Cc }; c.vary = r() < 0.5 ? 1 : -2; c.r = r;
    }
    void compose (const std::vector<int>& active, std::vector<Out>& out, double beat, double t)
    {
        const Genome& g = G; Cmp& c = cmp; std::vector<int> chord = active; std::sort (chord.begin(), chord.end());
        const double gk = g[mRate] + g[mRhy] + g[mOct];
        if (c.seed != (int) std::floor (g[mSeed] * 31.999) || ! c.gkSet || c.gk != gk) { buildMotif(); c.gk = gk; c.gkSet = true; }
        const int keyRoot = hasKey ? keyK : (chord.size() ? chord[0] % 12 : 9); const bool maj = hasKey && ! keyMinor;
        auto scv = [&] (int i) { return maj ? SC_MAJ[i] : SC_NMIN[i]; };
        auto d2m = [&] (int d) { const int o = (int) std::floor (d / 7.0), i = imod (d, 7); return 12 * o + keyRoot + scv (i); };
        auto m2d = [&] (int m) { const int o = (int) std::floor ((m - keyRoot) / 12.0), r = m - keyRoot - 12 * o; int i = 0; for (int kk = 0; kk < 7; ++kk) if (scv (kk) <= r) i = kk; return o * 7 + i; };
        std::vector<int> pcs; for (int m : chord) pcs.push_back (m % 12);
        auto inPcs = [&] (int x) { for (int p : pcs) if (p == x) return true; return false; };
        auto nearChord = [&] (int m, double dirBias) { int best = m; double bd = 99; for (int x = m - 6; x <= m + 6; ++x) if (inPcs (imod (x, 12))) { const double d = std::abs (x - m) + (jsSign (x - m) == -dirBias ? 0.4 : 0); if (d < bd) { bd = d; best = x; } } return best; };
        const double stepLen = beat / 4; const long long step = (long long) std::floor (t / stepLen); const int sib = (int) (step % 16); const long long bar = step / 16; const int pb = (int) (bar % 4); const int typ = pb == 1 ? 1 : pb == 3 ? 2 : 0;
        if (! c.ckSet || chord != c.ck)
        {
            c.ck = chord; c.ckSet = true; c.chordTrig = true;
            if (chord.size())
            {
                double center = 57; if (! c.voicing.empty()) { double sum = 0; for (int x : c.voicing) sum += x; center = sum / (double) c.voicing.size(); }
                std::vector<int> uniq; for (int p : pcs) if (std::find (uniq.begin(), uniq.end(), p) == uniq.end()) uniq.push_back (p);
                c.voicing.clear(); for (int p : uniq) { int x = 48 + imod (p - 48, 12); while (x < center - 6) x += 12; while (x > center + 6) x -= 12; c.voicing.push_back ((int) clampd (x, 45, 69)); }
                c.root = chord[0] % 12;
            }
        }
        if (chord.empty()) { if (c.hasCur && t < c.curEnd) out.push_back ({ c.cur, false, c.curAmp }); c.step = step; return; }
        if (step != c.step)
        {
            c.step = step;
            const auto& rh = c.rh[(size_t) typ];
            if (rh[(size_t) sib])
            {
                int idx = 0; for (int s2 = 0; s2 < sib; ++s2) idx += rh[(size_t) s2];
                int off = c.mot[(size_t) typ][(size_t) (idx % 16)]; if (pb == 2 && sib >= 8) off += c.vary;
                const int top = chord.back(); const int center = (int) clampd (top + 5 + std::round ((g[mOct] - 0.3) * 6), 64, 80);
                const int anchor = m2d (nearChord (center, 0));
                int m = d2m (anchor + off);
                if (sib % 2 == 0) m = nearChord (m, c.hasLast ? jsSign (m - c.last) : 0);
                { const int pc = imod (m, 12); if (! inPcs (pc)) for (int p : pcs) { const int dd = imod (p - pc, 12); if (dd == 1) { m += 1; break; } if (dd == 11) { m -= 1; break; } } }
                if (c.hasLast && std::abs (m - c.last) > 9 + g[mOct] * 6) m += m > c.last ? -12 : 12;
                while (m > 86) m -= 12; while (m < std::max (57, top + 1)) m += 12;
                int nx = sib + 1; while (nx < 16 && ! rh[(size_t) nx]) nx++;
                const double len = (nx - sib) * stepLen;
                c.hasCur = true; c.cur = m; c.curEnd = t + len * (0.55 + 0.42 * (1 - g[mRhy] * 0.4)); c.curAmp = (sib % 4 == 0 ? 1 : 0.82) * (typ == 2 && sib == 8 ? 0.9 : 1); c.mTrig = true; c.hasLast = true; c.last = m;
                c.hasGrace = false;
                if (g[mOrn] > 0.4 && sib % 4 == 0 && c.r() < g[mOrn] * 0.7) { const int dir = c.r() < 0.6 ? 1 : -1; c.hasGrace = true; c.graceM = scaleStep (m, dir); c.graceUntil = t + stepLen * 0.45; c.graceDone = false; }
                if (g[mEcho] > 0.45 && c.r() < 0.6) { static const double EB[3] = { 0.5, 0.75, 1 }; c.echo.push_back ({ t + beat * EB[pick (g[mRate], 3)], scaleStep (m, 2), 0, -1 }); }
            }
        }
        if (c.hasCur && t < c.curEnd)
        {
            if (c.hasGrace && t < c.graceUntil) { out.push_back ({ c.graceM, c.mTrig, c.curAmp * 0.75 }); c.mTrig = false; }
            else { const bool tr = c.mTrig || (c.hasGrace && ! c.graceDone); if (c.hasGrace) c.graceDone = true; out.push_back ({ c.cur, tr, c.curAmp }); c.mTrig = false; }
            if (g[mHarm] > 0.35) out.push_back ({ scaleStep (c.cur, -2), false, c.curAmp * 0.6 });
        }
        if (! c.echo.empty() || true)
        {
            std::vector<Cmp::E> keep; for (auto& q : c.echo) if (t < q.at + stepLen * 3) keep.push_back (q); c.echo = keep;
            for (auto& q : c.echo) if (t >= q.at) { out.push_back ({ q.m, q.n < 0, 0.5 }); q.n = 1; }
        }
        if (g[mBass] > 0.4) { int b = 36 + c.root; if (b > 47) b -= 12; out.push_back ({ b, c.chordTrig, 0.55 + 0.3 * g[mBass] }); }
        if (g[mPad] > 0.12)
        {
            const double lv = 0.18 + 0.5 * g[mPad];
            if (g[mArp] > 0.45)
            {
                const double al = beat / 2; const long long as = (long long) std::floor (t / al);
                std::vector<int> seq = c.voicing; for (int i = (int) c.voicing.size() - 2; i >= 1; --i) seq.push_back (c.voicing[(size_t) i]);
                if (std::fmod (t, al) / al < 0.8 && ! seq.empty()) out.push_back ({ seq[(size_t) (as % (long long) seq.size())], as != c.arpStep, lv });
                c.arpStep = as;
            }
            else for (int m : c.voicing) out.push_back ({ m, c.chordTrig, lv });
        }
        c.chordTrig = false;
    }
    // ---------------------------------------------------------------- qué notas tocar ahora
    double ampOf (int m) const { const double norm = 1 / std::max (1e-4, peakAmp); return clampd (std::sqrt (((sAmp[m] != 0) ? sAmp[m] : peakAmp * 0.5) * norm), 0.08, 1); }
    static std::vector<int> uniqSorted (std::vector<int> v) { std::vector<int> u; for (int x : v) if (std::find (u.begin(), u.end(), x) == u.end()) u.push_back (x); std::sort (u.begin(), u.end()); return u; }
    static std::vector<int> pingpong (const std::vector<int>& seq) { if (seq.size() <= 1) return seq; std::vector<int> f = seq; for (int i = (int) seq.size() - 2; i >= 1; --i) f.push_back (seq[(size_t) i]); return f; }
    std::vector<Out> generate()
    {
        const int maxV = (int) std::lround (P.poly), mode = (int) std::lround (P.toca) - 2; const double vr = P.vari;
        std::vector<int> active; for (int m = MIDI_LO; m <= MIDI_HI; ++m) if (sOn[m]) active.push_back (m);
        std::stable_sort (active.begin(), active.end(), [&] (int a, int b) { return sAmp[b] < sAmp[a]; });
        std::vector<int> base (active.begin(), active.begin() + std::min ((int) active.size(), std::max (1, maxV)));
        const double beat = sr * 60 / P.bpm, t = clock; std::vector<Out> out; const Genome& g = G;
        auto orStart = [&] (int m, bool ador) {
            Orn o; o.until = t + beat * (ador ? (0.1 + 0.08 * vr) : 0.12);
            o.grace = rnd() < 0.5 ? scaleStep (m, 1) : scaleStep (m, -1);
            if (ador) o.oct = rnd() < vr * 0.5 ? (rnd() < 0.5 ? 12 : -12) : 0; else o.oct = rnd() < std::max (0.0, g[mOct] - 0.3) * 1.3 ? 12 : 0;
            o.first = true; o.done = false;
            if (ador) o.use = rnd() < 0.55 + vr * 0.45; else o.use = g[mOrn] > 0.4 && rnd() < g[mOrn];
            orn[m] = o;
        };
        auto ornPlay = [&] (int m) {
            auto it = orn.find (m); Orn* o = it != orn.end() ? &it->second : nullptr;
            if (o && o->use && t < o->until) { out.push_back ({ o->grace, o->first, ampOf (m) * 0.8 }); o->first = false; }
            else { const bool tr = sTrig[m] || (o && o->use && ! o->done); if (o) o->done = true; out.push_back ({ m + (o ? o->oct : 0), tr, ampOf (m) }); }
        };
        auto echoRun = [&] (double d) {
            std::vector<EchoQ> keep; for (auto& q : echoQ) if (t < q.at + d * 2) keep.push_back (q); echoQ = keep;
            for (auto& q : echoQ) if (t >= q.at) { const int kk = (int) std::floor ((t - q.at) / d); out.push_back ({ q.m + (kk == 1 ? 12 : 0), kk != q.n, q.amp * (kk ? 0.6 : 1) }); q.n = kk; }
        };
        if (mode == -2) compose (active, out, beat, t);
        else if (mode == -1 && ! base.empty())
        {
            const int mel = base[0]; const bool tr0 = sTrig[mel]; const int harmN = g[mHarm] > 0.7 ? 2 : g[mHarm] > 0.35 ? 1 : 0;
            std::vector<int> harm; if (harmN) { harm.push_back (scaleStep (mel, -2)); if (harmN > 1) harm.push_back (scaleStep (mel, -4)); }
            if (g[mArp] > 0.45)
            {
                const double stepLen = beat * ARPR[pick (g[mRate], 4)]; const long long step = (long long) std::floor (t / stepLen); const double ph = std::fmod (t, stepLen) / stepLen;
                std::vector<int> all = base; all.insert (all.end(), harm.begin(), harm.end()); std::vector<int> notes = uniqSorted (all);
                if (notes.size() < 3) { std::vector<int> a2 = { scaleStep (mel, -4), scaleStep (mel, -2) }; a2.insert (a2.end(), notes.begin(), notes.end()); notes = uniqSorted (a2); }
                std::vector<int> seq; for (int o = 0; o < (g[mOct] > 0.45 ? 2 : 1); ++o) for (int m : notes) seq.push_back (m + o * 12);
                const auto full = pingpong (seq);
                if (ph < 0.72) out.push_back ({ full[(size_t) (step % (long long) full.size())], step != genStep, ampOf (mel) });
                genStep = (int) step;
            }
            else if (g[mRhy] > 0.5)
            {
                const char* pat = RHYP[pick (g[mPat], 6)]; const double stepLen = beat / 4; const long long step = (long long) std::floor (t / stepLen); const double ph = std::fmod (t, stepLen) / stepLen; const bool on = pat[step % 16] == '1';
                if (on && ph < 0.55) { std::vector<int> all = base; all.insert (all.end(), harm.begin(), harm.end()); for (int m : all) { const bool inBase = std::find (base.begin(), base.end(), m) != base.end(); out.push_back ({ m, step != genStep, ampOf (mel) * (inBase ? 1 : 0.75) }); } }
                if (on) genStep = (int) step;
            }
            else
            {
                for (int m : base) { if (sTrig[m]) orStart (m, false); ornPlay (m); }
                for (int m : harm) out.push_back ({ m, tr0, ampOf (mel) * 0.7 });
                if (g[mEcho] > 0.45) { static const double EB[3] = { 0.5, 0.75, 1 }; const double d = beat * EB[pick (g[mRate], 3)]; if (tr0) echoQ.push_back ({ t + d, scaleStep (mel, 2), ampOf (mel) * 0.6, -1 }); echoRun (d); }
            }
        }
        else if (mode == 0 || base.empty()) { for (int m : base) out.push_back ({ m, (bool) sTrig[m], ampOf (m) }); }
        else if (mode == 1)
        {
            const double stepLen = beat * ARPR[std::min (3, (int) std::floor (vr * 3.999))]; const long long step = (long long) std::floor (t / stepLen); const double ph = std::fmod (t, stepLen) / stepLen;
            std::vector<int> notes = base; std::sort (notes.begin(), notes.end()); const int oct = vr > 0.5 ? 2 : 1; std::vector<int> seq;
            for (int o = 0; o < oct; ++o) for (int m : notes) seq.push_back (m + o * 12);
            const auto full = pingpong (seq);
            if (ph < 0.7) out.push_back ({ full[(size_t) (step % (long long) full.size())], step != genStep, ampOf (notes[0]) });
            genStep = (int) step;
        }
        else if (mode == 2)
        {
            const char* pat = RHYP[std::min (5, (int) std::floor (vr * 6 * 0.999))]; const double stepLen = beat / 4; const long long step = (long long) std::floor (t / stepLen); const double ph = std::fmod (t, stepLen) / stepLen; const bool on = pat[step % 16] == '1';
            if (on && ph < 0.55) for (int m : base) out.push_back ({ m, step != genStep, ampOf (m) });
            if (on) genStep = (int) step;
        }
        else if (mode == 3)
        {
            for (size_t i = 0; i < std::min<size_t> (2, base.size()); ++i)
            {
                const int m = base[i]; const bool tr = sTrig[m]; const double a = ampOf (m);
                out.push_back ({ m, tr, a }); out.push_back ({ scaleStep (m, -2), tr, a * 0.8 }); out.push_back ({ scaleStep (m, -4), tr, a * 0.75 });
                if (vr > 0.5) out.push_back ({ scaleStep (m, -6), tr, a * 0.6 }); if (vr > 0.8) out.push_back ({ scaleStep (m, -8), tr, a * 0.45 });
            }
        }
        else if (mode == 4) { for (int m : base) { if (sTrig[m]) orStart (m, true); ornPlay (m); } }
        else if (mode == 5)
        {
            for (int m : base) out.push_back ({ m, (bool) sTrig[m], ampOf (m) });
            static const double EB[3] = { 0.5, 0.75, 1 }; const double d = beat * EB[std::min (2, (int) std::floor (vr * 2.999))];
            for (int m : base) if (sTrig[m]) echoQ.push_back ({ t + d, scaleStep (m, 2), ampOf (m) * 0.6, -1 });
            echoRun (d);
        }
        for (int m = MIDI_LO; m <= MIDI_HI; ++m) sTrig[m] = 0;
        return out;
    }
    void allocate (const std::vector<Out>& list)
    {
        const int shift = (int) std::lround (P.oct) * 12;
        auto fold = [&] (int m) { int x = m + shift; while (x > 84) x -= 12; while (x < 28) x += 12; return x; };
        std::vector<int> order; std::map<int, std::pair<bool, double>> want;
        for (auto& o : list) { const int kk = fold (o.m); auto it = want.find (kk); if (it == want.end()) { order.push_back (kk); want[kk] = { o.trig, o.amp }; } else { it->second.first = it->second.first || o.trig; it->second.second = std::max (o.amp, it->second.second); } }
        std::vector<int> keys (order.begin(), order.begin() + std::min (order.size(), voices.size()));
        for (auto& v : voices) if (v.on && std::find (keys.begin(), keys.end(), v.note) == keys.end()) { v.on = false; v.stage = 4; }
        for (int kk : keys)
        {
            const auto w = want[kk]; Voice* v = nullptr;
            for (auto& x : voices) if (x.on && x.note == kk) { v = &x; break; }
            if (! v)
            {
                for (auto& x : voices) if (! x.on && x.env < 0.002) { v = &x; break; }
                if (! v) for (auto& x : voices) if (! x.on) { v = &x; break; }
                if (! v) { v = &voices[0]; for (auto& x : voices) if (! (v->env < x.env)) v = &x; }
                v->note = kk; v->on = true; v->target = v->freq = mtof (kk); v->pan = (rnd() * 2 - 1) * 0.5; trig (*v); v->amp = v->ampT = w.second;
            }
            else { v->ampT = w.second; if (w.first) trig (*v); }
        }
        voicesOn = 0; for (auto& v : voices) if (v.on) voicesOn++;
        played = keys;
    }
    void syncSet (D_t& D, int n0, int n1)
    {
        const double sb = SYNCB[std::max (0, std::min (6, (int) std::lround (P.sync)))];
        if (sb == 0) { D.syncInc = 0; return; }
        D.syncInc = P.bpm / 60 / sb / sr;
        const double cs = clock - (n1 - n0); D.syncPh0 = std::fmod (cs * D.syncInc, 1.0);
    }
    void seqTick (double beat)
    {
        if (! seqOn) { if (! seqNotes.empty()) { for (int m : seqNotes) noteOff (m); seqNotes.clear(); } return; }
        const auto& pr = PROGS[std::max (0, std::min (5, seqProg))]; const long long bar = (long long) std::floor (clock / (beat * 4)); const int root = pr[bar % 4][0], maj = pr[bar % 4][1];
        const int r = 48 + root; std::vector<int> want = { r, r + (maj ? 4 : 3), r + 7 };
        if (want != seqNotes) { for (int m : seqNotes) noteOff (m); for (int m : want) noteOn (m, 0.75); seqNotes = want; }
    }
    void midiIn() { for (int m = 0; m < 128; ++m) { sOn[m] = mOn[m]; sAmp[m] = mOn[m] ? mVel[m] * (1 + m * 0.002) : 0; if (mTrig[m]) { sTrig[m] = 1; mTrig[m] = 0; } } }

    // ---------------------------------------------------------------- un bloque de audio (midiEvents: llamada por muestra para aplicar el MIDI del host en su sitio)
    void process (const float* inL, const float* inR, float* oL, float* oR, int N, const std::function<void (int)>& midiAt = nullptr)
    {
        D_t D = decode (G, P);
        if ((int) vL.size() < N) { vL.assign ((size_t) N, 0.f); vR.assign ((size_t) N, 0.f); }
        std::fill (vL.begin(), vL.begin() + N, 0.f); std::fill (vR.begin(), vR.begin() + N, 0.f);
        int n0 = 0; double inPk = 0;
        for (int n = 0; n < N; ++n)
        {
            if (midiAt) midiAt (n);
            const double x = (inL[n] + inR[n]) * 0.5; inPk = std::max (inPk, std::abs (x));
            clock++;
            const long long ck = (long long) clock;
            if ((ck & 127) == 0) { seqTick (sr * 60 / P.bpm); midiIn(); }
            if (++hopC >= HOP)
            {
                hopC = 0;
                for (int c = 0; c < 12; ++c) chroma[c] *= 0.998;
                notesNow.clear(); for (int m = 0; m < 128; ++m) if (sOn[m]) { notesNow.push_back (m); chroma[m % 12] += 0.06; }
                findKey();
            }
            if ((ck & 127) == 0)
            {
                syncSet (D, n0, n + 1);
                for (auto& v : voices) voiceRender (v, D, vL.data(), vR.data(), n0, n + 1, P.follow);
                n0 = n + 1;
                allocate (generate());
            }
        }
        syncSet (D, n0, N);
        for (auto& v : voices) voiceRender (v, D, vL.data(), vR.data(), n0, N, P.follow);
        // ensemble, eco, reverb, mezcla
        const double beat = sr * 60 / P.bpm, dT = beat * 0.75; const int EN = (int) ensL.size();
        const double dryG = clampd ((1 - P.blend) * 2, 0, 1) * 0.9, wetG = clampd (P.blend * 2, 0, 1), outG = std::pow (10.0, P.out / 20);
        const double ens = 0.12 + P.amb * 0.35, echo = P.amb * 0.28, verb = P.amb * 0.6;
        double pk = 0; const double shK = 1 - std::exp (-2 * PI * 6000 / sr);
        auto rdE = [&] (const std::vector<float>& b, double p) { const int i = (int) std::floor (p); const double f = p - i; const int a = imod (i, EN); return b[(size_t) a] + (b[(size_t) ((a + 1) % EN)] - b[(size_t) a]) * f; };
        for (int n = 0; n < N; ++n)
        {
            double l = vL[(size_t) n], r = vR[(size_t) n];
            if (ens > 0.01)
            {
                ensL[(size_t) eW] = (float) l; ensR[(size_t) eW] = (float) r; eP += 0.6 / sr; if (eP > 1) eP -= 1;
                const double d1 = (0.011 + 0.004 * std::sin (TWO_PI * eP)) * sr, d2 = (0.013 + 0.004 * std::cos (TWO_PI * eP)) * sr;
                const double cl = rdE (ensR, eW - d1), cr = rdE (ensL, eW - d2); eW = (eW + 1) % EN;
                l += cl * ens * 0.7; r += cr * ens * 0.7;
            }
            if (echo > 0.01)
            {
                const int DL = (int) dl.size(); const double rp = std::fmod (dw - dT + DL, (double) DL); const int i = (int) std::floor (rp); const double f = rp - i;
                const double ol = dl[(size_t) i] + (dl[(size_t) ((i + 1) % DL)] - dl[(size_t) i]) * f, orr = dr[(size_t) i] + (dr[(size_t) ((i + 1) % DL)] - dr[(size_t) i]) * f;
                dl[(size_t) dw] = (float) (l * 0.6 + orr * 0.45); dr[(size_t) dw] = (float) (r * 0.6 + ol * 0.45); dw = (dw + 1) % DL;
                l += ol * echo; r += orr * echo;
            }
            hpL = 0.995 * (hpL + l - hxL); hxL = l; hpR = 0.995 * (hpR + r - hxR); hxR = r; l = hpL; r = hpR;
            shL += (l - shL) * shK; shR += (r - shR) * shK; l -= (l - shL) * 0.45; r -= (r - shR) * 0.45;
            const double lv = std::max (std::abs (l), std::abs (r)); cEnv = lv > cEnv ? cEnv + (lv - cEnv) * 0.01 : cEnv * 0.9997;
            const double cdb = 20 * std::log10 (cEnv + 1e-9), cg = cdb > -16 ? std::pow (10.0, -(cdb + 16) * 0.5 / 20) : 1; l *= cg * 1.25; r *= cg * 1.25;
            l = std::tanh (l * 1.1) / 1.1; r = std::tanh (r * 1.1) / 1.1;
            if (verb > 0.01)
            {
                const int PN = (int) pre.size(); const double pin = pre[(size_t) pI]; pre[(size_t) pI] = (float) ((l + r) * 0.5); pI = (pI + 1) % PN;
                rLp += (pin - rLp) * 0.35; const double rin = rLp * 0.02; double rl = 0, rr = 0;
                for (auto& c : cL) { const double o = c.b[(size_t) c.i]; c.s = o * 0.6 + c.s * 0.4; c.b[(size_t) c.i] = (float) (rin + c.s * 0.85); c.i = (c.i + 1) % (int) c.b.size(); rl += o; }
                for (auto& c : cR) { const double o = c.b[(size_t) c.i]; c.s = o * 0.6 + c.s * 0.4; c.b[(size_t) c.i] = (float) (rin + c.s * 0.85); c.i = (c.i + 1) % (int) c.b.size(); rr += o; }
                for (auto& a : aL) { const double bo = a.b[(size_t) a.i], o = -rl + bo; a.b[(size_t) a.i] = (float) (rl + bo * 0.5); a.i = (a.i + 1) % (int) a.b.size(); rl = o; }
                for (auto& a : aR) { const double bo = a.b[(size_t) a.i], o = -rr + bo; a.b[(size_t) a.i] = (float) (rr + bo * 0.5); a.i = (a.i + 1) % (int) a.b.size(); rr = o; }
                l += rl * verb * 1.1; r += rr * verb * 1.1;
            }
            const double dL = dryL[(size_t) dI], dR = dryR[(size_t) dI]; dryL[(size_t) dI] = inL[n]; dryR[(size_t) dI] = inR[n]; dI = (dI + 1) % (int) dryL.size();
            double ol = (dL * dryG + l * wetG) * outG, orr = (dR * dryG + r * wetG) * outG;
            if (P.bypass) { ol = dL; orr = dR; }
            if (! std::isfinite (ol) || ! std::isfinite (orr)) { ol = orr = 0; }
            const double lp = std::max (std::abs (ol), std::abs (orr)), lt = lp > 0.97 ? 0.97 / lp : 1;
            lim = lt < lim ? lt : lim + (1 - lim) * 0.0003; ol *= lim; orr *= lim;
            oL[n] = (float) ol; oR[n] = (float) orr; pk = std::max (pk, std::abs (ol));
        }
        level = std::max (pk, level * 0.9); inLevel = std::max (inPk, inLevel * 0.9);
    }
};
} // namespace mg
