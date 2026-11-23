// Renderiza un escenario (genoma + mandos + notas MIDI) con el motor C++: para compararlo con la vista previa.
#include "MutagenEngine.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
int main (int argc, char** argv)
{
    if (argc < 3) { std::fprintf (stderr, "uso: render escenario.txt salida.f32\n"); return 1; }
    std::ifstream in (argv[1]); std::string line; double sr = 48000; int blocks = 100, B = 1024; unsigned seed = 12345;
    mg::Engine e; std::vector<std::tuple<int, int, int, double>> ev; mg::Genome g = mg::defGenome(); mg::Params P;
    while (std::getline (in, line))
    {
        std::istringstream ss (line); std::string k; ss >> k;
        if (k == "sr") ss >> sr; else if (k == "blocks") ss >> blocks; else if (k == "B") ss >> B; else if (k == "seed") ss >> seed;
        else if (k == "G") { std::string n; double v; ss >> n >> v; for (int i = 0; i < mg::G_COUNT; ++i) if (n == mg::GNAMES[i]) g[(size_t) i] = v; }
        else if (k == "P")
        {
            std::string n; double v; ss >> n >> v;
            double* f = n == "blend" ? &P.blend : n == "amb" ? &P.amb : n == "oct" ? &P.oct : n == "follow" ? &P.follow : n == "bright" ? &P.bright : n == "attOff" ? &P.attOff : n == "relOff" ? &P.relOff
                      : n == "move" ? &P.move : n == "out" ? &P.out : n == "poly" ? &P.poly : n == "bpm" ? &P.bpm : n == "sync" ? &P.sync : n == "toca" ? &P.toca : n == "vari" ? &P.vari
                      : n == "flUp" ? &P.flUp : n == "flDown" ? &P.flDown : n == "flFifth" ? &P.flFifth : n == "pgGain" ? &P.pgGain : n == "pgTame" ? &P.pgTame : nullptr;
            if (f) *f = v;
        }
        else if (k == "E") { int b, on, m; double v; ss >> b >> on >> m >> v; ev.push_back ({ b, on, m, v }); }
        else if (k == "SEQ") { int on, prog; ss >> on >> prog; e.seqOn = on; e.seqProg = prog; }
    }
    const bool so = e.seqOn; const int sp = e.seqProg;
    e.prepare (sr, seed); e.G = g; e.P = P; e.seqOn = so; e.seqProg = sp;
    std::vector<float> L ((size_t) B, 0.f), R ((size_t) B, 0.f), oL ((size_t) B), oR ((size_t) B), out;
    for (int b = 0; b < blocks; ++b)
    {
        for (auto& [eb, on, m, v] : ev) if (eb == b) { if (on) e.noteOn (m, v); else e.noteOff (m); }
        e.process (L.data(), R.data(), oL.data(), oR.data(), B);
        for (int i = 0; i < B; ++i) { out.push_back (oL[(size_t) i]); out.push_back (oR[(size_t) i]); }
    }
    FILE* f = std::fopen (argv[2], "wb"); std::fwrite (out.data(), sizeof (float), out.size(), f); std::fclose (f);
    return 0;
}
