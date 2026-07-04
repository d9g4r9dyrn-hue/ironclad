#pragma once
#include "PluginProcessor.h"
#include "ui/IroncladUI.h"
#include <array>
#include <memory>

class IroncladEditor : public juce::AudioProcessorEditor
{
public:
    explicit IroncladEditor(IroncladProcessor&);
    ~IroncladEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

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

    // ---- gate toggle (transparent hit zone over the gate switch artwork) ----
    juce::TextButton gateButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateAtt;

    void setupKnob(Knob&, const juce::String& paramID, const juce::String& label,
                   float cx, float cy);
    void setupFader(Fader&, const juce::String& paramID, const juce::String& label,
                    const juce::Rectangle<float>& box);
    void cyclePreset(int direction);
    void cycleType(int direction);

    void paintChrome(juce::Graphics&);
    void paintLabels(juce::Graphics&);
    void paintScreenOverlays(juce::Graphics&);

    juce::String typeName() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IroncladEditor)
};
