#include "PluginProcessor.h"
#include "PluginEditor.h"

PanPluginProcessor::PanPluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    panParam    = apvts.getRawParameterValue ("pan");
    widthParam  = apvts.getRawParameterValue ("width");
    lawParam    = apvts.getRawParameterValue ("law");
    heightParam = apvts.getRawParameterValue ("height");
    depthParam  = apvts.getRawParameterValue ("depth");
}

juce::AudioProcessorValueTreeState::ParameterLayout PanPluginProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "pan", 1 }, "Pan (Left/Right)",
        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "height", 1 }, "Height (Down/Up)",
        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "depth", 1 }, "Depth (Back)",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "width", 1 }, "Width",
        NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "law", 1 }, "Pan Law",
        StringArray { "0 dB", "-3 dB", "-4.5 dB", "-6 dB" }, 1));

    return layout;
}

void PanPluginProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    panSmooth.reset (sampleRate, 0.02);
    widthSmooth.reset (sampleRate, 0.02);
    depthGainSmooth.reset (sampleRate, 0.02);

    panSmooth.setCurrentAndTargetValue (panParam->load() / 100.0f);
    widthSmooth.setCurrentAndTargetValue (widthParam->load() / 100.0f);
    depthGainSmooth.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (-6.0f * depthParam->load() / 100.0f));

    for (int ch = 0; ch < 2; ++ch)
    {
        lpState[ch] = 0.0f;
        shelfState[ch] = 0.0f;
    }
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

    const float height = heightParam->load() / 100.0f;   // -1 = bas ... +1 = haut
    const float depth  = depthParam->load()  / 100.0f;   //  0 = devant ... 1 = arriere

    panSmooth.setTargetValue   (panParam->load()   / 100.0f);
    widthSmooth.setTargetValue (widthParam->load() / 100.0f);
    depthGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (-6.0f * depth));
    const int law = (int) lawParam->load();

    // Effet "hauteur / profondeur" : plus c'est bas ou loin, plus le son est sombre.
    // Plus c'est haut, plus il est brillant.
    const float sr        = (float) currentSampleRate;
    const float dark      = juce::jlimit (0.0f, 1.0f, juce::jmax (0.0f, -height) + 0.8f * depth);
    const float cutoff    = 20000.0f * std::pow (0.15f, dark);
    const float lpCoef    = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / sr);
    const float lpMix     = juce::jmin (1.0f, dark * 8.0f);
    const float shelfCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 3000.0f / sr);
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

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float pan   = panSmooth.getNextValue();
        const float width = widthSmooth.getNextValue();
        const float dGain = depthGainSmooth.getNextValue();

        // Largeur stereo (Mid/Side)
        const float mid  = 0.5f * (l[i] + r[i]);
        const float side = 0.5f * (l[i] - r[i]) * width;
        const float inL  = colour (mid + side, 0);
        const float inR  = colour (mid - side, 1);

        float gL, gR;
        computeGains (pan, law, gL, gR);

        l[i] = inL * gL * dGain;
        r[i] = inR * gR * dGain;
    }
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
