#include "PluginEditor.h"

namespace
{
    // Les 4 verts de l'ecran de la console
    const juce::Colour lcd0 (0xff0f380f);
    const juce::Colour lcd1 (0xff306230);
    const juce::Colour lcd2 (0xff8bac0f);
    const juce::Colour lcd3 (0xff9bbc0f);

    // '#' = sombre, 'o' = clair, '.' = transparent
    const char* const ghostSprite[8] = {
        "..####..",
        ".######.",
        "##o##o##",
        "########",
        "########",
        "########",
        "##.##.##",
        "#..##..#"
    };

    const char* const starSprite[8] = {
        "...##...",
        "...##...",
        "########",
        ".######.",
        "..####..",
        ".##..##.",
        ".#....#.",
        "........"
    };

    void drawSprite (juce::Graphics& g, const char* const* rows, float cx, float cy,
                     float scale, juce::Colour dark, juce::Colour light)
    {
        const float x0 = cx - 4.0f * scale;
        const float y0 = cy - 4.0f * scale;

        for (int y = 0; y < 8; ++y)
        {
            for (int x = 0; x < 8; ++x)
            {
                const char c = rows[y][x];
                if (c == '.')
                    continue;

                g.setColour (c == '#' ? dark : light);
                g.fillRect (x0 + (float) x * scale, y0 + (float) y * scale, scale, scale);
            }
        }
    }

    juce::Font makeFont (float size, int style = juce::Font::bold)
    {
        return juce::Font (juce::FontOptions (size, style));
    }

    // Positions des boutons (fenetre de 320 x 520)
    const juce::Point<float> dpadCentre (84.0f, 382.0f);
    const juce::Point<float> aCentre (262.0f, 368.0f);
    const juce::Point<float> bCentre (216.0f, 394.0f);
    const juce::Rectangle<float> selectRect (106.0f, 452.0f, 46.0f, 22.0f);
    const juce::Rectangle<float> startRect (162.0f, 452.0f, 46.0f, 22.0f);

    // Terrain de jeu (en pixels de l'ecran 160 x 144)
    constexpr float fieldX0 = 8.0f, fieldX1 = 152.0f, fieldY0 = 16.0f, fieldY1 = 132.0f;
}

GamePanEditor::GamePanEditor (PanPluginProcessor& p)
    : AudioProcessorEditor (&p), panProcessor (p)
{
    setSize (320, 520);
    spawnStar();
    startTimerHz (30);
}

GamePanEditor::~GamePanEditor()
{
    stopTimer();
    endGestures();
}

juce::Rectangle<float> GamePanEditor::screenArea() const
{
    return { 60.0f, 52.0f, 200.0f, 180.0f };
}

// ---------------------------------------------------------------- parametres

float GamePanEditor::getParam (const char* id) const
{
    return panProcessor.apvts.getRawParameterValue (id)->load();
}

