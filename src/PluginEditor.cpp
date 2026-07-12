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

    // One row of the Guitar Type popup: the archetype's pickup icon + its name,
    // highlighted when hovered, with a red bar on the current selection.
    class GuitarMenuItem : public juce::PopupMenu::CustomComponent
    {
    public:
        GuitarMenuItem(juce::Image ic, juce::String nm, bool current)
            : juce::PopupMenu::CustomComponent(true), icon(std::move(ic)),
              name(std::move(nm)), isCurrent(current) {}

        void getIdealSize(int& w, int& h) override { w = 268; h = 46; }

        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds();
            if (isItemHighlighted()) g.fillAll(juce::Colour(0xff2a2a30));
            g.setColour(isCurrent ? ic::Colours::glowRed : juce::Colours::transparentBlack);
            g.fillRect(b.removeFromLeft(4));
            auto ia = b.removeFromLeft(50).reduced(6);
            if (icon.isValid())
                g.drawImage(icon, ia.toFloat(), juce::RectanglePlacement::centred);
            g.setColour(isCurrent ? ic::Colours::glowRed : ic::Colours::label);
            g.setFont(juce::Font(15.0f, juce::Font::bold));
            g.drawText(name, b.reduced(8, 0), juce::Justification::centredLeft, false);
        }

    private:
        juce::Image icon;
        juce::String name;
        bool isCurrent;
    };
}

IroncladEditor::IroncladEditor(IroncladProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p)
{
    // --- knobs -----------------------------------------------------------
    // Left panel: six tone/gain knobs in a 2x3 grid (mockup region map).
    setupKnob(knobs[0], "DRIVE",    "DRIVE",    218, 166);
    setupKnob(knobs[1], "TREBLE",   "TONE",     361, 166);
    setupKnob(knobs[2], "BASS",     "BASS",     218, 317);
    setupKnob(knobs[3], "MID",      "MID",      361, 317);
    setupKnob(knobs[4], "PRESENCE", "PRESENCE", 218, 462);
    setupKnob(knobs[5], "OUTPUT",   "OUTPUT",   361, 462);

    // --- vertical sliders (right panel) ----------------------------------
    // LOW CUT -> TIGHT (pre-drive low-cut), HIGH CUT -> post-chain low-pass,
    // MIX -> dry/wet, LEVEL -> master output trim.
    setupFader(faders[0], "TIGHT",   "LOW CUT",  { 1098, 222, 36, 209 });
    setupFader(faders[1], "HIGHCUT", "HIGH CUT", { 1176, 222, 36, 209 });
    setupFader(faders[2], "MIX",     "MIX",      { 1254, 222, 36, 209 });
    setupFader(faders[3], "LEVEL",   "LEVEL",    { 1332, 222, 36, 209 });

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
    startTimerHz(30);

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

    // --- LEFT panel tabs: PEDAL FX / GUITAR ------------------------------
    for (int i = 0; i < (int) leftTabs.size(); ++i)
    {
        auto& b = leftTabs[(size_t) i];
        b.setButtonText({});
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(b);
        b.onClick = [this, i] { leftTab = i; updateLeftTabVisibility(); };
    }

    // Flanger + Phaser mini-knobs (bottom strip of the Pedal FX tab).
    setupSKnob(flgKnobs[0], "FLG_RATE",  "RATE",  { 234, 700, 44, 44 });
    setupSKnob(flgKnobs[1], "FLG_DEPTH", "DEPTH", { 288, 700, 44, 44 });
    setupSKnob(flgKnobs[2], "FLG_FB",    "FBACK", { 342, 700, 44, 44 });
    setupSKnob(flgKnobs[3], "FLG_MIX",   "MIX",   { 396, 700, 44, 44 });
    setupSKnob(phsKnobs[0], "PHS_RATE",  "RATE",  { 234, 766, 44, 44 });
    setupSKnob(phsKnobs[1], "PHS_DEPTH", "DEPTH", { 288, 766, 44, 44 });
    setupSKnob(phsKnobs[2], "PHS_FB",    "FBACK", { 342, 766, 44, 44 });
    setupSKnob(phsKnobs[3], "PHS_MIX",   "MIX",   { 396, 766, 44, 44 });
    setupPillToggle(flgOnBtn, "FLG_ON");
    setupPillToggle(phsOnBtn, "PHS_ON");

    // Guitar tab: 4 knobs + the type dropdown (a transparent hit-zone that opens
    // an icon menu) + the reference photo (drawn in paintGuitarTab).
    setupKnob(gtrKnobs[0], "GTR_MODEL",  "MODEL",  242, 250);
    setupKnob(gtrKnobs[1], "GTR_OUTPUT", "OUTPUT", 408, 250);
    setupKnob(gtrKnobs[2], "GTR_BODY",   "BODY",   242, 430);
    setupKnob(gtrKnobs[3], "GTR_BRIGHT", "BRIGHT", 408, 430);

    guitarSelectBtn.setButtonText({});
    guitarSelectBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    guitarSelectBtn.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(guitarSelectBtn);
    guitarSelectBtn.onClick = [this] { showGuitarMenu(); };

    // --- centre-screen navigation (page icons) + FX sub-tabs + preset menu ---
    auto transparent = [this](juce::TextButton& b)
    {
        b.setButtonText({});
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(b);
    };
    for (int i = 0; i < (int) navBtns.size(); ++i)
    {
        transparent(navBtns[(size_t) i]);
        navBtns[(size_t) i].onClick = [this, i] { navPage = i; updateNavVisibility(); repaint(); };
    }
    for (int i = 0; i < (int) fxSelBtns.size(); ++i)
    {
        transparent(fxSelBtns[(size_t) i]);
        fxSelBtns[(size_t) i].onClick = [this, i] { fxSel = i; updateNavVisibility(); repaint(); };
    }
    transparent(presetMenuBtn);
    presetMenuBtn.onClick = [this] { showPresetMenu(); };

    // Slice the reference sheet into per-type icon + photo sub-images once. The
    // sheet has its white background keyed to transparent, so the photos composite
    // straight onto the dark guitar panel.
    guitarSheet = assets.get("guitar_types_2_cut.png");
    if (guitarSheet.isValid())
        for (int i = 0; i < 7; ++i)
        {
            gtrIcon[(size_t) i]  = guitarSheet.getClippedImage(gtrIconSrc(i));
            gtrPhoto[(size_t) i] = guitarSheet.getClippedImage(gtrPhotoSrc(i));
        }

    updateLeftTabVisibility();

    // --- toggle switches (code-drawn) ------------------------------------
    // All three share one centred column (x=1262,w=300 -> centre 1412, aligned
    // with the four faders above). CHARACTER sits at the top; TIGHT/LOOSE and
    // GATE drop into the panel space below the faders.
    setupSwitch(switches[0], characterSwitch,  "CHARACTER",
                { 1169, 128, 170, 26 }, "DISTORTION CHARACTER", { 1101, 96, 264, 20 });
    setupSwitch(switches[1], tightLooseSwitch, "TIGHTLOOSE",
                { 1169, 450, 170, 28 }, {}, {});
    setupSwitch(switches[2], gateSwitch,       "GATE",
                { 1204, 516, 128, 26 }, "GATE", { 1198, 494, 140, 20 });

    // Deliberately NO setFixedAspectRatio(): pairing a fixed-aspect constrainer
    // with a resizable window makes FL Studio's wrapper window and JUCE's
    // constrainer negotiate the size back and forth forever, freezing FL's
    // message thread the instant the editor frame appears (AppHangB1). Making
    // the sizes exact 2:1 multiples did NOT stop it. The ic::Viewport already
    // letterboxes the artwork (uniform scale + centred black bars) at any size,
    // so we let the host choose the size freely and just clamp the range.
    setResizable(true, true);
    // Match the mockup faceplate aspect (~2.667:1). No setFixedAspectRatio() -
    // that hangs FL Studio; the ic::Viewport letterboxes uniformly instead.
    setResizeLimits(768, 288, 2304, 864);
    setSize(1408, 528); // 0.917x of the 1536x576 design canvas
}

