#include "PluginEditor.h"

namespace
{
    // Chrome layers, painted back-to-front. Each rectangle is in canonical
    // source-pixel coordinates taken straight from the UI package manifest.
    struct ChromeLayer { const char* path; float x, y, w, h; };

    const ChromeLayer kChrome[] = {
        { "Textures/Rubber/left_armor_grip_2x.png",              0,   20, 140, 830 },
        { "Textures/Rubber/right_armor_grip_2x.png",          1630,   20, 140, 830 },
        { "Textures/Lighting/left_vertical_led_2x.png",         40,  250,  60, 340 },
        { "Textures/Lighting/right_vertical_led_2x.png",      1670,  250,  60, 340 },
        { "Textures/Black_Metal/outer_frame_top_2x.png",       490,   12, 800, 115 },
        { "Textures/Mesh/top_hex_grille_2x.png",               476,   24, 800,  90 },
        { "Textures/Lighting/top_accent_slashes_2x.png",       760,   10, 250,  70 },
        { "Textures/Chrome/chrome_left_bevel_2x.png",           96,   15,  70, 825 },
        // Clean brushed tiles, NOT the *_panel_*_2x mockups — those bake in
        // static knobs / labels / value text that would fight the live controls.
        { "Textures/Red_Brushed_Aluminum/red_brushed_tile_512.png",  140, 40, 385, 780 },
        { "Textures/Red_Brushed_Aluminum/red_brushed_tile_512.png", 1240, 40, 380, 775 },
        { "Textures/Chrome/chrome_center_bezel_2x.png",        510,   95, 730, 590 },
        { "Screen/bezel_frame_2x.png",                         500,   95, 745, 595 },
        { "Screen/display_panel_full_2x.png",                  520,  120, 704, 560 },
        { "Textures/Black_Metal/bottom_trim_2x.png",           470,  680, 840, 180 },
        { "Textures/Lighting/bottom_red_glow_2x.png",          455,  760, 860,  55 },
        { "Switches/character_switch_full_2x.png",            1340,  160, 230,  45 },
        { "Switches/tight_loose_switch_full_2x.png",          1340,  585, 230,  45 },
        { "Switches/gate_switch_full_2x.png",                 1300,  640, 250,  50 },
    };
}

IroncladEditor::IroncladEditor(IroncladProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p)
{
    // --- knobs -----------------------------------------------------------
    setupKnob(knobs[0], "DRIVE",    "DRIVE",    242, 202);
    setupKnob(knobs[1], "TREBLE",   "TONE",     408, 202);
    setupKnob(knobs[2], "BASS",     "BASS",     242, 384);
    setupKnob(knobs[3], "MID",      "MID",      408, 384);
    setupKnob(knobs[4], "PRESENCE", "PRESENCE", 242, 592);
    setupKnob(knobs[5], "OUTPUT",   "OUTPUT",   408, 592);

    // --- vertical sliders ------------------------------------------------
    // LOW CUT -> TIGHT (pre-drive low-cut), HIGH CUT -> post-chain low-pass,
    // MIX -> dry/wet, LEVEL -> master output trim.
    setupFader(faders[0], "TIGHT",   "LOW CUT",  { 1238, 235, 84, 330 });
    setupFader(faders[1], "HIGHCUT", "HIGH CUT", { 1328, 235, 84, 330 });
    setupFader(faders[2], "MIX",     "MIX",      { 1418, 235, 84, 330 });
    setupFader(faders[3], "LEVEL",   "LEVEL",    { 1503, 235, 84, 330 });

    // --- on-screen steppers (transparent click zones over the baked arrows)
    for (auto* b : { &presetPrev, &presetNext, &typePrev, &typeNext })
    {
        b->setButtonText({});
        b->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(*b);
    }
    presetPrev.onClick = [this] { cyclePreset(-1); };
    presetNext.onClick = [this] { cyclePreset(+1); };
    typePrev.onClick   = [this] { cycleType(-1); };
    typeNext.onClick   = [this] { cycleType(+1); };

    // GATE: transparent toggle over the switch artwork; a red LED shows "on".
    gateButton.setClickingTogglesState(true);
    gateButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    gateButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(gateButton);
    gateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "GATE", gateButton);
    gateButton.onStateChange = [this] { repaint(); };

    setResizable(true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio((double) ic::kBaseW / (double) ic::kBaseH);
        setResizeLimits(887, 443, 1774, 887);
    }
    setSize(1331, 665); // 0.75x of the canonical artwork
}

