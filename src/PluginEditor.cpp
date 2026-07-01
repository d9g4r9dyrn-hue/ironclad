#include "PluginEditor.h"

IroncladEditor::IroncladEditor(IroncladProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p)
{
    typeLabel.setText("Type", juce::dontSendNotification);
    typeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(typeLabel);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("TYPE")))
        typeBox.addItemList(choice->choices, 1);
    addAndMakeVisible(typeBox);
    typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, "TYPE", typeBox);

    setupKnob(drive,    "DRIVE",    "Drive");
    setupKnob(bass,     "BASS",     "Bass");
    setupKnob(mid,      "MID",      "Mid");
    setupKnob(treble,   "TREBLE",   "Treble");
    setupKnob(presence, "PRESENCE", "Presence");
    setupKnob(tight,    "TIGHT",    "Tight");
    setupKnob(mix,      "MIX",      "Mix");
    setupKnob(output,   "OUTPUT",   "Output");

    addAndMakeVisible(prevPreset);
    addAndMakeVisible(nextPreset);
    prevPreset.onClick = [this] { cyclePreset(-1); };
    nextPreset.onClick = [this] { cyclePreset(+1); };

    presetName.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetName);
    refreshPresetName();

    setSize(560, 360);
}

IroncladEditor::~IroncladEditor() {}

void IroncladEditor::setupKnob(KnobRow& row, const juce::String& paramID, const juce::String& text)
{
    row.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    row.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible(row.slider);

    row.label.setText(text, juce::dontSendNotification);
    row.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(row.label);

    row.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, paramID, row.slider);
}

void IroncladEditor::refreshPresetName()
{
    presetName.setText(proc.getProgramName(proc.getCurrentProgram()), juce::dontSendNotification);
}

void IroncladEditor::cyclePreset(int direction)
{
    const int n = proc.getNumPrograms();
    if (n <= 0)
        return;
    int idx = (proc.getCurrentProgram() + direction + n) % n;
    proc.setCurrentProgram(idx);
    refreshPresetName();
}

void IroncladEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("IRONCLAD", getLocalBounds().removeFromTop(30),
               juce::Justification::centred, false);
}

void IroncladEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(30); // title

    // Preset bar.
    auto presetBar = area.removeFromTop(28);
    prevPreset.setBounds(presetBar.removeFromLeft(28));
    nextPreset.setBounds(presetBar.removeFromRight(28));
    presetName.setBounds(presetBar);

    area.removeFromTop(8);

    // Type selector.
    auto typeRow = area.removeFromTop(24);
    typeLabel.setBounds(typeRow.removeFromLeft(60));
    typeBox.setBounds(typeRow.removeFromLeft(160));

    area.removeFromTop(8);

    // 4x2 grid of knobs.
    KnobRow* rows[8] = { &drive, &bass, &mid, &treble, &presence, &tight, &mix, &output };
    const int cols = 4;
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / 2;

    for (int i = 0; i < 8; ++i)
    {
        const int r = i / cols;
        const int c = i % cols;
        juce::Rectangle<int> cell(area.getX() + c * cellW,
                                  area.getY() + r * cellH,
                                  cellW, cellH);
        rows[i]->label.setBounds(cell.removeFromTop(18));
        rows[i]->slider.setBounds(cell.reduced(4));
    }
}
