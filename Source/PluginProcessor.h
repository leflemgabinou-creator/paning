#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

class PanPluginProcessor : public juce::AudioProcessor
{
public:
    PanPluginProcessor();
    ~PanPluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Game Pan"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* panParam    = nullptr;
    std::atomic<float>* widthParam  = nullptr;
    std::atomic<float>* lawParam    = nullptr;
    std::atomic<float>* heightParam = nullptr;
    std::atomic<float>* depthParam  = nullptr;

    juce::SmoothedValue<float> panSmooth, widthSmooth, depthGainSmooth;

    double currentSampleRate = 44100.0;
    float lpState[2]    = {};
    float shelfState[2] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanPluginProcessor)
};
