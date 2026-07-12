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
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.5f), 80.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String((int) v) + " Hz"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("HIGHCUT", 1), "High Cut",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.35f), 12000.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) {
                return v >= 1000.0f ? juce::String(v / 1000.0f, 1) + " kHz"
                                    : juce::String((int) v) + " Hz";
            })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("MIX", 1), "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String((int) std::lround(v * 100.0f)) + "%"; })));

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

    // Oversampling quality selector shown on the screen (Off/2x/4x/8x). Default
    // 4x. Switching re-rates the engine (real-time safe) and re-reports latency.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("OVERSAMPLE", 1), "Oversampling",
        juce::StringArray { "Off", "2x", "4x", "8x" }, 2));

    // CHARACTER: Smooth (false) vs Raw (true) voicing.
    // TIGHTLOOSE: Tight (false) vs Loose (true) low-end feel.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("CHARACTER", 1), "Character", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("TIGHTLOOSE", 1), "Tight/Loose", false));

    // Amp "feel" macro: power-amp sag, pick-attack bite, and speaker compression.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DYNAMICS", 1), "Dynamics",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // Guitar source model: archetype (conditions gain staging, pre-distortion tone,
    // resonance, body, feedback sensitivity) + character depth + user trims. Stored
    // with presets as a categorization; also changes how the whole amp reacts.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("GUITARTYPE", 1), "Guitar",
        juce::StringArray { "S-Type SC", "Single-Cut HB", "Modern HB", "T-Type SC",
                            "Semi-Hollow", "P-90", "Extended Range" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("GTR_MODEL", 1), "Guitar Model",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("GTR_OUTPUT", 1), "Guitar Output",
        juce::NormalisableRange<float>(-6.0f, 6.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " dB"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("GTR_BODY", 1), "Guitar Body",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("GTR_BRIGHT", 1), "Guitar Brightness",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // Input stage: pickup type (RLC resonance voicing) + amp-input load/brightness.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("PICKUP", 1), "Pickup",
        juce::StringArray { "Single", "Humbucker", "Active", "Bass" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("PU_LOAD", 1), "Input Load",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.6f));

    // Cabinet: algorithmic voicing (type) + optional external IR that replaces it.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("CAB", 1), "Cabinet",
        juce::StringArray { "1x12 Open", "2x12", "4x12 British", "4x12 Modern" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("IR_ON", 1), "IR Cab", false));

    // Synthetic feedback: amount, and which harmonic of the played note it sings.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FEEDBACK", 1), "Feedback",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("FBHARMONIC", 1), "Feedback Harmonic",
        juce::StringArray { "Unison", "Fifth", "Octave" }, 0));

    // --- Delay (post-amp, base rate) -------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("DLY_ON", 1), "Delay On", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("DLY_SYNC", 1), "Delay Sync", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DLY_TIME", 1), "Delay Time",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.4f), 350.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String((int) v) + " ms"; })));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("DLY_DIV", 1), "Delay Division",
        juce::StringArray { "1/4", "1/8.", "1/8", "1/8T", "1/16" }, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DLY_FB", 1), "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DLY_TONE", 1), "Delay Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DLY_MIX", 1), "Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.28f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("DLY_PING", 1), "Delay Ping-Pong", false));

    // --- Reverb ----------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("RVB_ON", 1), "Reverb On", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("RVB_MODE", 1), "Reverb Mode",
        juce::StringArray { "Spring", "Plate", "Room", "Hall" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("RVB_SIZE", 1), "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("RVB_DAMP", 1), "Reverb Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("RVB_MIX", 1), "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));

    // --- Chorus ----------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("CHO_ON", 1), "Chorus On", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CHO_RATE", 1), "Chorus Rate",
        juce::NormalisableRange<float>(0.05f, 8.0f, 0.01f, 0.5f), 0.8f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 2) + " Hz"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CHO_DEPTH", 1), "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CHO_MIX", 1), "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    // --- Flanger ---------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("FLG_ON", 1), "Flanger On", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FLG_RATE", 1), "Flanger Rate",
        juce::NormalisableRange<float>(0.02f, 8.0f, 0.01f, 0.5f), 0.3f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 2) + " Hz"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FLG_DEPTH", 1), "Flanger Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FLG_FB", 1), "Flanger Feedback",
        juce::NormalisableRange<float>(0.0f, 0.9f, 0.01f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FLG_MIX", 1), "Flanger Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // --- Phaser ----------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("PHS_ON", 1), "Phaser On", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("PHS_RATE", 1), "Phaser Rate",
        juce::NormalisableRange<float>(0.02f, 8.0f, 0.01f, 0.5f), 0.4f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 2) + " Hz"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("PHS_DEPTH", 1), "Phaser Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("PHS_FB", 1), "Phaser Feedback",
        juce::NormalisableRange<float>(0.0f, 0.9f, 0.01f), 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("PHS_MIX", 1), "Phaser Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // --- Compressor / limiter (output bus) -------------------------------
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("CMP_ON", 1), "Comp On", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CMP_THRESH", 1), "Comp Threshold",
        juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), -18.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " dB"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CMP_RATIO", 1), "Comp Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 3.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return v >= 19.5f ? juce::String("Limit") : juce::String(v, 1) + ":1"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CMP_ATTACK", 1), "Comp Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.4f), 12.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " ms"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CMP_RELEASE", 1), "Comp Release",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.4f), 150.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String((int) v) + " ms"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CMP_MAKEUP", 1), "Comp Makeup",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " dB"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CMP_MIX", 1), "Comp Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    return { params.begin(), params.end() };
}

