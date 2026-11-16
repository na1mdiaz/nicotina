#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StateIO.h"

using namespace mg;

// nombres que se ven en la automatización de FL (únicos y claros)
static juce::String displayName (int i)
{
    static const std::map<juce::String, juce::String> over = {
        { "gon", "GROSS ON" }, { "gmix", "GROSS CANTIDAD" }, { "vdepth", "GROSS VOLUMEN" }, { "xfade", "GROSS SUAVIZADO" }, { "gstep", "GROSS CASILLA" },
        { "don", "DESTROZO ON" }, { "dtype", "DESTROZO TIPO" }, { "drive", "DESTROZO" }, { "low", "DESTROZO LIMPIEZA" }, { "tone", "DESTROZO TONO" },
        { "asym", "DESTROZO ASIMETRIA" }, { "dwet", "DESTROZO MEZCLA" }, { "autog", "AUTO NIVEL" },
        { "ftype", "FILTRO TIPO" }, { "cutoff", "FILTRO FRECUENCIA" }, { "reso", "FILTRO RESONANCIA" }, { "fenv", "FILTRO ENVOLVENTE" },
        { "glevel", "GRANOS NIVEL" }, { "density", "GRANOS DENSIDAD" }, { "size", "GRANOS TAMAÑO" }, { "position", "GRANOS POSICION" },
        { "spray", "GRANOS DISPERSION" }, { "reverse", "GRANOS REVERSA" }, { "shape", "GRANOS FORMA" }, { "feedback", "GRANOS REALIMENTAR" },
        { "count", "GRANOS VOCES" }, { "offset", "GRANOS SEPARACION" }, { "sizernd", "GRANOS TAMAÑO AZAR" }, { "stretch", "GRANOS ESTIRAR" },
        { "retrig", "GRANOS REDISPARO" }, { "gdsync", "GRANOS RETARDO" }, { "pitch", "GRANOS TONO" }, { "spread", "GRANOS NOTAS AZAR" },
        { "octave", "GRANOS OCTAVAS" }, { "detune", "GRANOS DESAFINAR" }, { "sync", "GRANOS A TEMPO" }, { "div", "GRANOS DIVISION" },
        { "dtime", "ECO TIEMPO" }, { "dsync", "ECO A TEMPO" }, { "dfb", "ECO REPETICIONES" }, { "dmix", "ECO" }, { "rsize", "REVERB SALA" }, { "rmix", "REVERB" },
        { "fx1t", "FX1 TIPO" }, { "fx1a", "FX1 MANDO A" }, { "fx1b", "FX1 MANDO B" }, { "fx1m", "FX1 MEZCLA" },
        { "fx2t", "FX2 TIPO" }, { "fx2a", "FX2 MANDO A" }, { "fx2b", "FX2 MANDO B" }, { "fx2m", "FX2 MEZCLA" },
        { "comp", "MASTER COMPRESION" }, { "mhp", "MASTER CORTA GRAVES" }, { "mlp", "MASTER CORTA AGUDOS" },
        { "ingain", "ENTRADA" }, { "width", "ANCHURA" }, { "duck", "ESQUIVAR" }, { "mix", "MEZCLA GENERAL" }, { "output", "VOLUMEN" }, { "bpm", "BPM (SIN HOST)" },
        { "xyx", "EJE X" }, { "xyy", "EJE Y" }, { "freeze", "FREEZE GRANOS" }, { "rfreeze", "FREEZE REVERB" }, { "bypass", "BYPASS INTERNO" },
    };
    auto it = over.find (PINFO[i].id);
    return it != over.end() ? it->second : juce::String (juce::CharPointer_UTF8 (PINFO[i].name));
}

juce::AudioProcessorValueTreeState::ParameterLayout MutagenProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < NP; ++i)
    {
        const auto& d = PINFO[i];
        juce::NormalisableRange<float> range;
        if (d.curve == 1)
        {
            const float mn = d.min, mx = d.max;
            range = juce::NormalisableRange<float> (mn, mx,
                [] (float a, float b, float n) { return a * std::pow (b / a, n); },
                [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                nullptr);
        }
        else if (d.curve == 2) range = juce::NormalisableRange<float> (d.min, d.max, 1.0f);
        else range = juce::NormalisableRange<float> (d.min, d.max);
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, displayName (i), range, d.def));
    }
    return layout;
}

