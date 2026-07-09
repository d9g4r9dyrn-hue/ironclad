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
        // top_accent_slashes_2x.png is NOT drawn: it bakes "DISTORTRON" under the
        // slashes, and outer_frame_top already supplies the red slashes anyway.
        { "Textures/Chrome/chrome_left_bevel_2x.png",           96,   15,  70, 825 },
        // Clean brushed tiles, NOT the *_panel_*_2x mockups — those bake in
        // static knobs / labels / value text that would fight the live controls.
        { "Textures/Red_Brushed_Aluminum/red_brushed_tile_512.png",  140, 40, 385, 780 },
        { "Textures/Red_Brushed_Aluminum/red_brushed_tile_512.png", 1240, 40, 380, 775 },
        { "Textures/Chrome/chrome_center_bezel_2x.png",        510,   95, 730, 590 },
        { "Screen/bezel_frame_2x.png",                         500,   95, 745, 595 },
        // Screen/display_panel_full_2x.png is deliberately NOT drawn: it is a
        // static mockup of the whole interface (spectrum, mode tabs, oversampling
        // row, toolbar) that nothing could click. The screen is now drawn live in
        // paintScreen() with real, interactive controls composited on top.
        { "Textures/Black_Metal/bottom_trim_2x.png",           470,  680, 840, 180 },
        { "Textures/Lighting/bottom_red_glow_2x.png",          455,  760, 860,  55 },
        // Switches are drawn in code (ic::ToggleSwitch); the *_switch_full
        // mockups bake in a static handle position, like the slider markers did.
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

    // --- preset category dropdown ----------------------------------------
    categoryBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff141416));
    categoryBox.setColour(juce::ComboBox::textColourId,       ic::Colours::glowRed);
    categoryBox.setColour(juce::ComboBox::arrowColourId,      ic::Colours::muted);
    categoryBox.setColour(juce::ComboBox::outlineColourId,    juce::Colours::black.withAlpha(0.6f));
    categoryBox.setJustificationType(juce::Justification::centred);
    {
        const auto cats = proc.getPresetCategories();
        for (int i = 0; i < cats.size(); ++i) categoryBox.addItem(cats[i], i + 1);
    }
    addAndMakeVisible(categoryBox);
    categoryBox.onChange = [this]
    {
        const auto cat = categoryBox.getText();
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            if (proc.getProgramCategory(i) == cat) { proc.setCurrentProgram(i); break; }
        repaint();
    };
    syncCategoryBox();
    startTimerHz(12);

    // Oversampling selector: transparent hit-zones over the drawn OFF/2X/4X/8X
    // pills; each sets the OVERSAMPLE choice parameter.
    for (int i = 0; i < (int) osButtons.size(); ++i)
    {
        auto& b = osButtons[(size_t) i];
        b.setButtonText({});
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(b);
        b.onClick = [this, i]
        {
            if (auto* p = proc.apvts.getParameter("OVERSAMPLE"))
                p->setValueNotifyingHost(p->convertTo0to1((float) i));
            repaint();
        };
    }

    // --- on-screen AMP FEEL controls -------------------------------------
    for (auto* k : { &dynKnob, &fbKnob })
    {
        k->setCore(assets.get("Knobs/output_knob_core_2x.png"));
        k->onValueChange = [this] { repaint(); };
        addAndMakeVisible(*k);
    }
    dynAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "DYNAMICS", dynKnob);
    fbAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "FEEDBACK", fbKnob);

    for (int i = 0; i < (int) harmButtons.size(); ++i)
    {
        auto& b = harmButtons[(size_t) i];
        b.setButtonText({});
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(b);
        b.onClick = [this, i]
        {
            if (auto* p = proc.apvts.getParameter("FBHARMONIC"))
                p->setValueNotifyingHost(p->convertTo0to1((float) i));
            repaint();
        };
    }

    // --- LCD page tabs ---------------------------------------------------
    for (int i = 0; i < (int) pageTabs.size(); ++i)
    {
        auto& b = pageTabs[(size_t) i];
        b.setButtonText({});
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(b);
        b.onClick = [this, i] { activePage = i; updatePageVisibility(); };
    }

    // --- DELAY page controls ---------------------------------------------
    setupSKnob(dlyKnobs[0], "DLY_TIME", "TIME",  { 604, 380, 76, 76 });
    setupSKnob(dlyKnobs[1], "DLY_FB",   "FBACK", { 744, 380, 76, 76 });
    setupSKnob(dlyKnobs[2], "DLY_TONE", "TONE",  { 884, 380, 76, 76 });
    setupSKnob(dlyKnobs[3], "DLY_MIX",  "MIX",   { 1024, 380, 76, 76 });
    setupPillToggle(dlyToggle[0], "DLY_ON");
    setupPillToggle(dlyToggle[1], "DLY_SYNC");
    setupPillToggle(dlyToggle[2], "DLY_PING");
    for (int i = 0; i < 5; ++i) setupPillChoice(dlyDivBtn[(size_t) i], "DLY_DIV", i);

    // --- REVERB page controls --------------------------------------------
    setupSKnob(rvbKnobs[0], "RVB_SIZE", "SIZE", { 686, 380, 76, 76 });
    setupSKnob(rvbKnobs[1], "RVB_DAMP", "DAMP", { 846, 380, 76, 76 });
    setupSKnob(rvbKnobs[2], "RVB_MIX",  "MIX",  { 1006, 380, 76, 76 });
    setupPillToggle(rvbOnBtn, "RVB_ON");
    for (int i = 0; i < 4; ++i) setupPillChoice(rvbModeBtn[(size_t) i], "RVB_MODE", i);

    // --- CHORUS page controls --------------------------------------------
    setupSKnob(choKnobs[0], "CHO_RATE",  "RATE",  { 686, 380, 76, 76 });
    setupSKnob(choKnobs[1], "CHO_DEPTH", "DEPTH", { 846, 380, 76, 76 });
    setupSKnob(choKnobs[2], "CHO_MIX",   "MIX",   { 1006, 380, 76, 76 });
    setupPillToggle(choOnBtn, "CHO_ON");

    // --- COMP page controls (2 rows of 3) --------------------------------
    setupSKnob(cmpKnobs[0], "CMP_THRESH",  "THRESH",  { 674, 312, 76, 76 });
    setupSKnob(cmpKnobs[1], "CMP_RATIO",   "RATIO",   { 834, 312, 76, 76 });
    setupSKnob(cmpKnobs[2], "CMP_MAKEUP",  "MAKEUP",  { 994, 312, 76, 76 });
    setupSKnob(cmpKnobs[3], "CMP_ATTACK",  "ATTACK",  { 674, 462, 76, 76 });
    setupSKnob(cmpKnobs[4], "CMP_RELEASE", "RELEASE", { 834, 462, 76, 76 });
    setupSKnob(cmpKnobs[5], "CMP_MIX",     "MIX",     { 994, 462, 76, 76 });
    setupPillToggle(cmpOnBtn, "CMP_ON");

    // --- CAB page controls (pickup / cab type / IR) ----------------------
    for (int i = 0; i < 4; ++i) setupPillChoice(pickupBtn[(size_t) i], "PICKUP", i);
    for (int i = 0; i < 4; ++i) setupPillChoice(cabBtn[(size_t) i], "CAB", i);
    setupPillToggle(irOnBtn, "IR_ON");
    setupSKnob(puLoadKnob, "PU_LOAD", "LOAD", { 1052, 236, 72, 72 });
    for (auto* b : { &irLoadBtn, &irClearBtn })
    {
        b->setButtonText({});
        b->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(*b);
    }
    irLoadBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser>("Load Cabinet IR",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.aiff;*.aif");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    proc.loadIR(f);
                    if (auto* p = proc.apvts.getParameter("IR_ON")) p->setValueNotifyingHost(1.0f);
                }
                repaint();
            });
    };
    irClearBtn.onClick = [this]
    {
        proc.clearIR();
        if (auto* p = proc.apvts.getParameter("IR_ON")) p->setValueNotifyingHost(0.0f);
        repaint();
    };

    updatePageVisibility();

    // --- toggle switches (code-drawn) ------------------------------------
    // All three share one centred column (x=1262,w=300 -> centre 1412, aligned
    // with the four faders above). CHARACTER sits at the top; TIGHT/LOOSE and
    // GATE drop into the panel space below the faders.
    setupSwitch(switches[0], characterSwitch,  "CHARACTER",
                { 1262, 158, 300, 46 }, "CHARACTER", { 1262, 132, 300, 22 });
    setupSwitch(switches[1], tightLooseSwitch, "TIGHTLOOSE",
                { 1262, 628, 300, 46 }, {}, {});
    setupSwitch(switches[2], gateSwitch,       "GATE",
                { 1262, 728, 300, 46 }, "GATE", { 1262, 702, 300, 22 });

    // Deliberately NO setFixedAspectRatio(): pairing a fixed-aspect constrainer
    // with a resizable window makes FL Studio's wrapper window and JUCE's
    // constrainer negotiate the size back and forth forever, freezing FL's
    // message thread the instant the editor frame appears (AppHangB1). Making
    // the sizes exact 2:1 multiples did NOT stop it. The ic::Viewport already
    // letterboxes the artwork (uniform scale + centred black bars) at any size,
    // so we let the host choose the size freely and just clamp the range.
    setResizable(true, true);
    setResizeLimits(886, 443, 1774, 887);
    setSize(1330, 665); // 0.75x of the canonical artwork, exact 2:1
}

