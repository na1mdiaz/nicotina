#include "PluginProcessor.h"
#include "PluginEditor.h"

// la semilla con la que arranca (LEAD LIMPIO, igual que la vista previa)
static const double LEAD_LIMPIO[mg::G_COUNT] = { 0.62,0.5,0.66,0,0.25,0.3,0.1,0,0,0.2,0.4,0,0,0,0,0.3,0.5,0.15,0.25,0.35,0,0,0.02,0.4,0.75,0.25,0.06,0.45,0.1,0.06,0,0.35,0,0.35,0,0,0,0,0,0,0.2,0.45,0 };

SemillaProcessor::SemillaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Salida", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < mg::G_COUNT; ++i) pendG[(size_t) i] = LEAD_LIMPIO[i];
    evs.reserve (1024); uiNotes.reserve (256);
}
juce::AudioProcessorEditor* SemillaProcessor::createEditor() { return new SemillaEditor (*this); }

void SemillaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; engine.prepare (sampleRate, (uint32_t) juce::Random::getSystemRandom().nextInt());
    engine.vL.assign ((size_t) juce::jmax (8192, samplesPerBlock * 2), 0.f); engine.vR = engine.vL;
    zeros.assign ((size_t) juce::jmax (8192, samplesPerBlock * 2), 0.f);
    const juce::SpinLock::ScopedLockType l (pendLock); dirty = true;
}

void SemillaProcessor::queueNote (int m, bool on, float v) { const juce::SpinLock::ScopedLockType l (noteLock); if (uiNotes.size() < 256) uiNotes.push_back ({ m, on ? 1 : 0, (int) (v * 1000) }); }

void SemillaProcessor::applyEngJson (const juce::String& json)
{
    const auto v = juce::JSON::parse (json); if (! v.isObject()) return;
    mg::Genome g; mg::Params p; int so = 0, sp = 0;
    { const juce::SpinLock::ScopedLockType l (pendLock); g = pendG; p = pendP; so = pendSeqOn; sp = pendSeqProg; }
    if (auto* G = v["G"].getDynamicObject()) for (int i = 0; i < mg::G_COUNT; ++i) if (G->hasProperty (mg::GNAMES[i])) g[(size_t) i] = (double) G->getProperty (mg::GNAMES[i]);
    if (auto* P = v["P"].getDynamicObject())
    {
        auto rd = [&] (const char* n, double& f) { if (P->hasProperty (n)) f = (double) P->getProperty (n); };
        rd ("blend", p.blend); rd ("amb", p.amb); rd ("oct", p.oct); rd ("follow", p.follow); rd ("bright", p.bright); rd ("attOff", p.attOff); rd ("relOff", p.relOff);
        rd ("move", p.move); rd ("out", p.out); rd ("poly", p.poly); rd ("bpm", p.bpm); rd ("sync", p.sync); rd ("toca", p.toca); rd ("vari", p.vari);
        rd ("flUp", p.flUp); rd ("flDown", p.flDown); rd ("flFifth", p.flFifth);
    }
    if (auto* a = v["pg"].getArray(); a != nullptr && a->size() >= 2) { p.pgGain = (double) (*a)[0]; p.pgTame = (double) (*a)[1]; }
    if (auto* a = v["seq"].getArray(); a != nullptr && a->size() >= 2) { so = (int) (*a)[0]; sp = (int) (*a)[1]; }
    p.bypass = (int) v["bypass"] != 0;
    // red de seguridad: valores dentro de rango
    for (auto& x : g) x = juce::jlimit (0.0, 1.0, std::isfinite (x) ? x : 0.0);
    p.pgGain = juce::jlimit (0.3, 3.0, std::isfinite (p.pgGain) ? p.pgGain : 1.0); p.pgTame = juce::jlimit (0.1, 1.0, std::isfinite (p.pgTame) ? p.pgTame : 1.0);
    p.poly = juce::jlimit (1.0, 6.0, p.poly); p.toca = juce::jlimit (0.0, 7.0, p.toca); p.bpm = juce::jlimit (40.0, 240.0, p.bpm); p.out = juce::jlimit (-18.0, 6.0, p.out);
    {
        const juce::SpinLock::ScopedLockType l (pendLock); pendG = g; pendP = p; pendSeqOn = so; pendSeqProg = juce::jlimit (0, 5, sp); dirty = true;
    }
    const juce::SpinLock::ScopedLockType l2 (savedLock); engSaved = json;
}

void SemillaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int N = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || N <= 0) { buffer.clear(); return; }
    if (pendLock.tryEnter())
    {
        if (dirty) { engine.G = pendG; engine.P = pendP; engine.seqOn = pendSeqOn != 0; engine.seqProg = pendSeqProg; dirty = false; }
        pendLock.exit();
    }
    // tempo del proyecto
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) if (*bpm > 20 && *bpm < 400) engine.P.bpm = *bpm;
    if (allOff.exchange (false)) engine.allNotesOff();
    if (noteLock.tryEnter()) { for (auto& n : uiNotes) { if (n[1]) engine.noteOn (n[0], n[2] / 1000.0); else engine.noteOff (n[0]); } uiNotes.clear(); noteLock.exit(); }
    // MIDI del host en su muestra exacta
    evs.clear();
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage(); const int pos = juce::jlimit (0, N - 1, meta.samplePosition);
        if (msg.isNoteOn()) evs.push_back ({ pos, 1, msg.getNoteNumber(), msg.getFloatVelocity() });
        else if (msg.isNoteOff()) evs.push_back ({ pos, 0, msg.getNoteNumber(), 0.f });
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) evs.push_back ({ pos, -1, 0, 0.f });
    }
    size_t ei = 0;
    auto midiAt = [&] (int n) { while (ei < evs.size() && evs[ei].pos <= n) { const auto& e = evs[ei++]; if (e.on == 1) engine.noteOn (e.m, e.v); else if (e.on == 0) engine.noteOff (e.m); else engine.allNotesOff(); } };
    if ((int) zeros.size() < N) zeros.assign ((size_t) N, 0.f);
    float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);
    for (int o = 0; o < N; o += 4096)
    {
        const int n = juce::jmin (4096, N - o);
        engine.process (zeros.data(), zeros.data(), L + o, R + o, n, [&] (int k) { midiAt (o + k); });
    }
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, N);
    // red de seguridad: nunca un pitido en FL
    bool bad = false; for (int i = 0; i < N && ! bad; ++i) bad = ! std::isfinite (L[i]) || ! std::isfinite (R[i]) || std::abs (L[i]) > 4.f || std::abs (R[i]) > 4.f;
    if (bad) { buffer.clear(); const auto G = engine.G; const auto P = engine.P; engine.prepare (sr, 777u); engine.G = G; engine.P = P; }
    if (vizLock.tryEnter())
    {
        viz.level = engine.level; viz.notes = engine.notesNow; viz.played = engine.played; viz.voicesOn = engine.voicesOn;
        viz.hasKey = engine.hasKey; viz.keyK = engine.keyK; viz.keyMinor = engine.keyMinor; viz.bpm = engine.P.bpm;
        vizLock.exit();
    }
}

void SemillaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto* o = new juce::DynamicObject();
    { const juce::SpinLock::ScopedLockType l (savedLock); o->setProperty ("full", saved); o->setProperty ("eng", engSaved); }
    o->setProperty ("version", 6);
    const auto s = juce::JSON::toString (juce::var (o), true);
    dest.replaceAll (s.toRawUTF8(), s.getNumBytesAsUTF8());
}
void SemillaProcessor::setStateInformation (const void* data, int size)
{
    const auto v = juce::JSON::parse (juce::String::fromUTF8 ((const char*) data, size));
    if (! v.isObject()) return;
    setSavedJson (v["full"].toString());
    const auto eng = v["eng"].toString(); if (eng.isNotEmpty()) applyEngJson (eng);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SemillaProcessor(); }