namespace
{
    struct PP { const char* id; float v; };
    struct PresetDef { const char* name; const char* category; std::vector<PP> params; };

    // Params a preset is allowed to set. setCurrentProgram resets ALL of these to
    // their defaults, then applies the preset's overrides below - so presets only
    // list what DIFFERS from default and there's no bleed between presets. OVERSAMPLE
    // and LEVEL are user/global and deliberately left out.
    const char* const managedParamIDs[] = {
        "TYPE","DRIVE","BASS","MID","TREBLE","PRESENCE","TIGHT","HIGHCUT","MIX","OUTPUT",
        "GATE","CHARACTER","TIGHTLOOSE","DYNAMICS","PICKUP","PU_LOAD","CAB","FEEDBACK","FBHARMONIC",
        "GUITARTYPE","GTR_MODEL","GTR_OUTPUT","GTR_BODY","GTR_BRIGHT",
        "DLY_ON","DLY_SYNC","DLY_DIV","DLY_TIME","DLY_FB","DLY_TONE","DLY_MIX","DLY_PING",
        "RVB_ON","RVB_MODE","RVB_SIZE","RVB_DAMP","RVB_MIX",
        "CHO_ON","CHO_RATE","CHO_DEPTH","CHO_MIX",
        "FLG_ON","FLG_RATE","FLG_DEPTH","FLG_FB","FLG_MIX",
        "PHS_ON","PHS_RATE","PHS_DEPTH","PHS_FB","PHS_MIX",
        "CMP_ON","CMP_THRESH","CMP_RATIO","CMP_ATTACK","CMP_RELEASE","CMP_MAKEUP","CMP_MIX"
    };