IroncladEditor::~IroncladEditor() { stopTimer(); }

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

void IroncladEditor::setupSwitch(Switch& s, ic::ToggleSwitch& ctrl,
                                 const juce::String& paramID,
                                 const juce::Rectangle<float>& box,
                                 const juce::String& title,
                                 const juce::Rectangle<float>& titleRect)
{
    s.ctrl      = &ctrl;
    s.box       = box;
    s.title     = title;
    s.titleRect = titleRect;

    addAndMakeVisible(ctrl);
    s.att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, paramID, ctrl);
    ctrl.onStateChange = [this] { repaint(); };
}

void IroncladEditor::setupSKnob(SKnob& s, const juce::String& paramID,
                                const juce::String& label, juce::Rectangle<float> box)
{
    s.paramID   = paramID;
    s.label     = label;
    s.box       = box;
    s.labelRect = { box.getX() - 22.0f, box.getY() - 20.0f,     box.getWidth() + 44.0f, 15.0f };
    s.valueRect = { box.getX() - 22.0f, box.getBottom() + 3.0f, box.getWidth() + 44.0f, 14.0f };
    s.knob.setCore(assets.get("Knobs/output_knob_core_2x.png"));
    s.knob.onValueChange = [this] { repaint(); };
    addAndMakeVisible(s.knob);
    s.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, paramID, s.knob);
}

