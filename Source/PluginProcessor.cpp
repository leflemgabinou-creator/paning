#include "PluginProcessor.h"
#include "PluginEditor.h"

PanPluginProcessor::PanPluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    panParam      = apvts.getRawParameterValue ("pan");
    widthParam    = apvts.getRawParameterValue ("width");
    lawParam      = apvts.getRawParameterValue ("law");
    heightParam   = apvts.getRawParameterValue ("height");
    depthParam    = apvts.getRawParameterValue ("depth");
    inputParam    = apvts.getRawParameterValue ("input");
    modeParam     = apvts.getRawParameterValue ("mode");
    divisionParam = apvts.getRawParameterValue ("division");
    rateParam     = apvts.getRawParameterValue ("rate");
    amountParam   = apvts.getRawParameterValue ("amount");
}

juce::AudioProcessorValueTreeState::ParameterLayout PanPluginProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "input", 1 }, "Input",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "AUTO", "MANUAL", "LFO", "TRIGGER" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "division", 1 }, "Sync Rate",
        StringArray { "1/1", "1/2", "1/4", "1/8", "1/16" }, 2));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "rate", 1 }, "LFO Rate",
        NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.4f), 1.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "amount", 1 }, "LFO Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "pan", 1 }, "Center (Left/Right)",
        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "width", 1 }, "Width",
        NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "law", 1 }, "Pan Law",
        StringArray { "0 dB", "-3 dB", "-4.5 dB", "-6 dB" }, 1));

    // Pas de bouton sur la console, mais toujours disponibles dans le logiciel
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "height", 1 }, "Height (Down/Up)",
        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "depth", 1 }, "Depth (Back)",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    return layout;
}

void PanPluginProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    panSmooth.reset (sampleRate, 0.02);
    widthSmooth.reset (sampleRate, 0.02);
    depthGainSmooth.reset (sampleRate, 0.02);
    inputGainSmooth.reset (sampleRate, 0.02);

    panSmooth.setCurrentAndTargetValue (panParam->load() / 100.0f);
    widthSmooth.setCurrentAndTargetValue (widthParam->load() / 100.0f);
    depthGainSmooth.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (-6.0f * depthParam->load() / 100.0f));
    inputGainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputParam->load()));

    for (int ch = 0; ch < 2; ++ch)
    {
        lpState[ch] = 0.0f;
        shelfState[ch] = 0.0f;
    }

    lfoPhase = 0.0;
    env = 0.0f;
    armed = true;
    sweeping = false;
    holdoff = 0;
}

bool PanPluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// pan : -1 (gauche) .. +1 (droite)
static void computeGains (float pan, int law, float& gL, float& gR)
{
    const float x = (pan + 1.0f) * 0.5f;   // 0 = gauche, 1 = droite

    const float powL = std::cos (x * juce::MathConstants<float>::halfPi);
    const float powR = std::sin (x * juce::MathConstants<float>::halfPi);
    const float linL = 1.0f - x;
    const float linR = x;

    switch (law)
    {
        case 0:  gL = juce::jmin (1.0f, 2.0f * linL);  gR = juce::jmin (1.0f, 2.0f * linR);  break; // 0 dB
        case 1:  gL = powL;                            gR = powR;                            break; // -3 dB
        case 2:  gL = std::sqrt (powL * linL);         gR = std::sqrt (powR * linR);         break; // -4.5 dB
        default: gL = linL;                            gR = linR;                            break; // -6 dB
    }
}

void PanPluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    const int   mode   = juce::jlimit (0, 3, (int) modeParam->load());   // 0 AUTO, 1 MANUAL, 2 LFO, 3 TRIGGER
    const float height = heightParam->load() / 100.0f;                   // -1 = bas ... +1 = haut
    const float depth  = depthParam->load()  / 100.0f;                   //  0 = devant ... 1 = arriere
    const float amount = amountParam->load() / 100.0f;

    panSmooth.setTargetValue   (panParam->load()   / 100.0f);
    widthSmooth.setTargetValue (widthParam->load() / 100.0f);
    depthGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (-6.0f * depth));
    inputGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (inputParam->load()));
    const int law = (int) lawParam->load();

    // ---- tempo de l'hote (pour AUTO et TRIGGER)
    double bpm = 120.0;
    double ppq = 0.0;
    bool havePpq = false, playing = false;

    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm())
                if (*b > 1.0) bpm = *b;

            if (auto q = pos->getPpqPosition())
            {
                ppq = *q;
                havePpq = true;
            }

            playing = pos->getIsPlaying();
        }
    }

    static const double beatsPerCycle[] = { 4.0, 2.0, 1.0, 0.5, 0.25 };
    const double bpc = beatsPerCycle[juce::jlimit (0, 4, (int) divisionParam->load())];
    const double sr = currentSampleRate;
    const double syncInc = (bpm / 60.0) / sr / bpc;                 // AUTO / TRIGGER
    const double hzInc   = (double) rateParam->load() / sr;         // LFO libre

    if (mode == 0 && playing && havePpq)
    {
        lfoPhase = std::fmod (ppq / bpc, 1.0);
        if (lfoPhase < 0.0) lfoPhase += 1.0;
    }

    const float envDecay = std::exp (-1.0f / (0.05f * (float) sr));
    const int holdoffSamples = (int) (0.12 * sr);

    // ---- effet "hauteur / profondeur" (sans bouton, via les parametres du logiciel)
    const float srf       = (float) sr;
    const float dark      = juce::jlimit (0.0f, 1.0f, juce::jmax (0.0f, -height) + 0.8f * depth);
    const float cutoff    = 20000.0f * std::pow (0.15f, dark);
    const float lpCoef    = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / srf);
    const float lpMix     = juce::jmin (1.0f, dark * 8.0f);
    const float shelfCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 3000.0f / srf);
    const float shelfGain = juce::Decibels::decibelsToGain (6.0f * juce::jmax (0.0f, height));

    auto colour = [&] (float x, int ch)
    {
        lpState[ch] += lpCoef * (x - lpState[ch]);
        x += lpMix * (lpState[ch] - x);

        shelfState[ch] += shelfCoef * (x - shelfState[ch]);
        return x + (shelfGain - 1.0f) * (x - shelfState[ch]);
    };

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    float lastPan = 0.0f;
    float peakL = 0.0f, peakR = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float centre = panSmooth.getNextValue();
        const float width  = widthSmooth.getNextValue();
        const float dGain  = depthGainSmooth.getNextValue();
        const float inGain = inputGainSmooth.getNextValue();

        const float xl = l[i] * inGain;
        const float xr = r[i] * inGain;

        // ---- modulation du panoramique
        float mod = 0.0f;

        if (mode != 1)
        {
            if (mode == 3)          // TRIGGER : chaque coup relance un balayage
            {
                env = juce::jmax (juce::jmax (std::abs (xl), std::abs (xr)), env * envDecay);

                if (holdoff > 0)
                    --holdoff;

                if (armed && env > 0.1f && holdoff == 0)
                {
                    lfoPhase = 0.0;
                    sweeping = true;
                    armed = false;
                    holdoff = holdoffSamples;
                }
                else if (! armed && env < 0.05f)
                {
                    armed = true;
                }

                if (sweeping)
                {
                    lfoPhase += syncInc;

                    if (lfoPhase >= 1.0)
                    {
                        sweeping = false;
                        lfoPhase = 0.0;
                    }
                }

                if (sweeping)
                    mod = amount * (float) std::sin (juce::MathConstants<double>::twoPi * lfoPhase);
            }
            else
            {
                lfoPhase += (mode == 0 ? syncInc : hzInc);
                if (lfoPhase >= 1.0)
                    lfoPhase -= std::floor (lfoPhase);

                mod = amount * (float) std::sin (juce::MathConstants<double>::twoPi * lfoPhase);
            }
        }

        const float panNow = juce::jlimit (-1.0f, 1.0f, centre + mod);
        lastPan = panNow;

        // Largeur stereo (Mid/Side)
        const float mid  = 0.5f * (xl + xr);
        const float side = 0.5f * (xl - xr) * width;
        const float inL  = colour (mid + side, 0);
        const float inR  = colour (mid - side, 1);

        float gL, gR;
        computeGains (panNow, law, gL, gR);

        l[i] = inL * gL * dGain;
        r[i] = inR * gR * dGain;

        peakL = juce::jmax (peakL, std::abs (l[i]));
        peakR = juce::jmax (peakR, std::abs (r[i]));
    }

    displayPan.store (lastPan * 100.0f);
    levelL.store (juce::jmax (peakL, levelL.load() * 0.85f));
    levelR.store (juce::jmax (peakR, levelR.load() * 0.85f));
}

juce::AudioProcessorEditor* PanPluginProcessor::createEditor()
{
    return new GamePanEditor (*this);
}

void PanPluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PanPluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PanPluginProcessor();
}
