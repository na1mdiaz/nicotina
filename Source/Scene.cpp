#include "Scene.h"

using namespace gfx;
using juce::Point;
using juce::String;

namespace
{
    const Point<float> EMBER { 206.0f, 217.0f };          // donde se apaga el cigarro (dentro del cenicero)
    struct { float x = 210, y = 262, k = 1.75f, dy = 38; } FG;   // zoom del primer plano
    constexpr float CIG_PAPER = 80.0f, CIG_FILTER = 26.0f, CIG_W = 7.0f;

    Colour C (juce::uint32 rgb, float a = 1.0f) { return Colour (0xff000000 | rgb).withAlpha (a); }
}

//==============================================================================
void Scene::burst (int n, float power, bool isAsh)
{
    for (int i = 0; i < n; ++i)
    {
        const float a = rr (-PI * 0.95f, -PI * 0.05f), sp = rr (60, 280) * power;
        P o { EMBER.x + rr (-8, 8), EMBER.y + rr (-4, 2), std::cos (a) * sp, std::sin (a) * sp, rr (0.5f, 1.4f), rr (0.8f, 2.4f) };
        (isAsh ? ash : sparks).push_back (o);
    }
}

void Scene::burstTobacco (int n, float power)
{
    for (int i = 0; i < n; ++i)
    {
        const float a = rr (-PI * 0.9f, -PI * 0.1f), sp = rr (40, 200) * power;
        if (tobacco.size() < 160)
            tobacco.push_back ({ EMBER.x + rr (-6, 6), EMBER.y - rr (0, 6), std::cos (a) * sp, std::sin (a) * sp, rr (0, 6), rr (-12, 12), rr (2, 5), 1.0f });
    }
}

void Scene::inkSplat()
{
    if (splats.size() > 40) return;
    splats.push_back ({ rr (30, SW - 30), rr (20, SH - 110), rr (6, 15), 1.0f, 0.0f, rr (4, 14), rng.nextInt (1000000) });
}

template <typename T, typename F> static void eraseIf (std::vector<T>& v, F f) { v.erase (std::remove_if (v.begin(), v.end(), f), v.end()); }

void Scene::update (float dt, float crush01, float lvl, int transients)
{
    dt = juce::jmin (dt, 0.05f);
    t += dt;
    crush = crush01;
    level = juce::jmin (1.5f, lvl);
    lvS += (level - lvS) * juce::jmin (1.0f, dt * 10.0f);
    pulse = juce::jmax (level, pulse - dt * 4.0f);

    const bool wants = crush >= 0.9f;
    const float target = wants ? 1.0f : (crush / 0.9f) * 0.72f;
    if (wants && press < 1.0f)
    {
        vel += 14.0f * dt; press = juce::jmin (1.0f, press + vel * dt);
        if (press >= 1.0f && ! slammed)
        {
            slammed = true; shake = 1; impactT = 1; flashT = 0.09f; glitchT = 0.35f; combo = 0;
            burstTobacco (26, 1.1f);
            burst (70, 1.3f, false); burst (40, 0.9f, true);
            for (int i = 0; i < 4; ++i) inkSplat();
            for (int i = 0; i < 6; ++i) puffs.push_back ({ EMBER.x + rr (-14, 14), EMBER.y - 4, rr (-30, 30), rr (-50, -20), rr (6, 10), 0.55f });
        }
    }
    else
    {
        vel = 0;
        press += (target - press) * juce::jmin (1.0f, dt * (target < press ? 2.5f : 9.0f));
    }
    if (crush < 0.86f) { slammed = false; combo = 0; }
    if (press > 0.12f && ! ashBroken) { ashBroken = true; burst (16, 0.5f, true); }
    if (press < 0.05f) ashBroken = false;
    impactT = juce::jmax (0.0f, impactT - dt * 1.4f);
    flashT  = juce::jmax (0.0f, flashT - dt);
    glitchT = juce::jmax (0.0f, glitchT - dt);
    comboT  = juce::jmax (0.0f, comboT - dt); if (comboT <= 0) combo = 0;
    for (auto& q : tobacco) { q.vy += 420 * dt; q.x += q.vx * dt; q.y += q.vy * dt; q.rot += q.vr * dt; q.life -= dt * 0.35f; if (q.y > 300) { q.y = 300; q.vy *= -0.2f; q.vx *= 0.4f; q.vr = 0; } }
    eraseIf (tobacco, [] (const Tob& q) { return q.life <= 0; });
    shake = juce::jmax (0.0f, shake - dt * 3.5f);

    ember += ((slammed ? 0.12f : 1.0f) - ember) * juce::jmin (1.0f, dt * (slammed ? 6.0f : 1.2f));
    glow += (clampf (crush * 1.1f + lvS * 0.3f * crush, 0, 1) - glow) * juce::jmin (1.0f, dt * 6.0f);

    if (slammed)
    {
        if (transients != lastTr)
        {
            burst (10 + (int) (lvS * 28), 0.6f + lvS, false);
            score = (score + 10 * (1 + combo) + (int) (lvS * 40)) % 1000000;
            ++combo; comboT = 2; glitchT = juce::jmax (glitchT, 0.12f + 0.12f * lvS);
            if (lvS > 0.5f) flashT = juce::jmax (flashT, 0.04f);
            burstTobacco (4 + (int) (lvS * 8), 0.6f);
            burst (6 + (int) (lvS * 10), 0.5f + lvS * 0.5f, true);
            shake = juce::jmax (shake, 0.35f + 0.4f * lvS);
            if (rng.nextFloat() < 0.55f) { inkSplat(); if (rng.nextFloat() < 0.4f) inkSplat(); }
        }
        if (rng.nextFloat() < dt * (2 + lvS * 20)) burst (1 + rng.nextInt (3), 0.35f, false);
    }
    else if (rng.nextFloat() < dt * (0.6f + lvS * 4) * ember) burst (1, 0.15f, false);
    lastTr = transients;

    for (auto& p : sparks) { p.vy += 380 * dt; p.vx *= 0.99f; p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt; if (p.y > 300) { p.y = 300; p.vy *= -0.3f; p.vx *= 0.5f; } }
    for (auto& p : ash) { p.vy += 160 * dt; p.vx *= 0.97f; p.x += p.vx * dt + std::sin (t * 4 + p.r * 10) * 8 * dt; p.y += p.vy * dt; p.life -= dt * 0.5f; if (p.y > 300) { p.y = 300; p.vy = 0; p.vx = 0; } }
    eraseIf (sparks, [] (const P& p) { return p.life <= 0; });
    eraseIf (ash, [] (const P& p) { return p.life <= 0; });

    for (auto& s : splats) { s.drip = juce::jmin (90.0f, s.drip + s.dripV * dt); s.dripV = juce::jmax (0.0f, s.dripV - dt * 1.5f); s.a -= dt * (slammed ? 0.1f : 1.1f); }
    eraseIf (splats, [] (const Splat& s) { return s.a <= 0; });

    if (rng.nextFloat() < dt * (3 + lvS * 12) * (0.25f + 0.75f * ember))
        puffs.push_back ({ EMBER.x + rr (-2, 2), EMBER.y - 3, rr (-8, 8), rr (-36, -22), rr (2.5f, 4.5f), rr (0.25f, 0.45f) });
    for (auto& p : puffs) { p.x += p.vx * dt + std::sin (t * 1.8f + p.y * 0.05f) * 10 * dt; p.y += p.vy * dt; p.r += dt * 8; p.a -= dt * 0.16f; }
    eraseIf (puffs, [] (const Puff& p) { return p.a <= 0; });

    if (rng.nextFloat() < dt * 0.9f && petals.size() < 14)
        petals.push_back ({ rr (0, SW), -8, rr (4, 16), rr (14, 26), rr (0, 6.28f), rr (-2, 2), 1.0f });
    for (auto& p : petals) { p.x += (p.vx + std::sin (t * 2 + p.rot) * 10) * dt; p.y += p.vy * dt; p.rot += p.vr * dt; if (p.y > 300) p.life -= dt * 0.6f; }
    eraseIf (petals, [] (const Petal& p) { return p.life <= 0 || p.x > SW + 10; });
}

