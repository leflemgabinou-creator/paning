#include "PluginEditor.h"

namespace
{
    constexpr float designW = 1322.0f;
    constexpr float designH = 701.0f;

    // ---- couleurs de la coque (bleu translucide)
    const juce::Colour shellTop   (0xff3a52d6);
    const juce::Colour shellMid   (0xff2237b4);
    const juce::Colour shellBot   (0xff0d1566);
    const juce::Colour paleText   (0xffdfe6ff);

    // ---- couleurs de l'ecran LCD
    const juce::Colour lcdBgTop   (0xffaab69c);
    const juce::Colour lcdBgBot   (0xff96a389);
    const juce::Colour lcdInk     (0xff1d251b);

    juce::Font makeFont (float size, int style = juce::Font::bold)
    {
        return juce::Font (juce::FontOptions (size, style));
    }

    juce::Font makeMono (float size, int style = juce::Font::bold)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, style));
    }

    void drawLabel (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size,
                    juce::Justification j = juce::Justification::centred,
                    juce::Colour c = paleText, bool mono = false, int style = juce::Font::bold)
    {
        g.setFont (mono ? makeMono (size, style) : makeFont (size, style));
        g.setColour (c);
        g.drawText (t, r, j, false);
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        g.setColour (juce::Colours::black.withAlpha (0.20f));
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.drawRoundedRectangle (r, radius, 1.6f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawRoundedRectangle (r.translated (0.0f, 1.5f), radius, 1.2f);
    }

    void drawPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, float size)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.6f);
        drawLabel (g, text, r, size);
    }

    void drawScrew (juce::Graphics& g, juce::Point<float> p, float r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.drawEllipse (p.x - r, p.y - r, 2 * r, 2 * r, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawEllipse (p.x - r + 1.5f, p.y - r + 1.5f, 2 * r, 2 * r, 1.2f);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillEllipse (p.x - r * 0.45f, p.y - r * 0.45f, r * 0.9f, r * 0.9f);
    }

    // ---- les 5 boutons rotatifs
    struct KnobDef { const char* id; juce::Point<float> c; float housing; float cap; };
    const KnobDef knobs[] = {
        { "input",  { 163.0f, 143.0f }, 56.0f, 45.0f },
        { "rate",   { 1085.0f, 130.0f }, 46.0f, 35.0f },
        { "amount", { 1238.0f, 130.0f }, 46.0f, 35.0f },
        { "width",  { 1068.0f, 448.0f }, 44.0f, 34.0f },
        { "pan",    { 1190.0f, 448.0f }, 44.0f, 34.0f }
    };

    // ---- les 4 boutons de mode
    const juce::Point<float> modeCentres[] = {
        { 1049.0f, 299.0f }, { 1120.0f, 299.0f }, { 1196.0f, 299.0f }, { 1269.0f, 299.0f }
    };
    const char* const modeNames[] = { "AUTO", "MANUAL", "LFO", "TRIGGER" };

    // ---- croix directionnelle et boutons du bas
    const juce::Point<float> dpadCentre (140.0f, 362.0f);
    constexpr float dpadL = 84.0f, dpadW = 29.0f;
    const juce::Point<float> syncCentre  (182.0f, 517.0f);
    const juce::Point<float> resetCentre (257.0f, 517.0f);

    // ---- ecran LCD
    const juce::Rectangle<float> lcdRect (378.0f, 80.0f, 550.0f, 340.0f);
    const juce::Rectangle<float> arcPanel (391.0f, 128.0f, 308.0f, 278.0f);
    const juce::Rectangle<float> rightPanel (710.0f, 128.0f, 204.0f, 278.0f);
    const juce::Point<float> arcCentre (545.0f, 316.0f);
    constexpr float arcR = 126.0f;

    // robot : '#' = pixel allume
    const char* const robotSprite[12] = {
        "..#######..",
        "..#.....#..",
        "..#.#.#.#..",
        "..#.....#..",
        "..#######..",
        "....###....",
        ".#########.",
        "##.#####.##",
        "#..#...#..#",
        "...#####...",
        "...#...#...",
        "..##...##.."
    };

    juce::String divisionText (int idx)
    {
        static const char* const names[] = { "1/1", "1/2", "1/4", "1/8", "1/16" };
        return names[juce::jlimit (0, 4, idx)];
    }
}

GamePanEditor::GamePanEditor (PanPluginProcessor& p)
    : AudioProcessorEditor (&p), panProcessor (p)
{
    setSize (880, 467);
    startTimerHz (30);
}

GamePanEditor::~GamePanEditor()
{
    stopTimer();
    endGesture();
}

float GamePanEditor::scaleFactor() const
{
    return (float) getWidth() / designW;
}

// ---------------------------------------------------------------- parametres

float GamePanEditor::rawParam (const char* id) const
{
    return panProcessor.apvts.getRawParameterValue (id)->load();
}

float GamePanEditor::normParam (const char* id) const
{
    if (auto* p = panProcessor.apvts.getParameter (id))
        return p->getValue();

    return 0.0f;
}

