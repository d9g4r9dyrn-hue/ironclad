#pragma once
#include "PluginProcessor.h"

// Barebones functional editor: every parameter is bound to a plain generic
// control so the plugin is fully testable. This is a PLACEHOLDER for the real
// mockup-based UI - deliberately no custom look & feel here.
class IroncladEditor : public juce::AudioProcessorEditor
{
public:
    explicit IroncladEditor(IroncladProcessor&);
    ~IroncladEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    IroncladProcessor& proc;

    struct KnobRow
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    // One row per float parameter, in display order.
    KnobRow drive, bass, mid, treble, presence, tight, mix, output;

    juce::ComboBox typeBox;
    juce::Label    typeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;

    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    juce::Label      presetName;

    void setupKnob(KnobRow& row, const juce::String& paramID, const juce::String& text);
    void refreshPresetName();
    void cyclePreset(int direction);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IroncladEditor)
};