//==============================================================================
void Scene::lantern (Canvas& c, float x, float top, const String& kanji, float phase)
{
    const float sway = std::sin (t * 1.3f + phase) * 0.05f + std::sin (t * 9) * pulse * 0.025f;
    const float cordL = 18, w = 54, h = 76, cy = cordL + 8 + h / 2;
    c.save();
    c.translate (x, top); c.rotate (sway);
    c.line (0, -top, 0, cordL, grey (20), 1.5f);
    const float gl = 0.35f + 0.65f * juce::jmin (1.0f, pulse * 1.2f);
    c.fill (circle (0, cy, 90), radial (0, cy, 4, 90, { { 0, rgba (211, 18, 26, 0.32f * gl) }, { 1, rgba (211, 18, 26, 0) } }));
    c.fill (ellipse (0, cy, w / 2, h / 2),
            radial (-4, cy - 5, 4, w * 0.7f, { { 0, rgba ((int) (200 + 55 * gl), (int) (40 + 40 * gl), (int) (30 + 20 * gl)) }, { 0.6f, C (0xa3101a) }, { 1, C (0x4a0508) } }));
    for (int i = -3; i <= 3; ++i)
    {
        const float yy = cy + i * h / 8, ww = (w / 2) * std::sqrt (juce::jmax (0.0f, 1 - std::pow ((yy - cy) / (h / 2), 2.0f)));
        juce::Path p; p.startNewSubPath (-ww, yy); p.quadraticTo (0, yy + 3, ww, yy);
        c.stroke (p, rgba (0, 0, 0, 0.35f), 1);
    }
    c.text (kanji, 0, cy + 2, Fonts::dot (36.5f), rgba (10, 10, 10, 0.92f), Canvas::center, Canvas::middle);
    c.rect (-15, cordL + 2, 30, 8, grey (14)); c.rect (-15, cy + h / 2 - 2, 30, 8, grey (14));
    c.rect (-15, cordL + 2, 30, 1.5f, grey (60)); c.rect (-15, cy + h / 2 - 2, 30, 1.5f, grey (60));
    for (int i = -3; i <= 3; ++i) c.line (0, cy + h / 2 + 6, i * 1.4f + std::sin (t * 3 + i) * 0.8f, cy + h / 2 + 22, RED_D, 1);
    c.restore();
}