MutagenProcessor::MutagenProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MUTAGEN", makeLayout())
{
    for (int i = 0; i < NP; ++i)
    {
        raw[i] = apvts.getRawParameterValue (PINFO[i].id);
        apvts.addParameterListener (PINFO[i].id, this);
    }
    engine.prepare (44100.0);
}

MutagenProcessor::~MutagenProcessor()
{
    for (int i = 0; i < NP; ++i) apvts.removeParameterListener (PINFO[i].id, this);
}

bool MutagenProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void MutagenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // conserva el estado complejo al re-preparar
    Complex keep = engine.cx;
    engine.prepare (sampleRate);
    engine.cx = keep;
    monoScratch.setSize (2, juce::jmax (1, samplesPerBlock));
}

void MutagenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int N = buffer.getNumSamples();
    if (N == 0) return;

    // estado complejo nuevo (sin bloquear nunca el audio)
    {
        const juce::SpinLock::ScopedTryLockType tl (pendingLock);
        if (tl.isLocked() && hasPending) { std::swap (engine.cx, pending); hasPending = false; }
    }
    for (int i = 0; i < NP; ++i) engine.P[i] = raw[i]->load (std::memory_order_relaxed);

    // tempo y posición del host (para que los cortes caigan en el compás)
    double bpm = engine.P[p_bpm]; bool playingNow = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) if (*b > 20 && *b < 400) bpm = *b;
            playingNow = pos->getIsPlaying();
            if (playingNow)
                if (auto ppq = pos->getPpqPosition())
                    engine.t = *ppq * (getSampleRate() * 60.0 / bpm);
        }
    engine.bpm = bpm; hostBpm.store (bpm); hostPlaying.store (playingNow);

    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if (monoScratch.getNumSamples() < N) monoScratch.setSize (2, N, false, false, true);
    float* L = buffer.getWritePointer (0);
    float* R = outCh > 1 ? buffer.getWritePointer (1) : monoScratch.getWritePointer (1);
    if (inCh < 2 || outCh < 2) juce::FloatVectorOperations::copy (R, L, N);
    engine.process (L, R, L, R, N);
}

// ---------------------------------------------------------------- puente con la interfaz
void MutagenProcessor::setComplexJson (const juce::String& json)
{
    {
        const juce::ScopedLock sl (jsonLock);
        complexJson = json;
    }
    fresh = false;
    Complex c;
    parseComplex (juce::JSON::parse (json), c);
    const juce::SpinLock::ScopedLockType l (pendingLock);
    pending = std::move (c);
    hasPending = true;
}

void MutagenProcessor::setParamFromUI (int index, float rawValue)
{
    if (index < 0 || index >= NP) return;
    if (auto* p = apvts.getParameter (PINFO[index].id))
    {
        const float nv = p->convertTo0to1 (rawValue);
        if (std::abs (p->getValue() - nv) < 1e-7f) return;
        applyingFromUI = true;
        p->beginChangeGesture();
        p->setValueNotifyingHost (nv);
        p->endChangeGesture();
        applyingFromUI = false;
    }
    fresh = false;
}

void MutagenProcessor::parameterChanged (const juce::String& id, float)
{
    if (applyingFromUI) return;
    const int i = paramIndex (id);
    if (i >= 0) dirty[i].store (true);
}

juce::Array<int> MutagenProcessor::takeDirtyParams()
{
    juce::Array<int> out;
    for (int i = 0; i < NP; ++i) if (dirty[i].exchange (false)) out.add (i);
    return out;
}

// ---------------------------------------------------------------- guardar / cargar (proyecto de FL)
void MutagenProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("complex", getComplexJson(), nullptr);
    state.setProperty ("fresh", fresh, nullptr);
    state.setProperty ("version", "1.0.0", nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MutagenProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        const juce::String json = tree.getProperty ("complex").toString();
        const bool fr = (bool) tree.getProperty ("fresh", false);
        tree.removeProperty ("complex", nullptr); tree.removeProperty ("fresh", nullptr); tree.removeProperty ("version", nullptr);
        apvts.replaceState (tree);
        if (json.isNotEmpty()) setComplexJson (json);
        fresh = fr && json.isEmpty();
        for (int i = 0; i < NP; ++i) dirty[i].store (true);
    }
}

juce::AudioProcessorEditor* MutagenProcessor::createEditor() { return new MutagenEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MutagenProcessor(); }
