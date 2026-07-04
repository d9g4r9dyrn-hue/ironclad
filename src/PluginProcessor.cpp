#include "PluginProcessor.h"
#include "PluginEditor.h"

IroncladProcessor::IroncladProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Load the first factory preset so the on-screen preset name and the actual
    // parameter values agree from first launch.
    setCurrentProgram(0);
}

IroncladProcessor::~IroncladProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout IroncladProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("TYPE", 1), "Type",
        juce::StringArray { "Clean", "Crunch", "Lead", "Fuzz" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DRIVE", 1), "Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("BASS", 1), "Bass",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("MID", 1), "Mid",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("TREBLE", 1), "Treble",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("PRESENCE", 1), "Presence",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("TIGHT", 1), "Tight",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.5f), 80.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("HIGHCUT", 1), "High Cut",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.35f), 20000.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) {
                return v >= 1000.0f ? juce::String(v / 1000.0f, 1) + " kHz"
                                    : juce::String((int) v) + " Hz";
            })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("MIX", 1), "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    // OUTPUT is the wet makeup gain applied before the dry/wet mix (part of the
    // amp). LEVEL is the final master trim in dB applied after the mix.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("OUTPUT", 1), "Output",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("LEVEL", 1), "Level",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " dB"; })));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("GATE", 1), "Gate", false));

    return { params.begin(), params.end() };
}

namespace
{
    // Order matches createParameterLayout() exactly - each FactoryPreset row
    // below is positional against this list, not looked up by name, so it's
    // cheap at the cost of needing to stay in sync if params are reordered.
    // TYPE is stored as its choice index cast to float.
    constexpr int numPresetParams = 9;
    const char* const presetParamIDs[numPresetParams] = {
        "TYPE", "DRIVE", "BASS", "MID", "TREBLE", "PRESENCE", "TIGHT", "MIX", "OUTPUT"
    };

    struct FactoryPreset { const char* name; float values[numPresetParams]; };

    //                              TYPE, DRIVE, BASS,  MID,   TREB,  PRES,  TIGHT, MIX,  OUT
    const FactoryPreset factoryPresets[] = {
        { "Clean Boost",     { 0.0f, 0.15f,  0.0f,  0.0f,  1.0f,  0.40f, 80.0f,  1.00f, 1.00f } },
        { "Blues Crunch",    { 1.0f, 0.40f,  2.0f,  2.0f,  1.0f,  0.50f, 90.0f,  1.00f, 1.00f } },
        { "Classic Rock",    { 1.0f, 0.60f,  1.0f,  3.0f,  2.0f,  0.60f, 100.0f, 1.00f, 1.00f } },
        { "Lead",            { 2.0f, 0.70f,  0.0f,  4.0f,  2.0f,  0.60f, 110.0f, 1.00f, 1.00f } },
        { "Modern High Gain",{ 2.0f, 0.80f,  3.0f, -2.0f,  3.0f,  0.70f, 120.0f, 1.00f, 0.90f } },
        { "Metal Rhythm",    { 2.0f, 0.90f,  4.0f, -3.0f,  4.0f,  0.80f, 150.0f, 1.00f, 0.85f } },
        { "Fuzz",            { 3.0f, 0.85f,  2.0f,  1.0f,  0.0f,  0.50f, 70.0f,  0.95f, 0.90f } },
        { "Warm Drive",      { 1.0f, 0.35f,  1.0f,  1.0f,  0.0f,  0.40f, 60.0f,  0.90f, 1.00f } },
    };
    constexpr int numFactoryPresets = (int)(sizeof(factoryPresets) / sizeof(FactoryPreset));
}

const juce::String IroncladProcessor::getName() const { return JucePlugin_Name; }
bool IroncladProcessor::acceptsMidi() const { return false; }
bool IroncladProcessor::producesMidi() const { return false; }
bool IroncladProcessor::isMidiEffect() const { return false; }
double IroncladProcessor::getTailLengthSeconds() const { return 0.0; }
int IroncladProcessor::getNumPrograms() { return numFactoryPresets; }
int IroncladProcessor::getCurrentProgram() { return currentProgramIndex; }

void IroncladProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= numFactoryPresets)
        return;

    currentProgramIndex = index;
    const auto& preset = factoryPresets[index];
    for (int i = 0; i < numPresetParams; ++i)
    {
        if (auto* param = apvts.getParameter(presetParamIDs[i]))
            param->setValueNotifyingHost(param->convertTo0to1(preset.values[i]));
    }
}

const juce::String IroncladProcessor::getProgramName(int index)
{
    if (index < 0 || index >= numFactoryPresets)
        return {};
    return factoryPresets[index].name;
}

void IroncladProcessor::changeProgramName(int, const juce::String&) {}

void IroncladProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare(sampleRate, samplesPerBlock);
}

void IroncladProcessor::releaseResources() {}

bool IroncladProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void IroncladProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    DistortionParams dp;
    dp.type     = (int) apvts.getRawParameterValue("TYPE")->load();
    dp.drive    = apvts.getRawParameterValue("DRIVE")->load();
    dp.bass     = apvts.getRawParameterValue("BASS")->load();
    dp.mid      = apvts.getRawParameterValue("MID")->load();
    dp.treble   = apvts.getRawParameterValue("TREBLE")->load();
    dp.presence = apvts.getRawParameterValue("PRESENCE")->load();
    dp.tight    = apvts.getRawParameterValue("TIGHT")->load();
    dp.highCut  = apvts.getRawParameterValue("HIGHCUT")->load();
    dp.mix      = apvts.getRawParameterValue("MIX")->load();
    dp.output   = apvts.getRawParameterValue("OUTPUT")->load();
    dp.level    = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("LEVEL")->load());
    dp.gate     = apvts.getRawParameterValue("GATE")->load() > 0.5f;

    engine.setParameters(dp);
    engine.processBlock(buffer.getWritePointer(0),
                        buffer.getWritePointer(1),
                        buffer.getNumSamples());

    if (! license.isActivated())
        applyDemoMute(buffer);
}

void IroncladProcessor::applyDemoMute(juce::AudioBuffer<float>& buffer)
{
    // Every `period` seconds, ramp the output to silence for `muteDur` seconds
    // with short fades so it's obvious but click-free. Purely audio-thread state.
    const int   period  = (int) (currentSampleRate * 30.0);
    const int   muteDur = (int) (currentSampleRate * 0.5);
    const int   ramp    = juce::jmax(1, (int) (currentSampleRate * 0.02));
    const int   muteBeg = period - muteDur;
    const int   n       = buffer.getNumSamples();
    const int   chans   = buffer.getNumChannels();

    for (int i = 0; i < n; ++i)
    {
        float gain = 1.0f;
        if (demoPhase >= muteBeg)
        {
            const int t = demoPhase - muteBeg;               // 0 .. muteDur
            if (t < ramp)                    gain = 1.0f - (float) t / (float) ramp;
            else if (t > muteDur - ramp)     gain = (float) (t - (muteDur - ramp)) / (float) ramp;
            else                             gain = 0.0f;
        }

        for (int ch = 0; ch < chans; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;

        if (++demoPhase >= period)
            demoPhase = 0;
    }
}

void IroncladProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void IroncladProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

bool IroncladProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* IroncladProcessor::createEditor()
{
    return new IroncladEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IroncladProcessor();
}