void Scene::background (Canvas& c)
{
    c.rect (0, 0, SW, SH, radial (SW / 2, 80, 10, 330, { { 0, grey (44) }, { 1, grey (12) } }));

    // paneles shoji
    for (float x = 0; x <= SW; x += 70)
    {
        c.rect (x + 3, 6, 64, 222, grey (255, 0.015f));
        juce::Path r; r.addRectangle (x + 3, 6, 64, 222); c.stroke (r, grey (0, 0.35f), 1, false);
    }
    for (float x = 0; x < SW; x += 70)
    {
        for (float y = 6; y < 228; y += 37) c.line (x + 3, y, x + 67, y, grey (0, 0.22f), 1);
        c.line (x + 24.3f, 6, x + 24.3f, 228, grey (0, 0.22f), 1);
        c.line (x + 45.6f, 6, x + 45.6f, 228, grey (0, 0.22f), 1);
    }

    // neón: 煙 dentro de un ensō rojo (el resplandor se hace con trazos anchos y suaves)
    const float flick = (std::sin (t * 31) > 0.97f ? 0.4f : 1.0f) * (0.75f + 0.25f * juce::jmin (1.0f, pulse * 1.3f));
    const float nx = 150, ny = 96;
    auto ring = arc (nx, ny, 38, -1.1f, 4.6f);
    for (float wd : { 16.0f, 10.0f, 6.0f }) c.stroke (ring, rgba (255, 40, 50, 0.12f * flick), wd);
    c.stroke (ring, rgba (255, (int) (70 + 80 * flick), (int) (80 + 60 * flick), flick), 3.2f);
    const String smoke = String::fromUTF8 ("\xe7\x85\x99");
    for (float wd : { 8.0f, 5.0f }) c.strokeText (smoke, nx, ny + 2, Fonts::dot (36.5f), rgba (255, 40, 50, 0.15f * flick), wd, Canvas::center, Canvas::middle);
    c.strokeText (smoke, nx, ny + 2, Fonts::dot (36.5f), rgba (255, (int) (70 + 80 * flick), (int) (80 + 60 * flick), flick), 2.2f, Canvas::center, Canvas::middle);
    c.text (smoke, nx, ny + 2, Fonts::dot (36.5f), rgba (255, 190, 190, 0.25f * flick), Canvas::center, Canvas::middle);

    // katana en su soporte
    c.save(); c.translate (-10, 196);
    c.rect (60, -2, 8, 22, grey (26)); c.rect (232, -2, 8, 22, grey (26));
    juce::Path k; k.startNewSubPath (40, 0); k.quadraticTo (150, -10, 270, -4); k.lineTo (270, 3); k.quadraticTo (150, -3, 40, 7); k.closeSubPath();
    c.fill (k, grey (8)); c.stroke (k, grey (80), 0.8f);
    c.rect (270, -6, 54, 9, RED_D);
    for (float x = 272; x < 322; x += 7) c.fill (quad ({ x, -1.5f }, { x + 3.5f, -6 }, { x + 7, -1.5f }, { x + 3.5f, 3 }), grey (14));
    c.rect (266, -9, 4, 15, grey (120));
    c.restore();

    lantern (c, 46, 0, String::fromUTF8 ("\xe7\xb5\x84"), 0);       // 組
    lantern (c, 376, 0, String::fromUTF8 ("\xe7\x81\xb0"), 1.7f);   // 灰

    // tatami
    c.rect (0, 228, SW, SH - 228, linear (0, 228, 0, SH, { { 0, grey (34) }, { 1, grey (12) } }));
    for (float y = 236; y < SH; y += 4) c.line (0, y, SW, y, grey (0, 0.1f), 1, false);
    c.rect (0, 300, SW, 4, grey (8));
    c.rect (0, 301, SW, 1.5f, rgba (122, 3, 9, 0.6f));

    // foco desde arriba
    c.fill (quad ({ 180, 0 }, { 240, 0 }, { 320, 300 }, { 100, 300 }), grey (255, 0.05f));
}

void Scene::table (Canvas& c)
{
    c.fill (ellipse (210, 300, 150, 10), rgba (0, 0, 0, 0.5f));
    c.rect (80, 252, 12, 48, grey (10)); c.rect (328, 252, 12, 48, grey (10));
    c.rect (60, 234, 300, 10, linear (0, 234, 0, 244, { { 0, grey (58) }, { 1, grey (22) } }));
    c.rect (60, 244, 300, 18, linear (0, 244, 0, 262, { { 0, grey (16) }, { 1, grey (4) } }));
    c.rect (60, 244, 300, 1.4f, RED);
    c.rect (60, 260, 300, 1, rgba (211, 18, 26, 0.35f));
    const auto refl = rgba (211, 18, 26, 0.12f + 0.2f * juce::jmin (1.0f, pulse));
    c.rect (70, 235, 40, 3, refl); c.rect (310, 235, 40, 3, refl);
}

void Scene::props (Canvas& c)
{
    // cajetilla ニコチン
    c.save(); c.translate (112, 236); c.rotate (-0.08f);
    c.rect (-2, -2, 40, 6, rgba (0, 0, 0, 0.5f));
    c.rect (0, -44, 34, 44, grey (235));
    c.rect (0, -44, 34, 14, RED);
    c.rect (0, -30, 34, 2, grey (20));
    c.rect (4, -50, 5, 8, grey (250)); c.rect (11, -48, 5, 6, grey (250));
    c.rect (4, -50, 5, 3, C (0xa88f6c)); c.rect (11, -48, 5, 3, C (0xa88f6c));
    c.text (String::fromUTF8 ("\xe3\x83\x8b\xe3\x82\xb3"), 17, -19, Fonts::dot (12), grey (15), Canvas::center, Canvas::middle);
    c.text (String::fromUTF8 ("\xe3\x83\x81\xe3\x83\xb3"), 17, -8, Fonts::dot (12), grey (15), Canvas::center, Canvas::middle);
    { juce::Path r; r.addRectangle (0, -44, 34, 44); c.stroke (r, grey (0, 0.8f), 1, false); }
    c.restore();
    // mechero metálico
    c.save(); c.translate (286, 236);
    c.rect (-2, -2, 24, 5, rgba (0, 0, 0, 0.5f));
    c.rect (0, -30, 20, 30, linear (0, 0, 20, 0, { { 0, grey (90) }, { 0.4f, grey (230) }, { 1, grey (70) } }));
    c.rect (0, -20, 20, 1.5f, grey (20));
    c.text (String::fromUTF8 ("\xe4\xb9\x9d"), 10, -9, Fonts::dot (14), RED, Canvas::center, Canvas::middle);
    { juce::Path r; r.addRectangle (0, -30, 20, 30); c.stroke (r, grey (0, 0.8f), 1, false); }
    c.restore();
}