void GamePanEditor::setNorm (const char* id, float norm)
{
    if (auto* p = panProcessor.apvts.getParameter (id))
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
}

void GamePanEditor::setPlain (const char* id, float plain)
{
    if (auto* p = panProcessor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

void GamePanEditor::setChoice (const char* id, int index)
{
    if (auto* p = panProcessor.apvts.getParameter (id))
    {
        beginGesture (id);
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        endGesture();
    }
}

void GamePanEditor::beginGesture (const char* id)
{
    endGesture();

    if (auto* p = panProcessor.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        activeGesture = id;
    }
}

void GamePanEditor::endGesture()
{
    if (activeGesture.isNotEmpty())
        if (auto* p = panProcessor.apvts.getParameter (activeGesture))
            p->endChangeGesture();

    activeGesture = {};
}

// ---------------------------------------------------------------- zones de clic

int GamePanEditor::knobAt (juce::Point<float> p) const
{
    for (int i = 0; i < 5; ++i)
        if (p.getDistanceFrom (knobs[i].c) <= knobs[i].housing + 4.0f)
            return i;

    return -1;
}

int GamePanEditor::modeButtonAt (juce::Point<float> p) const
{
    for (int i = 0; i < 4; ++i)
        if (p.getDistanceFrom (modeCentres[i]) <= 28.0f)
            return i;

    return -1;
}

GamePanEditor::Key GamePanEditor::keyAt (juce::Point<float> p) const
{
    const auto c = dpadCentre;
    const float L = dpadL, W = dpadW;

    if (juce::Rectangle<float> (c.x - W, c.y - L, 2 * W, L - W + 4.0f).contains (p)) return Key::up;
    if (juce::Rectangle<float> (c.x - W, c.y + W - 4.0f, 2 * W, L - W + 4.0f).contains (p)) return Key::down;
    if (juce::Rectangle<float> (c.x - L, c.y - W, L - W + 4.0f, 2 * W).contains (p)) return Key::left;
    if (juce::Rectangle<float> (c.x + W - 4.0f, c.y - W, L - W + 4.0f, 2 * W).contains (p)) return Key::right;

    if (p.getDistanceFrom (syncCentre) <= 26.0f)  return Key::sync;
    if (p.getDistanceFrom (resetCentre) <= 26.0f) return Key::reset;

    return Key::none;
}

// ---------------------------------------------------------------- souris

void GamePanEditor::moveFromArc (juce::Point<float> p)
{
    const float dx = p.x - arcCentre.x;
    const float dy = juce::jmax (1.0f, arcCentre.y - p.y);
    const float angle = juce::jlimit (-juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi,
                                      std::atan2 (dx, dy));

    setPlain ("pan", angle / juce::MathConstants<float>::halfPi * 100.0f);
}

void GamePanEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scaleFactor();

    // boutons rotatifs
    const int k = knobAt (p);
    if (k >= 0)
    {
        if (e.getNumberOfClicks() >= 2)
        {
            beginGesture (knobs[k].id);
            if (auto* param = panProcessor.apvts.getParameter (knobs[k].id))
                param->setValueNotifyingHost (param->getDefaultValue());
            endGesture();
            return;
        }

        activeKnob = k;
        dragStartY = e.position.y;
        dragStartNorm = normParam (knobs[k].id);
        beginGesture (knobs[k].id);
        return;
    }

    // boutons de mode
    const int m = modeButtonAt (p);
    if (m >= 0)
    {
        setChoice ("mode", m);
        return;
    }

    // liste des modes sur l'ecran
    if (juce::Rectangle<float> (722.0f, 160.0f, 182.0f, 100.0f).contains (p))
    {
        setChoice ("mode", juce::jlimit (0, 3, (int) ((p.y - 160.0f) / 25.0f)));
        return;
    }

    // arc de l'ecran : on deplace le robot
    if (arcPanel.contains (p))
    {
        draggingArc = true;
        beginGesture ("pan");
        moveFromArc (p);
        return;
    }

    // croix et boutons du bas
    held = keyAt (p);
    const int mode = (int) rawParam ("mode");

    switch (held)
    {
        case Key::up:    setChoice ("mode", (mode + 3) % 4); break;
        case Key::down:  setChoice ("mode", (mode + 1) % 4); break;
        case Key::left:
        case Key::right: beginGesture ("pan"); break;

        case Key::sync:
            setChoice ("division", ((int) rawParam ("division") + 1) % 5);
            break;

        case Key::reset:
            for (auto* id : { "pan", "input", "width" })
            {
                beginGesture (id);
                if (auto* param = panProcessor.apvts.getParameter (id))
                    param->setValueNotifyingHost (param->getDefaultValue());
                endGesture();
            }
            break;

        case Key::none: break;
    }
}

void GamePanEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (activeKnob >= 0)
    {
        const float sensitivity = e.mods.isShiftDown() ? 800.0f : 200.0f;
        setNorm (knobs[activeKnob].id, dragStartNorm + (dragStartY - e.position.y) / sensitivity);
    }
    else if (draggingArc)
    {
        moveFromArc (e.position / scaleFactor());
    }
}

