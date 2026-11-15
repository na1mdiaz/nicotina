#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout NicotinaProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout layout;
    auto pct = [] (float v, int) { return String (v, 1) + " %"; };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::crush, 1 }, "Crush",
        NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::bits, 1 }, "Bit Depth",
        NormalisableRange<float> (1.0f, 16.0f, 0.01f), 8.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " BIT"; })));

    NormalisableRange<float> rateRange (1.0f, 64.0f, 0.01f);
    rateRange.setSkewForCentre (8.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::rate, 1 }, "Sample Rate", rateRange, 8.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "1/" + String (v, 1); })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::mix, 1 }, "Dry/Wet",
        NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 0) + "%"; })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " DB"; })));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::mode, 1 }, "Mode",
        StringArray { "GAMEBOY", "SP-1200", "RADIO", "DESTROY" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::bypass, 1 }, "Bypass", false));
    return layout;
}

NicotinaProcessor::NicotinaProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NICOTINA", createLayout())
{
    crushP  = apvts.getRawParameterValue (ParamID::crush);
    bitsP   = apvts.getRawParameterValue (ParamID::bits);
    rateP   = apvts.getRawParameterValue (ParamID::rate);
    mixP    = apvts.getRawParameterValue (ParamID::mix);
    outputP = apvts.getRawParameterValue (ParamID::output);
    modeP   = apvts.getRawParameterValue (ParamID::mode);
    bypassP = apvts.getRawParameterValue (ParamID::bypass);
    apvts.state.setProperty ("palette", 0, nullptr);
    apvts.state.setProperty ("power", true, nullptr);
    apvts.state.setProperty ("preset", 0, nullptr);
}

const juce::String NicotinaProcessor::getProgramName (int index)
{
    if (index < 0 || index >= (int) kPresets.size()) return {};
    return juce::String (kPresets[(size_t) index].cat) + " - " + juce::String::fromUTF8 (kPresets[(size_t) index].name);
}

void NicotinaProcessor::setParam (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void NicotinaProcessor::loadPreset (int index)
{
    const int n = (int) kPresets.size();
    index = ((index % n) + n) % n;
    const auto& pr = kPresets[(size_t) index];
    currentPreset = index;
    apvts.state.setProperty ("preset", index, nullptr);
    setParam (ParamID::mode, (float) pr.mode);
    setParam (ParamID::crush, pr.crush);
    setParam (ParamID::bits, pr.bits);
    setParam (ParamID::rate, pr.rate);
    setParam (ParamID::mix, pr.mix);
    setParam (ParamID::output, pr.output);
}

int  NicotinaProcessor::getPalette() const { return (int) apvts.state.getProperty ("palette", 0); }
void NicotinaProcessor::setPalette (int p) { apvts.state.setProperty ("palette", p, nullptr); }
bool NicotinaProcessor::getPower() const   { return (bool) apvts.state.getProperty ("power", true); }
void NicotinaProcessor::setPower (bool on) { apvts.state.setProperty ("power", on, nullptr); }

bool NicotinaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void NicotinaProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    const double ramp = 0.03;
    crushS.reset (sampleRate, ramp); crushS.setCurrentAndTargetValue (crushP->load() * 0.01f);
    bitsS.reset (sampleRate, ramp);  bitsS.setCurrentAndTargetValue (bitsP->load());
    rateS.reset (sampleRate, ramp);  rateS.setCurrentAndTargetValue (rateP->load());
    mixS.reset (sampleRate, ramp);   mixS.setCurrentAndTargetValue (mixP->load() * 0.01f);
    gainS.reset (sampleRate, ramp);  gainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputP->load()));
    for (auto& c : channels) c = {};

    const auto s = (float) sampleRate;
    attackCoef  = std::exp (-1.0f / (0.001f * s));
    releaseFast = std::exp (-1.0f / (0.040f * s));
    attackSlow  = std::exp (-1.0f / (0.030f * s));
    releaseSlow = std::exp (-1.0f / (0.300f * s));
    holdSamples = (int) (0.09f * s);
    envFast = envSlow = 0.0f;
    transientHold = 0;
}

void NicotinaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels(), N = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, N);

    crushS.setTargetValue (crushP->load() * 0.01f);
    bitsS.setTargetValue (bitsP->load());
    rateS.setTargetValue (rateP->load());
    mixS.setTargetValue (mixP->load() * 0.01f);
    gainS.setTargetValue (juce::Decibels::decibelsToGain (outputP->load()));
    const int mode = juce::jlimit (0, 3, (int) std::lround (modeP->load()));
    const bool bypassed = bypassP->load() > 0.5f;

    const int numCh = juce::jmin (numOut, (int) channels.size());
    const float fs = (float) sr;
    auto onePole = [fs] (float fc) { return 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * fc / fs); };
    const float aHp = mode == 2 ? std::exp (-2.0f * juce::MathConstants<float>::pi * 420.0f / fs) : 0.0f;
    const float aLpRadio = onePole (2600.0f);
    float peak = 0.0f;

    for (int i = 0; i < N; ++i)
    {
        const float c = crushS.getNextValue(), bits = bitsS.getNextValue(), rate = rateS.getNextValue();
        const float mix = mixS.getNextValue(), gain = gainS.getNextValue();

        // CRUSH es el macro: lleva el sonido de limpio a BIT DEPTH / SAMPLE RATE y añade saturación
        const float effBits   = 16.0f - (16.0f - bits) * c;
        const float levels    = std::pow (2.0f, effBits - 1.0f);
        const float effFactor = 1.0f + (rate - 1.0f) * c;
        const float phaseInc  = 1.0f / effFactor;
        const float drive     = 1.0f + 5.0f * c * c;
        const float comp      = 1.0f / std::tanh (drive);
        const float fade      = juce::jmin (1.0f, c * 4.0f);
        const float autoGain  = 1.0f / (1.0f + 0.8f * c * c);   // compensación automática de volumen
        const float aLp = mode == 1 ? onePole (juce::jlimit (1800.0f, 18000.0f, fs * 0.42f / effFactor)) : aLpRadio;

        float mono = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto& st = channels[(size_t) ch];
            const float dry = data[i];
            mono = juce::jmax (mono, std::abs (dry));

            st.phase += mode == 3 ? phaseInc * (1.0f + (rng.nextFloat() - 0.5f) * 0.9f * c) : phaseInc;
            if (st.phase >= 1.0f)
            {
                st.phase -= std::floor (st.phase);
                float x = dry;
                if (mode == 3) x = std::sin (x * (1.0f + 5.0f * c) * 1.5707963f) * (0.6f + 0.4f * (1.0f - c));   // plegado
                st.held = std::tanh (x * drive) * comp;
            }

            float wet = std::round (st.held * levels) / levels;
            if (mode == 1) { st.lp1 += (wet - st.lp1) * aLp; st.lp2 += (st.lp1 - st.lp2) * aLp; wet = st.lp2; }    // cálido
            else if (mode == 2)                                                                                     // radio
            {
                const float hp = aHp * (st.hpOut + wet - st.hpIn); st.hpIn = wet; st.hpOut = hp;
                st.lp1 += (hp - st.lp1) * aLp; st.lp2 += (st.lp1 - st.lp2) * aLp;
                wet = std::tanh (st.lp2 * 2.2f) + (rng.nextFloat() - 0.5f) * 0.02f * c;
            }
            else if (mode == 3) wet = juce::jlimit (-0.85f, 0.85f, wet * 1.4f);                                      // recorte duro

            wet *= autoGain;
            const float hp = wet - st.dcIn + 0.9995f * st.dcOut; st.dcIn = wet; st.dcOut = hp; wet = hp;
            wet = dry + (wet - dry) * fade;

            float out = (dry + (wet - dry) * mix) * gain;
            if (bypassed) out = dry;
            data[i] = out;
            peak = juce::jmax (peak, std::abs (out));
        }

        envFast = mono > envFast ? attackCoef * envFast + (1.0f - attackCoef) * mono : releaseFast * envFast + (1.0f - releaseFast) * mono;
        envSlow = mono > envSlow ? attackSlow * envSlow + (1.0f - attackSlow) * mono : releaseSlow * envSlow + (1.0f - releaseSlow) * mono;
        if (transientHold > 0) --transientHold;
        else if (envFast > 0.05f && envFast > envSlow * 1.5f) { transientCount.fetch_add (1, std::memory_order_relaxed); transientHold = holdSamples; }
    }

    const float prev = outputLevel.load (std::memory_order_relaxed);
    outputLevel.store (juce::jmax (peak, prev * 0.85f), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* NicotinaProcessor::createEditor() { return new NicotinaEditor (*this); }

void NicotinaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void NicotinaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = (int) apvts.state.getProperty ("preset", 0);
        }
}

#ifndef NICOTINA_PREVIEW_BUILD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NicotinaProcessor(); }
#endif