void Scene::ashtray (Canvas& c, bool front)
{
    const float cx = 206;
    if (! front)
    {
        c.fill (ellipse (cx, 218, 46, 9), grey (70));
        c.fill (ellipse (cx, 219, 38, 6.5f), grey (18));
        c.fill (ellipse (cx + 2, 220, 26 + press * 6, 4), grey (70, 0.9f));
        for (auto [x, y, r, dir] : std::initializer_list<std::tuple<float, float, float, float>> { { cx - 22, 219, 0.15f, 1 }, { cx + 14, 221, -0.3f, -1 } })
        {
            c.save(); c.translate (x, y); c.rotate (r);
            if (dir > 0) { c.rect (0, -1.5f, 12, 3, grey (200)); c.rect (9, -1.5f, 5, 3, C (0xb9a58a)); }
            else         { c.rect (0, -1.5f, 10, 3, grey (200)); c.rect (-4, -1.5f, 5, 3, C (0xb9a58a)); }
            c.restore();
        }
        return;
    }
    // cuerpo delantero de cristal grueso
    juce::Path body;
    body.startNewSubPath (cx - 46, 218);
    for (int i = 0; i <= 24; ++i) { const float a = PI - PI * (float) i / 24.0f; body.lineTo (cx + std::cos (a) * 46, 218 + std::sin (a) * 9); }
    body.lineTo (cx + 42, 234);
    for (int i = 0; i <= 24; ++i) { const float a = PI * (float) i / 24.0f; body.lineTo (cx + std::cos (a) * 42, 234 + std::sin (a) * 6); }
    body.closeSubPath();
    c.fill (body, linear (cx - 46, 0, cx + 46, 0, { { 0, grey (40) }, { 0.3f, grey (150) }, { 0.45f, grey (80) }, { 1, grey (28) } }));
    c.stroke (body, grey (0, 0.7f), 1);
    for (float dx : { -30.0f, 30.0f }) c.fill (ellipse (cx + dx, 225, 5, 3), grey (14));
    juce::Path rim;
    for (int i = 0; i <= 20; ++i) { const float a = 0.1f + (PI - 0.2f) * (float) i / 20.0f; const float x = cx + std::cos (a) * 46, y = 218 + std::sin (a) * 9; if (i == 0) rim.startNewSubPath (x, y); else rim.lineTo (x, y); }
    c.stroke (rim, grey (230, 0.5f), 1.2f);
    c.text (String::fromUTF8 ("\xe4\xb9\x9d"), cx, 229, Fonts::yuji (12), rgba (211, 18, 26, 0.85f), Canvas::center, Canvas::middle);
}

//==============================================================================
// El cigarro se empuja recto contra el cenicero: la punta se chafa y se va acortando. No se dobla.
Scene::CigGeom Scene::cigGeom (float p) const
{
    const float X = 58 * p;
    const float crLen = 2 + X * 0.2f;
    const float grind = slammed ? std::sin (t * 16) * 0.06f * (0.4f + lvS) : 0.0f;
    const float ang = -0.98f - 0.32f * p + grind;
    const Point<float> v { std::cos (ang), std::sin (ang) };
    const Point<float> E = EMBER;
    const Point<float> Cc = E + v * crLen;
    const Point<float> F = Cc + v * (CIG_PAPER - X);
    const Point<float> T = F + v * CIG_FILTER;
    return { E, Cc, F, T, v, ang, X, crLen };
}

static Point<float> tube (Canvas& c, Point<float> a, Point<float> b, float w, std::initializer_list<Stop> stops)
{
    const float dx = b.x - a.x, dy = b.y - a.y, m = juce::jmax (0.0001f, std::hypot (dx, dy)), nx = -dy / m, ny = dx / m, h = w / 2;
    c.fill (quad ({ a.x - nx * h, a.y - ny * h }, { b.x - nx * h, b.y - ny * h }, { b.x + nx * h, b.y + ny * h }, { a.x + nx * h, a.y + ny * h }),
            linear (a.x - nx * h, a.y - ny * h, a.x + nx * h, a.y + ny * h, stops));
    return { nx, ny };
}
#define PAPER_STOPS { { 0, C (0x8f8a82) }, { 0.18f, C (0xe9e6df) }, { 0.42f, C (0xffffff) }, { 0.75f, C (0xd9d5cc) }, { 1, C (0x7d786f) } }
#define CORK_STOPS  { { 0, C (0x6e5638) }, { 0.2f, C (0xb99a6c) }, { 0.45f, C (0xe0c495) }, { 0.8f, C (0xa8875a) }, { 1, C (0x5a4429) } }

