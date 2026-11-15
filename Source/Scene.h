#pragma once
#include "Gfx.h"

// Escena: mesa de laca, cenicero, el cigarro y la mano tatuada, farolillos, neón, katana...
// Se dibuja en coordenadas lógicas de 420x340 (igual que la vista previa web).
class Scene
{
public:
    static constexpr float SW = 420.0f, SH = 340.0f;

    void update (float dt, float crush01, float level, int transients);
    void draw (gfx::Canvas& c);

    // estado que también usa la pantalla (HUD, efectos)
    float t = 0, crush = 0, level = 0, lvS = 0, press = 0, vel = 0, shake = 0, pulse = 0;
    float ember = 1, glow = 0;
    bool slammed = false, ashBroken = false;
    int lastTr = 0, score = 0, combo = 0;
    float comboT = 0, impactT = 0, flashT = 0, glitchT = 0;

private:
    struct P { float x, y, vx, vy, life, r; };
    struct Tob { float x, y, vx, vy, rot, vr, len, life; };
    struct Puff { float x, y, vx, vy, r, a; };
    struct Petal { float x, y, vx, vy, rot, vr, life; };
    struct Splat { float x, y, r, a, drip, dripV; int seed; };

    std::vector<P> sparks, ash;
    std::vector<Tob> tobacco;
    std::vector<Puff> puffs;
    std::vector<Petal> petals;
    std::vector<Splat> splats;
    juce::Random rng { 1234 };

    float rr (float a, float b) { return a + rng.nextFloat() * (b - a); }
    void burst (int n, float power, bool isAsh);
    void burstTobacco (int n, float power);
    void inkSplat();

    struct CigGeom { juce::Point<float> E, C, F, top, dir; float ang, X, crLen; };
    CigGeom cigGeom (float p) const;

    void lantern (gfx::Canvas&, float x, float top, const juce::String& kanji, float phase);
    void background (gfx::Canvas&);
    void table (gfx::Canvas&);
    void props (gfx::Canvas&);
    void ashtray (gfx::Canvas&, bool front);
    CigGeom cigarette (gfx::Canvas&);
    void hand (gfx::Canvas&, juce::Point<float> top, juce::Point<float> dir);
    void particles (gfx::Canvas&);
    void petalsDraw (gfx::Canvas&);
    void splatsDraw (gfx::Canvas&);
    void overlay (gfx::Canvas&);
};