    const std::vector<PresetDef>& presets()
    {
        // GUITARTYPE index: 0=S-Type SC, 1=Single-Cut HB, 2=Modern HB, 3=T-Type SC,
        // 4=Semi-Hollow, 5=P-90, 6=Extended Range. Each preset declares the guitar it
        // was voiced for (categorization) and the source model shapes how it reacts.
        static const std::vector<PresetDef> p = {
            // ---- Clean ----
            { "Clean Boost",      "Clean", { {"TYPE",0},{"GUITARTYPE",0},{"DRIVE",0.15f},{"TREBLE",1},{"PRESENCE",0.40f} } },
            { "Studio Clean",     "Clean", { {"TYPE",0},{"GUITARTYPE",0},{"DRIVE",0.10f},{"BASS",1},{"TREBLE",2},{"PRESENCE",0.45f},{"CHO_ON",1},{"CHO_MIX",0.22f} } },
            { "Jangle Verb",      "Clean", { {"TYPE",0},{"GUITARTYPE",3},{"DRIVE",0.18f},{"TREBLE",2},{"PRESENCE",0.50f},{"RVB_ON",1},{"RVB_MODE",1},{"RVB_MIX",0.30f} } },
            { "Funk Snap",        "Clean", { {"TYPE",0},{"GUITARTYPE",0},{"DRIVE",0.20f},{"MID",2},{"TREBLE",1},{"DYNAMICS",0.72f} } },
            { "Jazz Box",         "Clean", { {"TYPE",0},{"GUITARTYPE",4},{"DRIVE",0.16f},{"BASS",2},{"MID",1},{"TREBLE",-2},{"PRESENCE",0.30f},{"GTR_BODY",0.70f},{"RVB_ON",1},{"RVB_MODE",2},{"RVB_MIX",0.22f} } },
            // ---- Drive ----
            { "Blues Crunch",     "Drive", { {"TYPE",1},{"GUITARTYPE",5},{"DRIVE",0.40f},{"BASS",2},{"MID",2},{"PRESENCE",0.50f},{"TIGHT",90},{"DYNAMICS",0.70f} } },
            { "Classic Rock",     "Drive", { {"TYPE",1},{"GUITARTYPE",1},{"DRIVE",0.60f},{"MID",3},{"TREBLE",2},{"PRESENCE",0.60f},{"TIGHT",100} } },
            { "British Crunch",   "Drive", { {"TYPE",1},{"GUITARTYPE",1},{"DRIVE",0.55f},{"BASS",1},{"MID",3},{"TREBLE",3},{"PRESENCE",0.60f},{"DLY_ON",1},{"DLY_MIX",0.16f} } },
            { "Tweed Breakup",    "Drive", { {"TYPE",1},{"GUITARTYPE",5},{"DRIVE",0.45f},{"BASS",2},{"MID",1},{"DYNAMICS",0.78f} } },
            { "Country Twang",    "Drive", { {"TYPE",1},{"GUITARTYPE",3},{"DRIVE",0.32f},{"MID",1},{"TREBLE",3},{"PRESENCE",0.62f},{"GTR_BRIGHT",0.66f},{"DLY_ON",1},{"DLY_DIV",2},{"DLY_MIX",0.14f} } },
            // ---- High Gain ----
            { "Lead 800",         "High Gain", { {"TYPE",2},{"GUITARTYPE",1},{"DRIVE",0.70f},{"MID",4},{"TREBLE",2},{"PRESENCE",0.60f},{"TIGHT",110},{"GATE",1} } },
            { "Modern High Gain", "High Gain", { {"TYPE",2},{"GUITARTYPE",2},{"DRIVE",0.80f},{"BASS",3},{"MID",-2},{"TREBLE",3},{"PRESENCE",0.70f},{"TIGHT",120},{"OUTPUT",0.90f},{"GATE",1} } },
            { "Metal Rhythm",     "High Gain", { {"TYPE",2},{"GUITARTYPE",2},{"DRIVE",0.86f},{"BASS",4},{"MID",-3},{"TREBLE",2.5f},{"PRESENCE",0.62f},{"TIGHT",150},{"OUTPUT",0.85f},{"GATE",1},{"CMP_ON",1},{"CMP_THRESH",-16},{"CMP_RATIO",4},{"CMP_MAKEUP",4} } },
            { "Djent Chug",       "High Gain", { {"TYPE",2},{"GUITARTYPE",6},{"DRIVE",0.82f},{"BASS",3},{"MID",-4},{"TREBLE",3},{"PRESENCE",0.70f},{"TIGHT",180},{"OUTPUT",0.85f},{"GATE",1},{"CHARACTER",1} } },
            { "8-String Grind",   "High Gain", { {"TYPE",2},{"GUITARTYPE",6},{"DRIVE",0.88f},{"BASS",2},{"MID",-2},{"TREBLE",2},{"PRESENCE",0.66f},{"TIGHT",200},{"GTR_MODEL",0.60f},{"OUTPUT",0.82f},{"GATE",1} } },
            // ---- Fuzz ----
            { "Classic Fuzz",     "Fuzz", { {"TYPE",3},{"GUITARTYPE",0},{"DRIVE",0.85f},{"BASS",2},{"MID",1},{"PRESENCE",0.50f},{"TIGHT",70},{"MIX",0.95f},{"OUTPUT",0.90f},{"GATE",1} } },
            { "Woolly Fuzz",      "Fuzz", { {"TYPE",3},{"GUITARTYPE",1},{"DRIVE",0.90f},{"BASS",4},{"MID",2},{"TREBLE",-2},{"PRESENCE",0.40f},{"OUTPUT",0.85f},{"GATE",1} } },
            { "Gated Splat",      "Fuzz", { {"TYPE",3},{"GUITARTYPE",2},{"DRIVE",0.95f},{"MID",1},{"PRESENCE",0.55f},{"OUTPUT",0.80f},{"GATE",1} } },
            // ---- Lead ----
            { "Singing Lead",     "Lead", { {"TYPE",2},{"GUITARTYPE",1},{"DRIVE",0.75f},{"MID",4},{"TREBLE",2},{"PRESENCE",0.60f},{"GATE",1},{"FEEDBACK",0.50f},{"DLY_ON",1},{"DLY_MIX",0.24f},{"RVB_ON",1},{"RVB_MIX",0.20f},{"CMP_ON",1},{"CMP_THRESH",-20},{"CMP_RATIO",3},{"CMP_MAKEUP",5} } },
            { "Feedback Sustain", "Lead", { {"TYPE",2},{"GUITARTYPE",4},{"DRIVE",0.80f},{"MID",3},{"PRESENCE",0.65f},{"GATE",1},{"FEEDBACK",0.60f},{"FBHARMONIC",0},{"RVB_ON",1},{"RVB_SIZE",0.75f},{"RVB_MIX",0.32f},{"DLY_ON",1},{"DLY_FB",0.45f},{"DLY_MIX",0.28f} } },
            { "Harmonic Bloom",   "Lead", { {"TYPE",2},{"GUITARTYPE",1},{"DRIVE",0.72f},{"MID",4},{"TREBLE",1},{"GATE",1},{"FEEDBACK",0.55f},{"FBHARMONIC",1},{"DLY_ON",1},{"DLY_DIV",1},{"DLY_MIX",0.26f} } },
            { "Doom Feedback",    "Lead", { {"TYPE",2},{"GUITARTYPE",4},{"DRIVE",0.78f},{"BASS",3},{"MID",2},{"PRESENCE",0.55f},{"TIGHT",70},{"GATE",1},{"FEEDBACK",0.72f},{"FBHARMONIC",2},{"RVB_ON",1},{"RVB_MODE",3},{"RVB_SIZE",0.85f},{"RVB_MIX",0.34f} } },
            // ---- Ambient ----
            { "Ambient Wash",     "Ambient", { {"TYPE",0},{"GUITARTYPE",0},{"DRIVE",0.25f},{"TREBLE",2},{"PRESENCE",0.50f},{"CHO_ON",1},{"CHO_MIX",0.30f},{"DLY_ON",1},{"DLY_FB",0.50f},{"DLY_MIX",0.34f},{"RVB_ON",1},{"RVB_MODE",3},{"RVB_SIZE",0.80f},{"RVB_MIX",0.40f} } },
            { "Post-Rock Swell",  "Ambient", { {"TYPE",1},{"GUITARTYPE",4},{"DRIVE",0.40f},{"MID",2},{"PRESENCE",0.55f},{"DLY_ON",1},{"DLY_DIV",0},{"DLY_FB",0.45f},{"DLY_MIX",0.30f},{"RVB_ON",1},{"RVB_MODE",3},{"RVB_MIX",0.35f} } },
            { "Shimmer Pad",      "Ambient", { {"TYPE",0},{"GUITARTYPE",0},{"DRIVE",0.20f},{"TREBLE",3},{"CHO_ON",1},{"CHO_RATE",0.50f},{"CHO_DEPTH",0.70f},{"CHO_MIX",0.40f},{"RVB_ON",1},{"RVB_MODE",3},{"RVB_SIZE",0.90f},{"RVB_MIX",0.45f} } },
            // ---- Modulation (phaser / flanger showcases) ----
            { "Phaser Funk",      "Modulation", { {"TYPE",0},{"GUITARTYPE",0},{"DRIVE",0.22f},{"MID",2},{"TREBLE",1},{"DYNAMICS",0.72f},{"PHS_ON",1},{"PHS_RATE",0.9f},{"PHS_DEPTH",0.85f},{"PHS_FB",0.5f},{"PHS_MIX",0.6f} } },
            { "Jet Flanger",      "Modulation", { {"TYPE",1},{"GUITARTYPE",1},{"DRIVE",0.5f},{"MID",2},{"PRESENCE",0.55f},{"FLG_ON",1},{"FLG_RATE",0.25f},{"FLG_DEPTH",0.8f},{"FLG_FB",0.7f},{"FLG_MIX",0.55f} } },
            { "Swirl Lead",       "Modulation", { {"TYPE",2},{"GUITARTYPE",1},{"DRIVE",0.7f},{"MID",4},{"PRESENCE",0.6f},{"GATE",1},{"PHS_ON",1},{"PHS_RATE",0.35f},{"PHS_DEPTH",0.9f},{"PHS_FB",0.55f},{"PHS_MIX",0.45f},{"DLY_ON",1},{"DLY_MIX",0.22f} } },
            { "Flanged Crunch",   "Modulation", { {"TYPE",1},{"GUITARTYPE",5},{"DRIVE",0.5f},{"BASS",2},{"MID",2},{"PRESENCE",0.55f},{"FLG_ON",1},{"FLG_RATE",0.4f},{"FLG_DEPTH",0.6f},{"FLG_FB",0.4f},{"FLG_MIX",0.4f} } },
        };
        return p;
    }
}

