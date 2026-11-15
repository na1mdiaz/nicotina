#include "PluginEditor.h"

using namespace gfx;
using juce::String;
using juce::Point;

namespace
{
    constexpr float SCRX = 56, SCRY = 76, SCRW = 184 * 2, SCRH = 160 * 2;     // pantalla en el chasis (píxel x2)
    constexpr float A0 = PI * 1.22f, A1 = PI * 2.78f;
    constexpr float BOOT_LEN = 2.6f;
    constexpr int SUP = 4;                                                     // la pantalla se compone a x4

    struct Palette { const char* name; juce::uint32 c[4]; };
    const Palette PALETTES[4] = {
        { "DMG",    { 0xff0f380f, 0xff306230, 0xff8bac0f, 0xffc4cf8c } },
        { "POCKET", { 0xff1b1d1a, 0xff545a52, 0xff9aa395, 0xffd7dcc9 } },
        { "YAKUZA", { 0xff120203, 0xff5c0a10, 0xffc4141e, 0xfff3d2c4 } },
        { "NEON",   { 0xff0d0221, 0xff4b1d6e, 0xffe0307a, 0xffffd3e6 } },
    };
    const float BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

    Colour C (juce::uint32 rgb, float a = 1.0f) { return Colour (0xff000000 | rgb).withAlpha (a); }
    String U (const char* s) { return String::fromUTF8 (s); }
}

//==============================================================================
NicotinaEditor::NicotinaEditor (NicotinaProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    knobs = {{ { ParamID::crush,  128, 590, 58, true,  "CRUSH",       "\xe6\xbd\xb0" },
               { ParamID::bits,   312, 548, 25, false, "BIT DEPTH",   "\xe9\x87\x8f" },
               { ParamID::rate,   404, 516, 25, false, "SAMPLE RATE", "\xe9\x80\x9f" },
               { ParamID::mix,    300, 652, 25, false, "DRY/WET",     "\xe6\xb7\xb7" },
               { ParamID::output, 392, 620, 25, false, "OUTPUT",      "\xe9\x9f\xb3" } }};
    pills = {{ { "select", 196, 732, "SELECT", "PALETA" }, { "start", 270, 732, "START", "BYPASS" } }};

    low = juce::Image (juce::Image::ARGB, LW, LH, true);
    hud = juce::Image (juce::Image::ARGB, LW, LH, true);
    screenImg = juce::Image (juce::Image::RGB, LW * SUP, LH * SUP, true);
    ghost.assign ((size_t) (LW * LH * 3), 0.0f);
    lum.assign ((size_t) (LW * LH), 0.0f);
    powerOn = proc.getPower();

    makeShell();
    setResizable (true, true);
    setResizeLimits (BW / 2, BH / 2, BW * 2, BH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) BW / (double) BH);
    setSize ((int) (BW * 0.86f), (int) (BH * 0.86f));
    lastTick = juce::Time::getHighResolutionTicks();
    startTimerHz (60);
}

NicotinaEditor::~NicotinaEditor() { stopTimer(); }

float NicotinaEditor::value (const char* id) const { auto* p = param (id); return p->convertFrom0to1 (p->getValue()); }
String NicotinaEditor::text (const char* id) const { return param (id)->getCurrentValueAsText(); }
int NicotinaEditor::mode() const { return juce::jlimit (0, 3, juce::roundToInt (value (ParamID::mode))); }
void NicotinaEditor::setNorm (const char* id, float n) { param (id)->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n)); }

//==============================================================================
void NicotinaEditor::advance (float dt, float level, int transients)
{
    levelUi = level;
    peakUi = juce::jmax (level, peakUi - dt * 0.4f);
    selectFlash = juce::jmax (0.0f, selectFlash - dt);
    cartFlash = juce::jmax (0.0f, cartFlash - dt);
    cartAnim = juce::jmax (0.0f, cartAnim - dt * 3);
    if (! powerOn) return;                       // pantalla apagada: no se anima nada (ahorra CPU)
    if (bootT < BOOT_LEN) bootT += dt;
    scene.update (dt, value (ParamID::crush) / 100.0f, level, transients);
    renderLCD();
}

void NicotinaEditor::timerCallback()
{
    const auto now = juce::Time::getHighResolutionTicks();
    const float dt = (float) juce::Time::highResolutionTicksToSeconds (now - lastTick);
    lastTick = now;
    advance (dt, proc.outputLevel.load(), proc.transientCount.load());
    repaint();
}

//==============================================================================
// Pantalla LCD
void NicotinaEditor::renderBoot()
{
    {
        juce::Graphics lg (low);
        Canvas c (lg);
        c.rect (0, 0, (float) LW, (float) LH, juce::Colours::white);
        const float t = bootT;
        const float y = std::round (-20 + 78 * juce::jmin (1.0f, t / 1.25f));
        c.text ("NICOTINA", LW / 2.0f, y, Fonts::pix (16), juce::Colours::black, Canvas::center, Canvas::top);
        if (t > 1.35f) c.text (U ("\xe3\x83\x8b\xe3\x82\xb3\xe3\x83\x81\xe3\x83\xb3"), LW / 2.0f, y + 24, Fonts::dot (16), juce::Colours::black, Canvas::center, Canvas::top);
        if (t > 1.7f)  c.text ("NA1MDIAZ", LW / 2.0f, y + 50, Fonts::pix (8), juce::Colours::black, Canvas::center, Canvas::top);
        if (t > 1.25f && t < 1.45f) c.rect (LW / 2.0f - 70, y + 20, 140, 1, juce::Colours::black);
    }
    quantizeToScreen (true);
}