void GamePanEditor::mouseUp (const juce::MouseEvent&)
{
    activeKnob = -1;
    draggingArc = false;
    held = Key::none;
    endGesture();
}

void GamePanEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int k = knobAt (e.position / scaleFactor());
    if (k < 0)
        return;

    beginGesture (knobs[k].id);
    setNorm (knobs[k].id, normParam (knobs[k].id) + (wheel.deltaY > 0 ? 0.03f : -0.03f));
    endGesture();
}

void GamePanEditor::timerCallback()
{
    if (held == Key::left || held == Key::right)
    {
        const float step = (held == Key::left ? -2.0f : 2.0f);
        setPlain ("pan", juce::jlimit (-100.0f, 100.0f, rawParam ("pan") + step));
    }

    repaint();
}

// ---------------------------------------------------------------- dessin : cache + dynamique

void GamePanEditor::paint (juce::Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();

    if (! cache.isValid() || std::abs (scale - cacheScale) > 0.01f)
        rebuildCache (scale);

    g.drawImage (cache, getLocalBounds().toFloat());

    g.addTransform (juce::AffineTransform::scale (scaleFactor()));
    drawDynamic (g);
}

void GamePanEditor::rebuildCache (float scale)
{
    cacheScale = scale;

    const int pw = juce::jmax (1, juce::roundToInt ((float) getWidth()  * scale));
    const int ph = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));

    cache = juce::Image (juce::Image::ARGB, pw, ph, true);
    juce::Graphics cg (cache);
    cg.addTransform (juce::AffineTransform::scale ((float) pw / designW));
    drawStatic (cg);
}

// ---------------------------------------------------------------- dessin : partie fixe