IroncladEditor::~IroncladEditor() { stopTimer(); }

void IroncladEditor::setupKnob(Knob& k, const juce::String& paramID,
                               const juce::String& label, float cx, float cy)
{
    k.paramID = paramID;
    k.label   = label;
    k.centre  = { cx, cy };
    // 96px knob: label sits above (mockup shows no persistent value beneath).
    k.labelRect = { cx - 60.0f, cy - 78.0f, 120.0f, 22.0f };
    k.valueRect = { cx - 60.0f, cy + 54.0f, 120.0f, 18.0f };

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
    const float cx = box.getCentreX();
    f.labelRect = { cx - 44.0f, box.getY() - 24.0f,     88.0f, 18.0f };
    f.valueRect = { cx - 44.0f, box.getBottom() + 4.0f, 88.0f, 16.0f };

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

// Show the controls for the active LEFT tab (amp knobs + flanger/phaser on Pedal
// FX; the guitar dropdown + knobs on Guitar). Labels are drawn per-tab in paint().
void IroncladEditor::updateLeftTabVisibility()
{
    const bool fx = (leftTab == 0), gtr = (leftTab == 1);

    for (auto& k : knobs) k.slider.setVisible(fx);      // the 6 amp knobs
    for (auto& k : flgKnobs) k.knob.setVisible(fx);
    for (auto& k : phsKnobs) k.knob.setVisible(fx);
    flgOnBtn.setVisible(fx);
    phsOnBtn.setVisible(fx);

    for (auto& k : gtrKnobs) k.slider.setVisible(gtr);
    guitarSelectBtn.setVisible(gtr);

    repaint();
}

juce::Rectangle<float> IroncladEditor::leftTabSrc(int i) const
{
    return { 170.0f + (float) i * 168.0f, 76.0f, 158.0f, 32.0f };
}

juce::Rectangle<float> IroncladEditor::navIconSrc(int i) const
{
    return { 542.0f + (float) i * 91.0f, 452.0f, 82.0f, 29.0f };
}
juce::Rectangle<float> IroncladEditor::fxTabSrc(int i) const
{
    return { 616.0f + (float) i * 44.0f, 192.0f, 41.0f, 22.0f };
}

// A full preset picker: categories as submenus, current preset ticked.
void IroncladEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto cats = proc.getPresetCategories();
    const int cur = proc.getCurrentProgram();
    for (const auto& cat : cats)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            if (proc.getProgramCategory(i) == cat)
                sub.addItem(i + 1, proc.getProgramName(i), true, i == cur);
        menu.addSubMenu(cat, sub);
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetMenuBtn),
        [this](int r) { if (r > 0) { proc.setCurrentProgram(r - 1); repaint(); } });
}