void NicotinaEditor::renderLCD()
{
    if (bootT < BOOT_LEN) { renderBoot(); return; }
    {
        juce::Graphics lg (low);
        Canvas c (lg);
        c.rect (0, 0, (float) LW, (float) LH, juce::Colours::black);
        const float k = (float) LW / Scene::SW;
        c.save();
        c.clipRect (0, 10, (float) LW, 142);
        c.translate (0, 10); c.scale (k, k); c.translate (0, -4);
        scene.draw (c);
        c.restore();
        c.rect (0, 0, (float) LW, 10, juce::Colours::black); c.rect (0, 151, (float) LW, 9, juce::Colours::black);
        c.rect (0, 10, (float) LW, 1, juce::Colours::white); c.rect (0, 151, (float) LW, 1, juce::Colours::white);
    }
    quantizeToScreen (false);
}

void NicotinaEditor::quantizeToScreen (bool boot)
{
    const auto& P = PALETTES[juce::jlimit (0, 3, proc.getPalette())];
    Colour pal[4] = { Colour (P.c[0]), Colour (P.c[1]), Colour (P.c[2]), Colour (P.c[3]) };
    {
        juce::Image::BitmapData bd (low, juce::Image::BitmapData::readWrite);
        if (boot)
        {
            for (int y = 0; y < LH; ++y)
                for (int x = 0; x < LW; ++x)
                {
                    const float L = bd.getPixelColour (x, y).getFloatRed();
                    bd.setPixelColour (x, y, pal[L > 0.75f ? 3 : L > 0.45f ? 2 : L > 0.2f ? 1 : 0]);
                }
        }
        else
        {
            // luminancia (el rojo cuenta como tono claro-medio para que la tinta destaque)
            for (int y = 0; y < LH; ++y)
                for (int x = 0; x < LW; ++x)
                {
                    const auto px = bd.getPixelColour (x, y);
                    const float r = px.getFloatRed(), g = px.getFloatGreen(), b = px.getFloatBlue();
                    float L = 0.3f * r + 0.59f * g + 0.11f * b;
                    L = juce::jmax (L, juce::jmax (0.0f, r - juce::jmax (g, b)) * 0.62f);
                    lum[(size_t) (y * LW + x)] = std::pow (clampf ((L - 0.04f) / 0.7f, 0, 1), 0.8f);
                }
            if (scene.flashT > 0) std::swap (pal[0], pal[3]), std::swap (pal[1], pal[2]);   // fotograma de impacto
            // lo que le pasa al audio le pasa a la pantalla
            const float cc = value (ParamID::crush) / 100.0f;
            const float effBits = 16 - (16 - value (ParamID::bits)) * cc;
            const int tones = effBits >= 7 ? 4 : effBits >= 4 ? 3 : 2;
            const int tmap4[4] = { 0, 1, 2, 3 }, tmap3[3] = { 0, 2, 3 }, tmap2[2] = { 0, 3 };
            const int* toneMap = tones == 4 ? tmap4 : tones == 3 ? tmap3 : tmap2;
            const int blk = 1 + juce::roundToInt (cc * clampf ((value (ParamID::rate) - 1) / 24, 0, 1) * 3) + ((mode() == 3 && scene.slammed) ? 1 : 0);
            int shift[LH] = {};
            if (scene.glitchT > 0)
            {
                juce::Random& R = juce::Random::getSystemRandom();
                const int bands = 2 + R.nextInt (4);
                for (int b = 0; b < bands; ++b)
                {
                    const int y0 = 11 + R.nextInt (136), h = 2 + R.nextInt (10);
                    const int o = (int) ((R.nextFloat() - 0.5f) * 26 * (0.4f + scene.glitchT * 2));
                    for (int y = y0; y < juce::jmin (150, y0 + h); ++y) shift[y] = o;
                }
            }
            for (int y = 0; y < LH; ++y)
                for (int x = 0; x < LW; ++x)
                {
                    const bool inScene = y > 10 && y < 151;
                    int sx = x, sy = y;
                    if (inScene)
                    {
                        sx = juce::jlimit (0, LW - 1, x - shift[y]);
                        if (blk > 1) { sx -= sx % blk; sy = 11 + ((y - 11) / blk) * blk; }
                    }
                    const float L = lum[(size_t) (sy * LW + sx)];
                    const int bx = blk > 1 ? sx / blk : x, by = blk > 1 ? sy / blk : y;
                    const int tl = inScene ? tones - 1 : 3;
                    const float d = (BAYER[(by & 3) * 4 + (bx & 3)] + 0.5f) / 16.0f;
                    const int lv = juce::jlimit (0, tl, (int) std::floor (L * tl + 0.5f + (d - 0.5f) * 0.62f));
                    bd.setPixelColour (x, y, pal[inScene ? toneMap[lv] : lv]);
                }
        }
    }
    if (! boot) drawHUD();

    // persistencia: estela de la LCD
    {
        juce::Image::BitmapData bd (low, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < LH; ++y)
            for (int x = 0; x < LW; ++x)
            {
                const auto px = bd.getPixelColour (x, y);
                float* gp = &ghost[(size_t) ((y * LW + x) * 3)];
                gp[0] = gp[0] * 0.28f + px.getRed() * 0.72f;
                gp[1] = gp[1] * 0.28f + px.getGreen() * 0.72f;
                gp[2] = gp[2] * 0.28f + px.getBlue() * 0.72f;
            }
    }
    buildScreenImage();
}

