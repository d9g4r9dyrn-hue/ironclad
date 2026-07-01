#pragma once
#include <JuceHeader.h>
#include "dsp/DistortionEngine.h"
#include "LicenseManager.h"

class IroncladProcessor : public juce::AudioProcessor
{
public:
    IroncladProcessor();
    ~IroncladProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Licensing: the editor drives activation and reads state for its UI.
    LicenseManager& getLicense() noexcept { return license; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    IroncladEngine engine;
    int currentProgramIndex = 0;

    // Demo-mode enforcement (active only while unlicensed): a short, click-free
    // output mute every ~30 s so the plugin is usable to evaluate but not to
    // ship with. All state is audio-thread-local.
    LicenseManager license;
    double currentSampleRate = 44100.0;
    int    demoPhase = 0;
    void   applyDemoMute(juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IroncladProcessor)
};