Scene::CigGeom Scene::cigarette (Canvas& c)
{
    const float p = press, w = CIG_W;
    const auto G = cigGeom (p);
    const auto E = G.E, Cc = G.C, F = G.F, top = G.top;
    const float ang = G.ang;
    const Point<float> ax { std::cos (ang), std::sin (ang) }, nr { -ax.y, ax.x };

    c.line (E.x + 4, E.y + 3, F.x + 12, F.y + 14, rgba (0, 0, 0, 0.35f), w);
    c.line (Cc.x, Cc.y, top.x, top.y, rgba (10, 10, 10, 0.85f), w + 2.2f);
    tube (c, Cc, F, w, PAPER_STOPS);
    if (p > 0.04f)
    {
        const int n = 2 + juce::roundToInt (p * 4); const float reach = 0.25f + 0.35f * p;
        for (int i = 0; i < n; ++i)
        {
            const float tt = seed01 (i * 5.3f) * reach, x = Cc.x + (F.x - Cc.x) * tt, y = Cc.y + (F.y - Cc.y) * tt, sk = (seed01 (i * 2.1f) - 0.5f) * 4;
            c.line (x - nr.x * w * 0.45f + ax.x * sk, y - nr.y * w * 0.45f + ax.y * sk, x + nr.x * w * 0.45f, y + nr.y * w * 0.45f,
                    rgba (80, 75, 68, (0.3f + p * 0.5f) * (1 - tt / reach * 0.6f)), 0.7f);
        }
        const float bul = juce::jmin (1.0f, p * 1.4f);
        auto e = ellipse (Cc.x + ax.x * 2, Cc.y + ax.y * 2, w * 0.5f * (1 + 0.25f * bul), 2.2f, ang + PI / 2);
        c.fill (e, C (0xf4f1ea)); c.stroke (e, rgba (10, 10, 10, 0.7f), 0.8f);
    }

    // filtro
    const auto fn = tube (c, F, top, w, CORK_STOPS);
    for (int i = 0; i < 16; ++i)
    {
        const float tt = seed01 ((float) i) * 0.95f + 0.03f, o = (seed01 (i + 9.0f) - 0.5f) * w * 0.8f;
        c.rect (F.x + (top.x - F.x) * tt + fn.x * o - 0.5f, F.y + (top.y - F.y) * tt + fn.y * o - 0.5f, 1, 1, rgba (70, 45, 20, 0.55f));
    }
    auto ringAt = [&] (float tt, Colour col, float lw)
    {
        const float x = F.x + (top.x - F.x) * tt, y = F.y + (top.y - F.y) * tt;
        c.line (x - fn.x * w / 2, y - fn.y * w / 2, x + fn.x * w / 2, y + fn.y * w / 2, col, lw, false);
    };
    ringAt (0.04f, RED, 1.6f); ringAt (0.12f, C (0xd8b45a), 0.8f);
    { auto e = ellipse (top.x, top.y, w / 2, w / 4, ang + PI / 2); c.fill (e, C (0xe9dcc2)); c.stroke (e, rgba (0, 0, 0, 0.6f), 0.6f); }

    // masa chafada contra el cenicero, cada vez más ancha y quemada abajo
    if (p > 0.02f)
    {
        const int layers = 2 + juce::roundToInt (G.X / 7);
        for (int i = layers; i >= 0; --i)
        {
            const float tt = (float) i / (float) layers;
            const float x = E.x + (Cc.x - E.x) * tt, y = E.y + (Cc.y - E.y) * tt;
            const float wid = w * (1 + p * 2.1f * (1 - tt * 0.8f) + (slammed ? 0.5f * (1 - tt) : 0.0f)) * (0.85f + seed01 (i * 3.9f) * 0.3f);
            const float hgt = juce::jmax (1.4f, G.crLen / layers + 1.2f) * (0.7f + seed01 (i * 1.3f) * 0.6f);
            const float off = (seed01 (i * 7.7f) - 0.5f) * 3 * p;
            const float rot = (seed01 (i * 7.1f) - 0.5f) * 0.35f;
            const float burn = clampf (1 - tt * 1.6f, 0, 1) * juce::jmin (1.0f, p * 1.5f);
            const int lit = (i % 2) ? 255 : 228;
            auto mixc = [burn] (int v) { return (int) std::round (v * (1 - burn) + 70 * burn); };
            juce::Path layer;
            for (int k = 0; k <= 10; ++k)
            {
                const float a = (float) k / 10.0f * 2 * PI, jag = 0.82f + seed01 (i * 13 + k * 1.7f) * 0.32f;
                const float qx = off + std::cos (a) * wid / 2 * jag, qy = std::sin (a) * hgt * jag;
                const float rx = x + qx * std::cos (rot) - qy * std::sin (rot), ry = y + qx * std::sin (rot) + qy * std::cos (rot);
                if (k == 0) layer.startNewSubPath (rx, ry); else layer.lineTo (rx, ry);
            }
            layer.closeSubPath();
            c.fill (layer, linear (x, y - hgt, x, y + hgt, { { 0, rgba (mixc (lit), mixc (lit - 4), mixc (lit - 12)) }, { 1, rgba (mixc (150), mixc (146), mixc (138)) } }));
            c.stroke (layer, rgba (10, 10, 10, 0.7f), 0.7f);
            juce::Path cr; cr.startNewSubPath (x + off - wid * 0.3f, y); cr.lineTo (x + off + wid * 0.1f, y - hgt * 0.4f); cr.lineTo (x + off + wid * 0.32f, y + hgt * 0.1f);
            c.stroke (cr, rgba (40, 36, 32, 0.35f + burn * 0.4f), 0.6f);
        }
    }
    if (p > 0.3f)
    {
        const float a = clampf ((p - 0.3f) / 0.7f, 0, 1);
        for (int i = 0; i < 14; ++i)
        {
            const float a2 = seed01 (i * 1.7f) * 2 * PI, r = (3 + seed01 (i * 4.1f) * 9) * a;
            c.rect (E.x + std::cos (a2) * r - 1, E.y + 1 + std::sin (a2) * r * 0.35f - 0.6f, 2.4f, 1.2f, (i % 3) ? rgba (110, 78, 44, a) : rgba (90, 88, 84, a));
        }
    }

    // ceniza en la punta y brasa
    const float e = ember;
    if (p < 0.1f)
    {
        const Point<float> A2 { E.x - ax.x * 7, E.y - ax.y * 7 };
        tube (c, E, A2, w * 0.95f, { { 0, C (0x3c3b39) }, { 0.35f, C (0x8f8c86) }, { 0.6f, C (0x6f6c67) }, { 1, C (0x2c2b29) } });
        for (float tt : { 0.35f, 0.7f })
        {
            const float x = E.x - ax.x * 7 * tt, y = E.y - ax.y * 7 * tt;
            c.line (x - nr.x * 3, y - nr.y * 3, x + nr.x * 2, y + nr.y * 2, rgba (20, 20, 20, 0.7f), 0.7f);
        }
    }
    c.fill (circle (E.x, E.y, 30), radial (E.x, E.y, 1, 16 + 14 * lvS * e,
            { { 0, rgba (255, (int) (120 + 100 * e), 60, 0.9f * e) }, { 0.35f, rgba (211, 18, 26, 0.55f * e) }, { 1, rgba (211, 18, 26, 0) } }));
    c.stroke (ellipse (E.x, E.y + 1, w * 0.5f * (1 + p * 1.6f), 2.2f * (1 - p * 0.4f)), e > 0.3f ? rgba (255, (int) (90 + 130 * e), (int) (40 * e)) : grey (45), 1.8f);
    return G;
}