IroncladEditor::~IroncladEditor() = default;

void IroncladEditor::setupKnob(Knob& k, const juce::String& paramID,
                               const juce::String& label, float cx, float cy)
{
    k.paramID = paramID;
    k.label   = label;
    k.centre  = { cx, cy };
    // Rows are pitched 182px apart over a 152px box, so label/value must tuck
    // into the ~30px gaps (just outside the tick ring) without a lower row's
    // label colliding with the upper row's value.
    k.labelRect = { cx - 76.0f, cy - 90.0f, 152.0f, 16.0f };
    k.valueRect = { cx - 76.0f, cy + 74.0f, 152.0f, 16.0f };

    // All six knobs are the same dial; share one evenly-cropped sprite so they
    // render identically (the per-knob sprites are cropped off-centre).
    k.slider.setCore(assets.get("Knobs/output_knob_core_2x.png"));
    k.slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible(k.slider);

    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, paramID, k.slider);
}

void IroncladEditor::setupFader(Fader& f, const juce::String& paramID,
                                const juce::String& label,
                                const juce::Rectangle<float>& box)
{
    f.paramID   = paramID;
    f.label     = label;
    f.box       = box;
    f.labelRect = { box.getX() - 8.0f, box.getY() - 26.0f,      box.getWidth() + 16.0f, 22.0f };
    f.valueRect = { box.getX() - 8.0f, box.getBottom() + 6.0f,  box.getWidth() + 16.0f, 22.0f };

    // Slider is drawn in code (see ic::ImageSlider); no sprite needed.
    addAndMakeVisible(f.slider);

    if (paramID.isNotEmpty())
    {
        f.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            proc.apvts, paramID, f.slider);
        f.slider.onValueChange = [this] { repaint(); };
    }
    else
    {
        // Inert placeholder: park the handle mid-track so it reads as "present".
        f.slider.setRange(0.0, 1.0, 0.001);
        f.slider.setValue(0.5, juce::dontSendNotification);
    }
}

juce::String IroncladEditor::typeName() const
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("TYPE")))
        return c->getCurrentChoiceName();
    return {};
}

void IroncladEditor::cyclePreset(int direction)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    proc.setCurrentProgram((proc.getCurrentProgram() + direction + n) % n);
    repaint();
}

void IroncladEditor::cycleType(int direction)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("TYPE")))
    {
        const int n = c->choices.size();
        const int idx = (c->getIndex() + direction + n) % n;
        c->setValueNotifyingHost(c->convertTo0to1((float) idx));
        repaint();
    }
}