const juce::String IroncladProcessor::getName() const { return JucePlugin_Name; }
bool IroncladProcessor::acceptsMidi() const { return false; }
bool IroncladProcessor::producesMidi() const { return false; }
bool IroncladProcessor::isMidiEffect() const { return false; }
double IroncladProcessor::getTailLengthSeconds() const { return 0.0; }
int IroncladProcessor::getNumPrograms() { return (int) presets().size(); }
int IroncladProcessor::getCurrentProgram() { return currentProgramIndex; }

void IroncladProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= (int) presets().size())
        return;

    currentProgramIndex = index;

    // Reset every preset-managed param to its default, then apply this preset's
    // overrides - so presets only specify what differs and nothing bleeds over.
    for (auto* id : managedParamIDs)
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->getDefaultValue());

    for (const auto& pv : presets()[(size_t) index].params)
        if (auto* p = apvts.getParameter(pv.id))
            p->setValueNotifyingHost(p->convertTo0to1(pv.v));
}

const juce::String IroncladProcessor::getProgramName(int index)
{
    if (index < 0 || index >= (int) presets().size())
        return {};
    return presets()[(size_t) index].name;
}

juce::StringArray IroncladProcessor::getPresetCategories() const
{
    juce::StringArray cats;
    for (const auto& pr : presets()) cats.addIfNotAlreadyThere(pr.category);
    return cats;
}