void GamePanEditor::drawStatic (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff05060f));

    // ---- coque bleue translucide
    juce::Path shell;
    shell.addRoundedRectangle (0.0f, 0.0f, designW, designH, 46.0f);

    g.setGradientFill (juce::ColourGradient (shellTop, 0.0f, 0.0f, shellBot, 0.0f, designH, false));
    g.fillPath (shell);
    g.setGradientFill (juce::ColourGradient (shellMid.withAlpha (0.0f), designW * 0.5f, 120.0f,
                                             shellMid.withAlpha (0.55f), designW * 0.5f, 420.0f, false));
    g.fillPath (shell);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (shell);

        // grain du plastique
        juce::Random grain (11);
        for (int i = 0; i < 700; ++i)
        {
            const float a = 0.03f + 0.06f * grain.nextFloat();
            g.setColour ((i & 1) ? juce::Colours::white.withAlpha (a) : juce::Colours::black.withAlpha (a));
            g.fillRect (grain.nextFloat() * designW, grain.nextFloat() * designH, 2.0f, 2.0f);
        }

        // pieces internes qu'on devine a travers le plastique
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (330.0f, 560.0f, 150.0f, 90.0f, 10.0f);
        g.fillRoundedRectangle (500.0f, 575.0f, 110.0f, 70.0f, 10.0f);
        g.fillRoundedRectangle (640.0f, 560.0f, 170.0f, 90.0f, 10.0f);
        g.fillRoundedRectangle (830.0f, 575.0f, 130.0f, 70.0f, 10.0f);
        g.fillRect (990.0f, 30.0f, 4.0f, 610.0f);
        g.fillRect (302.0f, 30.0f, 4.0f, 610.0f);

        for (auto s : { juce::Point<float> (30.0f, 66.0f), juce::Point<float> (296.0f, 22.0f),
                        juce::Point<float> (1004.0f, 22.0f), juce::Point<float> (1296.0f, 64.0f),
                        juce::Point<float> (30.0f, 400.0f), juce::Point<float> (300.0f, 640.0f),
                        juce::Point<float> (1012.0f, 640.0f), juce::Point<float> (1298.0f, 640.0f),
                        juce::Point<float> (36.0f, 650.0f), juce::Point<float> (1296.0f, 360.0f),
                        juce::Point<float> (150.0f, 600.0f), juce::Point<float> (258.0f, 612.0f),
                        juce::Point<float> (1118.0f, 628.0f), juce::Point<float> (32.0f, 250.0f) })
            drawScrew (g, s, 11.0f);

        // reflets
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f), 0.0f, 0.0f,
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, 260.0f, false));
        g.fillRect (0.0f, 0.0f, designW, 260.0f);

        juce::Path streak;
        streak.addTriangle (0.0f, 0.0f, 420.0f, 0.0f, 0.0f, 520.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillPath (streak);

        // epaules plus claires dans les coins du haut
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.fillRoundedRectangle (1180.0f, -20.0f, 160.0f, 46.0f, 20.0f);
        g.fillRoundedRectangle (-20.0f, -20.0f, 100.0f, 40.0f, 20.0f);

        // bord biseaute
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), 0.0f, 0.0f,
                                                 juce::Colours::black.withAlpha (0.55f), 0.0f, designH, false));
        g.strokePath (shell, juce::PathStrokeType (10.0f));
    }

    // ---- panneaux sous le plastique
    drawPanel (g, { 52.0f, 38.0f, 220.0f, 206.0f }, 24.0f);                // INPUT
    drawPanel (g, { 1012.0f, 40.0f, 306.0f, 196.0f }, 24.0f);              // LFO
    drawPanel (g, { 1012.0f, 246.0f, 306.0f, 116.0f }, 24.0f);             // MODE
    drawPanel (g, { 1015.0f, 372.0f, 232.0f, 160.0f }, 22.0f);             // WIDTH / CENTER
    drawPanel (g, { 148.0f, 488.0f, 154.0f, 100.0f }, 24.0f);              // boutons du bas

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillRect (1162.0f, 78.0f, 1.5f, 150.0f);
    g.fillRect (1130.0f, 408.0f, 1.5f, 118.0f);

    // puits de la croix
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillEllipse (dpadCentre.x - 94.0f, dpadCentre.y - 94.0f, 188.0f, 188.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (dpadCentre.x - 94.0f, dpadCentre.y - 94.0f, 188.0f, 188.0f, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawEllipse (dpadCentre.x - 92.0f, dpadCentre.y - 91.0f, 188.0f, 188.0f, 1.4f);

    // petit bouton plat en bas a gauche
    g.setColour (juce::Colour (0xff15161c));
    g.fillRoundedRectangle (48.0f, 516.0f, 70.0f, 30.0f, 12.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (48.0f, 516.0f, 70.0f, 30.0f, 12.0f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRoundedRectangle (54.0f, 520.0f, 58.0f, 8.0f, 4.0f);

    // fentes du haut-parleur
    for (int i = 0; i < 7; ++i)
    {
        const float y = 392.0f + (float) i * 25.0f;
        g.setColour (juce::Colour (0xff0a0f3d).withAlpha (0.85f));
        g.fillRoundedRectangle (1258.0f, y, 80.0f, 11.0f, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillRect (1258.0f, y + 12.0f, 80.0f, 1.5f);
    }

    // ---- cadre noir de l'ecran
    juce::Path bezel;
    bezel.addRoundedRectangle (318.0f, 4.0f, 670.0f, 538.0f, 74.0f);

    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 24, { 0, 8 }).drawForPath (g, bezel);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff222229), 0.0f, 4.0f,
                                             juce::Colour (0xff09090d), 0.0f, 542.0f, false));
    g.fillPath (bezel);
    g.setColour (juce::Colour (0xff4a4a58));
    g.strokePath (bezel, juce::PathStrokeType (2.5f));

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (bezel);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0.0f, 4.0f,
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, 120.0f, false));
        g.fillRect (318.0f, 4.0f, 670.0f, 120.0f);
    }

    g.setColour (juce::Colour (0xff050507));
    g.fillRoundedRectangle (356.0f, 52.0f, 594.0f, 398.0f, 30.0f);
    g.setColour (juce::Colour (0xff2e2e38));
    g.drawRoundedRectangle (356.0f, 52.0f, 594.0f, 398.0f, 30.0f, 2.0f);

    // titre
    {
        const juce::Rectangle<float> r (330.0f, 452.0f, 650.0f, 66.0f);
        g.setFont (makeFont (48.0f, juce::Font::bold | juce::Font::italic));
        g.setColour (juce::Colours::black);
        g.drawText ("GAME PAN", r.translated (2.0f, 3.0f), juce::Justification::centred, false);
        g.setColour (juce::Colour (0xff9fa0b4));
        g.drawText ("GAME PAN", r, juce::Justification::centred, false);
    }

    // ---- textes et reperes
    drawPill (g, { 110.0f, 56.0f, 108.0f, 30.0f }, "INPUT", 18.0f);
    drawLabel (g, "-12", { 66.0f, 192.0f, 50.0f, 22.0f }, 16.0f);
    drawLabel (g, "dB", { 135.0f, 206.0f, 50.0f, 22.0f }, 17.0f);
    drawLabel (g, "+12", { 204.0f, 192.0f, 50.0f, 22.0f }, 16.0f);

    drawPill (g, { 1095.0f, 42.0f, 72.0f, 30.0f }, "LFO", 18.0f);
    drawLabel (g, "0.1Hz", { 1016.0f, 180.0f, 64.0f, 20.0f }, 14.0f);
    drawLabel (g, "10Hz",  { 1100.0f, 180.0f, 56.0f, 20.0f }, 14.0f);
    drawLabel (g, "RATE",  { 1035.0f, 200.0f, 100.0f, 24.0f }, 18.0f);
    drawLabel (g, "0%",    { 1168.0f, 180.0f, 50.0f, 20.0f }, 14.0f);
    drawLabel (g, "100%",  { 1252.0f, 180.0f, 60.0f, 20.0f }, 14.0f);
    drawLabel (g, "AMOUNT", { 1180.0f, 200.0f, 116.0f, 24.0f }, 18.0f);

    drawPill (g, { 1018.0f, 246.0f, 84.0f, 28.0f }, "MODE", 17.0f);
    for (int i = 0; i < 4; ++i)
        drawLabel (g, modeNames[i], { modeCentres[i].x - 40.0f, 328.0f, 80.0f, 22.0f }, 15.0f);

    drawLabel (g, "WIDTH",  { 1020.0f, 376.0f, 100.0f, 24.0f }, 20.0f);
    drawLabel (g, "CENTER", { 1135.0f, 376.0f, 110.0f, 24.0f }, 20.0f);
    drawLabel (g, "0%",    { 1015.0f, 498.0f, 50.0f, 20.0f }, 14.0f);
    drawLabel (g, "200%",  { 1070.0f, 498.0f, 56.0f, 20.0f }, 14.0f);
    drawLabel (g, "L",     { 1138.0f, 498.0f, 40.0f, 20.0f }, 14.0f);
    drawLabel (g, "R",     { 1200.0f, 498.0f, 40.0f, 20.0f }, 14.0f);

    drawLabel (g, "SYNC",  { syncCentre.x - 40.0f, 546.0f, 80.0f, 22.0f }, 16.0f);
    drawLabel (g, "RESET", { resetCentre.x - 40.0f, 546.0f, 80.0f, 22.0f }, 16.0f);

    // POWER
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (1028.0f, 552.0f, 128.0f, 46.0f, 23.0f);
    g.setColour (juce::Colours::white.withAlpha (0.30f));
    g.drawRoundedRectangle (1028.0f, 552.0f, 128.0f, 46.0f, 23.0f, 1.6f);
    drawLabel (g, "POWER", { 1068.0f, 556.0f, 82.0f, 38.0f }, 19.0f, juce::Justification::centredLeft);

    // anneaux autour des boutons de mode
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (modeCentres[i].x - 27.0f, modeCentres[i].y - 27.0f, 54.0f, 54.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (modeCentres[i].x - 27.0f, modeCentres[i].y - 27.0f, 54.0f, 54.0f, 1.4f);
    }

    // anneaux sous les boutons du bas
    for (auto c : { syncCentre, resetCentre })
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - 26.0f, c.y - 26.0f, 52.0f, 52.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (c.x - 26.0f, c.y - 26.0f, 52.0f, 52.0f, 1.4f);
    }
}

