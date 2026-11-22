// Render offline para comparar el C++ con la vista previa JS.
// uso: PepelusRender <satura|slowed|ninja> in.f32 out.f32 fs k p2 preset bpm [kSwitch tSwitch]
#include "satura/SaturaEngine.h"
#include "slowed/SlowedEngine.h"
#include "ninja/NinjaEngine.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace pp;
int main (int argc, char** argv)
{
    if (argc < 9) { std::printf ("uso\n"); return 1; }
    const char* which = argv[1]; FILE* f = std::fopen (argv[2], "rb"); if (! f) return 2;
    std::fseek (f, 0, SEEK_END); long sz = std::ftell (f); std::fseek (f, 0, SEEK_SET);
    std::vector<float> in ((size_t) sz / 4); std::fread (in.data(), 4, in.size(), f); std::fclose (f);
    const double fs = std::atof (argv[4]), k = std::atof (argv[5]), p2 = std::atof (argv[6]); const int preset = std::atoi (argv[7]); const double bpm = std::atof (argv[8]);
    const double k2 = argc > 10 ? std::atof (argv[9]) : -1, tsw = argc > 10 ? std::atof (argv[10]) : 1e9;
    const int n = (int) (in.size() / 2), B = 512; std::vector<float> out (in.size()), L (B), R (B), oL (B), oR (B);
    SaturaEngine sa; SlowedEngine sl; NinjaEngine nj;
    sa.prepare (fs); sl.prepare (fs); nj.prepare (fs);
    for (int o = 0; o + B <= n; o += B)
    {
        const double kk = (o >= tsw * fs && k2 >= 0) ? k2 : k;
        for (int i = 0; i < B; ++i) { L[(size_t) i] = in[(size_t) (o + i) * 2]; R[(size_t) i] = in[(size_t) (o + i) * 2 + 1]; }
        if (! std::strcmp (which, "satura")) { sa.k = kk; sa.mix = p2; sa.preset = preset; sa.process (L.data(), R.data(), oL.data(), oR.data(), B); }
        else if (! std::strcmp (which, "slowed")) { sl.k = kk; sl.mix = p2; sl.preset = preset; sl.bpm = bpm; sl.process (L.data(), R.data(), oL.data(), oR.data(), B); }
        else { nj.k = kk; nj.div = (int) p2; nj.preset = preset; nj.bpm = bpm; nj.process (L.data(), R.data(), oL.data(), oR.data(), B); }
        for (int i = 0; i < B; ++i) { out[(size_t) (o + i) * 2] = oL[(size_t) i]; out[(size_t) (o + i) * 2 + 1] = oR[(size_t) i]; }
    }
    f = std::fopen (argv[3], "wb"); std::fwrite (out.data(), 4, out.size(), f); std::fclose (f);
    return 0;
}
