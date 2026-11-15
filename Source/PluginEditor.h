#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Scene.h"

// Interfaz: una consola portátil con pantalla LCD de 4 tonos.
class NicotinaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NicotinaEditor (NicotinaProcessor&);
    ~NicotinaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int BW = 480, BH = 846, OY = 46, LW = 184, LH = 160;

    // usado por la herramienta de previsualización
    void advance (float dt, float level, int transients);

private:
    struct Knob { const char* id; float cx, cy, r; bool big; const char* name; const char* kanji; };
    struct Pill { const char* id; float cx, cy; const char* label; const char* sub; };

    void timerCallback() override;
    void renderLCD();
    void renderBoot();
    void drawHUD();
    void quantizeToScreen (bool boot);
    void buildScreenImage();
    void makeShell();

    void drawCart (gfx::Canvas&);
    void drawScreen (gfx::Canvas&);
    void drawPowerSwitch (gfx::Canvas&);
    void drawLive (gfx::Canvas&);
    void drawBigKnob (gfx::Canvas&, const Knob&);
    void drawSmallKnob (gfx::Canvas&, const Knob&);
    void drawPill (gfx::Canvas&, const Pill&);
    void lcdChip (gfx::Canvas&, float cx, float cy, float w, const juce::String&, bool hot);

    juce::RangedAudioParameter* param (const char* id) const { return proc.apvts.getParameter (id); }
    float value (const char* id) const;
    float norm (const char* id) const { return param (id)->getValue(); }
    void setNorm (const char* id, float n);
    juce::String text (const char* id) const;
    int mode() const;

    juce::String whatIsAt (juce::Point<float> page) const;
    const Knob* hitKnob (juce::Point<float> local) const;
    const Pill* hitPill (juce::Point<float> local) const;
    juce::Point<float> toPage (juce::Point<float> p) const { return p / scaleFactor(); }
    float scaleFactor() const { return (float) getWidth() / (float) BW; }

    NicotinaProcessor& proc;
    Scene scene;
    std::array<Knob, 5> knobs;
    std::array<Pill, 2> pills;

    juce::Image low, hud, screenImg, shellImg;
    std::vector<float> ghost;                 // persistencia de la LCD (RGB en float)
    std::vector<float> lum;

    bool powerOn = true;
    float bootT = 0, cartAnim = 0, cartFlash = 0, selectFlash = 0;
    juce::String hover;
    const Knob* dragKnob = nullptr;
    float dragStartNorm = 0, dragStartY = 0;
    juce::int64 lastTick = 0;
    float levelUi = 0, peakUi = 0;
    int lastTransients = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NicotinaEditor)
};