juce::String IroncladProcessor::getProgramCategory(int index) const
{
    if (index < 0 || index >= (int) presets().size())
        return {};
    return presets()[(size_t) index].category;
}

void IroncladProcessor::changeProgramName(int, const juce::String&) {}

void IroncladProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    baseSampleRate    = sampleRate;
    baseBlockSize     = samplesPerBlock;

    // Pitch tracking runs at the base rate on the dry input (cheaper, and pitch
    // needs no oversampling); it aims the feedback bloom.
    pitch.prepare(sampleRate);
    monoBuf.assign((size_t) juce::jmax(1, samplesPerBlock), 0.0f);

    chorus.prepare(sampleRate);
    flanger.prepare(sampleRate);
    phaser.prepare(sampleRate);
    delay.prepare(sampleRate, 2.0);
    reverb.prepare(sampleRate, samplesPerBlock, 2);
    comp.prepare(sampleRate);

    cabConv.prepare({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    cabConv.reset();
    if (irPath.isNotEmpty()) loadIR(juce::File(irPath));   // restore IR after state load

    for (int i = 0; i < kNumOS; ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>>(
            2, (size_t) osStages(i),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversamplers[i]->initProcessing((size_t) samplesPerBlock);
        oversamplers[i]->reset();
    }

    activeOS = (int) apvts.getRawParameterValue("OVERSAMPLE")->load();
    configureEngineForOS(activeOS);

    // Tell the host how much delay the oversampling filters add (reported at the
    // base rate) so it can latency-compensate and stay phase-aligned.
    setLatencySamples((int) std::lround(oversamplers[activeOS]->getLatencyInSamples()));
}

// Re-rate the engine's filters/gate for the given oversampling factor. Only sets
// sample rates and clears state (no allocation, no locks), so it is safe to call
// from the audio thread when the on-screen selector changes.
void IroncladProcessor::configureEngineForOS(int idx)
{
    const int ratio = osRatio(idx);
    engine.prepare(baseSampleRate * ratio, baseBlockSize * ratio);
}

void IroncladProcessor::handleAsyncUpdate()
{
    setLatencySamples(pendingLatency.load());
}

void IroncladProcessor::releaseResources()
{
    for (auto& os : oversamplers)
        if (os != nullptr)
            os->reset();
}

double IroncladProcessor::getHostBpm()
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 0.0)
                    return *bpm;
    return 120.0;
}

