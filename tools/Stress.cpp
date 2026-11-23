// Prueba de estrés: genomas al azar, todos los modos, notas a lo loco, varios sample rates.
#include "MutagenEngine.h"
#include <cstdio>
#include <chrono>
int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        mg::Engine e; e.prepare (fs, 4242u); mg::Rand r; r.a = 99;
        const int B = 480; std::vector<float> z (B, 0.f), L (B), R (B); double pk = 0; bool bad = false; int blocks = (int) (fs * 60 / B);
        auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b)
        {
            if (b % 400 == 0) { for (auto& g : e.G) g = r(); e.P.toca = std::floor (r() * 8); e.P.vari = r(); e.P.poly = 1 + std::floor (r() * 6); e.P.sync = std::floor (r() * 7); e.P.amb = r(); e.P.oct = std::floor (r() * 5) - 2; e.P.move = r(); e.P.flUp = r(); e.P.flDown = r(); e.P.flFifth = r(); e.P.bright = r() * 2 - 1; }
            if (b % 37 == 0) e.noteOn (36 + (int) (r() * 50), r());
            if (b % 23 == 0) e.noteOff (36 + (int) (r() * 50));
            if (b % 997 == 0) e.allNotesOff();
            if (b % 1500 == 0) { e.seqOn = ! e.seqOn; e.seqProg = (int) (r() * 6); }
            e.P.bpm = 60 + 140 * r() * 0 + 140;
            e.process (z.data(), z.data(), L.data(), R.data(), B);
            for (int i = 0; i < B; ++i) { if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) bad = true; pk = std::max ({ pk, (double) std::abs (L[(size_t) i]), (double) std::abs (R[(size_t) i]) }); }
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("%6.0f Hz  pico %.3f  %s  CPU %.1f%% de un núcleo\n", fs, pk, bad ? "¡NaN/INF!" : "ok", secs / 60 * 100);
    }
}