void GamePanEditor::setParam (const char* id, float plainValue)
{
    if (auto* p = panProcessor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void GamePanEditor::nudge (const char* id, float delta)
{
    if (auto* p = panProcessor.apvts.getParameter (id))
    {
        const auto range = p->getNormalisableRange();
        setParam (id, juce::jlimit (range.start, range.end, getParam (id) + delta));
    }
}

void GamePanEditor::beginGestures (std::initializer_list<const char*> ids)
{
    endGestures();

    for (auto* id : ids)
    {
        if (auto* p = panProcessor.apvts.getParameter (id))
        {
            p->beginChangeGesture();
            activeGestures.add (id);
        }
    }
}

void GamePanEditor::endGestures()
{
    for (auto& id : activeGestures)
        if (auto* p = panProcessor.apvts.getParameter (id))
            p->endChangeGesture();

    activeGestures.clear();
}

// ---------------------------------------------------------------- jeu

juce::Point<float> GamePanEditor::playerPosition() const
{
    const float px = juce::jlimit (0.0f, 1.0f, (getParam ("pan") + 100.0f) / 200.0f);
    const float py = juce::jlimit (0.0f, 1.0f, (100.0f - getParam ("height")) / 200.0f);

    return { fieldX0 + px * (fieldX1 - fieldX0), fieldY0 + py * (fieldY1 - fieldY0) };
}

float GamePanEditor::playerScale() const
{
    return juce::jmap (juce::jlimit (0.0f, 1.0f, getParam ("depth") / 100.0f), 2.0f, 1.0f);
}

void GamePanEditor::spawnStar()
{
    const auto player = playerPosition();

    for (int tries = 0; tries < 20; ++tries)
    {
        juce::Point<float> candidate (14.0f + rng.nextFloat() * 132.0f,
                                      22.0f + rng.nextFloat() * 104.0f);
        star = candidate;

        if (candidate.getDistanceFrom (player) > 30.0f)
            break;
    }
}

void GamePanEditor::timerCallback()
{
    ++frame;

    switch (held)
    {
        case Key::up:    nudge ("height", +3.0f); break;
        case Key::down:  nudge ("height", -3.0f); break;
        case Key::left:  nudge ("pan",    -3.0f); break;
        case Key::right: nudge ("pan",    +3.0f); break;
        case Key::a:     nudge ("depth",  +2.0f); break;   // A = vers l'arriere
        case Key::b:     nudge ("depth",  -2.0f); break;   // B = vers l'avant
        default: break;
    }

    if (playerPosition().getDistanceFrom (star) < 4.0f * playerScale() + 5.0f)
    {
        ++score;
        flash = 8;
        spawnStar();
    }

    repaint();
}

// ---------------------------------------------------------------- souris

GamePanEditor::Key GamePanEditor::hitTest (juce::Point<float> p) const
{
    const auto c = dpadCentre;

    if (juce::Rectangle<float> (c.x - 13.0f, c.y - 41.0f, 26.0f, 28.0f).contains (p)) return Key::up;
    if (juce::Rectangle<float> (c.x - 13.0f, c.y + 13.0f, 26.0f, 28.0f).contains (p))  return Key::down;
    if (juce::Rectangle<float> (c.x - 41.0f, c.y - 13.0f, 28.0f, 26.0f).contains (p)) return Key::left;
    if (juce::Rectangle<float> (c.x + 13.0f, c.y - 13.0f, 28.0f, 26.0f).contains (p))  return Key::right;

    if (p.getDistanceFrom (aCentre) <= 22.0f) return Key::a;
    if (p.getDistanceFrom (bCentre) <= 22.0f) return Key::b;

    if (selectRect.contains (p)) return Key::select;
    if (startRect.contains (p))  return Key::start;

    return Key::none;
}

void GamePanEditor::moveFromMouse (juce::Point<float> p)
{
    const auto area = screenArea();
    const float k = area.getWidth() / 160.0f;

    const float lx = (p.x - area.getX()) / k;
    const float ly = (p.y - area.getY()) / k;

    const float px = juce::jlimit (0.0f, 1.0f, (lx - fieldX0) / (fieldX1 - fieldX0));
    const float py = juce::jlimit (0.0f, 1.0f, (ly - fieldY0) / (fieldY1 - fieldY0));

    setParam ("pan",    px * 200.0f - 100.0f);
    setParam ("height", 100.0f - py * 200.0f);
}

void GamePanEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;

    if (screenArea().contains (p))
    {
        draggingScreen = true;
        beginGestures ({ "pan", "height" });
        moveFromMouse (p);
        return;
    }

    held = hitTest (p);

    switch (held)
    {
        case Key::up:
        case Key::down:   beginGestures ({ "height" }); break;
        case Key::left:
        case Key::right:  beginGestures ({ "pan" });    break;
        case Key::a:
        case Key::b:      beginGestures ({ "depth" });  break;

        case Key::select:
            beginGestures ({ "pan", "height", "depth" });
            setParam ("pan", 0.0f);
            setParam ("height", 0.0f);
            setParam ("depth", 0.0f);
            break;

        case Key::start:
            score = 0;
            spawnStar();
            break;

        default: break;
    }

    repaint();
}

void GamePanEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingScreen)
        moveFromMouse (e.position);
}

void GamePanEditor::mouseUp (const juce::MouseEvent&)
{
    draggingScreen = false;
    held = Key::none;
    endGestures();
    repaint();
}

void GamePanEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! screenArea().contains (e.position))
        return;

    beginGestures ({ "depth" });
    nudge ("depth", -wheel.deltaY * 30.0f);
    endGestures();
}

// ---------------------------------------------------------------- dessin