// Show + position the controls for the active centre-screen page; everything
// else is hidden. Called on resize and whenever the nav/FX selection changes.
void IroncladEditor::updateNavVisibility()
{
    hideSecondaryForRedesign();

    presetPrev.setVisible(true); presetNext.setVisible(true); presetMenuBtn.setVisible(true);
    for (auto& b : navBtns) b.setVisible(true);

    const bool dist = navPage == NavDistortion, fx = navPage == NavEffects,
               cab  = navPage == NavCabinet;

    // Distortion page: mode steppers + oversampling (drawn text in paintScreen).
    typePrev.setVisible(dist); typeNext.setVisible(dist);
    for (auto& b : osButtons) b.setVisible(dist);

    auto pill = [this](juce::TextButton& b, juce::Rectangle<float> r) { b.setBounds(view.map(r)); b.setVisible(true); };
    auto sk   = [this](SKnob& k, float cx, float cy, const juce::String& lbl)
    {
        const float sz = 54.0f;
        k.label = lbl;
        k.box = { cx - sz * 0.5f, cy - sz * 0.5f, sz, sz };
        k.labelRect = { cx - 42.0f, cy - sz * 0.5f - 15.0f, 84.0f, 13.0f };
        k.valueRect = { cx - 42.0f, cy + sz * 0.5f + 2.0f,  84.0f, 12.0f };
        k.knob.setBounds(view.map(k.box));
        k.knob.setVisible(true);
    };
    const float c3[3] = { 690.0f, 770.0f, 850.0f };
    const float c4[4] = { 656.0f, 728.0f, 800.0f, 872.0f };

    if (fx)
    {
        for (int i = 0; i < (int) fxSelBtns.size(); ++i) pill(fxSelBtns[(size_t) i], fxTabSrc(i));
        switch (fxSel)
        {
            case 0: // DYN (dynamics + feedback + voice)
                dynKnob.setBounds(view.map(672 - 27, 268 - 27, 54, 54)); dynKnob.setVisible(true);
                fbKnob.setBounds (view.map(792 - 27, 268 - 27, 54, 54)); fbKnob.setVisible(true);
                for (int i = 0; i < 3; ++i) pill(harmButtons[(size_t) i], { 668.0f + i * 60.0f, 348.0f, 56.0f, 24.0f });
                break;
            case 1: // CHORUS
                pill(choOnBtn, { 624, 232, 60, 26 });
                sk(choKnobs[0], c3[0], 320, "RATE"); sk(choKnobs[1], c3[1], 320, "DEPTH"); sk(choKnobs[2], c3[2], 320, "MIX");
                break;
            case 2: // FLANGER
                pill(flgOnBtn, { 624, 232, 60, 26 });
                sk(flgKnobs[0], c4[0], 320, "RATE"); sk(flgKnobs[1], c4[1], 320, "DEPTH");
                sk(flgKnobs[2], c4[2], 320, "FBACK"); sk(flgKnobs[3], c4[3], 320, "MIX");
                break;
            case 3: // PHASER
                pill(phsOnBtn, { 624, 232, 60, 26 });
                sk(phsKnobs[0], c4[0], 320, "RATE"); sk(phsKnobs[1], c4[1], 320, "DEPTH");
                sk(phsKnobs[2], c4[2], 320, "FBACK"); sk(phsKnobs[3], c4[3], 320, "MIX");
                break;
            case 4: // DELAY
                pill(dlyToggle[0], { 624, 230, 52, 24 }); pill(dlyToggle[1], { 682, 230, 58, 24 }); pill(dlyToggle[2], { 746, 230, 70, 24 });
                for (int i = 0; i < 5; ++i) pill(dlyDivBtn[(size_t) i], { 624.0f + i * 44.0f, 260.0f, 40.0f, 22.0f });
                sk(dlyKnobs[0], c4[0], 340, "TIME"); sk(dlyKnobs[1], c4[1], 340, "FBACK");
                sk(dlyKnobs[2], c4[2], 340, "TONE"); sk(dlyKnobs[3], c4[3], 340, "MIX");
                break;
            case 5: // REVERB
                pill(rvbOnBtn, { 624, 230, 52, 24 });
                for (int i = 0; i < 4; ++i) pill(rvbModeBtn[(size_t) i], { 686.0f + i * 58.0f, 230.0f, 54.0f, 24.0f });
                sk(rvbKnobs[0], c3[0], 330, "SIZE"); sk(rvbKnobs[1], c3[1], 330, "DAMP"); sk(rvbKnobs[2], c3[2], 330, "MIX");
                break;
            case 6: // COMP
                pill(cmpOnBtn, { 624, 230, 52, 24 });
                sk(cmpKnobs[0], c3[0], 300, "THRESH"); sk(cmpKnobs[1], c3[1], 300, "RATIO");  sk(cmpKnobs[2], c3[2], 300, "MAKEUP");
                sk(cmpKnobs[3], c3[0], 388, "ATTACK"); sk(cmpKnobs[4], c3[1], 388, "RELEASE"); sk(cmpKnobs[5], c3[2], 388, "MIX");
                break;
            default: break;
        }
    }
    else if (cab)
    {
        guitarSelectBtn.setBounds(view.map(620, 196, 300, 34)); guitarSelectBtn.setVisible(true);
        const float gc[4] = { 656.0f, 728.0f, 800.0f, 872.0f };
        auto gknob = [this](Knob& k, float cx, float cy, const juce::String& lbl)
        {
            k.label = lbl; k.centre = { cx, cy };
            k.labelRect = { cx - 42.0f, cy - 43.0f, 84.0f, 13.0f };
            k.valueRect = { cx - 42.0f, cy + 30.0f, 84.0f, 12.0f };
            k.slider.setBounds(view.map(cx - 27.0f, cy - 27.0f, 54.0f, 54.0f));
            k.slider.setVisible(true);
        };
        gknob(gtrKnobs[0], gc[0], 268, "MODEL");  gknob(gtrKnobs[1], gc[1], 268, "OUTPUT");
        gknob(gtrKnobs[2], gc[2], 268, "BODY");   gknob(gtrKnobs[3], gc[3], 268, "BRIGHT");
        for (int i = 0; i < 4; ++i) pill(pickupBtn[(size_t) i], { 620.0f + i * 74.0f, 330.0f, 68.0f, 22.0f });
        for (int i = 0; i < 4; ++i) pill(cabBtn[(size_t) i],    { 620.0f + i * 74.0f, 366.0f, 68.0f, 22.0f });
        pill(irOnBtn, { 620, 402, 52, 22 }); pill(irLoadBtn, { 680, 402, 80, 22 }); pill(irClearBtn, { 768, 402, 62, 22 });
    }
}

int IroncladEditor::currentGuitarType() const
{
    return (int) proc.apvts.getRawParameterValue("GUITARTYPE")->load();
}

// Sub-image rects inside the 1024x1536 reference sheet (7 evenly-spaced rows;
// icon column at left, guitar photo at right).
juce::Rectangle<int> IroncladEditor::gtrIconSrc(int i)
{
    const float rh = 1536.0f / 7.0f;
    const int   cy = (int) (rh * ((float) i + 0.5f));
    return { 18, cy - 66, 132, 132 };
}
juce::Rectangle<int> IroncladEditor::gtrPhotoSrc(int i)
{
    const float rh = 1536.0f / 7.0f;
    const int   cy = (int) (rh * ((float) i + 0.5f));
    return { 478, cy - 96, 542, 192 };
}

const char* IroncladEditor::guitarName(int i)
{
    static const char* n[7] = { "S-Type SC", "Single-Cut HB", "Modern HB",
                                "T-Type SC", "Semi-Hollow", "P-90", "Extended Range" };
    return n[juce::jlimit(0, 6, i)];
}
const char* IroncladEditor::guitarBlurb(int i)
{
    static const char* b[7] = {
        "Bright, glassy, dynamic. Clean, funk, blues.",
        "Thick, warm, sustaining. Rock, lead, heavy rhythm.",
        "Tight, focused, high output. Metal, technical.",
        "Twangy, dry, percussive. Country, indie, rhythm.",
        "Open, woody, resonant. Jazz, blues, feedback.",
        "Raw, mid-forward, dynamic. Garage, punk, classic rock.",
        "Deep, tight, aggressive. Low tunings, modern metal."
    };
    return b[juce::jlimit(0, 6, i)];
}

void IroncladEditor::showGuitarMenu()
{
    juce::PopupMenu m;
    const int cur = currentGuitarType();
    for (int i = 0; i < 7; ++i)
        m.addCustomItem(i + 1,
            std::make_unique<GuitarMenuItem>(gtrIcon[(size_t) i], guitarName(i), i == cur),
            nullptr, guitarName(i));

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(guitarSelectBtn),
        [this](int r)
        {
            if (r > 0)
                if (auto* p = proc.apvts.getParameter("GUITARTYPE"))
                    p->setValueNotifyingHost(p->convertTo0to1((float) (r - 1)));
            repaint();
        });
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

    // Meter ballistics: instant attack, ~30 dB/s decay (fast peak-follow, smooth fall).
    auto step = [](float& disp, float lin)
    {
        const float db = lin > 1.0e-5f ? juce::Decibels::gainToDecibels(lin) : -100.0f;
        disp = db > disp ? db : juce::jmax(-100.0f, disp - 2.4f);
    };
    step(inMeterDb,  proc.getInputPeak());
    step(outMeterDb, proc.getOutputPeak());

    repaint();   // keep preset name + live FX readouts + meters fresh
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

    paintChassis(g);
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
    const juce::String stamp = juce::String::fromUTF8("v" JucePlugin_VersionString
                               "  \xc2\xb7  " __DATE__ " " __TIME__);
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
    const juce::Rectangle<float> src { 614.0f, 500.0f, 307.0f, 48.0f }; // bottom logo plate
    const auto r = view.map(src);

    auto f = juce::Font(22.0f * view.scale, juce::Font::bold)
                 .withExtraKerningFactor(0.40f)
                 .withHorizontalScale(1.04f);
    g.setFont(f);

    const juce::String name("IRONCLAD");

    // Embossed chrome: dark drop shadow, then a light silver face.
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawText(name, r.translated(0, juce::jmax(1, (int) (2.0f * view.scale))),
               juce::Justification::centred, false);
    g.setColour(juce::Colour(0xffdedee3));
    g.drawText(name, r, juce::Justification::centred, false);
}

