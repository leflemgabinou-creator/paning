#pragma once
#include "PluginProcessor.h"

// Interface "console portable" : un ecran avec un mini-jeu.
// La position du petit fantome = le panoramique.
//   gauche / droite = Pan, haut / bas = Height, taille du fantome = Depth (arriere)
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
    enum class Key { none, up, down, left, right, a, b, start, select };

    void timerCallback() override;

    juce::Rectangle<float> screenArea() const;
    Key hitTest (juce::Point<float> p) const;

    void drawScreen (juce::Graphics& g);
    void drawBody (juce::Graphics& g);

    float getParam (const char* id) const;
    void setParam (const char* id, float plainValue);
    void nudge (const char* id, float delta);
    void beginGestures (std::initializer_list<const char*> ids);
    void endGestures();

    void moveFromMouse (juce::Point<float> p);
    void spawnStar();
    juce::Point<float> playerPosition() const;
    float playerScale() const;

    PanPluginProcessor& panProcessor;

    Key held = Key::none;
    bool draggingScreen = false;
    juce::StringArray activeGestures;

    juce::Point<float> star { 100.0f, 60.0f };
    int score = 0;
    int flash = 0;
    int frame = 0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GamePanEditor)
};