// ---------------------------------------------------------------------------
//  Painting
// ---------------------------------------------------------------------------
void IroncladEditor::paint(juce::Graphics& g)
{
    view.update(getWidth(), getHeight());
    g.fillAll(juce::Colours::black);

    if (! assets.isValid())
    {
        g.setColour(ic::Colours::glowRed);
        g.setFont(24.0f);
        g.drawText("UI assets not found: images/Ironclad_UI_Implementation_Package",
                   getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    paintChrome(g);

    // GATE "on" indicator LED, drawn over the switch artwork.
    if (auto* gp = proc.apvts.getRawParameterValue("GATE"); gp && gp->load() > 0.5f)
    {
        const auto& led = assets.get("LEDs/small_red_dot_gate_2x.png");
        if (led.isValid())
            g.drawImage(led, view.map(1462, 662, 35, 35).toFloat(),
                        juce::RectanglePlacement::stretchToFit);
    }

    paintScreenOverlays(g);
    paintLabels(g);
}

void IroncladEditor::paintChrome(juce::Graphics& g)
{
    for (const auto& layer : kChrome)
    {
        const auto& img = assets.get(layer.path);
        if (! img.isValid())
            continue;

        const auto dest = view.map(layer.x, layer.y, layer.w, layer.h).toFloat();

        // Seamless textures are tiled (grain scaled with the UI) rather than
        // stretched, and given a little edge depth so the panel doesn't read
        // as a flat rectangle now that the baked mockup border is gone.
        if (juce::String(layer.path).contains("_tile_"))
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(dest.toNearestInt());

            g.setFillType(juce::FillType(img,
                juce::AffineTransform::scale(view.scale)
                    .translated(dest.getX(), dest.getY())));
            g.fillRect(dest);

            // inner vignette + hairline border for depth
            g.setColour(juce::Colours::black.withAlpha(0.28f));
            g.drawRect(dest, juce::jmax(1.0f, 3.0f * view.scale));
            juce::ColourGradient vg(juce::Colours::transparentBlack, dest.getCentre(),
                                    juce::Colours::black.withAlpha(0.30f), dest.getTopLeft(), true);
            g.setGradientFill(vg);
            g.fillRect(dest);
        }
        else
        {
            g.drawImage(img, dest, juce::RectanglePlacement::stretchToFit);
        }
    }
}

void IroncladEditor::paintScreenOverlays(juce::Graphics& g)
{
    // Cover the display's baked preset name / mode name and redraw them live,
    // so the on-screen steppers actually reflect plugin state.
    auto cover = [&](juce::Rectangle<float> src, const juce::String& text,
                     float pt, juce::Colour col)
    {
        auto r = view.map(src);
        g.setColour(ic::Colours::screenBg);
        g.fillRect(r);
        g.setColour(col);
        g.setFont(juce::Font(pt * view.scale, juce::Font::bold));
        g.drawText(text, r, juce::Justification::centred, false);
    };

    cover({ 700, 168, 350, 36 }, proc.getProgramName(proc.getCurrentProgram()),
          28.0f, ic::Colours::glowRed);
    cover({ 700, 250, 350, 30 }, typeName().toUpperCase(),
          22.0f, ic::Colours::glowRed);
}

void IroncladEditor::paintLabels(juce::Graphics& g)
{
    auto drawLabel = [&](const juce::Rectangle<float>& src, const juce::String& text)
    {
        g.setColour(ic::Colours::label);
        g.setFont(juce::Font(16.0f * view.scale, juce::Font::bold));
        g.drawText(text, view.map(src), juce::Justification::centred, false);
    };
    auto drawValue = [&](const juce::Rectangle<float>& src, const juce::String& text)
    {
        g.setColour(ic::Colours::glowRed);
        g.setFont(juce::Font(15.0f * view.scale, juce::Font::bold));
        g.drawText(text, view.map(src), juce::Justification::centred, false);
    };

    for (const auto& k : knobs)
    {
        drawLabel(k.labelRect, k.label);
        if (auto* prm = proc.apvts.getParameter(k.paramID))
            drawValue(k.valueRect, prm->getCurrentValueAsText());
    }

    for (const auto& f : faders)
    {
        drawLabel(f.labelRect, f.label);
        if (f.paramID.isNotEmpty())
            if (auto* prm = proc.apvts.getParameter(f.paramID))
                drawValue(f.valueRect, prm->getCurrentValueAsText());
    }
}

// ---------------------------------------------------------------------------
//  Layout
// ---------------------------------------------------------------------------
void IroncladEditor::resized()
{
    view.update(getWidth(), getHeight());

    for (auto& k : knobs)
        k.slider.setBounds(view.map(k.centre.x - 76.0f, k.centre.y - 76.0f, 152.0f, 152.0f));

    for (auto& f : faders)
        f.slider.setBounds(view.map(f.box));

    presetPrev.setBounds(view.map(655, 152, 55, 50));
    presetNext.setBounds(view.map(1035, 152, 55, 50));
    typePrev.setBounds(view.map(628, 228, 50, 48));
    typeNext.setBounds(view.map(1058, 228, 50, 48));

    gateButton.setBounds(view.map(1300, 640, 250, 50));
}