void IroncladProcessor::loadIR(const juce::File& f)
{
    if (! f.existsAsFile()) { clearIR(); return; }
    cabConv.loadImpulseResponse(f, juce::dsp::Convolution::Stereo::yes,
                                juce::dsp::Convolution::Trim::yes, 0);
    irPath  = f.getFullPathName();
    irReady = true;
}

void IroncladProcessor::clearIR()
{
    irReady = false;
    irPath  = {};
}

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

    // Track the clean input pitch before the engine overwrites the buffer, and
    // grab the input peak for the on-screen meter.
    {
        const int   ns  = buffer.getNumSamples();
        const auto* L   = buffer.getReadPointer(0);
        const auto* R   = buffer.getReadPointer(totalIn > 1 ? 1 : 0);
        const int   cap = juce::jmin(ns, (int) monoBuf.size());
        float pk = 0.0f;
        for (int i = 0; i < cap; ++i) { monoBuf[(size_t) i] = 0.5f * (L[i] + R[i]); pk = juce::jmax(pk, std::abs(L[i]), std::abs(R[i])); }
        pitch.process(monoBuf.data(), cap);
        engine.setFeedbackTarget(pitch.getFrequency(), pitch.getConfidence());
        inPeak.store(pk, std::memory_order_relaxed);
    }

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
    dp.raw      = apvts.getRawParameterValue("CHARACTER")->load() > 0.5f;
    dp.loose    = apvts.getRawParameterValue("TIGHTLOOSE")->load() > 0.5f;
    dp.dynamics   = apvts.getRawParameterValue("DYNAMICS")->load();
    dp.pickupType = (int) apvts.getRawParameterValue("PICKUP")->load();
    dp.pickupLoad = apvts.getRawParameterValue("PU_LOAD")->load();
    dp.cabType    = (int) apvts.getRawParameterValue("CAB")->load();
    dp.cabBypass  = (apvts.getRawParameterValue("IR_ON")->load() > 0.5f) && irReady.load();
    dp.guitarType  = (int) apvts.getRawParameterValue("GUITARTYPE")->load();
    dp.gtrModel    = apvts.getRawParameterValue("GTR_MODEL")->load();
    dp.gtrOutputDb = apvts.getRawParameterValue("GTR_OUTPUT")->load();
    dp.gtrBody     = apvts.getRawParameterValue("GTR_BODY")->load();
    dp.gtrBright   = apvts.getRawParameterValue("GTR_BRIGHT")->load();
    dp.feedback = apvts.getRawParameterValue("FEEDBACK")->load();
    {
        const int h = (int) apvts.getRawParameterValue("FBHARMONIC")->load();
        dp.fbRatio  = (h == 1 ? 1.5f : (h == 2 ? 2.0f : 1.0f)); // Unison/Fifth/Octave
    }

    // Apply an on-screen oversampling change (re-rate engine + re-report latency).
    const int want = (int) apvts.getRawParameterValue("OVERSAMPLE")->load();
    if (want != activeOS)
    {
        activeOS = want;
        configureEngineForOS(activeOS);   // RT-safe (no allocation)
        pendingLatency.store((int) std::lround(oversamplers[activeOS]->getLatencyInSamples()));
        triggerAsyncUpdate();             // setLatencySamples() on the message thread
    }

    engine.setParameters(dp);

    if (osRatio(activeOS) == 1)
    {
        // Oversampling Off: run the chain at the base rate.
        engine.processBlock(buffer.getWritePointer(0),
                            buffer.getWritePointer(1),
                            buffer.getNumSamples());
    }
    else
    {
        // Upsample -> run the whole chain (waveshaper included) -> downsample.
        auto& os = *oversamplers[activeOS];
        juce::dsp::AudioBlock<float> block(buffer);
        auto osBlock = os.processSamplesUp(block);
        engine.processBlock(osBlock.getChannelPointer(0),
                            osBlock.getChannelPointer(1),
                            (int) osBlock.getNumSamples());
        os.processSamplesDown(block);
    }

    // --- Cabinet IR (replaces the algorithmic cab, which the engine skipped) ---
    if (dp.cabBypass)
    {
        juce::dsp::AudioBlock<float> cabBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> cabCtx(cabBlock);
        cabConv.process(cabCtx);
    }

    // --- Post-amp FX (base rate): chorus -> delay -> reverb --------------
    auto* fxL = buffer.getWritePointer(0);
    auto* fxR = buffer.getWritePointer(1);
    const int fxN = buffer.getNumSamples();

    if (apvts.getRawParameterValue("CHO_ON")->load() > 0.5f)
    {
        chorus.setParameters(apvts.getRawParameterValue("CHO_RATE")->load(),
                             apvts.getRawParameterValue("CHO_DEPTH")->load(),
                             apvts.getRawParameterValue("CHO_MIX")->load());
        chorus.processBlock(fxL, fxR, fxN);
    }

    if (apvts.getRawParameterValue("FLG_ON")->load() > 0.5f)
    {
        flanger.setParameters(apvts.getRawParameterValue("FLG_RATE")->load(),
                              apvts.getRawParameterValue("FLG_DEPTH")->load(),
                              apvts.getRawParameterValue("FLG_FB")->load(),
                              apvts.getRawParameterValue("FLG_MIX")->load());
        flanger.processBlock(fxL, fxR, fxN);
    }

    if (apvts.getRawParameterValue("PHS_ON")->load() > 0.5f)
    {
        phaser.setParameters(apvts.getRawParameterValue("PHS_RATE")->load(),
                             apvts.getRawParameterValue("PHS_DEPTH")->load(),
                             apvts.getRawParameterValue("PHS_FB")->load(),
                             apvts.getRawParameterValue("PHS_MIX")->load());
        phaser.processBlock(fxL, fxR, fxN);
    }

    if (apvts.getRawParameterValue("DLY_ON")->load() > 0.5f)
    {
        float timeSamples;
        if (apvts.getRawParameterValue("DLY_SYNC")->load() > 0.5f)
        {
            static const float qn[5] = { 1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f }; // 1/4,1/8.,1/8,1/8T,1/16
            const int div = (int) apvts.getRawParameterValue("DLY_DIV")->load();
            timeSamples = (float) (60.0 / getHostBpm() * qn[div] * baseSampleRate);
        }
        else
        {
            timeSamples = apvts.getRawParameterValue("DLY_TIME")->load() * 0.001f * (float) baseSampleRate;
        }
        const float tone = apvts.getRawParameterValue("DLY_TONE")->load();
        delay.setParameters(timeSamples,
                            apvts.getRawParameterValue("DLY_FB")->load(),
                            1000.0f * std::pow(16.0f, tone),   // 0..1 -> 1k..16k
                            apvts.getRawParameterValue("DLY_MIX")->load(),
                            apvts.getRawParameterValue("DLY_PING")->load() > 0.5f);
        delay.processBlock(fxL, fxR, fxN);
    }

    if (apvts.getRawParameterValue("RVB_ON")->load() > 0.5f)
    {
        reverb.setParameters((int) apvts.getRawParameterValue("RVB_MODE")->load(),
                             apvts.getRawParameterValue("RVB_SIZE")->load(),
                             apvts.getRawParameterValue("RVB_DAMP")->load(),
                             apvts.getRawParameterValue("RVB_MIX")->load());
        reverb.processBlock(fxL, fxR, fxN);
    }

    if (apvts.getRawParameterValue("CMP_ON")->load() > 0.5f)
    {
        comp.setParameters(apvts.getRawParameterValue("CMP_THRESH")->load(),
                           apvts.getRawParameterValue("CMP_RATIO")->load(),
                           apvts.getRawParameterValue("CMP_ATTACK")->load(),
                           apvts.getRawParameterValue("CMP_RELEASE")->load(),
                           apvts.getRawParameterValue("CMP_MAKEUP")->load(),
                           apvts.getRawParameterValue("CMP_MIX")->load());
        comp.processBlock(fxL, fxR, fxN);
    }

    // Output peak for the meter (after the whole chain, before the demo mute).
    {
        float pk = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            pk = juce::jmax(pk, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
        outPeak.store(pk, std::memory_order_relaxed);
    }

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
    apvts.state.setProperty("irPath", irPath, nullptr);   // remember the loaded cab IR
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void IroncladProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        irPath = apvts.state.getProperty("irPath", "").toString();
        if (irPath.isNotEmpty() && getSampleRate() > 0.0)
            loadIR(juce::File(irPath));   // if already prepared; else prepareToPlay does it
    }
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