// Angled red-panel outline in design space (left; right is mirrored about x=768).
juce::Path IroncladEditor::makePanelPath(bool left) const
{
    juce::Path p;
    if (left)
    {
        p.startNewSubPath(126.0f,  94.0f);
        p.lineTo(171.0f,  47.0f);  p.lineTo(405.0f,  47.0f);  p.lineTo(477.0f, 116.0f);
        p.lineTo(477.0f, 470.0f);  p.lineTo(405.0f, 545.0f);  p.lineTo(171.0f, 545.0f);
        p.lineTo(126.0f, 498.0f);  p.closeSubPath();
    }
    else
    {
        const float m = 1536.0f;   // mirror
        p.startNewSubPath(m-126.0f,  94.0f);
        p.lineTo(m-171.0f,  47.0f);  p.lineTo(m-405.0f,  47.0f);  p.lineTo(m-477.0f, 116.0f);
        p.lineTo(m-477.0f, 470.0f);  p.lineTo(m-405.0f, 545.0f);  p.lineTo(m-171.0f, 545.0f);
        p.lineTo(m-126.0f, 498.0f);  p.closeSubPath();
    }
    return p;
}

// A red brushed-aluminium panel: texture clipped to the angled outline, edge
// darkening for depth, a dark red border and a chrome inner highlight.
void IroncladEditor::paintRedPanel(juce::Graphics& g, const juce::Path& designPath)
{
    const float s = view.scale;
    juce::Path p = designPath;
    p.applyTransform(view.getTransform());
    const auto b = p.getBounds();

    {
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(p);

        // Deep anodised-red base, then the brushed texture at low opacity so it
        // adds grain/brushing without lifting the whole panel to bright red.
        g.setColour(ic::Colours::brushedRed);
        g.fillRect(b);
        const auto& tex = assets.get("Textures/Red_Brushed_Aluminum/red_brushed_tile_512.png");
        if (tex.isValid())
        {
            g.setFillType(juce::FillType(tex,
                juce::AffineTransform::scale(s).translated(b.getX(), b.getY())));
            g.setOpacity(0.34f);
            g.fillRect(b);
            g.setOpacity(1.0f);
        }

        // Broad vertical luminance shaping: darker top/bottom, faint centre light.
        juce::ColourGradient lum(juce::Colours::black.withAlpha(0.34f), b.getCentreX(), b.getY(),
                                 juce::Colours::white.withAlpha(0.04f), b.getCentreX(), b.getCentreY(), false);
        lum.addColour(1.0, juce::Colours::black.withAlpha(0.44f));
        g.setGradientFill(lum);
        g.fillRect(b);
        // Edge vignette (radial) for a recessed, machined-metal feel.
        juce::ColourGradient vg(juce::Colours::transparentBlack, b.getCentre(),
                                juce::Colours::black.withAlpha(0.52f), b.getTopLeft(), true);
        g.setGradientFill(vg);
        g.fillRect(b);
    }

    // dark red border + chrome inner edge
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.strokePath(p, juce::PathStrokeType(juce::jmax(2.0f, 5.0f * s)));
    g.setColour(ic::Colours::darkRed.withAlpha(0.8f));
    g.strokePath(p, juce::PathStrokeType(juce::jmax(1.0f, 2.5f * s)));
    g.setColour(ic::Colours::chrome.withAlpha(0.30f));
    g.strokePath(p, juce::PathStrokeType(juce::jmax(1.0f, 1.2f * s)));
}