void IroncladEditor::setupPillToggle(juce::TextButton& b, const juce::String& boolParamID)
{
    b.setButtonText({});
    b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    b.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(b);
    b.onClick = [this, boolParamID]
    {
        if (auto* p = proc.apvts.getParameter(boolParamID))
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
        repaint();
    };
}

void IroncladEditor::setupPillChoice(juce::TextButton& b, const juce::String& choiceParamID, int index)
{
    b.setButtonText({});
    b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    b.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(b);
    b.onClick = [this, choiceParamID, index]
    {
        if (auto* p = proc.apvts.getParameter(choiceParamID))
            p->setValueNotifyingHost(p->convertTo0to1((float) index));
        repaint();
    };
}

// Show only the active page's LCD controls (physical panel knobs stay visible).
void IroncladEditor::updatePageVisibility()
{
    const bool amp = (activePage == 0), dly = (activePage == 1), rvb = (activePage == 2),
               cho = (activePage == 3), cmp = (activePage == 4), cab = (activePage == 5);

    typePrev.setVisible(amp); typeNext.setVisible(amp);
    for (auto& b : osButtons) b.setVisible(amp);
    dynKnob.setVisible(amp); fbKnob.setVisible(amp);
    for (auto& b : harmButtons) b.setVisible(amp);

    for (auto& k : dlyKnobs) k.knob.setVisible(dly);
    for (auto& b : dlyToggle) b.setVisible(dly);
    for (auto& b : dlyDivBtn) b.setVisible(dly);

    for (auto& k : rvbKnobs) k.knob.setVisible(rvb);
    rvbOnBtn.setVisible(rvb);
    for (auto& b : rvbModeBtn) b.setVisible(rvb);

    for (auto& k : choKnobs) k.knob.setVisible(cho);
    choOnBtn.setVisible(cho);

    for (auto& k : cmpKnobs) k.knob.setVisible(cmp);
    cmpOnBtn.setVisible(cmp);

    for (auto& b : pickupBtn) b.setVisible(cab);
    for (auto& b : cabBtn)    b.setVisible(cab);
    irOnBtn.setVisible(cab);
    irLoadBtn.setVisible(cab);
    irClearBtn.setVisible(cab);
    puLoadKnob.knob.setVisible(cab);

    repaint();
}