void NicotinaEditor::drawHUD()
{
    const auto& P = PALETTES[juce::jlimit (0, 3, proc.getPalette())];
    const Colour light (P.c[3]), dark (P.c[0]);
    hud.clear (hud.getBounds());
    {
        juce::Graphics hg (hud);
        Canvas h (hg);
        const auto green = juce::Colours::lime, red = juce::Colours::red;
        const auto f8 = Fonts::pix (8);
        String hint;
        if (hover == "mode") hint = "MODO: " + String (kModeHints[mode()]);
        else if (hover == ParamID::crush)  hint = "CRUSH = CUANTO ROMPE";
        else if (hover == ParamID::bits)   hint = "BITS = ASPEREZA";
        else if (hover == ParamID::rate)   hint = "RATE = METAL/ROBOT";
        else if (hover == ParamID::mix)    hint = "MIX = LIMPIO + ROTO";
        else if (hover == ParamID::output) hint = "OUT = VOLUMEN FINAL";
        else if (hover == "select")        hint = "SELECT = PALETA";
        else if (hover == "start")         hint = "START = BYPASS";
        else if (hover == "cart")          hint = "CARTUCHO = PRESETS";
        else if (hover == "power")         hint = "POWER = PANTALLA";
        if (hint.isNotEmpty()) h.text (hint, 1, 1, f8, green, Canvas::left, Canvas::top);
        else
        {
            h.text (U ("\xe2\x97\x80") + kModeNames[mode()] + U ("\xe2\x96\xb6"), 1, 1, f8, green, Canvas::left, Canvas::top);
            if (scene.combo > 1) h.text ("COMBO x" + String (scene.combo), LW - 1.0f, 1, f8, green, Canvas::right, Canvas::top);
            else h.text ("PTS" + String (scene.score).paddedLeft ('0', 6), LW - 1.0f, 1, f8, green, Canvas::right, Canvas::top);
        }
        h.text ("OUT", 1, 152, f8, green, Canvas::left, Canvas::top);
        if (scene.impactT > 0)
        {
            const float t = 1 - scene.impactT;
            const float bounce = std::round (juce::jmax (0.0f, 1 - t * 4) * -18 + std::sin (t * 30) * juce::jmax (0.0f, 0.5f - t) * 4);
            const float y = 72 + bounce;
            const String crushK = U ("\xe6\xbd\xb0!");
            h.strokeText (crushK, LW / 2.0f, y, Fonts::dot (32), red, 6, Canvas::center, Canvas::middle);
            h.text (crushK, LW / 2.0f, y, Fonts::dot (32), green, Canvas::center, Canvas::middle);
            if (t > 0.15f)
            {
                h.strokeText ("CRUSHED", LW / 2.0f, y + 24, f8, red, 4, Canvas::center, Canvas::middle);
                h.text ("CRUSHED", LW / 2.0f, y + 24, f8, green, Canvas::center, Canvas::middle);
            }
        }
        else if (scene.slammed && scene.combo > 2 && scene.comboT > 1.6f)
        {
            const String cm = String (scene.combo) + U ("\xe9\x80\xa3");
            h.strokeText (cm, 150, 30, Fonts::dot (16), red, 4, Canvas::center, Canvas::middle);
            h.text (cm, 150, 30, Fonts::dot (16), green, Canvas::center, Canvas::middle);
        }
    }
    {
        juce::Image::BitmapData hb (hud, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData lb (low, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < LH; ++y)
            for (int x = 0; x < LW; ++x)
            {
                const auto px = hb.getPixelColour (x, y);
                if (px.getAlpha() < 110) continue;
                if (px.getGreen() > 128) lb.setPixelColour (x, y, light);
                else if (px.getRed() > 128) lb.setPixelColour (x, y, dark);
            }
    }
    // bloques del medidor
    juce::Graphics lg (low);
    auto db = [] (float v) { return 20.0f * std::log10 (juce::jmax (v, 1e-3f)); };
    const float lvl = clampf ((db (levelUi) + 48) / 48, 0, 1), pk = clampf ((db (peakUi) + 48) / 48, 0, 1);
    const int segs = 20, lit = juce::roundToInt (lvl * segs), pkSeg = juce::roundToInt (pk * segs) - 1;
    for (int i = 0; i < segs; ++i)
    {
        const bool on = i < lit || i == pkSeg, hot = i >= segs - 5;
        const int x = 28 + juce::roundToInt (i * 7.75f);
        lg.setColour (Colour (on ? P.c[3] : P.c[1])); lg.fillRect (x, 153, 6, 6);
        if (hot) { lg.setColour (Colour (on ? P.c[1] : P.c[0])); lg.fillRect (x + 2, 155, 2, 2); }
    }
}

void NicotinaEditor::buildScreenImage()
{
    // cada píxel de la LCD se convierte en un bloque de 4x4, con rejilla y la sombra que proyecta sobre el fondo
    const auto& P = PALETTES[juce::jlimit (0, 3, proc.getPalette())];
    const Colour p0 (P.c[0]);
    const float gr = p0.getRed(), gg = p0.getGreen(), gb = p0.getBlue();
    const int W = LW * SUP, H = LH * SUP;
    juce::Image::BitmapData bd (screenImg, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < H; ++y)
    {
        auto* line = bd.getLinePointer (y);
        const int ly = y / SUP;
        for (int x = 0; x < W; ++x)
        {
            const int lx = x / SUP;
            const float* c = &ghost[(size_t) ((ly * LW + lx) * 3)];
            float r = c[0], g = c[1], b = c[2];
            // sombra (multiplicar por la imagen desplazada medio píxel)
            const int sx = juce::jmax (0, x - 2) / SUP, sy = juce::jmax (0, y - 2) / SUP;
            const float* s = &ghost[(size_t) ((sy * LW + sx) * 3)];
            r *= 0.7f + 0.3f * s[0] / 255.0f; g *= 0.7f + 0.3f * s[1] / 255.0f; b *= 0.7f + 0.3f * s[2] / 255.0f;
            // rejilla de píxeles
            if ((x % SUP) == SUP - 1 || (y % SUP) == SUP - 1) { r = r * 0.84f + gr * 0.16f; g = g * 0.84f + gg * 0.16f; b = b * 0.84f + gb * 0.16f; }
            auto* px = reinterpret_cast<juce::PixelRGB*> (line + x * bd.pixelStride);
            px->setARGB (255, (juce::uint8) juce::jlimit (0.0f, 255.0f, r), (juce::uint8) juce::jlimit (0.0f, 255.0f, g), (juce::uint8) juce::jlimit (0.0f, 255.0f, b));
        }
    }
}

//==============================================================================
// Carcasa (se pinta una vez a doble resolución)
void NicotinaEditor::makeShell()
{
    shellImg = juce::Image (juce::Image::ARGB, BW * 2, BH * 2, true);
    juce::Graphics sg (shellImg);
    sg.addTransform (juce::AffineTransform::scale (2.0f));
    Canvas c (sg);
    c.translate (0, OY);
    const float H = (float) (BH - OY);

    const auto body = roundRect (6, 6, BW - 12.0f, H - 12, 18, 18, 92, 18);
    { auto sh = body; sh.applyTransform (juce::AffineTransform::translation (0, 6)); c.fill (sh, rgba (0, 0, 0, 0.6f)); }
    c.fill (body, linear (0, 0, (float) BW, (float) BH, { { 0, C (0x3a3a3f) }, { 0.5f, C (0x2a2a2e) }, { 1, C (0x1d1d20) } }));
    c.save(); c.clip (body);
    {
        juce::Random r (5);
        for (int i = 0; i < 60000; ++i) c.rect (r.nextFloat() * BW, r.nextFloat() * H, 0.6f, 0.6f, r.nextBool() ? rgba (255, 255, 255, 0.028f) : rgba (0, 0, 0, 0.05f));
    }
    c.rect (0, 6, (float) BW, 34, linear (0, 6, 0, 40, { { 0, rgba (255, 255, 255, 0.10f) }, { 1, rgba (255, 255, 255, 0) } }));
    c.rect (0, 0, (float) BW, H, linear (0, 0, 260, 260, { { 0, rgba (255, 255, 255, 0.07f) }, { 0.5f, rgba (255, 255, 255, 0) }, { 1, rgba (255, 255, 255, 0) } }));
    c.restore();
    c.stroke (body, rgba (255, 255, 255, 0.12f), 1);
    { auto o = body; o.applyTransform (juce::AffineTransform::translation (0.5f, 0.5f)); c.stroke (o, rgba (0, 0, 0, 0.8f), 1.5f); }

    // ranura superior con el interruptor
    c.rect (20, 26, BW - 40.0f, 1.2f, rgba (0, 0, 0, 0.45f));
    c.rect (20, 27.4f, BW - 40.0f, 1, rgba (255, 255, 255, 0.07f));
    c.rect (42, 6, 1.2f, 20, rgba (0, 0, 0, 0.45f)); c.rect (BW - 43.0f, 6, 1.2f, 20, rgba (0, 0, 0, 0.45f));
    c.inkText ("OFF", 58, 17, Fonts::pix (6), C (0x6f7486), Canvas::center, 0);
    c.inkText ("ON", 136, 17, Fonts::pix (6), C (0x6f7486), Canvas::center, 0);
    c.fill (roundRect (74, 12, 48, 10, 5), rgba (0, 0, 0, 0.65f));
    c.rect (76, 22, 44, 1, rgba (255, 255, 255, 0.07f));
    c.inkText (U ("NA1MDIAZ  //  \xe7\xac\xac\xe4\xb9\x9d\xe7\xb5\x84"), BW - 100.0f, 17, Fonts::silk (7), C (0x6f7486));

    // marco de la pantalla
    const auto bez = roundRect (24, 40, BW - 48.0f, 384, 12, 12, 52, 12);
    { auto b2 = bez; b2.applyTransform (juce::AffineTransform::translation (0, 2)); c.fill (b2, rgba (255, 255, 255, 0.06f)); }
    c.fill (bez, linear (0, 40, 0, 424, { { 0, C (0x4a4b55) }, { 1, C (0x383943) } }));
    c.stroke (bez, rgba (0, 0, 0, 0.6f), 1);
    c.rect (38, 54, 96, 1.6f, RED); c.rect (BW - 134.0f, 54, 96, 1.6f, RED);
    c.rect (38, 58, 96, 1.6f, C (0x2b2c75)); c.rect (BW - 134.0f, 58, 96, 1.6f, C (0x2b2c75));
    c.inkText (U ("DOT MATRIX \xc2\xb7 8-BIT CRUSH"), BW / 2.0f, 57, Fonts::silk (7.5f), C (0xb9bccb));
    c.fill (roundRect (SCRX - 3, SCRY - 3, SCRW + 6, SCRH + 6, 3), C (0x16171c));
    c.inkText (U ("SE\xc3\x91" "AL"), 40, 254, Fonts::silk (6, false), C (0x9a9db0), Canvas::center, 0);

    // logotipo grabado
    const auto logoF = Fonts::inter (26, 900);
    c.inkText ("NICOTINA", 34, 447, logoF, rgba (0, 0, 0, 0.55f), Canvas::left, -0.2f);
    c.inkText ("NICOTINA", 34, 449.2f, logoF, rgba (255, 255, 255, 0.10f), Canvas::left, -0.2f);
    c.inkText ("NICOTINA", 34, 448, logoF, INK, Canvas::left, -0.2f);
    const float lw = Canvas::width (logoF, "NICOTINA");
    c.inkText (U ("\xe3\x83\x8b\xe3\x82\xb3\xe3\x83\x81\xe3\x83\xb3"), 46 + lw, 446, Fonts::dot (18), RED, Canvas::left, 0);
    c.inkText ("8-BIT CRUSH SYSTEM", 36, 470, Fonts::pix (7), C (0x6f7486), Canvas::left, 0);

    // pozos de los mandos
    for (auto& kn : knobs)
    {
        const float R = kn.big ? kn.r + 30 : kn.r + 6;
        c.fill (circle (kn.cx, kn.cy, R), radial (kn.cx, kn.cy - R * 0.15f, R * 0.2f, R, { { 0, rgba (0, 0, 0, 0.55f) }, { 1, rgba (0, 0, 0, 0.25f) } }));
        c.stroke (arc (kn.cx, kn.cy + 0.8f, R, 0.15f * PI, 0.85f * PI), rgba (255, 255, 255, 0.08f), 1);
    }
    for (auto [a, b] : std::initializer_list<std::pair<int, int>> { { 1, 2 }, { 3, 4 } })
    {
        const auto& A = knobs[(size_t) a]; const auto& B = knobs[(size_t) b];
        const float mx = (A.cx + B.cx) / 2, my = (A.cy + B.cy) / 2, ang = std::atan2 (B.cy - A.cy, B.cx - A.cx), len = std::hypot (B.cx - A.cx, B.cy - A.cy);
        c.save(); c.translate (mx, my); c.rotate (ang);
        c.fill (roundRect (-len / 2 - 36, -36, len + 72, 72, 36), rgba (0, 0, 0, 0.18f));
        c.restore();
    }
    auto lab = [&] (const Knob& kn, float dy)
    {
        const auto f = Fonts::silk (kn.big ? 10.0f : 7.5f);
        c.inkText (kn.name, kn.cx - 6, kn.cy + dy, f, INK);
        const float w = Canvas::width (f, kn.name);
        c.inkText (U (kn.kanji), kn.cx - 6 + w / 2 + 9, kn.cy + dy, Fonts::dot (kn.big ? 13.0f : 10.0f), RED, Canvas::center, 0);
    };
    lab (knobs[0], 108);
    for (size_t i = 1; i < knobs.size(); ++i) lab (knobs[i], 38);

    for (auto& p : pills)
    {
        c.save(); c.translate (p.cx, p.cy); c.rotate (-0.45f);
        c.fill (roundRect (-23, -9, 46, 18, 9), rgba (0, 0, 0, 0.45f));
        c.restore();
        c.inkText (p.label, p.cx - 4, p.cy + 24, Fonts::silk (7.5f), INK);
        c.inkText (p.sub, p.cx - 4, p.cy + 34, Fonts::silk (6, false), C (0x6f7486));
    }

    c.fill (arc (330, H - 6, 6, PI, 2 * PI), rgba (0, 0, 0, 0.6f));
    c.inkText ("PHONES", 330, H - 18, Fonts::pix (5.5f), C (0x6f7486), Canvas::center, 0);

    c.save(); c.translate (404, 724); c.rotate (-0.45f);
    for (int i = 0; i < 6; ++i)
    {
        c.fill (roundRect (-6.0f + i * 11 - 33, -26, 6, 52, 3), rgba (0, 0, 0, 0.65f));
        c.rect (-6.0f + i * 11 - 33, 24, 6, 1, rgba (255, 255, 255, 0.06f));
    }
    c.restore();

    for (float x : { 40.0f, BW / 2.0f })
    {
        c.fill (circle (x, H - 22, 5), rgba (0, 0, 0, 0.5f));
        c.fill (circle (x, H - 22, 3.6f), C (0x4a4a50));
        c.line (x - 2.5f, H - 22, x + 2.5f, H - 22, C (0x1a1a1d), 1);
    }
}

//==============================================================================
void NicotinaEditor::drawCart (Canvas& c)
{
    const float cx0 = 132, cw = 216;
    const auto& pr = kPresets[(size_t) juce::jlimit (0, (int) kPresets.size() - 1, proc.currentPreset)];
    const float dy = -std::sin (juce::jmin (1.0f, cartAnim) * PI) * 14;
    c.save(); c.translate (0, dy);
    c.fill (roundRect (cx0 + 3, 6, cw, 60, 6), rgba (0, 0, 0, 0.5f));
    const auto body = roundRect (cx0, 2, cw, 60, 6, 6, 0, 0);
    c.fill (body, linear (0, 2, 0, 60, { { 0, C (0x7c7e85) }, { 1, C (0x4b4d53) } }));
    c.stroke (body, rgba (0, 0, 0, 0.6f), 1);
    c.rect (cx0 + 2, 3, cw - 4, 1, rgba (255, 255, 255, 0.18f));
    for (float x = cx0 + 8; x < cx0 + cw - 8; x += 5) c.rect (x, 5, 2, 4, rgba (0, 0, 0, 0.25f));
    c.fill (roundRect (cx0 + 14, 12, cw - 28, 30, 3), C (0x17171a));
    c.rect (cx0 + 14, 12, cw - 28, 4, RED);
    if (cartFlash > 0) c.fill (roundRect (cx0 + 14, 12, cw - 28, 30, 3), rgba (255, 60, 60, cartFlash));
    // icono pixelado de cigarro
    static const char* icon[5] = { "....................", ".FFFFPPPPPPPPPPPPAE.", ".FFFFPPPPPPPPPPPPAEE", ".FFFFPPPPPPPPPPPPAE.", "...................." };
    for (int yy = 0; yy < 5; ++yy)
        for (int xx = 0; xx < 20; ++xx)
        {
            const char ch = icon[yy][xx];
            if (ch == '.') continue;
            const Colour col = ch == 'F' ? C (0xc79a5a) : ch == 'P' ? C (0xe8e6e1) : ch == 'A' ? C (0x7d7d7d) : C (0xff4a2a);
            c.rect (cx0 + 40 + xx * 1.5f, 24 + yy * 1.5f, 1.5f, 1.5f, col);
        }
    const String name = U (pr.name);
    // el kanji del nombre (ONI 鬼) va con la fuente japonesa
    const int sp = name.lastIndexOfChar (' ');
    const bool hasK = sp > 0 && (juce::juce_wchar) name[sp + 1] > 0x2000;
    if (hasK)
    {
        const String a = name.substring (0, sp + 1), b = name.substring (sp + 1);
        const float wa = Canvas::width (Fonts::pix (8), a), wb = Canvas::width (Fonts::dot (10), b);
        const float x0 = cx0 + cw / 2 + 14 - (wa + wb) / 2;
        c.text (a, x0, 31, Fonts::pix (8), BONE, Canvas::left, Canvas::middle);
        c.text (b, x0 + wa, 31, Fonts::dot (10), BONE, Canvas::left, Canvas::middle);
    }
    else c.text (name, cx0 + cw / 2 + 14, 31, Fonts::pix (8), BONE, Canvas::center, Canvas::middle);
    c.text (String (pr.cat) + U (" \xc2\xb7 ") + String (proc.currentPreset + 1) + "/" + String ((int) kPresets.size()),
            cx0 + cw / 2, 20.5f, Fonts::pix (6), C (0x8a8da0), Canvas::center, Canvas::middle);
    const auto ac = hover == "cart" ? RED : C (0x8a8da0);
    c.text (U ("\xe2\x97\x80"), cx0 + 24, 31, Fonts::pix (8), ac, Canvas::center, Canvas::middle);
    c.text (U ("\xe2\x96\xb6"), cx0 + cw - 24, 31, Fonts::pix (8), ac, Canvas::center, Canvas::middle);
    c.restore();
}

void NicotinaEditor::drawScreen (Canvas& c)
{
    const auto& P = PALETTES[juce::jlimit (0, 3, proc.getPalette())];
    if (! powerOn)
    {
        const Colour p1 (P.c[1]);
        c.rect (SCRX, SCRY, SCRW, SCRH, rgba ((int) (p1.getRed() * 0.35f + 96 * 0.65f), (int) (p1.getGreen() * 0.35f + 104 * 0.65f), (int) (p1.getBlue() * 0.35f + 88 * 0.65f)));
    }
    else
    {
        c.g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        c.g.setOpacity (1.0f);
        c.g.drawImage (screenImg, juce::Rectangle<float> (SCRX, SCRY, SCRW, SCRH));
    }
    c.rect (SCRX, SCRY, SCRW, 18, linear (0, SCRY, 0, SCRY + 18, { { 0, rgba (0, 0, 0, 0.45f) }, { 1, rgba (0, 0, 0, 0) } }));
    c.rect (SCRX, SCRY, 14, SCRH, linear (SCRX, 0, SCRX + 14, 0, { { 0, rgba (0, 0, 0, 0.35f) }, { 1, rgba (0, 0, 0, 0) } }));
    c.rect (SCRX, SCRY, SCRW, SCRH, linear (SCRX, SCRY, SCRX + SCRW, SCRY + SCRH, { { 0, rgba (255, 255, 255, 0.10f) }, { 0.35f, rgba (255, 255, 255, 0.03f) }, { 0.36f, rgba (255, 255, 255, 0) }, { 1, rgba (255, 255, 255, 0) } }));
    juce::Path fr; fr.addRectangle (SCRX - 0.75f, SCRY - 0.75f, SCRW + 1.5f, SCRH + 1.5f);
    c.stroke (fr, rgba (0, 0, 0, 0.7f), 1.5f, false);
}

void NicotinaEditor::drawPowerSwitch (Canvas& c)
{
    const float x = powerOn ? 106.0f : 76.0f;
    c.fill (roundRect (x, 12, 16, 11, 3), rgba (0, 0, 0, 0.5f));
    c.fill (roundRect (x, 11, 16, 10, 3), linear (0, 11, 0, 23, { { 0, C (0x8d9099) }, { 1, C (0x45474f) } }));
    for (int i = 0; i < 4; ++i) c.rect (x + 3 + i * 3.0f, 13, 1, 6, rgba (0, 0, 0, 0.35f));
    c.rect (x + 1, 11.5f, 14, 1, rgba (255, 255, 255, 0.3f));
}

void NicotinaEditor::drawLive (Canvas& c)
{
    const bool live = powerOn && levelUi > 0.01f;
    const float a = live ? 0.55f + 0.45f * juce::jmin (1.0f, levelUi * 1.5f) : 0.0f;
    const float x = 40, y = 240;
    if (live) c.fill (circle (x, y, 14), radial (x, y, 1, 14, { { 0, rgba (255, 40, 40, 0.6f * a) }, { 1, rgba (255, 40, 40, 0) } }));
    c.fill (circle (x, y, 4.2f), live ? rgba ((int) (180 + 75 * a), (int) (30 + 40 * a), (int) (30 + 30 * a)) : C (0x3a1013));
    c.fill (circle (x - 1.3f, y - 1.3f, 1.3f), rgba (255, 255, 255, 0.4f));
    const auto f = Fonts::pix (6);
    c.text (String ("PALETA: ") + PALETTES[juce::jlimit (0, 3, proc.getPalette())].name, SCRX + SCRW, SCRY + SCRH + 13, f, C (0x8a8da0), Canvas::right, Canvas::middle);
    if (value (ParamID::bypass) > 0.5f) c.text ("BYPASS ON", SCRX, SCRY + SCRH + 13, f, C (0x8a8da0), Canvas::left, Canvas::middle);
}

void NicotinaEditor::lcdChip (Canvas& c, float cx, float cy, float w, const String& t, bool hot)
{
    const auto& P = PALETTES[juce::jlimit (0, 3, proc.getPalette())];
    const float x = std::round (cx - w / 2), y = std::round (cy - 8);
    c.fill (roundRect (x - 2, y - 2, w + 4, 20, 3), rgba (0, 0, 0, 0.55f));
    c.rect (x, y, w, 16, Colour (hot ? P.c[2] : P.c[3]));
    for (float i = 0; i < w; i += 2) c.rect (x + i, y, 0.5f, 16, Colour (P.c[0]).withAlpha (0.12f));
    c.text (t.toUpperCase(), cx, cy + 1, Fonts::pix (8), Colour (P.c[0]), Canvas::center, Canvas::middle);
}

void NicotinaEditor::drawBigKnob (Canvas& c, const Knob& k)
{
    const float pos = norm (k.id), cx = k.cx, cy = k.cy, r = k.r;
    const float angle = A0 + pos * (A1 - A0);
    const int n = 24, lit = juce::roundToInt (pos * n);
    for (int i = 0; i < n; ++i)
    {
        const float a = A0 + (float) i / (n - 1) * (A1 - A0);
        const float x = cx + std::sin (a) * (r + 17), y = cy - std::cos (a) * (r + 17);
        const bool on = i < lit, hot = i >= n - 3;
        c.save(); c.translate (x, y); c.rotate (a);
        if (on) c.rect (-5, -5, 10, 10, hot ? rgba (255, 60, 60, 0.35f) : rgba (255, 40, 40, 0.22f));
        c.rect (-2.8f, -2.8f, 5.6f, 5.6f, on ? (hot ? C (0xff4a3a) : RED) : C (0x3a1013));
        c.rect (-2.8f, -2.8f, 5.6f, 1.2f, on ? rgba (255, 255, 255, 0.5f) : rgba (255, 255, 255, 0.06f));
        c.restore();
    }
    c.fill (circle (cx, cy + 5, r + 1), rgba (0, 0, 0, 0.6f));
    c.fill (circle (cx, cy, r), linear (cx, cy - r, cx, cy + r, { { 0, C (0x55565e) }, { 1, C (0x141417) } }));
    c.save(); c.translate (cx, cy); c.rotate (angle);
    for (int i = 0; i < 48; ++i)
    {
        c.save(); c.rotate ((float) i / 48 * 2 * PI);
        c.rect (-1.6f, -r, 3.2f, 8, (i % 2) ? rgba (0, 0, 0, 0.55f) : rgba (255, 255, 255, 0.07f));
        c.restore();
    }
    c.restore();
    c.fill (circle (cx, cy, r - 10), radial (cx - r * 0.1f, cy - r * 0.18f, 4, r * 0.8f, { { 0, C (0x2c2d33) }, { 1, C (0x1a1a1e) } }));
    c.stroke (arc (cx, cy, r - 10, PI * 1.1f, PI * 1.9f), rgba (255, 255, 255, 0.10f), 1);
    c.stroke (arc (cx, cy, r - 10, PI * 0.1f, PI * 0.9f), rgba (0, 0, 0, 0.7f), 1);
    c.text (U (k.kanji), cx, cy + 2, Fonts::dot (34), rgba (211, 18, 26, 0.22f), Canvas::center, Canvas::middle);
    c.save(); c.translate (cx, cy); c.rotate (angle);
    c.rect (-4, -r + 4, 8, 22, rgba (255, 40, 40, 0.35f));
    for (int i = 0; i < 4; ++i) c.rect (-2, -r + 6 + i * 5.0f, 4, 4, RED);
    c.restore();
    lcdChip (c, cx - 2, cy + 128, 96, text (k.id), value (k.id) >= 90);
}

void NicotinaEditor::drawSmallKnob (Canvas& c, const Knob& k)
{
    const float pos = norm (k.id), cx = k.cx, cy = k.cy, r = k.r;
    const float angle = A0 + pos * (A1 - A0);
    for (int i = 0; i < 11; ++i)
    {
        const float a = A0 + (float) i / 10 * (A1 - A0);
        const bool on = (float) i / 10 <= pos + 1e-4f;
        const float x = std::round (cx + std::sin (a) * (r + 11)), y = std::round (cy - std::cos (a) * (r + 11));
        c.rect (x - 1.5f, y - 1.5f, 3, 3, on ? BONE : C (0x3c3d44));
    }
    c.fill (circle (cx, cy + 4, r), rgba (0, 0, 0, 0.6f));
    const auto body = circle (cx, cy, r);
    c.fill (body, radial (cx - r * 0.2f, cy - r * 0.25f, 2, r * 1.05f, { { 0, C (0xd0405f) }, { 0.55f, C (0x9b1b39) }, { 1, C (0x4d0b1b) } }));
    c.stroke (body, rgba (0, 0, 0, 0.6f), 1);
    c.fill (ellipse (cx - r * 0.3f, cy - r * 0.48f, r * 0.38f, r * 0.18f, -0.4f), rgba (255, 255, 255, 0.28f));
    c.save(); c.translate (cx, cy); c.rotate (angle);
    c.rect (-1.6f, -r + 4, 3.2f, 9, BONE);
    c.restore();
    lcdChip (c, cx - 6, cy + 56, 74, text (k.id), false);
}

void NicotinaEditor::drawPill (Canvas& c, const Pill& p)
{
    const bool isStart = String (p.id) == "start";
    const bool byp = value (ParamID::bypass) > 0.5f;
    const bool pressed = isStart ? byp : selectFlash > 0;
    c.save(); c.translate (p.cx, p.cy + (pressed ? 1.0f : 0.0f)); c.rotate (-0.45f);
    c.fill (roundRect (-19, -3, 38, 10, 5), rgba (0, 0, 0, 0.5f));
    const auto gr = (isStart && byp) ? linear (0, -5, 0, 5, { { 0, C (0xff5a4a) }, { 1, C (0x8a0d14) } })
                                     : linear (0, -5, 0, 5, { { 0, pressed ? C (0x8a8d9a) : C (0x767986) }, { 1, C (0x3c3e47) } });
    c.fill (roundRect (-19, -5, 38, 10, 5), gr);
    c.fill (roundRect (-15, -4, 30, 2, 1), rgba (255, 255, 255, 0.25f));
    c.restore();
}

void NicotinaEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0c));
    g.addTransform (juce::AffineTransform::scale (scaleFactor()));
    Canvas c (g);
    drawCart (c);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (shellImg, juce::Rectangle<float> (0, 0, (float) BW, (float) BH));
    c.save(); c.translate (0, (float) OY);
    drawScreen (c);
    drawPowerSwitch (c);
    drawLive (c);
    drawBigKnob (c, knobs[0]);
    for (size_t i = 1; i < knobs.size(); ++i) drawSmallKnob (c, knobs[i]);
    for (auto& p : pills) drawPill (c, p);
    c.restore();
}

