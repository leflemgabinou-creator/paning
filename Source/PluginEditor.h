#pragma once
#include "PluginProcessor.h"

// Console retro translucide : un ecran LCD avec l'arc de panoramique et un robot,
// des boutons rotatifs (Input, Rate, Amount, Width, Center), 4 modes et une croix directionnelle.
class GamePanEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit GamePanEditor (PanPluginProcessor&);
    ~GamePanEditor() override;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class Key { none, up, down, left, right, sync, reset };

    void timerCallback() override;

    void rebuildCache (float scale);
    void drawStatic (juce::Graphics& g);
    void drawDynamic (juce::Graphics& g);
    void drawLcd (juce::Graphics& g);
    void drawKnobAt (juce::Graphics& g, int index);

    float scaleFactor() const;
    int knobAt (juce::Point<float> p) const;
    int modeButtonAt (juce::Point<float> p) const;
    Key keyAt (juce::Point<float> p) const;

    float rawParam (const char* id) const;
    float normParam (const char* id) const;
    void setNorm (const char* id, float norm);
    void setPlain (const char* id, float plain);
    void setChoice (const char* id, int index);
    void beginGesture (const char* id);
    void endGesture();
    void moveFromArc (juce::Point<float> p);

    PanPluginProcessor& panProcessor;

    juce::Image cache;
    float cacheScale = 0.0f;

    Key held = Key::none;
    int activeKnob = -1;
    bool draggingArc = false;
    float dragStartY = 0.0f;
    float dragStartNorm = 0.0f;
    juce::String activeGesture;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GamePanEditor)
};