juce::String IroncladEditor::typeName() const
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("TYPE")))
        return c->getCurrentChoiceName();
    return {};
}

// Step through presets WITHIN the current category (the dropdown picks category).
void IroncladEditor::cyclePreset(int direction)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    const auto cat = proc.getProgramCategory(proc.getCurrentProgram());
    juce::Array<int> inCat;
    for (int i = 0; i < n; ++i) if (proc.getProgramCategory(i) == cat) inCat.add(i);
    if (inCat.isEmpty()) return;
    int pos = inCat.indexOf(proc.getCurrentProgram());
    if (pos < 0) pos = 0;
    proc.setCurrentProgram(inCat[(pos + direction + inCat.size()) % inCat.size()]);
    syncCategoryBox();
    repaint();
}

void IroncladEditor::syncCategoryBox()
{
    const auto cats = proc.getPresetCategories();
    const int idx = cats.indexOf(proc.getProgramCategory(proc.getCurrentProgram()));
    if (idx >= 0 && categoryBox.getSelectedId() != idx + 1)
        categoryBox.setSelectedId(idx + 1, juce::dontSendNotification);
}

void IroncladEditor::timerCallback()
{
    syncCategoryBox();
    repaint();   // keep preset name + live FX readouts fresh on host/automation changes
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
    paintScreen(g);
    paintLabels(g);
    paintWordmark(g);
    paintVersion(g);
}

// Build stamp, bottom-right in real screen pixels (not view.map, so it never
// gets hidden by the letterbox bars). Version + compile timestamp: the timestamp
// changes on EVERY build, so what's on screen is unambiguous proof of which
// binary the host actually loaded. See deploy.ps1 for why that ever mattered.
void IroncladEditor::paintVersion(juce::Graphics& g)
{
    const juce::String stamp = "v" JucePlugin_VersionString
                               "  \xc2\xb7  " __DATE__ " " __TIME__;
    g.setColour(ic::Colours::muted.withAlpha(0.55f));
    g.setFont(juce::Font(juce::jmax(9.0f, 11.0f * view.scale)));
    g.drawText(stamp, getLocalBounds().reduced(8).removeFromBottom(16),
               juce::Justification::bottomRight, false);
}

