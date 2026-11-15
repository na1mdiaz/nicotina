#include "Gfx.h"
#include "BinaryData.h"

namespace gfx
{
static juce::Typeface::Ptr load (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); }

juce::Font Fonts::pix (float px)
{
    static auto tf = load (BinaryData::ps2p_ttf, BinaryData::ps2p_ttfSize);
    return juce::Font (juce::FontOptions (tf).withPointHeight (px));
}
juce::Font Fonts::silk (float px, bool bold)
{
    static auto b = load (BinaryData::silkb_ttf, BinaryData::silkb_ttfSize);
    static auto r = load (BinaryData::silkr_ttf, BinaryData::silkr_ttfSize);
    return juce::Font (juce::FontOptions (bold ? b : r).withPointHeight (px));
}
juce::Font Fonts::dot (float px)
{
    static auto tf = load (BinaryData::dotg_ttf, BinaryData::dotg_ttfSize);
    return juce::Font (juce::FontOptions (tf).withPointHeight (px));
}
juce::Font Fonts::yuji (float px)
{
    static auto tf = load (BinaryData::yuji_ttf, BinaryData::yuji_ttfSize);
    return juce::Font (juce::FontOptions (tf).withPointHeight (px));
}
juce::Font Fonts::inter (float px, int weight)
{
    static auto w9 = load (BinaryData::inter900_ttf, BinaryData::inter900_ttfSize);
    static auto w8 = load (BinaryData::inter800_ttf, BinaryData::inter800_ttfSize);
    return juce::Font (juce::FontOptions (weight >= 900 ? w9 : w8).withPointHeight (px));
}
}
