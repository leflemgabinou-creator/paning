#include "PluginProcessor.h"

PanPluginProcessor::PanPluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    panParam   = apvts.getRawParameterValue ("pan");
    widthParam = apvts.getRawParameterValue ("width");
    lawParam   = apvts.getRawParameterValue ("law");
}

juce::AudioProcessorValueTreeState::ParameterLayout PanPluginProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "pan", 1 }, "Pan",
        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
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
    panSmooth.reset (sampleRate, 0.02);
    widthSmooth.reset (sampleRate, 0.02);
    panSmooth.setCurrentAndTargetValue (panParam->load() / 100.0f);
    widthSmooth.setCurrentAndTargetValue (widthParam->load() / 100.0f);
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

    panSmooth.setTargetValue   (panParam->load()   / 100.0f);
    widthSmooth.setTargetValue (widthParam->load() / 100.0f);
    const int law = (int) lawParam->load();

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float pan   = panSmooth.getNextValue();
        const float width = widthSmooth.getNextValue();

        // Largeur stéréo (Mid/Side)
        const float mid  = 0.5f * (l[i] + r[i]);
        const float side = 0.5f * (l[i] - r[i]) * width;
        const float inL  = mid + side;
        const float inR  = mid - side;

        float gL, gR;
        computeGains (pan, law, gL, gR);

        l[i] = inL * gL;
        r[i] = inR * gR;
    }
}

juce::AudioProcessorEditor* PanPluginProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
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