void GamePanEditor::drawScreen (juce::Graphics& g)
{
    // Ici on dessine dans un repere de 160 x 144 pixels.
    g.setColour (lcd3);
    g.fillRect (0.0f, 0.0f, 160.0f, 144.0f);

    // petits points de grille
    g.setColour (lcd2);
    for (float y = fieldY0; y <= fieldY1 + 0.1f; y += 16.0f)
        for (float x = fieldX0; x <= fieldX1 + 0.1f; x += 16.0f)
            g.fillRect (x, y, 1.0f, 1.0f);

    // axes du centre
    g.setColour (lcd2);
    g.fillRect (79.5f, fieldY0, 1.0f, fieldY1 - fieldY0);
    g.fillRect (fieldX0, 73.5f, fieldX1 - fieldX0, 1.0f);

    // textes
    g.setColour (lcd0);
    g.setFont (makeFont (7.0f));

    g.drawText ("SCORE " + juce::String (score).paddedLeft ('0', 3),
                juce::Rectangle<float> (3.0f, 1.0f, 60.0f, 10.0f), juce::Justification::centredLeft, false);
    g.drawText ("HAUT", juce::Rectangle<float> (65.0f, 1.0f, 30.0f, 10.0f), juce::Justification::centred, false);
    g.drawText ("BAS",  juce::Rectangle<float> (65.0f, 134.0f, 30.0f, 10.0f), juce::Justification::centred, false);
    g.drawText ("G",    juce::Rectangle<float> (1.0f, 69.0f, 8.0f, 10.0f), juce::Justification::centred, false);
    g.drawText ("D",    juce::Rectangle<float> (151.0f, 69.0f, 8.0f, 10.0f), juce::Justification::centred, false);

    g.drawText ("PAN " + juce::String ((int) std::round (getParam ("pan"))),
                juce::Rectangle<float> (3.0f, 134.0f, 60.0f, 10.0f), juce::Justification::centredLeft, false);
    g.drawText ("ARR " + juce::String ((int) std::round (getParam ("depth"))),
                juce::Rectangle<float> (97.0f, 134.0f, 60.0f, 10.0f), juce::Justification::centredRight, false);

    // etoile a attraper (elle clignote)
    const bool blink = ((frame / 8) % 2) == 0;
    drawSprite (g, starSprite, star.x, star.y, 1.0f, blink ? lcd0 : lcd1, lcd0);

    // joueur
    const auto pos = playerPosition();
    const float scale = playerScale();

    drawSprite (g, ghostSprite, pos.x + 1.0f, pos.y + 1.0f, scale, lcd2, lcd2);   // ombre
    drawSprite (g, ghostSprite, pos.x, pos.y, scale, lcd0, lcd3);

    if (flash > 0)
    {
        --flash;
        g.setColour (lcd0);
        g.drawEllipse (pos.x - 12.0f, pos.y - 12.0f, 24.0f, 24.0f, 1.0f);
    }
}