// ---------------------------------------------------------------- dessin : partie vivante

void GamePanEditor::drawKnobAt (juce::Graphics& g, int index)
{
    const auto& k = knobs[index];
    const auto c = k.c;
    const float v = normParam (k.id);

    // graduations
    g.setColour (paleText.withAlpha (0.85f));
    for (int i = 0; i < 11; ++i)
    {
        const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) i);
        const float r0 = k.housing + 6.0f;
        const float r1 = k.housing + ((i % 5 == 0) ? 15.0f : 11.0f);
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0,
                    c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, 1.8f);
    }

    // logement sombre
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - k.housing - 3.0f, c.y - k.housing - 3.0f, 2 * (k.housing + 3.0f), 2 * (k.housing + 3.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff30313a), c.x - k.housing * 0.5f, c.y - k.housing * 0.6f,
                                             juce::Colour (0xff060608), c.x + k.housing, c.y + k.housing, true));
    g.fillEllipse (c.x - k.housing, c.y - k.housing, 2 * k.housing, 2 * k.housing);

    // capuchon argente
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6f6f9), c.x - k.cap * 0.35f, c.y - k.cap * 0.5f,
                                             juce::Colour (0xff767783), c.x + k.cap * 0.95f, c.y + k.cap * 0.95f, true));
    g.fillEllipse (c.x - k.cap, c.y - k.cap, 2 * k.cap, 2 * k.cap);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (c.x - k.cap, c.y - k.cap, 2 * k.cap, 2 * k.cap, 1.8f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (c.x - k.cap + 3.0f, c.y - k.cap + 3.0f, 2 * k.cap - 6.0f, 2 * k.cap - 6.0f, 1.2f);

    // repere
    const float a = juce::degreesToRadians (-135.0f + 270.0f * v);
    const float r0 = k.cap * 0.22f, r1 = k.cap * 0.92f;
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.drawLine (c.x + std::sin (a) * r0 + 1.5f, c.y - std::cos (a) * r0 + 1.5f,
                c.x + std::sin (a) * r1 + 1.5f, c.y - std::cos (a) * r1 + 1.5f, 4.0f);
    g.setColour (juce::Colours::white);
    g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0,
                c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, 3.6f);
}