// The package artwork was branded "DISTORTRON"; that text is now cleaned out of
// bottom_trim_2x.png and the real name is drawn live on the bottom bezel plate,
// matching images/ironclad_background.png (the goal look: no top wordmark, a
// chrome "IRONCLAD" on the bottom plate). Wide tracking echoes the extended
// techno logo face in the package typography spec.
void IroncladEditor::paintWordmark(juce::Graphics& g)
{
    const juce::Rectangle<float> src { 700.0f, 735.0f, 366.0f, 54.0f }; // bottom bezel plate
    const auto r = view.map(src);

    auto f = juce::Font(30.0f * view.scale, juce::Font::bold)
                 .withExtraKerningFactor(0.42f)
                 .withHorizontalScale(1.06f);
    g.setFont(f);

    const juce::String name("IRONCLAD");

    // Embossed chrome: dark drop shadow, then a light silver face.
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawText(name, r.translated(0, juce::jmax(1, (int) (2.0f * view.scale))),
               juce::Justification::centred, false);
    g.setColour(juce::Colour(0xffdedee3));
    g.drawText(name, r, juce::Justification::centred, false);
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

// The whole LCD is drawn live here (the static mockup panel is no longer used):
// an empty screen background, then real, state-reflecting content — preset and
// distortion-mode selectors, a live transfer curve, the drive readout, and the
// interactive OVERSAMPLING pills.
void IroncladEditor::paintScreen(juce::Graphics& g)
{
    const juce::Rectangle<float> screenSrc { 520, 120, 704, 560 };
    const auto screen = view.map(screenSrc).toFloat();

    // --- LCD background ---------------------------------------------------
    g.setColour(ic::Colours::screenBg);
    g.fillRoundedRectangle(screen, 10.0f * view.scale);
    juce::ColourGradient vign(juce::Colours::black.withAlpha(0.0f), screen.getCentre(),
                              juce::Colours::black.withAlpha(0.55f), screen.getTopLeft(), true);
    g.setGradientFill(vign);
    g.fillRoundedRectangle(screen, 10.0f * view.scale);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawRoundedRectangle(screen.reduced(0.5f), 10.0f * view.scale, juce::jmax(1.0f, 1.5f * view.scale));

    auto text = [&](juce::Rectangle<float> src, const juce::String& s, float pt,
                    juce::Colour col, juce::Justification just = juce::Justification::centred)
    {
        g.setColour(col);
        g.setFont(juce::Font(pt * view.scale, juce::Font::bold));
        g.drawText(s, view.map(src), just, false);
    };
    auto arrow = [&](juce::Rectangle<float> src, bool left)
    {
        auto r = view.map(src).toFloat();
        juce::Path p;
        if (left) { p.addTriangle(r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getCentreY()); }
        else      { p.addTriangle(r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY()); }
        g.setColour(ic::Colours::glowRed);
        g.fillPath(p);
    };

    // --- PRESET selector (category dropdown is a child at the top) -------
    text({ 697, 156, 350, 30 }, proc.getProgramName(proc.getCurrentProgram()),
         24.0f, ic::Colours::glowRed);
    arrow({ 646, 158, 22, 26 }, true);   arrow({ 1074, 158, 22, 26 }, false);

    paintTabs(g);
    if (activePage != 0) { paintFxPage(g); return; }   // DELAY / REVERB / CHORUS pages

    // --- DISTORTION MODE selector (AMP page) -----------------------------
    text({ 697, 194, 350, 15 }, "DISTORTION MODE", 12.0f, ic::Colours::muted);
    text({ 697, 212, 350, 30 }, typeName().toUpperCase(), 22.0f, ic::Colours::glowRed);
    arrow({ 646, 216, 22, 24 }, true);   arrow({ 1076, 216, 22, 24 }, false);

    // --- live transfer curve ---------------------------------------------
    const juce::Rectangle<float> boxSrc { 627, 258, 490, 182 };
    const auto box = view.map(boxSrc).toFloat();
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(box.toNearestInt());

        // faint grid + zero axes
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        for (int i = 1; i < 6; ++i)
        {
            const float x = box.getX() + box.getWidth() * i / 6.0f;
            const float y = box.getY() + box.getHeight() * i / 6.0f;
            g.drawVerticalLine((int) x, box.getY(), box.getBottom());
            g.drawHorizontalLine((int) y, box.getX(), box.getRight());
        }
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawHorizontalLine((int) box.getCentreY(), box.getX(), box.getRight());
        g.drawVerticalLine((int) box.getCentreX(), box.getY(), box.getBottom());

        const int   type  = (int) proc.apvts.getRawParameterValue("TYPE")->load();
        const float drive = proc.apvts.getRawParameterValue("DRIVE")->load();
        const float gdisp = Waveshaper::driveGain(type, drive);

        constexpr int N = 128;
        float ys[N]; float maxA = 1.0e-4f;
        for (int i = 0; i < N; ++i)
        {
            const float xin = -1.0f + 2.0f * (float) i / (float) (N - 1);
            ys[i] = Waveshaper::shape(type, xin * gdisp);
            maxA  = juce::jmax(maxA, std::abs(ys[i]));
        }
        juce::Path curve;
        for (int i = 0; i < N; ++i)
        {
            const float px = box.getX() + box.getWidth() * (float) i / (float) (N - 1);
            const float py = box.getCentreY() - (ys[i] / maxA) * (box.getHeight() * 0.46f);
            if (i == 0) curve.startNewSubPath(px, py); else curve.lineTo(px, py);
        }
        g.setColour(ic::Colours::glowRed.withAlpha(0.85f));
        g.strokePath(curve, juce::PathStrokeType(juce::jmax(1.5f, 2.0f * view.scale)));
    }

    // --- big DRIVE readout (over the curve) ------------------------------
    const int drivePct = juce::roundToInt(proc.apvts.getRawParameterValue("DRIVE")->load() * 100.0f);
    {
        auto r = view.map({ 627, 300, 490, 90 });
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.setFont(juce::Font(64.0f * view.scale, juce::Font::bold));
        g.drawText(juce::String(drivePct) + "%", r.translated(0, (int)(2*view.scale)),
                   juce::Justification::centred, false);
        g.setColour(ic::Colours::glowRed);
        g.drawText(juce::String(drivePct) + "%", r, juce::Justification::centred, false);
    }
    text({ 627, 392, 490, 18 }, "DRIVE", 13.0f, ic::Colours::muted);

    // --- OVERSAMPLING pills (interactive) --------------------------------
    text({ 677, 452, 390, 15 }, "OVERSAMPLING", 12.0f, ic::Colours::muted);
    const int osIdx = (int) proc.apvts.getRawParameterValue("OVERSAMPLE")->load();
    const char* osLabels[4] = { "OFF", "2X", "4X", "8X" };
    for (int i = 0; i < 4; ++i)
    {
        auto r = view.map(osPillSrc(i)).toFloat();
        const bool sel = (i == osIdx);
        g.setColour(sel ? ic::Colours::glowRed.withAlpha(0.9f) : juce::Colour(0xff191a1e));
        g.fillRoundedRectangle(r, 5.0f * view.scale);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(r.reduced(0.5f), 5.0f * view.scale, 1.0f);
        g.setColour(sel ? juce::Colours::white : ic::Colours::muted);
        g.setFont(juce::Font(15.0f * view.scale, juce::Font::bold));
        g.drawText(osLabels[i], r, juce::Justification::centred, false);
    }

    // --- AMP FEEL: dynamics + feedback knobs (drawn as child components) and
    //     the feedback harmonic selector ---
    text({ 541, 528, 138, 15 }, "DYNAMICS", 12.0f, ic::Colours::muted);
    text({ 673, 528, 138, 15 }, "FEEDBACK", 12.0f, ic::Colours::muted);
    text({ 848, 528, 320, 15 }, "FEEDBACK VOICE", 12.0f, ic::Colours::muted);
    if (auto* pd = proc.apvts.getParameter("DYNAMICS"))
        text({ 541, 634, 138, 14 }, pd->getCurrentValueAsText(), 12.0f, ic::Colours::glowRed);
    if (auto* pf = proc.apvts.getParameter("FEEDBACK"))
        text({ 673, 634, 138, 14 }, pf->getCurrentValueAsText(), 12.0f, ic::Colours::glowRed);

    const int hIdx = (int) proc.apvts.getRawParameterValue("FBHARMONIC")->load();
    const char* hLabels[3] = { "UNI", "5TH", "OCT" };
    for (int i = 0; i < 3; ++i)
    {
        auto r = view.map(harmPillSrc(i)).toFloat();
        const bool sel = (i == hIdx);
        g.setColour(sel ? ic::Colours::glowRed.withAlpha(0.9f) : juce::Colour(0xff191a1e));
        g.fillRoundedRectangle(r, 5.0f * view.scale);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(r.reduced(0.5f), 5.0f * view.scale, 1.0f);
        g.setColour(sel ? juce::Colours::white : ic::Colours::muted);
        g.setFont(juce::Font(14.0f * view.scale, juce::Font::bold));
        g.drawText(hLabels[i], r, juce::Justification::centred, false);
    }
}