void GamePanEditor::drawBody (juce::Graphics& g)
{
    // corps de la console
    g.fillAll (juce::Colour (0xffcbc8bd));
    g.setColour (juce::Colour (0xffe2dfd5));
    g.drawRect (getLocalBounds().toFloat().reduced (1.0f), 2.0f);

    // cadre sombre autour de l'ecran
    g.setColour (juce::Colour (0xff55556a));
    g.fillRoundedRectangle (24.0f, 24.0f, 272.0f, 236.0f, 12.0f);

    g.setColour (juce::Colour (0xff8b2252));
    g.fillRect (34.0f, 35.0f, 66.0f, 2.0f);
    g.fillRect (220.0f, 35.0f, 66.0f, 2.0f);
    g.setColour (juce::Colour (0xff2a3a8c));
    g.fillRect (34.0f, 40.0f, 66.0f, 2.0f);
    g.fillRect (220.0f, 40.0f, 66.0f, 2.0f);

    g.setColour (juce::Colour (0xffb8b8c8));
    g.setFont (makeFont (9.0f));
    g.drawText ("STEREO PANNING", juce::Rectangle<float> (100.0f, 30.0f, 120.0f, 16.0f),
                juce::Justification::centred, false);

    // petite lumiere rouge
    g.setColour (juce::Colour (0xffd03030));
    g.fillEllipse (34.0f, 130.0f, 8.0f, 8.0f);

    // titre
    g.setColour (juce::Colour (0xff2d2d83));
    g.setFont (makeFont (28.0f, juce::Font::bold | juce::Font::italic));
    g.drawText ("GAME PAN", juce::Rectangle<float> (24.0f, 266.0f, 272.0f, 38.0f),
                juce::Justification::centred, false);

    // croix directionnelle
    const auto c = dpadCentre;
    g.setColour (juce::Colour (0xff2b2b33));
    g.fillRoundedRectangle (c.x - 13.0f, c.y - 41.0f, 26.0f, 82.0f, 5.0f);
    g.fillRoundedRectangle (c.x - 41.0f, c.y - 13.0f, 82.0f, 26.0f, 5.0f);

    g.setColour (juce::Colour (0xff4f4f5c));
    auto pressedArm = [&] (Key k, juce::Rectangle<float> r)
    {
        if (held == k)
            g.fillRoundedRectangle (r, 4.0f);
    };
    pressedArm (Key::up,    { c.x - 13.0f, c.y - 41.0f, 26.0f, 28.0f });
    pressedArm (Key::down,  { c.x - 13.0f, c.y + 13.0f, 26.0f, 28.0f });
    pressedArm (Key::left,  { c.x - 41.0f, c.y - 13.0f, 28.0f, 26.0f });
    pressedArm (Key::right, { c.x + 13.0f, c.y - 13.0f, 28.0f, 26.0f });

    g.setColour (juce::Colour (0xff1a1a20));
    g.fillEllipse (c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);

    // boutons A et B
    auto drawRound = [&] (juce::Point<float> p, bool pressed, const char* label)
    {
        g.setColour (juce::Colour (0xffa9a69a));
        g.fillEllipse (p.x - 23.0f, p.y - 23.0f, 46.0f, 46.0f);
        g.setColour (pressed ? juce::Colour (0xff6e1d45) : juce::Colour (0xff9b2a62));
        g.fillEllipse (p.x - 19.0f, p.y - 19.0f, 38.0f, 38.0f);
        g.setColour (juce::Colour (0xff2d2d83));
        g.setFont (makeFont (14.0f));
        g.drawText (label, juce::Rectangle<float> (p.x + 6.0f, p.y + 22.0f, 20.0f, 16.0f),
                    juce::Justification::centred, false);
    };
    drawRound (bCentre, held == Key::b, "B");
    drawRound (aCentre, held == Key::a, "A");

    // select / start
    auto drawPill = [&] (juce::Rectangle<float> r, bool pressed, const char* label)
    {
        g.setColour (pressed ? juce::Colour (0xff62626e) : juce::Colour (0xff8a8a96));
        g.fillRoundedRectangle (r.getX(), r.getY(), r.getWidth(), 10.0f, 5.0f);
        g.setColour (juce::Colour (0xff2d2d83));
        g.setFont (makeFont (9.0f));
        g.drawText (label, juce::Rectangle<float> (r.getX() - 6.0f, r.getY() + 12.0f, r.getWidth() + 12.0f, 11.0f),
                    juce::Justification::centred, false);
    };
    drawPill (selectRect, held == Key::select, "SELECT");
    drawPill (startRect,  held == Key::start,  "START");

    // aide
    g.setColour (juce::Colour (0xff7a776c));
    g.setFont (makeFont (9.0f, juce::Font::plain));
    g.drawText ("SOURIS / CROIX = BOUGER", juce::Rectangle<float> (20.0f, 478.0f, 200.0f, 12.0f),
                juce::Justification::centredLeft, false);
    g.drawText ("A / B = ARRIERE / AVANT", juce::Rectangle<float> (20.0f, 490.0f, 200.0f, 12.0f),
                juce::Justification::centredLeft, false);
    g.drawText ("SELECT = CENTRE", juce::Rectangle<float> (20.0f, 502.0f, 200.0f, 12.0f),
                juce::Justification::centredLeft, false);

    // haut-parleur
    g.setColour (juce::Colour (0xff9a978b));
    for (int i = 0; i < 6; ++i)
    {
        const float x = 238.0f + (float) i * 10.0f;
        g.drawLine (x + 10.0f, 482.0f, x, 508.0f, 4.0f);
    }
}

void GamePanEditor::paint (juce::Graphics& g)
{
    drawBody (g);

    // ecran
    const auto area = screenArea();
    const float k = area.getWidth() / 160.0f;

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (area.toNearestInt());
        g.addTransform (juce::AffineTransform::scale (k).translated (area.getX(), area.getY()));
        drawScreen (g);
    }

    // bord de l'ecran
    g.setColour (juce::Colour (0xff2b2b33));
    g.drawRect (area.expanded (1.0f), 2.0f);
}