void GamePanEditor::drawDynamic (juce::Graphics& g)
{
    for (int i = 0; i < 5; ++i)
        drawKnobAt (g, i);

    // ---- boutons de mode avec LED verte
    const int mode = juce::jlimit (0, 3, (int) rawParam ("mode"));

    for (int i = 0; i < 4; ++i)
    {
        const auto c = modeCentres[i];

        if (i == mode)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7bff5a).withAlpha (0.55f), c.x, c.y,
                                                     juce::Colour (0xff7bff5a).withAlpha (0.0f), c.x + 44.0f, c.y, true));
            g.fillEllipse (c.x - 44.0f, c.y - 44.0f, 88.0f, 88.0f);

            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8ffc0), c.x - 5.0f, c.y - 6.0f,
                                                     juce::Colour (0xff19a82a), c.x + 16.0f, c.y + 16.0f, true));
            g.fillEllipse (c.x - 19.0f, c.y - 19.0f, 38.0f, 38.0f);
            g.setColour (juce::Colour (0xff0d4a14));
            g.drawEllipse (c.x - 19.0f, c.y - 19.0f, 38.0f, 38.0f, 1.6f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4b55), c.x - 6.0f, c.y - 8.0f,
                                                     juce::Colour (0xff131318), c.x + 16.0f, c.y + 18.0f, true));
            g.fillEllipse (c.x - 19.0f, c.y - 19.0f, 38.0f, 38.0f);
            g.setColour (juce::Colours::black);
            g.drawEllipse (c.x - 19.0f, c.y - 19.0f, 38.0f, 38.0f, 1.6f);
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.fillEllipse (c.x - 11.0f, c.y - 14.0f, 18.0f, 8.0f);
        }
    }

    // ---- SYNC / RESET
    auto roundButton = [&] (juce::Point<float> c, bool pressed)
    {
        const float off = pressed ? 1.5f : 0.0f;
        g.setGradientFill (juce::ColourGradient (pressed ? juce::Colour (0xffa0a1aa) : juce::Colour (0xffd2d2d8),
                                                 c.x - 7.0f, c.y - 9.0f + off,
                                                 juce::Colour (0xff6b6c77), c.x + 16.0f, c.y + 18.0f + off, true));
        g.fillEllipse (c.x - 20.0f, c.y - 20.0f + off, 40.0f, 40.0f);
        g.setColour (juce::Colour (0xff111116));
        g.drawEllipse (c.x - 20.0f, c.y - 20.0f + off, 40.0f, 40.0f, 1.8f);
    };
    roundButton (syncCentre,  held == Key::sync);
    roundButton (resetCentre, held == Key::reset);

    // ---- croix directionnelle grise
    {
        const auto c = dpadCentre;
        const float L = dpadL, W = dpadW;

        juce::Path cross;
        cross.addRoundedRectangle (c.x - W, c.y - L, 2 * W, 2 * L, 9.0f);
        cross.addRoundedRectangle (c.x - L, c.y - W, 2 * L, 2 * W, 9.0f);
        cross.setUsingNonZeroWinding (true);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillPath (cross, juce::AffineTransform::translation (0.0f, 4.0f));

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd0d0d4), c.x - 40.0f, c.y - 70.0f,
                                                 juce::Colour (0xff85858e), c.x + 50.0f, c.y + 80.0f, false));
        g.fillPath (cross);
        g.setColour (juce::Colour (0xff17171c));
        g.strokePath (cross, juce::PathStrokeType (2.2f));

        auto arm = [&] (Key k, juce::Rectangle<float> r, juce::Point<float> a, juce::Point<float> b, juce::Point<float> t)
        {
            if (held == k)
            {
                g.setColour (juce::Colours::black.withAlpha (0.28f));
                g.fillRoundedRectangle (r, 7.0f);
            }
            juce::Path tri;
            tri.addTriangle (a.x, a.y, b.x, b.y, t.x, t.y);
            g.setColour (juce::Colour (0xff6e6f7a));
            g.fillPath (tri);
        };
        arm (Key::up,    { c.x - W, c.y - L, 2 * W, L - W }, { c.x - 13.0f, c.y - L + 34.0f }, { c.x + 13.0f, c.y - L + 34.0f }, { c.x, c.y - L + 14.0f });
        arm (Key::down,  { c.x - W, c.y + W, 2 * W, L - W }, { c.x - 13.0f, c.y + L - 34.0f }, { c.x + 13.0f, c.y + L - 34.0f }, { c.x, c.y + L - 14.0f });
        arm (Key::left,  { c.x - L, c.y - W, L - W, 2 * W }, { c.x - L + 34.0f, c.y - 13.0f }, { c.x - L + 34.0f, c.y + 13.0f }, { c.x - L + 14.0f, c.y });
        arm (Key::right, { c.x + W, c.y - W, L - W, 2 * W }, { c.x + L - 34.0f, c.y - 13.0f }, { c.x + L - 34.0f, c.y + 13.0f }, { c.x + L - 14.0f, c.y });

        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (c.x - 11.0f, c.y - 11.0f, 22.0f, 22.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (c.x - 8.0f, c.y - 9.0f, 14.0f, 8.0f);
    }

    // ---- LED POWER
    {
        const juce::Point<float> c (1050.0f, 575.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7bff5a).withAlpha (0.5f), c.x, c.y,
                                                 juce::Colour (0xff7bff5a).withAlpha (0.0f), c.x + 30.0f, c.y, true));
        g.fillEllipse (c.x - 30.0f, c.y - 30.0f, 60.0f, 60.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8ffc0), c.x - 3.0f, c.y - 4.0f,
                                                 juce::Colour (0xff19a82a), c.x + 10.0f, c.y + 10.0f, true));
        g.fillEllipse (c.x - 10.0f, c.y - 10.0f, 20.0f, 20.0f);
    }

    drawLcd (g);
}

