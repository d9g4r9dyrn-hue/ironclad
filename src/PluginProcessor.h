#pragma once
#include <JuceHeader.h>
#include "dsp/DistortionEngine.h"
#include "dsp/PitchTracker.h"
#include "dsp/Delay.h"
#include "dsp/Reverb.h"
#include "dsp/Chorus.h"
#include "dsp/Compressor.h"
#include "LicenseManager.h"

class IroncladProcessor : public juce::AudioProcessor,
                          private juce::AsyncUpdater
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

    // Preset categories (for the editor's category browser).
    juce::StringArray getPresetCategories() const;
    juce::String getProgramCategory(int index) const;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Licensing: the editor drives activation and reads state for its UI.
    LicenseManager& getLicense() noexcept { return license; }

    // Cab IR loading (driven by the editor's file chooser).
    void loadIR(const juce::File&);
    void clearIR();
    juce::String getIRName() const { return juce::File(irPath).getFileNameWithoutExtension(); }
    bool hasIR() const { return irReady.load(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    IroncladEngine engine;
    int currentProgramIndex = 0;

    // Monophonic pitch of the clean input, tracked at base rate to aim the
    // feedback bloom at the played note.
    PitchTracker pitch;
    std::vector<float> monoBuf;

    // Cabinet IR convolution (base rate, replaces the algorithmic cab when loaded).
    juce::dsp::Convolution cabConv;
    juce::String irPath;
    std::atomic<bool> irReady { false };

    // Post-amp effects (base rate): chorus -> delay -> reverb -> compressor bus.
    Chorus chorus;
    StereoDelay delay;
    ReverbFx reverb;
    Compressor comp;
    double getHostBpm();

    // The waveshaper is a hard nonlinearity, so at the base rate heavy drive
    // folds aliasing back into the audible band (buzzy, "digital" fizz). We run
    // the whole engine oversampled and band-limit on the way in/out. All four
    // factors (Off/2x/4x/8x) are built up front so the on-screen selector can
    // switch without allocating on the audio thread; polyphase-IIR keeps the
    // added latency tiny, which matters for live playing.
    static constexpr int kNumOS = 4;                 // index -> Off,2x,4x,8x
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumOS> oversamplers;
    double baseSampleRate = 44100.0;
    int    baseBlockSize  = 512;
    int    activeOS       = 2;                        // 4x default
    std::atomic<int> pendingLatency { 0 };

    static int osRatio (int idx)  { static const int r[kNumOS] { 1, 2, 4, 8 }; return r[idx]; }
    static int osStages(int idx)  { return idx; }    // 2^stages == ratio
    void configureEngineForOS(int idx);              // RT-safe: re-rates the engine
    void handleAsyncUpdate() override;               // pushes new latency to the host

    // Demo-mode enforcement (active only while unlicensed): a short, click-free
    // output mute every ~30 s so the plugin is usable to evaluate but not to
    // ship with. All state is audio-thread-local.
    LicenseManager license;
    double currentSampleRate = 44100.0;
    int    demoPhase = 0;
    void   applyDemoMute(juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IroncladProcessor)
};
