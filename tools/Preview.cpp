// Herramienta de desarrollo: renderiza la interfaz a imagen y prueba el DSP de cada modo.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static void setParam (NicotinaProcessor& p, const char* id, float v) { p.setParam (id, v); }

static void shot (NicotinaProcessor& p, float crush, float level, int frames, int every, const File& out)
{
    std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
    auto* e = dynamic_cast<NicotinaEditor*> (ed.get());
    e->setSize (NicotinaEditor::BW, NicotinaEditor::BH);
    setParam (p, ParamID::crush, crush);
    int tr = 0;
    for (int i = 0; i < frames; ++i)
    {
        if (every > 0 && i % every == 0) ++tr;
        e->advance (1.0f / 60.0f, level * (0.7f + 0.3f * std::sin ((float) i * 0.4f)), tr);
    }
    auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 2.0f);
    out.deleteFile();
    FileOutputStream os (out);
    JPEGImageFormat jpg; jpg.setQuality (0.92f); jpg.writeImageToStream (img, os); os.flush();
    std::cout << "wrote " << out.getFileName() << std::endl;
}

static void dspTest (NicotinaProcessor& p)
{
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    for (int m = 0; m < 4; ++m)
        for (float crush : { 0.0f, 50.0f, 100.0f })
        {
            setParam (p, ParamID::mode, (float) m); setParam (p, ParamID::crush, crush);
            setParam (p, ParamID::bits, 6); setParam (p, ParamID::rate, 10); setParam (p, ParamID::mix, 100); setParam (p, ParamID::output, 0);
            p.prepareToPlay (48000.0, 512);
            AudioBuffer<float> buf (2, 512); MidiBuffer midi;
            double ph = 0; float err = 0, peak = 0; bool bad = false;
            for (int b = 0; b < 40; ++b)
            {
                float dry[512];
                for (int i = 0; i < 512; ++i) { dry[i] = 0.5f * (float) std::sin (ph); ph += 2 * MathConstants<double>::pi * 220 / 48000; buf.setSample (0, i, dry[i]); buf.setSample (1, i, dry[i]); }
                p.processBlock (buf, midi);
                if (b == 39) for (int i = 0; i < 512; ++i) { const float o = buf.getSample (0, i); if (! std::isfinite (o)) bad = true; err += std::abs (o - dry[i]); peak = jmax (peak, std::abs (o)); }
            }
            std::cout << kModeNames[m] << " crush " << crush << ": diff=" << err / 512 << " peak=" << peak << (bad ? " NaN!" : "") << std::endl;
        }
    for (int i = 0; i < p.getNumPrograms(); ++i) std::cout << "  program " << i << ": " << p.getProgramName (i) << std::endl;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File dir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    NicotinaProcessor p;
    dspTest (p);
    p.loadPreset (0);
    shot (p, 0, 0, 60, 0, dir.getChildFile ("jb_boot.jpg"));
    shot (p, 0, 0.3f, 200, 0, dir.getChildFile ("j_0.jpg"));
    shot (p, 55, 0.6f, 200, 0, dir.getChildFile ("j_55.jpg"));
    shot (p, 100, 0.8f, 175, 14, dir.getChildFile ("j_impact.jpg"));
    shot (p, 100, 0.9f, 320, 14, dir.getChildFile ("j_100.jpg"));
    p.setPalette (2); p.loadPreset (6);
    shot (p, 82, 0.6f, 200, 0, dir.getChildFile ("j_oni_yakuza.jpg"));
    return 0;
}