//==============================================================================
// Ratón
const NicotinaEditor::Knob* NicotinaEditor::hitKnob (Point<float> p) const
{
    for (auto& k : knobs) if (p.getDistanceFrom ({ k.cx, k.cy }) < (k.big ? k.r + 20 : k.r + 8)) return &k;
    return nullptr;
}
const NicotinaEditor::Pill* NicotinaEditor::hitPill (Point<float> p) const
{
    for (auto& q : pills) if (p.getDistanceFrom ({ q.cx, q.cy }) < 22) return &q;
    return nullptr;
}
String NicotinaEditor::whatIsAt (Point<float> page) const
{
    const Point<float> p (page.x, page.y - OY);
    if (page.x > 132 && page.x < 348 && page.y > 0 && page.y < OY + 4) return "cart";
    if (p.x > 48 && p.x < 146 && p.y > 4 && p.y < 28) return "power";
    if (p.x > SCRX && p.x < SCRX + 192 && p.y > SCRY && p.y < SCRY + 22) return "mode";
    if (auto* k = hitKnob (p)) return k->id;
    if (auto* q = hitPill (p)) return q->id;
    return {};
}

void NicotinaEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto page = toPage (e.position);
    hover = whatIsAt (page);
    setMouseCursor (hitKnob ({ page.x, page.y - OY }) ? juce::MouseCursor::UpDownResizeCursor
                    : hover.isNotEmpty() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
void NicotinaEditor::mouseExit (const juce::MouseEvent&) { if (dragKnob == nullptr) hover = {}; }

void NicotinaEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto page = toPage (e.position);
    const Point<float> p (page.x, page.y - OY);
    const auto what = whatIsAt (page);
    if (what == "cart") { proc.loadPreset (proc.currentPreset + (page.x < 240 ? -1 : 1)); cartAnim = 1; cartFlash = 0.25f; return; }
    if (what == "power") { powerOn = ! powerOn; proc.setPower (powerOn); if (powerOn) bootT = 0; return; }
    if (what == "mode")
    {
        const int m = (mode() + (p.x < SCRX + 48 ? -1 : 1) + 4) % 4;
        param (ParamID::mode)->beginChangeGesture(); setNorm (ParamID::mode, param (ParamID::mode)->convertTo0to1 ((float) m)); param (ParamID::mode)->endChangeGesture();
        return;
    }
    if (auto* q = hitPill (p))
    {
        if (String (q->id) == "select") { proc.setPalette ((proc.getPalette() + 1) % 4); selectFlash = 0.15f; }
        else { auto* b = param (ParamID::bypass); b->beginChangeGesture(); b->setValueNotifyingHost (b->getValue() > 0.5f ? 0.0f : 1.0f); b->endChangeGesture(); }
        return;
    }
    if (auto* k = hitKnob (p))
    {
        dragKnob = k; dragStartNorm = norm (k->id); dragStartY = e.position.y;
        param (k->id)->beginChangeGesture();
    }
}

void NicotinaEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragKnob == nullptr) return;
    const float sens = (dragKnob->big ? 260.0f : 200.0f) * scaleFactor() * (e.mods.isShiftDown() ? 4.0f : 1.0f);
    setNorm (dragKnob->id, dragStartNorm + (dragStartY - e.position.y) / sens);
}

void NicotinaEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragKnob != nullptr) param (dragKnob->id)->endChangeGesture();
    dragKnob = nullptr;
}

void NicotinaEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto page = toPage (e.position);
    if (auto* k = hitKnob ({ page.x, page.y - OY }))
    {
        auto* p = param (k->id);
        p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture();
    }
}

void NicotinaEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto page = toPage (e.position);
    if (auto* k = hitKnob ({ page.x, page.y - OY }))
    {
        auto* p = param (k->id);
        p->beginChangeGesture();
        setNorm (k->id, p->getValue() + (w.deltaY > 0 ? 0.02f : -0.02f) * (w.isReversed ? -1.0f : 1.0f));
        p->endChangeGesture();
    }
}