//==============================================================================
void Scene::hand (Canvas& c, Point<float> top, Point<float> dir)
{
    const float p = press;
    const float twist = slammed ? std::sin (t * 16) * 0.07f * (0.4f + lvS) : std::sin (t * 1.7f) * 0.015f;
    const float th = std::atan2 (dir.x, -dir.y) + twist;
    const float cs = std::cos (th), sn = std::sin (th);
    auto W = [&] (float x, float y) { return Point<float> (top.x + x * cs - y * sn, top.y + x * sn + y * cs); };

    // ---------------- antebrazo tatuado
    const float GRIP = 10;
    const auto wrist = W (7, -56 + GRIP);
    Point<float> ad; { const float lx = 0.40f, ly = -0.92f, m = std::hypot (lx, ly); ad = { (lx * cs - ly * sn) / m, (lx * sn + ly * cs) / m }; }
    const auto elbow = wrist + ad * 260.0f;
    const Point<float> n { -ad.y, ad.x };
    const float w0 = 16, w1 = 27;
    juce::Path arm;
    arm.startNewSubPath (wrist + n * w0);
    arm.quadraticTo (wrist + ad * 60.0f + n * (w0 + 6), elbow + n * w1);
    arm.lineTo (elbow - n * w1);
    arm.quadraticTo (wrist + ad * 60.0f - n * (w0 + 4), wrist - n * w0);
    arm.closeSubPath();
    { juce::Path sh (arm); sh.applyTransform (juce::AffineTransform::translation (6, 8)); c.fill (sh, rgba (0, 0, 0, 0.45f)); }
    c.fill (arm, linear (wrist.x - n.x * 22, wrist.y - n.y * 22, wrist.x + n.x * 22, wrist.y + n.y * 22, { { 0, C (0x8f887d) }, { 0.45f, C (0xddd6cb) }, { 1, C (0x6f685e) } }));
    c.save(); c.clip (arm);
    c.translate (wrist.x, wrist.y); c.rotate (std::atan2 (ad.y, ad.x) + PI / 2);
    for (int row = 0; row < 20; ++row)
        for (int col = -3; col <= 3; ++col)
        {
            const float x = col * 13 + (row % 2) * 6.5f, y = -row * 8.5f - 26;
            for (float r : { 6.5f, 4.2f, 2.0f }) c.stroke (arc (x, y, r, PI, 2 * PI), rgba (12, 12, 12, 0.9f), 1.3f);
        }
    for (auto [x, y, r] : std::initializer_list<std::tuple<float, float, float>> { { -7, -70, 7 }, { 9, -120, 8 }, { -5, -175, 7.5f }, { 7, -220, 6 } })
    {
        for (int k = 0; k < 5; ++k) { const float a = k * 1.2566f; c.fill (ellipse (x + std::cos (a) * r * 0.6f, y + std::sin (a) * r * 0.6f, r * 0.55f, r * 0.36f, a), rgba (211, 18, 26, 0.95f)); }
        c.fill (circle (x, y, r * 0.25f), C (0x111111));
    }
    c.restore();
    c.stroke (arm, rgba (10, 10, 10, 0.9f), 1.4f);
    // manga negra + puño de camisa
    const auto c0 = wrist + ad * 150.0f; const float sw = 30;
    auto sleeve = quad (c0 + n * sw, c0 + ad * 200.0f + n * (sw + 8), c0 + ad * 200.0f - n * (sw + 8), c0 - n * sw);
    { juce::Path cuff (sleeve); cuff.applyTransform (juce::AffineTransform::translation (-ad.x * 7, -ad.y * 7)); c.fill (cuff, C (0xece8e1)); }
    c.fill (sleeve, grey (14)); c.stroke (sleeve, grey (80), 1);

    // ---------------- mano en su marco local
    c.save();
    c.translate (top.x, top.y); c.rotate (th); c.translate (0, GRIP);
    const Colour SK_L = C (0xebe4d9), SK_M = C (0xcbc3b6), SK_D = C (0x968e81), OUT = C (0x121212);
    struct J { float x, y, r; };

    auto fillLimb = [&] (const std::vector<J>& pts, float grow, auto fillFn)
    {
        for (size_t k = 0; k + 1 < pts.size(); ++k)
        {
            const auto a = pts[k], b = pts[k + 1];
            const float dx = b.x - a.x, dy = b.y - a.y, m = juce::jmax (0.0001f, std::hypot (dx, dy)), nx = -dy / m, ny = dx / m;
            fillFn (quad ({ a.x + nx * (a.r + grow), a.y + ny * (a.r + grow) }, { b.x + nx * (b.r + grow), b.y + ny * (b.r + grow) },
                          { b.x - nx * (b.r + grow), b.y - ny * (b.r + grow) }, { a.x - nx * (a.r + grow), a.y - ny * (a.r + grow) }));
        }
        for (auto& q : pts) fillFn (circle (q.x, q.y, q.r + grow));
    };
    auto finger = [&] (const std::vector<J>& pts, Colour base, float nail, bool ringOn)
    {
        fillLimb (pts, 1.3f, [&] (const juce::Path& pp) { c.fill (pp, OUT); });
        const auto a = pts.front(), z = pts.back();
        const float dx = z.x - a.x, dy = z.y - a.y, m = juce::jmax (0.0001f, std::hypot (dx, dy)), nx = -dy / m, ny = dx / m;
        const float mx = (a.x + z.x) / 2, my = (a.y + z.y) / 2;
        float R = 0; for (auto& q : pts) R = juce::jmax (R, q.r); R *= 1.1f;
        const float ls = (-nx - ny) > 0 ? 1.0f : -1.0f;
        const auto gr = linear (mx + nx * R * ls, my + ny * R * ls, mx - nx * R * ls, my - ny * R * ls, { { 0, SK_L }, { 0.5f, base }, { 1, SK_D } });
        fillLimb (pts, 0, [&] (const juce::Path& pp) { c.fill (pp, gr); });
        for (size_t k = 0; k + 1 < pts.size(); ++k)
        {
            const auto p0 = pts[k], p1 = pts[k + 1];
            const float ddx = p1.x - p0.x, ddy = p1.y - p0.y, mm = juce::jmax (0.0001f, std::hypot (ddx, ddy)), qx = -ddy / mm * ls, qy = ddx / mm * ls;
            c.line (p0.x + qx * p0.r * 0.5f + ddx / mm * p0.r * 0.4f, p0.y + qy * p0.r * 0.5f + ddy / mm * p0.r * 0.4f,
                    p1.x + qx * p1.r * 0.5f - ddx / mm * p1.r * 0.4f, p1.y + qy * p1.r * 0.5f - ddy / mm * p1.r * 0.4f,
                    rgba (255, 255, 255, 0.4f), juce::jmin (p0.r, p1.r) * 0.45f);
        }
        for (size_t k = 1; k + 1 < pts.size(); ++k)
            for (float off : { -0.6f, 0.6f })
                c.stroke (arc (pts[k].x + off, pts[k].y + off * 0.3f, pts[k].r * 0.7f, PI * 1.05f, PI * 1.45f), rgba (40, 30, 25, 0.55f), 0.8f);
        if (ringOn)
        {
            const auto p0 = pts[0], p1 = pts[1];
            const float cx = p0.x + (p1.x - p0.x) * 0.55f, cy = p0.y + (p1.y - p0.y) * 0.55f;
            c.save(); c.translate (cx, cy); c.rotate (std::atan2 (p1.y - p0.y, p1.x - p0.x));
            c.rect (-2.2f, -p0.r - 1.2f, 4.4f, p0.r * 2 + 2.4f, OUT);
            c.rect (-1.5f, -p0.r - 0.6f, 3, p0.r * 2 + 1.2f, linear (0, -p0.r, 0, p0.r, { { 0, C (0xfff6c2) }, { 0.4f, C (0xe0b23c) }, { 1, C (0x6e4c0c) } }));
            c.restore();
        }
        if (std::abs (nail) > 0.01f)
        {
            const auto p0 = pts[pts.size() - 2], p1 = pts.back();
            const float r1 = p1.r;
            c.save(); c.translate (p1.x, p1.y); c.rotate (std::atan2 (p1.y - p0.y, p1.x - p0.x)); c.translate (-r1 * 0.35f, nail * r1 * 0.22f);
            auto nl = roundRect (-r1 * 0.75f, -r1 * 0.62f, r1 * 1.45f, r1 * 1.24f, r1 * 0.3f, r1 * 0.62f, r1 * 0.62f, r1 * 0.3f);
            c.fill (nl, linear (0, -r1, 0, r1, { { 0, C (0xfffaf2) }, { 1, C (0xcfc5b6) } }));
            c.stroke (nl, rgba (0, 0, 0, 0.55f), 0.7f);
            c.stroke (arc (-r1 * 0.75f, 0, r1 * 0.5f, -1.1f, 1.1f), rgba (120, 90, 80, 0.6f), 0.7f);
            c.restore();
        }
    };

    const float bend = clampf (p, 0, 1);
    finger ({ { 19, -22, 4.4f }, { 26, -11, 4.0f }, { 21, -3, 3.5f } }, SK_D, 0, false);
    finger ({ { 16, -17, 3.9f }, { 22, -12, 3.5f } }, SK_D, 0, false);
    c.fill (ellipse (23, -11.5f, 2.4f, 2.0f, 0.6f), C (0x7a3b3b));
    finger ({ { 15, -28, 4.9f }, { 23, -14 + bend, 4.4f }, { 10, 3, 3.8f } }, SK_M, 1, true);

    juce::Path palm;
    palm.startNewSubPath (-9, -63);
    palm.quadraticTo (6, -68, 22, -62);
    palm.quadraticTo (30, -46, 27, -30);
    palm.quadraticTo (26, -22, 20, -21);
    palm.quadraticTo (14, -25, 8, -28);
    palm.quadraticTo (0, -30, -6, -32);
    palm.quadraticTo (-14, -42, -9, -63);
    palm.closeSubPath();
    c.fill (palm, linear (-12, -64, 26, -24, { { 0, SK_L }, { 0.6f, SK_M }, { 1, SK_D } }));
    c.save(); c.clip (palm);
    for (int row = 0; row < 3; ++row)
        for (int col = -2; col <= 3; ++col)
        {
            const float x = col * 11 + (row % 2) * 5.5f, y = -60.0f + row * 7;
            for (float r : { 5.5f, 3.4f }) c.stroke (arc (x, y, r, PI, 2 * PI), rgba (15, 15, 15, 0.75f), 1.1f);
        }
    c.restore();
    for (auto [kx, ky, wx] : std::initializer_list<std::tuple<float, float, float>> { { 9, -29, 2 }, { 16, -27, 9 }, { 21, -23, 15 } })
    {
        juce::Path tnd; tnd.startNewSubPath (kx, ky - 2); tnd.quadraticTo ((kx + wx) / 2 + 1, -42, wx, -54);
        c.stroke (tnd, rgba (0, 0, 0, 0.22f), 1);
    }
    for (auto [kx, ky] : std::initializer_list<std::pair<float, float>> { { 8, -30 }, { 15, -28.5f }, { 21, -24.5f } }) c.fill (ellipse (kx, ky, 3, 1.8f, -0.3f), rgba (255, 255, 255, 0.5f));
    c.stroke (palm, OUT, 1.4f);

    finger ({ { 8, -29, 5.1f }, { lerp (14, 17, bend), lerp (-15, -12, bend), 4.6f }, { 5.5f, 1.5f, 4.0f } }, SK_L, 1, false);
    c.fill (ellipse (-8, -48, 9.5f, 12.5f, 0.3f), OUT);
    c.fill (ellipse (-8, -48, 8.3f, 11.3f, 0.3f), radial (-9, -50, 1, 12, { { 0, SK_L }, { 1, SK_M } }));
    finger ({ { -8, -44, 6.0f }, { -13, -24, 5.2f }, { -6, 2, 4.3f } }, SK_M, -1, false);
    c.restore();
}

