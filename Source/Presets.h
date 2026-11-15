#pragma once
#include <array>

struct NicotinaPreset
{
    const char* cat;
    const char* name;
    int mode;
    float crush, bits, rate, mix, output;
};

// Los mismos 14 presets que la vista previa (volumen igualado salvo 808 SUCIO)
inline constexpr std::array<NicotinaPreset, 14> kPresets {{
    { "INIT",   "INIT",        0, 0,   8,  8,    100, 0    },
    { "808",    "808 SUCIO",   1, 55,  7,  2,    55,  -1   },
    { "808",    "808 RABIA",   3, 60,  9,  1.5f, 45,  -3   },
    { "DRUMS",  "KICK PUNCH",  1, 42,  10, 2,    35,  -1   },
    { "DRUMS",  "SNARE CRUJE", 0, 72,  6,  9,    60,  -3   },
    { "DRUMS",  "HATS METAL",  0, 90,  9,  22,   85,  -4   },
    { "VOZ",    "ONI \xe9\xac\xbc", 3, 82, 5, 7, 45, -9 },
    { "VOZ",    "VOZ ROBOT",   0, 100, 7,  16,   65,  -5.5f },
    { "VOZ",    "RADIO ROTA",  2, 75,  7,  4,    100, -8   },
    { "SYNTH",  "CHIPTUNE",    0, 100, 4,  6,    100, -4.5f },
    { "SYNTH",  "PAD LOFI",    1, 60,  9,  4,    70,  -3   },
    { "SYNTH",  "BAJO REESE",  3, 55,  8,  1.5f, 55,  -5   },
    { "MASTER", "CINTA VIEJA", 1, 32,  12, 2,    30,  -1   },
    { "FX",     "APLASTADO",   3, 100, 2,  32,   100, -3.5f },
}};

inline constexpr const char* kModeNames[4] = { "GAMEBOY", "SP-1200", "RADIO", "DESTROY" };
inline constexpr const char* kModeHints[4] = { "8 BIT CRUDO", "12 BIT CALIDO", "RADIO ROTA", "PLIEGUE+CAOS" };
