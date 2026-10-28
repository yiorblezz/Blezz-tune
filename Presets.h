#pragma once
#include <juce_core/juce_core.h>

// Presets "inspirados en" el sonido de artistas urbanos latinos.
// Son aproximaciones de oído a cómo suenan sus voces en disco, NO configuraciones oficiales.
// El preset NO cambia la Tonalidad (Key): esa la pones según tu beat.
namespace blezz
{
struct Preset
{
    const char* name;
    int   scale;      // 0 Cromática, 1 Mayor, 2 Menor, 3 Menor armónica, 4 Frigia, 5 Pentatónica menor
    float retuneMs;   // 0 = robot duro
    float humanize;   // 0..100
    bool  fetOn;  float fetIn, fetOut, fetAtk, fetRel; int fetRatio; // ratio idx: 0=4,1=8,2=12,3=20,4=ALL
    bool  optoOn; float optoPeak, optoGain; int optoMode;            // 0 Compress, 1 Limit
    float brillo;     // 0..100
};

inline const Preset presets[] =
{
//    nombre                          esc  ret   hum  fet  in    out   atk  rel  rat  opto peak gain mode brillo
    { "Default Natural",              2,   25,   30,  true, 12,  -4,  4,   5,   0,  true, 35,  4,  0,   35 },
    { "Estilo Bad Bunny",             2,   12,   15,  true, 16,  -6,  5,   6,   0,  true, 40,  5,  0,   55 },
    { "Estilo Anuel AA (Hard Tune)",  2,    0,    0,  true, 20,  -8,  6,   6,   1,  true, 45,  6,  0,   50 },
    { "Estilo Myke Towers",           2,    5,   10,  true, 18,  -7,  5,   6,   0,  true, 40,  5,  0,   50 },
    { "Estilo Ozuna (Melódico)",      2,    3,   10,  true, 14,  -5,  4,   5,   0,  true, 45,  5,  0,   60 },
    { "Estilo Rauw Alejandro (R&B)",  2,   20,   35,  true, 12,  -4,  3,   5,   0,  true, 38,  4,  0,   65 },
    { "Estilo Feid (Suave)",          2,   15,   25,  true, 12,  -4,  4,   5,   0,  true, 42,  5,  0,   60 },
    { "Estilo Karol G (Pop Urbano)",  1,   30,   40,  true, 10,  -3,  4,   5,   0,  true, 35,  4,  0,   60 },
    { "Estilo Eladio Carrión (Trap)", 2,    2,    5,  true, 22,  -8,  6,   7,   1,  true, 45,  6,  0,   45 },
    { "Estilo Duki (Trap AR)",        2,    0,    0,  true, 24, -10,  7,   7,   4,  true, 50,  6,  1,   50 },
    { "Dembow Frigio",                4,    8,   10,  true, 16,  -6,  5,   6,   0,  true, 40,  5,  0,   55 },
    { "Robot 777",                    0,    0,    0,  true, 24, -10,  7,   7,   4,  true, 55,  7,  1,   45 },
};

inline constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));
} // namespace blezz