void Scene::particles (Canvas& c)
{
    for (auto& q : tobacco)
    {
        c.save(); c.translate (q.x, q.y); c.rotate (q.rot);
        c.rect (-q.len / 2, -0.8f, q.len, 1.6f, rgba (120, 86, 50, juce::jmin (1.0f, q.life * 2)));
        c.restore();
    }
    for (auto& p : puffs) c.fill (circle (p.x, p.y, p.r), grey (215, p.a * 0.5f));
    for (auto& p : ash) c.rect (p.x - p.r, p.y - p.r * 0.6f, p.r * 2, p.r * 1.2f, grey (150, juce::jmin (1.0f, p.life)));
    for (auto& p : sparks)
    {
        const float a = juce::jmin (1.0f, p.life / 0.5f);
        c.line (p.x, p.y, p.x - p.vx * 0.02f, p.y - p.vy * 0.02f, rgba (255, (int) (150 + 80 * a), 90, a), p.r);
    }
}

void Scene::petalsDraw (Canvas& c)
{
    for (auto& p : petals)
    {
        c.save(); c.translate (p.x, p.y); c.rotate (p.rot);
        juce::Path pe; pe.startNewSubPath (0, -4); pe.quadraticTo (4, -1, 0, 4); pe.quadraticTo (-4, -1, 0, -4); pe.closeSubPath();
        c.fill (pe, rgba (230, 120, 130, 0.75f * p.life));
        c.restore();
    }
}

