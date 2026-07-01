# Ironclad - VST3 Distortion Plugin

Amp-style distortion. Started from the CortVerb reverb project's architecture
(CMake/JUCE setup, APVTS + factory presets, Gumroad licensing + demo mute).

## Build System
- CMake 3.22+ with JUCE fetched via FetchContent (v8.0.4)
- Targets: VST3 and Standalone
- Build: `"C:\Program Files\CMake\bin\cmake.exe" -B build -G "Visual Studio 17 2022"` then `cmake --build build --config Release`

## Architecture
- **PluginProcessor** — Audio effect processor (stereo in/out), manages APVTS
- **PluginEditor** — PLACEHOLDER barebones UI (generic controls only). Real UI
  comes from Cort's mockup; do not build custom look & feel until then.
- **dsp/DistortionEngine** — per-channel chain: tight low-cut -> input gain ->
  waveshaper -> DC block -> bass/mid/treble tone stack -> presence -> cabinet LP
- **dsp/Waveshaper** — four voicings (Clean/Crunch/Lead/Fuzz)
- **dsp/Biquad** — RBJ cookbook shelves/peaks/pass filters + DC blocker

## Parameters
TYPE (Clean/Crunch/Lead/Fuzz), DRIVE, BASS, MID, TREBLE, PRESENCE, TIGHT (pre-drive
low-cut), MIX (dry/wet), OUTPUT

## Requirements
- Visual Studio 2022 with C++ Desktop workload
- CMake 3.22+
- No external dependencies beyond JUCE (auto-fetched)

## Notes
- This is an EFFECT plugin (IS_SYNTH FALSE), not a synth — audio passes through
- Never use buffer.clear() on the input in processBlock — it destroys the input audio
- Never auto-copy VST3 to Program Files (no admin); Cort copies manually
- Test via Standalone first; FL Studio requires closing/reopening to rescan VST3
- No oversampling yet — heavy drive will alias; revisit before shipping
