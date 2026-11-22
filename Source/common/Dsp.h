#pragma once
// Piezas de DSP compartidas (traducción directa de las vistas previas web)
#include <cmath>
#include <cstdint>
#include <vector>
#include <atomic>
#include <algorithm>

namespace pp {
constexpr double TAU = 6.283185307179586;
inline double clampd (double x, double a, double b) { return std::max (a, std::min (b, x)); }
inline double lerpd (double a, double b, double t) { return a + (b - a) * t; }
inline double smoothd (double a, double b, double x) { const double t = clampd ((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
inline double dB (double x) { return std::pow (10.0, x / 20.0); }
inline double sgn (double x) { return x > 0 ? 1.0 : x < 0 ? -1.0 : 0.0; }

// Biquad (mismas fórmulas que la clase BQ de JS). Coeficientes en double, estado en double.
struct BQ {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    BQ& lp (double fc, double Q, double fs, bool clampFc = true) { if (clampFc) fc = std::min (fc, fs * 0.45); const double w = TAU * fc / fs, c = std::cos (w), al = std::sin (w) / (2 * Q), n = 1 + al; b0 = (1 - c) / 2 / n; b1 = (1 - c) / n; b2 = b0; a1 = -2 * c / n; a2 = (1 - al) / n; return *this; }
    BQ& hp (double fc, double Q, double fs) { const double w = TAU * fc / fs, c = std::cos (w), al = std::sin (w) / (2 * Q), n = 1 + al; b0 = (1 + c) / 2 / n; b1 = -(1 + c) / n; b2 = b0; a1 = -2 * c / n; a2 = (1 - al) / n; return *this; }
    BQ& pk (double fc, double Q, double gdb, double fs) { const double A = std::pow (10.0, gdb / 40), w = TAU * fc / fs, c = std::cos (w), al = std::sin (w) / (2 * Q), n = 1 + al / A; b0 = (1 + al * A) / n; b1 = -2 * c / n; b2 = (1 - al * A) / n; a1 = -2 * c / n; a2 = (1 - al / A) / n; return *this; }
    inline double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

// Freeverb (8 peines + 4 pasatodos)
struct Rev {
    struct Comb { std::vector<float> b; int i = 0; double f = 0; };
    struct AP { std::vector<float> b; int i = 0; };
    Comb comb[8]; AP ap[4];
    void make (double fs, int off, double combScale) {
        static const int CT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, AT[4] = { 556, 441, 341, 225 };
        for (int k = 0; k < 8; ++k) { comb[k].b.assign ((size_t) std::max (1L, std::lround ((CT[k] + off) * fs / 44100.0 * combScale)), 0.f); comb[k].i = 0; comb[k].f = 0; }
        for (int k = 0; k < 4; ++k) { ap[k].b.assign ((size_t) std::max (1L, std::lround ((AT[k] + off) * fs / 44100.0)), 0.f); ap[k].i = 0; }
    }
    inline double run (double x, double fb, double damp) {
        double out = 0;
        for (auto& c : comb) { const double o = c.b[(size_t) c.i]; c.f = o * (1 - damp) + c.f * damp; c.b[(size_t) c.i] = (float) (x + c.f * fb); if (++c.i >= (int) c.b.size()) c.i = 0; out += o; }
        for (auto& a : ap) { const double bo = a.b[(size_t) a.i], y = bo - out; a.b[(size_t) a.i] = (float) (out + bo * 0.5); if (++a.i >= (int) a.b.size()) a.i = 0; out = y; }
        return out;
    }
};

struct Rng { uint32_t s = 0x9E3779B9u; double next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / 16777216.0; } };

// atómicos para la pantalla
struct AtomF { std::atomic<float> v { 0 }; void set (double x) { v.store ((float) x, std::memory_order_relaxed); } float get() const { return v.load (std::memory_order_relaxed); } };
}