// Source-pixel rect for oversampling pill i (shared by paint + hit-zones).
juce::Rectangle<float> IroncladEditor::osPillSrc(int i) const
{
    return { 677.0f + (float) i * 100.0f, 470.0f, 90.0f, 44.0f };
}

juce::Rectangle<float> IroncladEditor::harmPillSrc(int i) const
{
    return { 862.0f + (float) i * 108.0f, 562.0f, 100.0f, 42.0f };
}

void IroncladEditor::drawPill(juce::Graphics& g, juce::Rectangle<float> src, const juce::String& t, bool on)
{
    auto r = view.map(src).toFloat();
    g.setColour(on ? ic::Colours::glowRed.withAlpha(0.9f) : juce::Colour(0xff191a1e));
    g.fillRoundedRectangle(r, 5.0f * view.scale);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(r.reduced(0.5f), 5.0f * view.scale, 1.0f);
    g.setColour(on ? juce::Colours::white : ic::Colours::muted);
    g.setFont(juce::Font(14.0f * view.scale, juce::Font::bold));
    g.drawText(t, r, juce::Justification::centred, false);
}

// Page tabs along the bottom of the LCD (AMP / DELAY / REVERB / CHORUS).
void IroncladEditor::paintTabs(juce::Graphics& g)
{
    const char* tn[6] = { "AMP", "DELAY", "REVERB", "CHORUS", "COMP", "CAB" };
    for (int i = 0; i < 6; ++i)
    {
        auto r = view.map(540.0f + (float) i * 110.0f, 650.0f, 104.0f, 28.0f).toFloat();
        const bool sel = (i == activePage);
        g.setColour(sel ? ic::Colours::glowRed.withAlpha(0.85f) : juce::Colour(0xff17181c));
        g.fillRoundedRectangle(r, 4.0f * view.scale);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawRoundedRectangle(r.reduced(0.5f), 4.0f * view.scale, 1.0f);
        g.setColour(sel ? juce::Colours::white : ic::Colours::muted);
        g.setFont(juce::Font(12.0f * view.scale, juce::Font::bold));
        g.drawText(tn[i], r, juce::Justification::centred, false);
    }
}

