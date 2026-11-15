#pragma once
// Utilidades de dibujo: una capa fina sobre juce::Graphics con una API parecida a canvas 2D,
// para que el dibujo sea idéntico al de la vista previa web.

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cmath>

namespace gfx
{
using juce::Colour;
using juce::Path;
using juce::Point;

constexpr float PI = juce::MathConstants<float>::pi;

inline Colour rgba (int r, int g, int b, float a = 1.0f) { return Colour ((juce::uint8) juce::jlimit (0, 255, r), (juce::uint8) juce::jlimit (0, 255, g), (juce::uint8) juce::jlimit (0, 255, b), juce::jlimit (0.0f, 1.0f, a)); }
inline Colour grey (int v, float a = 1.0f) { return rgba (v, v, v, a); }
inline Colour hex (juce::uint32 argb) { return Colour (argb); }

inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float seed01 (float i) { const float x = std::sin (i * 12.9898f) * 43758.5453f; return x - std::floor (x); }

const Colour RED   = hex (0xffd3121a);
const Colour RED_D = hex (0xff7a0309);
const Colour BONE  = hex (0xffe8e6e1);
const Colour ASH   = hex (0xff8d8c89);
const Colour INK   = hex (0xff8f95ad);

// ---------------------------------------------------------------- fuentes
struct Fonts
{
    static juce::Font pix  (float px);    // Press Start 2P
    static juce::Font silk (float px, bool bold = true);
    static juce::Font dot  (float px);    // DotGothic16 (pixel con japonés)
    static juce::Font yuji (float px);    // pincel
    static juce::Font inter (float px, int weight = 900);
};

// ---------------------------------------------------------------- degradados
struct Stop { float o; Colour c; };

inline juce::ColourGradient linear (float x0, float y0, float x1, float y1, std::initializer_list<Stop> stops)
{
    std::vector<Stop> s (stops);
    juce::ColourGradient gr (s.front().c, x0, y0, s.back().c, x1, y1, false);
    for (size_t i = 1; i + 1 < s.size(); ++i) gr.addColour (s[i].o, s[i].c);
    return gr;
}

// radial como canvas: centro (x1,y1), radio r1; r0 recoloca las paradas
inline juce::ColourGradient radial (float x1, float y1, float r0, float r1, std::initializer_list<Stop> stops)
{
    std::vector<Stop> s (stops);
    auto off = [&] (float o) { return juce::jlimit (0.0, 1.0, (double) ((r0 + o * (r1 - r0)) / juce::jmax (0.001f, r1))); };
    juce::ColourGradient gr (s.front().c, x1, y1, s.back().c, x1 + r1, y1, true);
    if (r0 > 0.0f) gr.addColour (off (0.0f), s.front().c);
    for (size_t i = 1; i + 1 < s.size(); ++i) gr.addColour (off (s[i].o), s[i].c);
    return gr;
}

// ---------------------------------------------------------------- formas
inline Path ellipse (float cx, float cy, float rx, float ry, float rot = 0.0f)
{
    Path p; p.addEllipse (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
    if (std::abs (rot) > 1.0e-6f) p.applyTransform (juce::AffineTransform::rotation (rot, cx, cy));
    return p;
}
inline Path circle (float cx, float cy, float r) { return ellipse (cx, cy, r, r); }

// arco con convención de canvas (0 = +x, sentido horario en pantalla)
inline void arcTo (Path& p, float cx, float cy, float r, float a0, float a1, bool startNew)
{
    while (a1 < a0) a1 += 2.0f * PI;                       // canvas: el sentido horario siempre avanza
    const int n = juce::jmax (6, (int) (std::abs (a1 - a0) * r * 0.6f));
    for (int i = 0; i <= n; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / (float) n;
        const float x = cx + std::cos (a) * r, y = cy + std::sin (a) * r;
        if (i == 0 && startNew) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
}
inline Path arc (float cx, float cy, float r, float a0, float a1) { Path p; arcTo (p, cx, cy, r, a0, a1, true); return p; }

// rectángulo con radio distinto por esquina (tl, tr, br, bl)
inline Path roundRect (float x, float y, float w, float h, float tl, float tr, float br, float bl)
{
    Path p;
    const float k = 0.5523f;
    p.startNewSubPath (x + tl, y);
    p.lineTo (x + w - tr, y);
    p.cubicTo (x + w - tr + tr * k, y, x + w, y + tr - tr * k, x + w, y + tr);
    p.lineTo (x + w, y + h - br);
    p.cubicTo (x + w, y + h - br + br * k, x + w - br + br * k, y + h, x + w - br, y + h);
    p.lineTo (x + bl, y + h);
    p.cubicTo (x + bl - bl * k, y + h, x, y + h - bl + bl * k, x, y + h - bl);
    p.lineTo (x, y + tl);
    p.cubicTo (x, y + tl - tl * k, x + tl - tl * k, y, x + tl, y);
    p.closeSubPath();
    return p;
}
inline Path roundRect (float x, float y, float w, float h, float r) { Path p; p.addRoundedRectangle (x, y, w, h, r); return p; }

inline Path quad (Point<float> a, Point<float> b, Point<float> c, Point<float> d)
{
    Path p; p.startNewSubPath (a); p.lineTo (b); p.lineTo (c); p.lineTo (d); p.closeSubPath(); return p;
}

// ---------------------------------------------------------------- lienzo
class Canvas
{
public:
    explicit Canvas (juce::Graphics& gr) : g (gr) {}
    juce::Graphics& g;

    void save()    { g.saveState(); }
    void restore() { g.restoreState(); }
    void translate (float x, float y) { g.addTransform (juce::AffineTransform::translation (x, y)); }
    void rotate (float a)             { g.addTransform (juce::AffineTransform::rotation (a)); }
    void scale (float sx, float sy)   { g.addTransform (juce::AffineTransform::scale (sx, sy)); }
    void transform (const juce::AffineTransform& t) { g.addTransform (t); }
    void clip (const Path& p)         { g.reduceClipRegion (p); }
    void clipRect (float x, float y, float w, float h) { g.reduceClipRegion (juce::Rectangle<float> (x, y, w, h).getSmallestIntegerContainer()); }

    void fill (const Path& p, Colour c)                        { g.setColour (c); g.fillPath (p); }
    void fill (const Path& p, const juce::ColourGradient& gr)  { g.setGradientFill (gr); g.fillPath (p); }
    void stroke (const Path& p, Colour c, float w, bool round = true)
    {
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (w, round ? juce::PathStrokeType::curved : juce::PathStrokeType::mitered,
                                                  round ? juce::PathStrokeType::rounded : juce::PathStrokeType::butt));
    }
    void line (float x0, float y0, float x1, float y1, Colour c, float w, bool round = true)
    {
        Path p; p.startNewSubPath (x0, y0); p.lineTo (x1, y1); stroke (p, c, w, round);
    }
    void rect (float x, float y, float w, float h, Colour c) { g.setColour (c); g.fillRect (juce::Rectangle<float> (x, y, w, h)); }
    void rect (float x, float y, float w, float h, const juce::ColourGradient& gr) { g.setGradientFill (gr); g.fillRect (juce::Rectangle<float> (x, y, w, h)); }

    enum Align { left, center, right };
    enum Base  { top, middle, alphabetic };

    static float width (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); }

    juce::GlyphArrangement glyphs (const juce::String& s, float x, float y, const juce::Font& f, Align al, Base bl) const
    {
        const float w = width (f, s);
        const float x0 = al == left ? x : (al == center ? x - w * 0.5f : x - w);
        const float asc = f.getAscent(), desc = f.getDescent();
        const float base = bl == alphabetic ? y : (bl == top ? y + asc : y + (asc - desc) * 0.5f);
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, x0, base);
        return ga;
    }
    void text (const juce::String& s, float x, float y, const juce::Font& f, Colour c, Align al = left, Base bl = alphabetic)
    {
        g.setColour (c);
        glyphs (s, x, y, f, al, bl).draw (g);
    }
    void strokeText (const juce::String& s, float x, float y, const juce::Font& f, Colour c, float w, Align al = left, Base bl = alphabetic)
    {
        Path p; glyphs (s, x, y, f, al, bl).createPath (p);
        stroke (p, c, w);
    }
    // texto inclinado (cursiva falsa) como la serigrafía de las consolas
    void inkText (const juce::String& s, float x, float y, const juce::Font& f, Colour c, Align al = center, float skew = -0.22f)
    {
        save();
        translate (x, y);
        transform (juce::AffineTransform (1.0f, skew, 0.0f, 0.0f, 1.0f, 0.0f));
        text (s, 0, 0, f, c, al, middle);
        restore();
    }
};

} // namespace gfx
