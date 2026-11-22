// Prueba de estrés: automatización loca, cambios de modo, silencio, ruido a tope, CC, varios sample rates.
#include "satura/SaturaEngine.h"
#include "slowed/SlowedEngine.h"
#include "ninja/NinjaEngine.h"
#include <cstdio>
#include <chrono>
using namespace pp;
template <class E, class F> void run (const char* name, double fs, F setp)
{
    E e; e.prepare (fs); Rng r; const int B = 333; std::vector<float> L (B), R (B), oL (B), oR (B);
    double peak = 0; bool bad = false; const int blocks = (int) (fs * 40 / B);
    auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        const int phase = (b / 300) % 5;
        for (int i = 0; i < B; ++i)
        {
            double v = 0;
            if (phase == 0) v = 0.5 * std::sin (TAU * 110 * (b * B + i) / fs);
            else if (phase == 1) v = (r.next() * 2 - 1);                 // ruido a tope
            else if (phase == 2) v = 0;                                  // silencio
            else if (phase == 3) v = 0.9;                                // continua
            else v = ((b * B + i) % (int) (fs * 0.25) < 200) ? 1.0 : 0; // golpes
            L[(size_t) i] = (float) v; R[(size_t) i] = (float) (phase == 1 ? (r.next() * 2 - 1) : v);
        }
        setp (e, b, r);
        e.process (L.data(), R.data(), oL.data(), oR.data(), B);
        for (int i = 0; i < B; ++i) { const double a = std::abs (oL[(size_t) i]) + 0 * std::abs (oR[(size_t) i]); if (! std::isfinite (oL[(size_t) i]) || ! std::isfinite (oR[(size_t) i])) bad = true; peak = std::max ({ peak, (double) std::abs (oL[(size_t) i]), (double) std::abs (oR[(size_t) i]) }); (void) a; }
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("%-7s %6.0f Hz  pico %.3f  %s  CPU %.1f%% de un núcleo\n", name, fs, peak, bad ? "¡NaN/INF!" : "ok", secs / 40 * 100);
}
int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        run<SaturaEngine> ("satura", fs, [] (SaturaEngine& e, int b, Rng& r) { e.k = (b % 97 == 0) ? r.next() : e.k; e.mix = 0.5 + 0.5 * std::sin (b * 0.01); if (b % 211 == 0) e.preset = (int) (r.next() * 4); });
        run<SlowedEngine> ("slowed", fs, [] (SlowedEngine& e, int b, Rng& r) { e.k = (b % 53 == 0) ? r.next() : e.k; if (b % 400 < 40) e.k = 1.0; e.mix = 1; e.bpm = 90 + 80 * r.next(); if (b % 300 == 0) e.preset = (int) (r.next() * 3); });
        run<NinjaEngine>  ("ninja",  fs, [] (NinjaEngine& e, int b, Rng& r) { e.k = (b % 41 == 0) ? r.next() : e.k; if (b % 500 < 60) e.k = 1.0; if (b % 150 == 0) e.preset = (int) (r.next() * 12); if (b % 90 == 0) e.div = (int) (r.next() * 6); e.bpm = 60 + 140 * (b % 1000) / 1000.0; });
    }
}
