#pragma once
#include "PluginProcessor.h"
#include "ui/IroncladUI.h"
#include <array>
#include <memory>

class IroncladEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit IroncladEditor(IroncladProcessor&);
    ~IroncladEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;   // keeps the category box + readouts in sync

private:
    IroncladProcessor& proc;
    ic::AssetManager assets;
    ic::Viewport view;

    // ---- knobs (6) ---------------------------------------------------------
    struct Knob
    {
        ic::ImageKnob slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::String paramID, label;
        juce::Point<float> centre;                 // source-pixel knob centre
        juce::Rectangle<float> labelRect, valueRect;
    };
    std::array<Knob, 6> knobs;

    // ---- vertical sliders (4) ---------------------------------------------
    struct Fader
    {
        ic::ImageSlider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::String paramID, label;               // paramID empty => inert placeholder
        juce::Rectangle<float> box, labelRect, valueRect;
    };
    std::array<Fader, 4> faders;

    // ---- on-screen preset + distortion-mode steppers -----------------------
    juce::TextButton presetPrev { "<" }, presetNext { ">" };
    juce::TextButton typePrev    { "<" }, typeNext    { ">" };
    juce::ComboBox   categoryBox;                 // preset category dropdown
    void syncCategoryBox();

    // ---- on-screen OVERSAMPLING selector (Off/2x/4x/8x) --------------------
    std::array<juce::TextButton, 4> osButtons;

    // ---- on-screen AMP FEEL controls (DYNAMICS, FEEDBACK, harmonic) --------
    ic::ImageKnob dynKnob, fbKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynAtt, fbAtt;
    std::array<juce::TextButton, 3> harmButtons;

    // ---- LCD pages: AMP / DELAY / REVERB / CHORUS / COMP / CAB ------------
    int activePage = 0;
    std::array<juce::TextButton, 6> pageTabs;

    struct SKnob   // an on-screen knob bound to a parameter, drawn on the LCD
    {
        ic::ImageKnob knob;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::String paramID, label;
        juce::Rectangle<float> box, labelRect, valueRect;
    };
    std::array<SKnob, 4> dlyKnobs;    // TIME, FB, TONE, MIX
    std::array<SKnob, 3> rvbKnobs;    // SIZE, DAMP, MIX
    std::array<SKnob, 3> choKnobs;    // RATE, DEPTH, MIX
    std::array<SKnob, 6> cmpKnobs;    // THRESH, RATIO, ATTACK, RELEASE, MAKEUP, MIX
    std::array<juce::TextButton, 3> dlyToggle;    // ON, SYNC, PING
    std::array<juce::TextButton, 5> dlyDivBtn;    // 1/4 .. 1/16
    std::array<juce::TextButton, 4> rvbModeBtn;   // Spring/Plate/Room/Hall
    juce::TextButton rvbOnBtn, choOnBtn, cmpOnBtn;

    // ---- CAB page: pickup + cab type + IR loader -------------------------
    std::array<juce::TextButton, 4> pickupBtn;    // Single/Humbucker/Active/Bass
    std::array<juce::TextButton, 4> cabBtn;       // 1x12/2x12/4x12 Brit/4x12 Modern
    juce::TextButton irOnBtn, irLoadBtn, irClearBtn;
    SKnob puLoadKnob;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void setupSKnob(SKnob&, const juce::String& paramID, const juce::String& label,
                    juce::Rectangle<float> box);
    void setupPillToggle(juce::TextButton&, const juce::String& boolParamID);
    void setupPillChoice(juce::TextButton&, const juce::String& choiceParamID, int index);
    void updatePageVisibility();
    void paintAmpPage(juce::Graphics&);
    void paintFxPage(juce::Graphics&);
    void paintTabs(juce::Graphics&);
    void drawPill(juce::Graphics&, juce::Rectangle<float> src, const juce::String& text, bool on);

    // ---- code-drawn toggle switches (CHARACTER / TIGHT-LOOSE / GATE) --------
    struct Switch
    {
        ic::ToggleSwitch* ctrl;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
        juce::Rectangle<float> box;      // source-pixel bounds
        juce::String title;              // section title drawn above (may be empty)
        juce::Rectangle<float> titleRect;
    };
    ic::ToggleSwitch characterSwitch { "SMOOTH", "RAW" };
    ic::ToggleSwitch tightLooseSwitch { "TIGHT", "LOOSE" };
    ic::ToggleSwitch gateSwitch { "OFF", "ON" };
    std::array<Switch, 3> switches;

    void setupSwitch(Switch&, ic::ToggleSwitch&, const juce::String& paramID,
                     const juce::Rectangle<float>& box,
                     const juce::String& title, const juce::Rectangle<float>& titleRect);

    void setupKnob(Knob&, const juce::String& paramID, const juce::String& label,
                   float cx, float cy);
    void setupFader(Fader&, const juce::String& paramID, const juce::String& label,
                    const juce::Rectangle<float>& box);
    void cyclePreset(int direction);
    void cycleType(int direction);

    void paintChrome(juce::Graphics&);
    void paintWordmark(juce::Graphics&);
    void paintLabels(juce::Graphics&);
    void paintScreen(juce::Graphics&);
    void paintVersion(juce::Graphics&);

    juce::String typeName() const;
    juce::Rectangle<float> osPillSrc(int i) const;   // source-px rect for OS pill i
    juce::Rectangle<float> harmPillSrc(int i) const; // source-px rect for harmonic pill i

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IroncladEditor)
};