// The full hardware shell, built to the mockup proportions in 1536x576 space.
void IroncladEditor::paintChassis(juce::Graphics& g)
{
    const float s = view.scale;
    auto D = [&](float x, float y, float w, float h) { return view.map(x, y, w, h).toFloat(); };
    auto plate = [&](juce::Rectangle<float> r, juce::Colour c, float rad)
    {
        g.setColour(c); g.fillRoundedRectangle(r, rad * s);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(r.reduced(0.5f), rad * s, juce::jmax(1.0f, 1.5f * s));
    };

    // 1. Outer chassis: black-metal base with a gunmetal chrome edge.
    auto outer = D(27, 27, 1482, 530);
    juce::ColourGradient base(juce::Colour(0xff17171b), outer.getCentreX(), outer.getY(),
                              juce::Colour(0xff0a0a0c), outer.getCentreX(), outer.getBottom(), false);
    g.setGradientFill(base);
    g.fillRoundedRectangle(outer, 22.0f * s);
    g.setColour(ic::Colours::gunmetal);
    g.drawRoundedRectangle(outer.reduced(1.5f * s), 22.0f * s, juce::jmax(2.0f, 3.5f * s));
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(outer.reduced(7.0f * s), 18.0f * s, juce::jmax(1.0f, 2.0f * s));

    // 2. Side armour rails (reuse the rendered grip assets on the outer edges).
    if (const auto& la = assets.get("Textures/Rubber/left_armor_grip_2x.png"); la.isValid())
        g.drawImage(la, D(24, 60, 108, 460), juce::RectanglePlacement::stretchToFit);
    if (const auto& ra = assets.get("Textures/Rubber/right_armor_grip_2x.png"); ra.isValid())
        g.drawImage(ra, D(1404, 60, 108, 460), juce::RectanglePlacement::stretchToFit);

    // 3. Angled red panels.
    paintRedPanel(g, makePanelPath(true));
    paintRedPanel(g, makePanelPath(false));

    // 4. Top vent: hex mesh grille + red accent slashes.
    auto vent = D(482, 28, 573, 77);
    plate(vent, ic::Colours::blackMetal, 8.0f);
    if (const auto& mesh = assets.get("Textures/Mesh/top_hex_grille_2x.png"); mesh.isValid())
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(vent.reduced(4.0f * s).toNearestInt());
        g.drawImage(mesh, vent.reduced(4.0f * s), juce::RectanglePlacement::stretchToFit);
    }
    // four red accent bars centred in the vent
    for (int i = 0; i < 4; ++i)
    {
        auto bar = D(690.0f + (float) i * 40.0f, 44.0f, 24.0f, 44.0f);
        juce::ColourGradient rg(ic::Colours::glowRed, bar.getCentreX(), bar.getCentreY(),
                                ic::Colours::darkRed.withAlpha(0.4f), bar.getX(), bar.getY(), true);
        g.setGradientFill(rg);
        g.fillRoundedRectangle(bar, 3.0f * s);
    }

    // 5. Centre assembly enclosure (screen + meters sit inside; drawn in paintScreen).
    plate(D(478, 101, 578, 390), ic::Colours::darkGray, 14.0f);
    plate(D(600, 128, 336, 314), ic::Colours::blackMetal, 10.0f);   // recessed screen frame

    // 6. Bottom: two angled vents flanking the centred logo plate.
    if (const auto& mesh = assets.get("Textures/Mesh/top_hex_grille_2x.png"); mesh.isValid())
    {
        g.setOpacity(0.85f);
        g.drawImage(mesh, D(434, 497, 185, 54), juce::RectanglePlacement::stretchToFit);
        g.drawImage(mesh, D(916, 497, 185, 54), juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
    }
    plate(D(614, 493, 307, 62), ic::Colours::blackMetal, 10.0f);
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
    const float s = view.scale;
    auto text = [&](juce::Rectangle<float> src, const juce::String& str, float pt,
                    juce::Colour col, juce::Justification just = juce::Justification::centred,
                    bool bold = true)
    {
        g.setColour(col);
        g.setFont(juce::Font(pt * s, bold ? juce::Font::bold : juce::Font::plain));
        g.drawText(str, view.map(src), just, false);
    };
    auto arrow = [&](juce::Rectangle<float> src, bool left)
    {
        auto r = view.map(src).toFloat();
        juce::Path p;
        if (left) p.addTriangle(r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getCentreY());
        else      p.addTriangle(r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
        g.setColour(ic::Colours::glowRed);
        g.fillPath(p);
    };

    // --- screen glass -----------------------------------------------------
    auto screen = view.map(609, 134, 318, 302).toFloat();
    g.setColour(ic::Colours::screenBg);
    g.fillRoundedRectangle(screen, 8.0f * s);
    juce::ColourGradient vign(juce::Colours::black.withAlpha(0.0f), screen.getCentre(),
                              juce::Colours::black.withAlpha(0.5f), screen.getTopLeft(), true);
    g.setGradientFill(vign);
    g.fillRoundedRectangle(screen, 8.0f * s);
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawRoundedRectangle(screen.reduced(0.5f), 8.0f * s, juce::jmax(1.0f, 1.5f * s));

    // --- input / output meters (live; ballistics in timerCallback) ---------
    auto meter = [&](juce::Rectangle<float> box, float db)
    {
        auto r = view.map(box).toFloat();
        g.setColour(ic::Colours::blackMetal); g.fillRoundedRectangle(r, 3.0f * s);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(r, 3.0f * s, 1.0f);
        auto inner = r.reduced(2.0f * s);
        const int seg = 22;
        const float segH = inner.getHeight() / (float) seg;
        for (int i = 0; i < seg; ++i)
        {
            const float segDb = juce::jmap((float) i / (float) (seg - 1), 0.0f, 1.0f, -60.0f, 6.0f);
            const bool  lit   = segDb <= db;
            const juce::Colour c = segDb > 0.0f  ? ic::Colours::glowRed
                                 : segDb > -12.0f ? juce::Colour(0xffd8a020)
                                                  : juce::Colour(0xff30a030);
            juce::Rectangle<float> sr(inner.getX(), inner.getBottom() - (i + 1) * segH + 0.8f * s,
                                      inner.getWidth(), segH - 1.2f * s);
            g.setColour(lit ? c : c.withAlpha(0.11f));
            g.fillRect(sr);
        }
    };
    meter({ 530, 192, 29, 198 }, inMeterDb);
    meter({ 976, 192, 29, 198 }, outMeterDb);
    text({ 520, 172, 50, 14 }, "IN",  10.0f, ic::Colours::muted);
    text({ 968, 172, 50, 14 }, "OUT", 10.0f, ic::Colours::muted);
    auto dbTxt = [](float db) { return db <= -59.0f ? juce::String::fromUTF8("-\xe2\x88\x9e") : juce::String(db, 1); };
    text({ 512, 392, 66, 12 }, dbTxt(inMeterDb),  9.0f, ic::Colours::muted);
    text({ 960, 392, 66, 12 }, dbTxt(outMeterDb), 9.0f, ic::Colours::muted);

    // --- preset row -------------------------------------------------------
    text({ 625, 140, 286, 14 }, "PRESET", 11.0f, ic::Colours::muted);
    text({ 655, 152, 226, 26 }, proc.getProgramName(proc.getCurrentProgram()).toUpperCase(),
         21.0f, ic::Colours::glowRed);
    arrow({ 630, 158, 16, 24 }, true);   arrow({ 862, 158, 16, 24 }, false);
    // menu glyph (far right of preset row)
    {
        auto m = view.map(892, 158, 20, 22).toFloat();
        g.setColour(ic::Colours::muted);
        for (int i = 0; i < 3; ++i)
            g.fillRect(m.getX(), m.getY() + i * 8.0f * s, m.getWidth(), juce::jmax(1.0f, 2.0f * s));
    }

    // --- page content (Distortion / Effects / Analysis / Cabinet / Settings) ---
    paintNavContent(g);

    // --- bottom navigation bar (5 icon slots) -----------------------------
    auto nav = view.map(530, 447, 477, 39).toFloat();
    g.setColour(ic::Colours::blackMetal); g.fillRoundedRectangle(nav, 6.0f * s);
    g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(nav, 6.0f * s, 1.0f);
    for (int i = 0; i < 5; ++i)
    {
        auto slot = view.map(navIconSrc(i)).toFloat();
        const bool sel = (i == navPage);
        if (sel) { g.setColour(ic::Colours::glowRed.withAlpha(0.18f)); g.fillRoundedRectangle(slot, 4.0f * s); }
        g.setColour(sel ? ic::Colours::glowRed : ic::Colours::muted);
        // simple glyph per page: waveform / sliders / curve / cab / gear
        auto c = slot.getCentre(); const float u = 8.0f * s;
        juce::Path gp;
        if (i == 0) { gp.startNewSubPath(c.x-u,c.y); gp.lineTo(c.x-u*0.4f,c.y-u); gp.lineTo(c.x+u*0.2f,c.y+u); gp.lineTo(c.x+u,c.y); }
        else if (i == 1) { for (int k=-1;k<=1;++k){ gp.addRectangle(c.x+k*u*0.7f-1, c.y-u, 2.0f, 2*u); } }
        else if (i == 2) { gp.startNewSubPath(c.x-u,c.y+u); gp.quadraticTo(c.x,c.y-u*1.6f,c.x+u,c.y-u*0.2f); }
        else if (i == 3) { gp.addRoundedRectangle(c.x-u,c.y-u*0.7f,2*u,1.4f*u,1.5f); }
        else { gp.addEllipse(c.x-u*0.6f,c.y-u*0.6f,1.2f*u,1.2f*u); }
        g.strokePath(gp, juce::PathStrokeType(juce::jmax(1.2f, 1.6f * s)));
    }
}

// Source-pixel rect for oversampling pill i (shared by paint + hit-zones).
juce::Rectangle<float> IroncladEditor::osPillSrc(int i) const
{
    return { 637.0f + (float) i * 68.0f, 401.0f, 60.0f, 29.0f };
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

// The two tabs at the top of the left panel (PEDAL FX / GUITAR).
void IroncladEditor::paintLeftTabs(juce::Graphics& g)
{
    const char* tn[2] = { "PEDAL FX", "GUITAR" };
    for (int i = 0; i < 2; ++i)
    {
        auto r = view.map(leftTabSrc(i)).toFloat();
        const bool sel = (i == leftTab);
        g.setColour(sel ? ic::Colours::glowRed.withAlpha(0.85f) : juce::Colour(0xff17181c));
        g.fillRoundedRectangle(r, 5.0f * view.scale);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawRoundedRectangle(r.reduced(0.5f), 5.0f * view.scale, 1.0f);
        g.setColour(sel ? juce::Colours::white : ic::Colours::muted);
        g.setFont(juce::Font(13.0f * view.scale, juce::Font::bold));
        g.drawText(tn[i], r, juce::Justification::centred, false);
    }
}

// PEDAL FX tab: the 6 amp knobs (labels drawn in paintLabels) plus the flanger +
// phaser strip along the bottom (ON pill + RATE/DEPTH/FBACK/MIX mini-knobs).
void IroncladEditor::paintPedalFxTab(juce::Graphics& g)
{
    auto pv = [&](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
    auto header = [&](juce::Rectangle<float> src, const juce::String& s)
    {
        g.setColour(ic::Colours::label);
        g.setFont(juce::Font(11.0f * view.scale, juce::Font::bold));
        g.drawText(s, view.map(src), juce::Justification::centredLeft, false);
    };
    auto knobLabels = [&](const std::array<SKnob, 4>& ks)
    {
        for (const auto& k : ks)
        {
            g.setColour(ic::Colours::muted);
            g.setFont(juce::Font(9.5f * view.scale, juce::Font::bold));
            g.drawText(k.label, view.map({ k.box.getX() - 8.0f, k.box.getBottom() + 1.0f,
                                           k.box.getWidth() + 16.0f, 11.0f }),
                       juce::Justification::centred, false);
        }
    };

    header({ 172, 685, 260, 12 }, "FLANGER");
    drawPill(g, { 170, 702, 52, 40 }, "ON", pv("FLG_ON") > 0.5f);
    knobLabels(flgKnobs);

    header({ 172, 751, 260, 12 }, "PHASER");
    drawPill(g, { 170, 768, 52, 40 }, "ON", pv("PHS_ON") > 0.5f);
    knobLabels(phsKnobs);
}

// GUITAR tab: the type dropdown (icon + name), 4 knobs, and the reference photo
// with a short character blurb.
void IroncladEditor::paintGuitarTab(juce::Graphics& g)
{
    const int t = currentGuitarType();

    // --- dropdown ---
    auto dd = view.map({ 168, 120, 332, 40 }).toFloat();
    g.setColour(juce::Colour(0xff141416));
    g.fillRoundedRectangle(dd, 6.0f * view.scale);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(dd.reduced(0.5f), 6.0f * view.scale, 1.0f);
    if (gtrIcon[(size_t) t].isValid())
        g.drawImage(gtrIcon[(size_t) t], view.map({ 178, 124, 34, 34 }).toFloat(),
                    juce::RectanglePlacement::centred);
    g.setColour(ic::Colours::glowRed);
    g.setFont(juce::Font(16.0f * view.scale, juce::Font::bold));
    g.drawText(guitarName(t), view.map({ 222, 120, 244, 40 }), juce::Justification::centredLeft, false);
    {
        auto a = view.map({ 476, 134, 18, 12 }).toFloat();
        juce::Path p;
        p.addTriangle(a.getX(), a.getY(), a.getRight(), a.getY(), a.getCentreX(), a.getBottom());
        g.setColour(ic::Colours::muted);
        g.fillPath(p);
    }

    // --- knob labels + values ---
    for (const auto& k : gtrKnobs)
    {
        g.setColour(ic::Colours::label);
        g.setFont(juce::Font(16.0f * view.scale, juce::Font::bold));
        g.drawText(k.label, view.map(k.labelRect), juce::Justification::centred, false);
        if (auto* prm = proc.apvts.getParameter(k.paramID))
        {
            g.setColour(ic::Colours::glowRed);
            g.setFont(juce::Font(15.0f * view.scale, juce::Font::bold));
            g.drawText(prm->getCurrentValueAsText(), view.map(k.valueRect),
                       juce::Justification::centred, false);
        }
    }

    // --- reference photo + blurb ---
    auto panel = view.map({ 156, 526, 356, 280 }).toFloat();
    g.setColour(juce::Colour(0xff141416));
    g.fillRoundedRectangle(panel, 8.0f * view.scale);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(panel.reduced(0.5f), 8.0f * view.scale, 1.0f);
    if (gtrPhoto[(size_t) t].isValid())
        g.drawImage(gtrPhoto[(size_t) t], view.map({ 162, 538, 344, 124 }).toFloat(),
                    juce::RectanglePlacement::centred);
    g.setColour(ic::Colours::glowRed);
    g.setFont(juce::Font(17.0f * view.scale, juce::Font::bold));
    g.drawText(guitarName(t), view.map({ 156, 674, 356, 24 }), juce::Justification::centred, false);
    g.setColour(ic::Colours::muted);
    g.setFont(juce::Font(12.5f * view.scale, juce::Font::plain));
    g.drawFittedText(guitarBlurb(t), view.map({ 172, 702, 324, 94 }),
                     juce::Justification::centredTop, 3);
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

    // Knob labels sit above each dial; the mockup shows no persistent value.
    for (const auto& k : knobs)
        drawLabel(k.labelRect, k.label);

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

    // Left panel: six 96 px tone/gain knobs.
    for (auto& k : knobs)
        k.slider.setBounds(view.map(k.centre.x - 48.0f, k.centre.y - 48.0f, 96.0f, 96.0f));

    // Right panel: four vertical sliders + three switches (boxes set in setup).
    for (auto& f : faders)   f.slider.setBounds(view.map(f.box));
    for (auto& s : switches) s.ctrl->setBounds(view.map(s.box));

    // Centre screen: preset stepper + menu, distortion-mode steppers, oversampling.
    presetPrev.setBounds(view.map(625, 152, 26, 38));
    presetNext.setBounds(view.map(853, 152, 26, 38));
    presetMenuBtn.setBounds(view.map(886, 152, 30, 30));
    typePrev.setBounds(view.map(628, 206, 26, 36));
    typeNext.setBounds(view.map(882, 206, 26, 36));
    for (int i = 0; i < (int) osButtons.size(); ++i)
        osButtons[(size_t) i].setBounds(view.map(osPillSrc(i)));
    for (int i = 0; i < (int) navBtns.size(); ++i)
        navBtns[(size_t) i].setBounds(view.map(navIconSrc(i)));

    updateNavVisibility();
}

// Pass 1: hide every control that the mockup does not place on the faceplate;
// these move onto the centre-screen pages (Distortion/Effects/Analysis/Cabinet/
// Settings) in Pass 5. They stay instantiated + attached, just not visible.
void IroncladEditor::hideSecondaryForRedesign()
{
    categoryBox.setVisible(false);
    dynKnob.setVisible(false);  fbKnob.setVisible(false);
    for (auto& b : harmButtons) b.setVisible(false);
    for (auto& b : pageTabs)    b.setVisible(false);
    for (auto& k : dlyKnobs) k.knob.setVisible(false);
    for (auto& k : rvbKnobs) k.knob.setVisible(false);
    for (auto& k : choKnobs) k.knob.setVisible(false);
    for (auto& k : cmpKnobs) k.knob.setVisible(false);
    for (auto& b : dlyToggle) b.setVisible(false);
    for (auto& b : dlyDivBtn) b.setVisible(false);
    for (auto& b : rvbModeBtn) b.setVisible(false);
    rvbOnBtn.setVisible(false); choOnBtn.setVisible(false); cmpOnBtn.setVisible(false);
    for (auto& b : pickupBtn) b.setVisible(false);
    for (auto& b : cabBtn)    b.setVisible(false);
    irOnBtn.setVisible(false); irLoadBtn.setVisible(false); irClearBtn.setVisible(false);
    puLoadKnob.knob.setVisible(false);
    for (auto& k : flgKnobs) k.knob.setVisible(false);
    for (auto& k : phsKnobs) k.knob.setVisible(false);
    flgOnBtn.setVisible(false); phsOnBtn.setVisible(false);
    for (auto& k : gtrKnobs) k.slider.setVisible(false);
    guitarSelectBtn.setVisible(false);
    for (auto& b : leftTabs) b.setVisible(false);
    for (auto& b : fxSelBtns) b.setVisible(false);
}

// Draws the active centre-screen page's chrome: headers, knob labels/values, and
// the pill visuals under the (already-positioned) transparent hit-zone buttons.
void IroncladEditor::paintNavContent(juce::Graphics& g)
{
    const float s = view.scale;
    auto text = [&](juce::Rectangle<float> src, const juce::String& str, float pt, juce::Colour col,
                    juce::Justification j = juce::Justification::centred)
    { g.setColour(col); g.setFont(juce::Font(pt * s, juce::Font::bold)); g.drawText(str, view.map(src), j, false); };
    auto arrow = [&](juce::Rectangle<float> src, bool left)
    { auto r = view.map(src).toFloat(); juce::Path p;
      if (left) p.addTriangle(r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getCentreY());
      else      p.addTriangle(r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
      g.setColour(ic::Colours::glowRed); g.fillPath(p); };
    auto pillScr = [&](const juce::TextButton& b, const juce::String& t, bool on, float pt = 11.0f)
    {
        if (! b.isVisible()) return;
        auto r = b.getBounds().toFloat();
        g.setColour(on ? ic::Colours::glowRed.withAlpha(0.9f) : juce::Colour(0xff191a1e));
        g.fillRoundedRectangle(r, 4.0f * s);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f * s, 1.0f);
        g.setColour(on ? juce::Colours::white : ic::Colours::muted);
        g.setFont(juce::Font(pt * s, juce::Font::bold));
        g.drawText(t, r, juce::Justification::centred, false);
    };
    auto knobLbls = [&](const auto& arr)
    {
        for (const auto& k : arr)
        {
            if (! k.knob.isVisible()) continue;
            text(k.labelRect, k.label, 10.0f, ic::Colours::muted);
            if (auto* p = proc.apvts.getParameter(k.paramID))
                text(k.valueRect, p->getCurrentValueAsText(), 10.0f, ic::Colours::glowRed);
        }
    };
    auto pv = [&](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };

    if (navPage == NavDistortion)
    {
        text({ 625, 194, 286, 14 }, "DISTORTION MODE", 11.0f, ic::Colours::muted);
        text({ 655, 208, 226, 28 }, typeName().toUpperCase(), 22.0f, ic::Colours::glowRed);
        arrow({ 632, 214, 16, 24 }, true);   arrow({ 886, 214, 16, 24 }, false);
        const int drivePct = juce::roundToInt(pv("DRIVE") * 100.0f);
        auto r = view.map(638, 250, 260, 104);
        g.setColour(ic::Colours::glowRed);
        g.setFont(juce::Font(84.0f * s, juce::Font::bold));
        g.drawText(juce::String(drivePct), r.withTrimmedRight((int)(46 * s)), juce::Justification::centredRight, false);
        g.setFont(juce::Font(42.0f * s, juce::Font::bold));
        g.drawText("%", r.withTrimmedLeft(r.getWidth() - (int)(50 * s)), juce::Justification::centredLeft, false);
        text({ 638, 352, 260, 24 }, "DRIVE", 15.0f, ic::Colours::glowRed);
        text({ 638, 380, 260, 14 }, "OVERSAMPLING", 10.0f, ic::Colours::muted);
        const int osIdx = (int) pv("OVERSAMPLE");
        const char* osL[4] = { "OFF", "2X", "4X", "8X" };
        for (int i = 0; i < 4; ++i) pillScr(osButtons[(size_t) i], osL[i], i == osIdx, 12.0f);
    }
    else if (navPage == NavEffects)
    {
        const char* tn[7] = { "DYN", "CHO", "FLG", "PHS", "DLY", "RVB", "CMP" };
        for (int i = 0; i < 7; ++i) pillScr(fxSelBtns[(size_t) i], tn[i], i == fxSel, 10.0f);
        switch (fxSel)
        {
            case 0:
                text({ 618, 238, 108, 13 }, "DYNAMICS", 10.0f, ic::Colours::muted);
                text({ 738, 238, 108, 13 }, "FEEDBACK", 10.0f, ic::Colours::muted);
                if (auto* p = proc.apvts.getParameter("DYNAMICS")) text({ 645, 300, 54, 12 }, p->getCurrentValueAsText(), 10.0f, ic::Colours::glowRed);
                if (auto* p = proc.apvts.getParameter("FEEDBACK")) text({ 765, 300, 54, 12 }, p->getCurrentValueAsText(), 10.0f, ic::Colours::glowRed);
                text({ 640, 330, 240, 12 }, "FEEDBACK VOICE", 9.0f, ic::Colours::muted);
                { const int h = (int) pv("FBHARMONIC"); const char* hl[3] = { "UNI", "5TH", "OCT" };
                  for (int i = 0; i < 3; ++i) pillScr(harmButtons[(size_t) i], hl[i], i == h, 10.0f); }
                break;
            case 1: pillScr(choOnBtn, "ON", pv("CHO_ON") > 0.5f); knobLbls(choKnobs); break;
            case 2: pillScr(flgOnBtn, "ON", pv("FLG_ON") > 0.5f); knobLbls(flgKnobs); break;
            case 3: pillScr(phsOnBtn, "ON", pv("PHS_ON") > 0.5f); knobLbls(phsKnobs); break;
            case 4:
                pillScr(dlyToggle[0], "ON", pv("DLY_ON") > 0.5f);
                pillScr(dlyToggle[1], pv("DLY_SYNC") > 0.5f ? "SYNC" : "FREE", pv("DLY_SYNC") > 0.5f);
                pillScr(dlyToggle[2], "PING", pv("DLY_PING") > 0.5f);
                { const int d = (int) pv("DLY_DIV"); const char* dn[5] = { "1/4", "1/8.", "1/8", "1/8T", "1/16" };
                  for (int i = 0; i < 5; ++i) pillScr(dlyDivBtn[(size_t) i], dn[i], i == d, 9.0f); }
                knobLbls(dlyKnobs); break;
            case 5:
                pillScr(rvbOnBtn, "ON", pv("RVB_ON") > 0.5f);
                { const int m = (int) pv("RVB_MODE"); const char* mn[4] = { "SPRING", "PLATE", "ROOM", "HALL" };
                  for (int i = 0; i < 4; ++i) pillScr(rvbModeBtn[(size_t) i], mn[i], i == m, 9.0f); }
                knobLbls(rvbKnobs); break;
            case 6: pillScr(cmpOnBtn, "ON", pv("CMP_ON") > 0.5f); knobLbls(cmpKnobs); break;
            default: break;
        }
    }
    else if (navPage == NavAnalysis)
    {
        text({ 609, 190, 318, 14 }, "TRANSFER CURVE", 11.0f, ic::Colours::muted);
        auto box = view.map(628, 212, 280, 220).toFloat();
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(box.toNearestInt());
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawHorizontalLine((int) box.getCentreY(), box.getX(), box.getRight());
        g.drawVerticalLine((int) box.getCentreX(), box.getY(), box.getBottom());
        const int type = (int) pv("TYPE"); const float gdisp = Waveshaper::driveGain(type, pv("DRIVE"));
        constexpr int N = 128; float ys[N]; float maxA = 1.0e-4f;
        for (int i = 0; i < N; ++i) { const float xin = -1.0f + 2.0f * (float) i / (N - 1); ys[i] = Waveshaper::shape(type, xin * gdisp); maxA = juce::jmax(maxA, std::abs(ys[i])); }
        juce::Path curve;
        for (int i = 0; i < N; ++i) { const float px = box.getX() + box.getWidth() * (float) i / (N - 1); const float py = box.getCentreY() - (ys[i] / maxA) * (box.getHeight() * 0.46f); if (i == 0) curve.startNewSubPath(px, py); else curve.lineTo(px, py); }
        g.setColour(ic::Colours::glowRed);
        g.strokePath(curve, juce::PathStrokeType(juce::jmax(1.5f, 2.0f * s)));
    }
    else if (navPage == NavCabinet)
    {
        const int t = currentGuitarType();
        auto dd = view.map(620, 196, 300, 34).toFloat();
        g.setColour(juce::Colour(0xff141416)); g.fillRoundedRectangle(dd, 5.0f * s);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(dd, 5.0f * s, 1.0f);
        if (gtrIcon[(size_t) t].isValid())
            g.drawImage(gtrIcon[(size_t) t], view.map(626, 200, 28, 26).toFloat(), juce::RectanglePlacement::centred);
        text({ 662, 196, 220, 34 }, guitarName(t), 14.0f, ic::Colours::glowRed, juce::Justification::centredLeft);
        { auto a = view.map(898, 206, 14, 12).toFloat(); juce::Path p;
          p.addTriangle(a.getX(), a.getY(), a.getRight(), a.getY(), a.getCentreX(), a.getBottom());
          g.setColour(ic::Colours::muted); g.fillPath(p); }
        for (const auto& k : gtrKnobs)
        {
            if (! k.slider.isVisible()) continue;
            text(k.labelRect, k.label, 10.0f, ic::Colours::muted);
            if (auto* p = proc.apvts.getParameter(k.paramID)) text(k.valueRect, p->getCurrentValueAsText(), 10.0f, ic::Colours::glowRed);
        }
        text({ 618, 316, 120, 12 }, "PICKUP", 9.0f, ic::Colours::muted);
        { const int pu = (int) pv("PICKUP"); const char* pn[4] = { "SINGLE", "HUMB", "ACTIVE", "BASS" };
          for (int i = 0; i < 4; ++i) pillScr(pickupBtn[(size_t) i], pn[i], i == pu, 9.0f); }
        text({ 618, 352, 120, 12 }, "CABINET", 9.0f, ic::Colours::muted);
        { const int cb = (int) pv("CAB"); const char* cn[4] = { "1x12", "2x12", "4x12B", "4x12M" };
          for (int i = 0; i < 4; ++i) pillScr(cabBtn[(size_t) i], cn[i], i == cb, 9.0f); }
        pillScr(irOnBtn, "IR", pv("IR_ON") > 0.5f, 9.0f);
        pillScr(irLoadBtn, "LOAD", false, 9.0f);
        pillScr(irClearBtn, "CLEAR", false, 9.0f);
    }
    else // NavSettings
    {
        text({ 609, 250, 318, 20 }, "SETTINGS", 13.0f, ic::Colours::muted);
        text({ 609, 280, 318, 14 }, "More options coming soon.", 10.0f, ic::Colours::muted);
    }
}