// ---------------------------------------------------------------- dessin : ecran LCD

void GamePanEditor::drawLcd (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (lcdRect.toNearestInt());

    // fond vert-gris
    g.setGradientFill (juce::ColourGradient (lcdBgTop, 0.0f, lcdRect.getY(), lcdBgBot, 0.0f, lcdRect.getBottom(), false));
    g.fillRect (lcdRect);

    const float pan = panProcessor.displayPan.load();                 // -100 .. +100
    const int mode = juce::jlimit (0, 3, (int) rawParam ("mode"));

    // ---- bandeau du haut
    g.setColour (lcdInk);
    g.fillRect (378.0f, 80.0f, 550.0f, 36.0f);
    drawLabel (g, "GAME PAN", { 396.0f, 80.0f, 240.0f, 36.0f }, 24.0f, juce::Justification::centredLeft, lcdBgTop, true);
    drawLabel (g, "v1.0", { 744.0f, 80.0f, 80.0f, 36.0f }, 17.0f, juce::Justification::centredRight, lcdBgTop, true);

    g.setColour (lcdBgTop);
    for (int i = 0; i < 3; ++i)
        g.fillRect (836.0f + (float) i * 8.0f, 106.0f - 6.0f - (float) i * 4.0f, 5.0f, 6.0f + (float) i * 4.0f);
    g.drawRect (868.0f, 90.0f, 44.0f, 18.0f, 2.0f);
    g.fillRect (912.0f, 95.0f, 4.0f, 8.0f);
    for (int i = 0; i < 3; ++i)
        g.fillRect (872.0f + (float) i * 13.0f, 94.0f, 10.0f, 10.0f);

    // ---- cadres des deux panneaux
    g.setColour (lcdInk);
    g.drawRect (arcPanel, 2.0f);
    g.drawRect (rightPanel, 2.0f);

    // ---- arc de panoramique
    const auto C = arcCentre;
    auto polar = [&] (float angle, float radius) -> juce::Point<float>
    {
        return { C.x + std::sin (angle) * radius, C.y - std::cos (angle) * radius };
    };

    g.setColour (lcdInk);
    for (float a = -90.0f; a <= 90.1f; a += 4.2f)
    {
        const auto p = polar (juce::degreesToRadians (a), arcR);
        g.fillRect (p.x - 1.5f, p.y - 1.5f, 3.0f, 3.0f);
    }
    g.setColour (lcdInk.withAlpha (0.55f));
    for (float a = -90.0f; a <= 90.1f; a += 5.6f)
    {
        const auto p = polar (juce::degreesToRadians (a), 80.0f);
        g.fillRect (p.x - 1.0f, p.y - 1.0f, 2.0f, 2.0f);
    }

    // axe central
    g.setColour (lcdInk.withAlpha (0.85f));
    g.fillRect (C.x - 1.0f, C.y - 118.0f, 2.0f, 118.0f);

    // repere triangulaire sur l'arc + tige jusqu'au robot
    const float angle = pan / 100.0f * juce::MathConstants<float>::halfPi;
    {
        juce::Graphics::ScopedSaveState s2 (g);
        g.addTransform (juce::AffineTransform::rotation (angle, C.x, C.y));

        g.setColour (lcdInk);
        juce::Path tri;
        tri.addTriangle (C.x - 14.0f, C.y - arcR - 16.0f, C.x + 14.0f, C.y - arcR - 16.0f, C.x, C.y - arcR + 6.0f);
        g.fillPath (tri);
        g.fillRect (C.x - 1.0f, C.y - arcR + 4.0f, 2.0f, 30.0f);
    }

    // robot
    {
        const auto rp = polar (angle, 66.0f);
        const float px = 4.4f;
        const float x0 = rp.x - 5.5f * px;
        const float y0 = rp.y - 6.0f * px;

        g.setColour (lcdInk);
        for (int y = 0; y < 12; ++y)
            for (int x = 0; x < 11; ++x)
                if (robotSprite[y][x] == '#')
                    g.fillRect (x0 + (float) x * px, y0 + (float) y * px, px, px);
    }

    drawLabel (g, "L", { 398.0f, 308.0f, 28.0f, 26.0f }, 20.0f, juce::Justification::centred, lcdInk, true);
    drawLabel (g, "R", { 664.0f, 308.0f, 28.0f, 26.0f }, 20.0f, juce::Justification::centred, lcdInk, true);

    const int percent = juce::roundToInt ((pan + 100.0f) * 0.5f);
    drawLabel (g, "PAN", { 470.0f, 324.0f, 150.0f, 26.0f }, 21.0f, juce::Justification::centred, lcdInk, true, juce::Font::plain);
    drawLabel (g, juce::String (percent) + "%", { 470.0f, 348.0f, 150.0f, 32.0f }, 28.0f,
               juce::Justification::centred, lcdInk, true, juce::Font::plain);

    // petits indicateurs de niveau gauche / droite
    auto levelBar = [&] (float x0, float level)
    {
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float n = juce::jlimit (0.0f, 1.0f, (db + 40.0f) / 40.0f);

        for (int i = 0; i < 10; ++i)
        {
            g.setColour (i < (int) std::ceil (n * 10.0f) ? lcdInk : lcdInk.withAlpha (0.22f));
            g.fillRect (x0 + (float) i * 6.5f, 372.0f, 5.0f, 16.0f);
        }
    };
    levelBar (402.0f, panProcessor.levelL.load());
    levelBar (624.0f, panProcessor.levelR.load());
    g.setColour (lcdInk);
    g.fillRect (402.0f, 389.5f, 286.0f, 1.6f);

    // ---- panneau de droite : liste des modes
    drawLabel (g, "MODE", { 722.0f, 134.0f, 120.0f, 24.0f }, 19.0f, juce::Justification::centredLeft, lcdInk, true, juce::Font::plain);

    for (int i = 0; i < 4; ++i)
    {
        const juce::Rectangle<float> row (722.0f, 160.0f + 25.0f * (float) i, 182.0f, 25.0f);

        if (i == mode)
        {
            g.setColour (lcdInk);
            g.fillRect (row);

            juce::Path arrow;
            arrow.addTriangle (730.0f, row.getCentreY() - 8.0f, 730.0f, row.getCentreY() + 8.0f,
                               744.0f, row.getCentreY());
            g.setColour (lcdBgTop);
            g.fillPath (arrow);
        }

        drawLabel (g, modeNames[i], row.withTrimmedLeft (36.0f), 20.0f, juce::Justification::centredLeft,
                   i == mode ? lcdBgTop : lcdInk, true, i == mode ? juce::Font::bold : juce::Font::plain);
    }

    g.setColour (lcdInk);
    g.fillRect (722.0f, 266.0f, 182.0f, 1.6f);

    // rate
    drawLabel (g, "RATE", { 722.0f, 272.0f, 120.0f, 24.0f }, 19.0f, juce::Justification::centredLeft, lcdInk, true, juce::Font::plain);

    juce::String rateText;
    if (mode == 1)        rateText = "---";
    else if (mode == 2)   rateText = juce::String (rawParam ("rate"), rawParam ("rate") < 1.0f ? 2 : 1) + "Hz";
    else                  rateText = divisionText ((int) rawParam ("division"));

    drawLabel (g, rateText, { 752.0f, 298.0f, 150.0f, 30.0f }, 24.0f, juce::Justification::centredLeft, lcdInk, true, juce::Font::plain);

    g.setColour (lcdInk);
    g.fillRect (722.0f, 345.0f, 182.0f, 1.6f);

    // width
    drawLabel (g, "WIDTH", { 722.0f, 352.0f, 110.0f, 24.0f }, 19.0f, juce::Justification::centredLeft, lcdInk, true, juce::Font::plain);
    drawLabel (g, juce::String (juce::roundToInt (rawParam ("width"))) + "%", { 804.0f, 352.0f, 100.0f, 24.0f }, 19.0f,
               juce::Justification::centredRight, lcdInk, true, juce::Font::plain);

    g.setColour (lcdInk);
    g.drawRect (722.0f, 380.0f, 182.0f, 18.0f, 1.8f);
    const float fillN = juce::jlimit (0.0f, 1.0f, rawParam ("width") / 200.0f);
    for (int i = 0; i < 36; ++i)
    {
        g.setColour (i < (int) std::ceil (fillN * 36.0f) ? lcdInk : lcdInk.withAlpha (0.20f));
        g.fillRect (725.0f + (float) i * 4.95f, 383.0f, 3.2f, 12.0f);
    }

    // ---- ombres interieures et vignette pour donner du relief a la dalle
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, lcdRect.getCentreX(), lcdRect.getCentreY(),
                                             juce::Colours::black.withAlpha (0.20f), lcdRect.getX(), lcdRect.getY(), true));
    g.fillRect (lcdRect);

    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.30f), 0.0f, lcdRect.getBottom() - 16.0f,
                                             juce::Colours::transparentBlack, 0.0f, lcdRect.getBottom() - 40.0f, false));
    g.fillRect (lcdRect.getX(), lcdRect.getBottom() - 16.0f, lcdRect.getWidth(), 16.0f);
}