void Scene::splatsDraw (Canvas& c)
{
    for (auto& s : splats)
    {
        juce::Random R (s.seed + 1);
        const float a = clampf (s.a, 0, 1);
        const int n = 11; Point<float> pts[n];
        for (int i = 0; i < n; ++i) { const float an = (float) i / n * 2 * PI, d = s.r * (0.8f + R.nextFloat() * 0.45f); pts[i] = { s.x + std::cos (an) * d, s.y + std::sin (an) * d }; }
        juce::Path path;
        path.startNewSubPath ((pts[0] + pts[n - 1]) * 0.5f);
        for (int i = 0; i < n; ++i) path.quadraticTo (pts[i], (pts[i] + pts[(i + 1) % n]) * 0.5f);
        path.closeSubPath();
        { juce::Path sh (path); sh.applyTransform (juce::AffineTransform::translation (1, 1.5f)); c.fill (sh, rgba (122, 3, 9, a * 0.9f)); }
        const auto col = rgba (211, 18, 26, a);
        c.fill (path, col);
        for (int i = 0; i < 8; ++i) { const float an = R.nextFloat() * 6.28f, d = s.r * (1.6f + R.nextFloat() * 1.4f), r = 0.8f + R.nextFloat() * 2.6f; c.fill (circle (s.x + std::cos (an) * d, s.y + std::sin (an) * d, r), col); }
        for (int i = 0; i < 2; ++i)
        {
            const float dx = (R.nextFloat() - 0.5f) * s.r, len = s.drip * (0.5f + R.nextFloat() * 0.5f), w = 2 + R.nextFloat() * 2;
            c.rect (s.x + dx - w / 2, s.y, w, s.r * 0.6f + len, col);
            c.fill (circle (s.x + dx, s.y + s.r * 0.6f + len, w * 0.75f), col);
        }
    }
}

void Scene::overlay (Canvas& c)
{
    c.rect (0, 0, SW, SH, radial (SW / 2, SH * 0.45f, 80, 300, { { 0, rgba (0, 0, 0, 0) }, { 1, rgba (0, 0, 0, 0.55f) } }));
    if (shake > 0) c.rect (0, 0, SW, SH, grey (255, 0.35f * shake * shake));
}

void Scene::draw (Canvas& c)
{
    c.save();
    c.clipRect (0, 0, SW, SH);
    if (shake > 0) c.translate (rr (-1, 1) * shake * 4, rr (-1, 1) * shake * 4);
    background (c);
    c.save();
    c.translate (FG.x, FG.y + FG.dy); c.scale (FG.k, FG.k); c.translate (-FG.x, -FG.y);
    table (c);
    props (c);
    ashtray (c, false);
    const auto G = cigarette (c);
    ashtray (c, true);
    hand (c, G.top, G.dir);
    particles (c);
    c.restore();
    petalsDraw (c);
    overlay (c);
    splatsDraw (c);
    c.restore();
}