// DELAY / REVERB / CHORUS pages: toggles, selectors, and knob labels/values
// (the knobs themselves are child components shown/hidden per page).
void IroncladEditor::paintFxPage(juce::Graphics& g)
{
    auto text = [&](juce::Rectangle<float> src, const juce::String& s, float pt, juce::Colour col)
    { g.setColour(col); g.setFont(juce::Font(pt * view.scale, juce::Font::bold));
      g.drawText(s, view.map(src), juce::Justification::centred, false); };
    auto pv = [&](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
    auto knobText = [&](const auto& knobs)
    {
        for (const auto& k : knobs)
        {
            text(k.labelRect, k.label, 11.0f, ic::Colours::muted);
            if (auto* p = proc.apvts.getParameter(k.paramID))
                text(k.valueRect, p->getCurrentValueAsText(), 11.0f, ic::Colours::glowRed);
        }
    };

    if (activePage == 1)        // DELAY
    {
        text({ 697, 200, 350, 16 }, "DELAY", 13.0f, ic::Colours::label);
        drawPill(g, { 600, 236, 118, 40 }, "ON",        pv("DLY_ON")   > 0.5f);
        drawPill(g, { 730, 236, 118, 40 }, pv("DLY_SYNC") > 0.5f ? "SYNC" : "FREE", pv("DLY_SYNC") > 0.5f);
        drawPill(g, { 862, 236, 150, 40 }, "PING-PONG", pv("DLY_PING") > 0.5f);
        const int div = (int) pv("DLY_DIV");
        const char* dn[5] = { "1/4", "1/8.", "1/8", "1/8T", "1/16" };
        for (int i = 0; i < 5; ++i) drawPill(g, { 600.0f + (float) i * 104.0f, 292, 96, 38 }, dn[i], i == div);
        knobText(dlyKnobs);
    }
    else if (activePage == 2)   // REVERB
    {
        text({ 697, 200, 350, 16 }, "REVERB", 13.0f, ic::Colours::label);
        drawPill(g, { 600, 236, 118, 40 }, "ON", pv("RVB_ON") > 0.5f);
        const int md = (int) pv("RVB_MODE");
        const char* mn[4] = { "SPRING", "PLATE", "ROOM", "HALL" };
        for (int i = 0; i < 4; ++i) drawPill(g, { 740.0f + (float) i * 122.0f, 236, 114, 40 }, mn[i], i == md);
        knobText(rvbKnobs);
    }
    else if (activePage == 3)   // CHORUS
    {
        text({ 697, 200, 350, 16 }, "CHORUS", 13.0f, ic::Colours::label);
        drawPill(g, { 600, 236, 118, 40 }, "ON", pv("CHO_ON") > 0.5f);
        knobText(choKnobs);
    }
    else if (activePage == 4)   // COMP
    {
        text({ 697, 200, 350, 16 }, "COMPRESSOR", 13.0f, ic::Colours::label);
        drawPill(g, { 600, 236, 118, 40 }, "ON", pv("CMP_ON") > 0.5f);
        knobText(cmpKnobs);
    }
    else                        // CAB (pickup + cab type + IR)
    {
        text({ 697, 200, 350, 16 }, "CABINET  +  INPUT", 13.0f, ic::Colours::label);

        text({ 560, 224, 220, 13 }, "PICKUP", 11.0f, ic::Colours::muted);
        const int pu = (int) pv("PICKUP");
        const char* pn[4] = { "SINGLE", "HUMB", "ACTIVE", "BASS" };
        for (int i = 0; i < 4; ++i) drawPill(g, { 600.0f + (float) i * 112.0f, 242, 106, 38 }, pn[i], i == pu);
        text(puLoadKnob.labelRect, "LOAD", 11.0f, ic::Colours::muted);
        if (auto* p = proc.apvts.getParameter("PU_LOAD"))
            text(puLoadKnob.valueRect, p->getCurrentValueAsText(), 11.0f, ic::Colours::glowRed);

        text({ 560, 300, 220, 13 }, "CABINET", 11.0f, ic::Colours::muted);
        const int cb = (int) pv("CAB");
        const char* cn[4] = { "1x12", "2x12", "4x12B", "4x12M" };
        for (int i = 0; i < 4; ++i) drawPill(g, { 600.0f + (float) i * 112.0f, 318, 106, 38 }, cn[i], i == cb);

        text({ 560, 378, 220, 13 }, "CAB IR (overrides cab model)", 11.0f, ic::Colours::muted);
        drawPill(g, { 600, 396, 100, 38 }, "IR", pv("IR_ON") > 0.5f);
        drawPill(g, { 712, 396, 150, 38 }, "LOAD IR", false);
        drawPill(g, { 874, 396, 100, 38 }, "CLEAR", false);
        const juce::String irn = proc.hasIR() ? proc.getIRName() : juce::String("(no IR loaded)");
        text({ 600, 446, 470, 16 }, irn, 12.0f, proc.hasIR() ? ic::Colours::glowRed : ic::Colours::muted);
    }
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

    // Switch section titles (the switches draw their own SMOOTH/RAW etc.).
    for (const auto& s : switches)
        if (s.title.isNotEmpty())
            drawLabel(s.titleRect, s.title);
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

    categoryBox.setBounds(view.map(700, 124, 344, 26));

    // Transparent hit-zones sit over the red arrow glyphs drawn in paintScreen.
    presetPrev.setBounds(view.map(638, 152, 40, 40));
    presetNext.setBounds(view.map(1066, 152, 40, 40));
    typePrev.setBounds(view.map(638, 210, 38, 36));
    typeNext.setBounds(view.map(1068, 210, 38, 36));

    for (int i = 0; i < (int) osButtons.size(); ++i)
        osButtons[(size_t) i].setBounds(view.map(osPillSrc(i)));

    dynKnob.setBounds(view.map(572, 552, 76, 76));
    fbKnob.setBounds (view.map(704, 552, 76, 76));
    for (int i = 0; i < (int) harmButtons.size(); ++i)
        harmButtons[(size_t) i].setBounds(view.map(harmPillSrc(i)));

    // LCD page tabs + effect-page controls (hit-zones match paintFxPage rects).
    for (int i = 0; i < (int) pageTabs.size(); ++i)
        pageTabs[(size_t) i].setBounds(view.map(540 + i * 110, 650, 104, 28));

    for (auto& k : dlyKnobs) k.knob.setBounds(view.map(k.box));
    for (auto& k : rvbKnobs) k.knob.setBounds(view.map(k.box));
    for (auto& k : choKnobs) k.knob.setBounds(view.map(k.box));
    for (auto& k : cmpKnobs) k.knob.setBounds(view.map(k.box));

    dlyToggle[0].setBounds(view.map(600, 236, 118, 40));
    dlyToggle[1].setBounds(view.map(730, 236, 118, 40));
    dlyToggle[2].setBounds(view.map(862, 236, 150, 40));
    for (int i = 0; i < 5; ++i) dlyDivBtn[(size_t) i].setBounds(view.map(600 + i * 104, 292, 96, 38));

    rvbOnBtn.setBounds(view.map(600, 236, 118, 40));
    for (int i = 0; i < 4; ++i) rvbModeBtn[(size_t) i].setBounds(view.map(740 + i * 122, 236, 114, 40));

    choOnBtn.setBounds(view.map(600, 236, 118, 40));
    cmpOnBtn.setBounds(view.map(600, 236, 118, 40));

    for (int i = 0; i < 4; ++i) pickupBtn[(size_t) i].setBounds(view.map(600 + i * 112, 242, 106, 38));
    for (int i = 0; i < 4; ++i) cabBtn[(size_t) i].setBounds(view.map(600 + i * 112, 318, 106, 38));
    irOnBtn.setBounds(view.map(600, 396, 100, 38));
    irLoadBtn.setBounds(view.map(712, 396, 150, 38));
    irClearBtn.setBounds(view.map(874, 396, 100, 38));
    puLoadKnob.knob.setBounds(view.map(puLoadKnob.box));

    for (auto& s : switches)
        s.ctrl->setBounds(view.map(s.box));
}
