// Render offline: MutagenRender entrada.wav estado.json salida.wav [segundos] [bpm]
// estado.json = { "P": {id: valor...}, "complex": {...} }
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/StateIO.h"
#include <chrono>

int main (int argc, char** argv)
{
    if (argc < 4) { std::puts ("uso: MutagenRender in.wav state.json out.wav [seg] [bpm]"); return 1; }
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File (argv[1])));
    if (! rd) { std::puts ("no se pudo leer el wav"); return 1; }
    const double sr = rd->sampleRate; const double secs = argc > 4 ? std::atof (argv[4]) : 8.0;
    const int N = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (secs * sr));
    juce::AudioBuffer<float> buf (2, N); rd->read (&buf, 0, N, 0, true, true);
    mg::Engine e; e.prepare (sr); e.bpm = argc > 5 ? std::atof (argv[5]) : 150.0;
    auto st = juce::JSON::parse (juce::File (argv[2]));
    if (auto* p = st["P"].getDynamicObject())
        for (auto& kv : p->getProperties()) { const int i = mg::paramIndex (kv.name.toString()); if (i >= 0) e.P[i] = (float) (double) kv.value; }
    mg::parseComplex (st["complex"], e.cx);
    juce::AudioBuffer<float> out (2, N);
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int o = 0; o < N; o += 1024)
    {
        const int n = std::min (1024, N - o);
        e.process (buf.getReadPointer (0) + o, buf.getReadPointer (1) + o, out.getWritePointer (0) + o, out.getWritePointer (1) + o, n);
    }
    const double el = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
    double ss = 0, pk = 0; bool bad = false;
    for (int i = 0; i < N; ++i) { const float v = out.getSample (0, i); if (! std::isfinite (v)) bad = true; ss += v * v; pk = std::max (pk, (double) std::abs (v)); }
    std::printf ("rms %.1f dB  pico %.1f dB  cpu %.2f%%  %s\n", 10 * std::log10 (ss / N + 1e-12), 20 * std::log10 (pk + 1e-12), el / (N / sr) * 100, bad ? "NaN!" : "ok");
    juce::File of (argv[3]); of.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = of.createOutputStream();
    auto opts = juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (16);
    if (auto w = wav.createWriterFor (os, opts)) w->writeFromAudioSampleBuffer (out, 0, N);
    return 0;
}
