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
- **dsp/DistortionEngine** — per-channel physically-modeled amp chain (all inside
  the oversampled block): pickup -> tight low-cut -> pre-emphasis -> PREAMP stage
  (TYPE-voiced shape with a DYNAMIC, level-dependent bias) -> interstage HP ->
  POWER-AMP stage (tanh) -> output TRANSFORMER (saturation + hysteresis) ->
  post-emphasis -> DC block -> bass/mid/treble tone stack -> presence -> cab
  (resonance + cone-breakup + roll-off, skipped when an IR is loaded) -> high-cut.
  Drive is split across preamp/power (`preStaticGain`=gain^0.72, power=^0.28).
  Also houses the amp-feel and feedback DSP:
    - **NoiseGate** — stereo-linked, pre-drive (gates line noise before the gain)
    - **AmpDynamics** — power-amp sag + pick-attack bite (stereo-linked drive mod)
    - **SpeakerComp** — program-dependent cone compression, post-cab
    - **FeedbackModule** — hybrid synthetic feedback: a harmonic-ladder bloom that
      CLIMBS (moving overlapping window, climb rate ∝ FEEDBACK) + regen resonator,
      injected into the amp input; engages while the note is strong, cuts on mute
      (fast), FX carry the tail. tanh-limited + clipped so it never runs away
- **dsp/PickupStage** — pickup RLC resonance (2-6 kHz) + amp-input load; Single/
  Humbucker/Active/Bass voicings
- **dsp/Transformer** — output-transformer saturation + magnetic hysteresis via a
  direction-of-travel bias into a bounded tanh (unconditionally stable, verified)
- **dsp/PitchTracker** — MPM/NSDF monophonic pitch of the clean input, run at
  BASE rate in the processor; aims the feedback bloom. Verified sub-cent E2..E5
- **dsp/Waveshaper** — four preamp voicings (Clean/Crunch/Lead/Fuzz) + powerAmp()
- **dsp/Biquad** — RBJ cookbook shelves/peaks/pass/band-pass + DC blocker
- Oversampling (juce::dsp::Oversampling) wraps the whole engine in PluginProcessor;
  Off/2x/4x/8x selectable at runtime (real-time-safe), latency reported to the host
- Cabinet IR: juce::dsp::Convolution in PluginProcessor (base rate, post-engine).
  When IR_ON + an IR is loaded, the engine's algorithmic cab is bypassed
  (`DistortionParams::cabBypass`) and the convolution runs instead. IR path is
  saved in the apvts state ("irPath") and reloaded in prepareToPlay/setState.
- **dsp/Delay, dsp/Reverb, dsp/Chorus, dsp/Compressor** — post-amp FX/bus, BASE
  rate in PluginProcessor, order chorus -> delay -> reverb -> compressor. Reverb =
  juce::dsp::Reverb + Schroeder dispersion for Spring. Delay tempo-syncs (getHostBpm).
  Compressor is a soft-knee comp/limiter (ratio to 20:1) on the output bus.
- **UI (PluginEditor)** — the LCD is a live-drawn multi-page panel: a global PRESET
  selector + category dropdown at the top, bottom tabs switching AMP / DELAY /
  REVERB / CHORUS / COMP / CAB pages (per-page controls shown/hidden via
  updatePageVisibility(); pill hit-zones are transparent TextButtons over drawn pills).

## Presets
Category-based, defined in `presets()` in PluginProcessor.cpp: each is {name,
category, list of param overrides}. setCurrentProgram resets all `managedParamIDs`
to their defaults then applies the overrides (so presets only list non-defaults and
nothing bleeds between them). The editor's category dropdown + within-category
steppers browse them. OVERSAMPLE and LEVEL are user/global, not preset-managed.

## Parameters
TYPE (Clean/Crunch/Lead/Fuzz), DRIVE, BASS, MID, TREBLE, PRESENCE, TIGHT (pre-drive
low-cut), HIGHCUT, MIX (dry/wet), OUTPUT (wet makeup), LEVEL (master trim), GATE,
CHARACTER (Smooth/Raw), TIGHTLOOSE, OVERSAMPLE (Off/2x/4x/8x), DYNAMICS (amp feel:
sag/pick/speaker comp), FEEDBACK (0..1), FBHARMONIC (Unison/Fifth/Octave).
PICKUP (Single/Humbucker/Active/Bass), PU_LOAD (amp-input load), CAB (1x12/2x12/
4x12 British/4x12 Modern), IR_ON.
FX: DLY_ON/SYNC/DIV/TIME/FB/TONE/MIX/PING, RVB_ON/MODE/SIZE/DAMP/MIX, CHO_ON/RATE/
DEPTH/MIX, CMP_ON/THRESH/RATIO/ATTACK/RELEASE/MAKEUP/MIX.
Presets list only non-default overrides; `managedParamIDs` (in PluginProcessor.cpp)
is the set reset-to-default before each preset applies — add new preset-managed
params there. OVERSAMPLE, LEVEL, and IR_ON are user/global (not preset-managed).

## Requirements
- Visual Studio 2022 with C++ Desktop workload
- CMake 3.22+
- No external dependencies beyond JUCE (auto-fetched)

## Notes
- This is an EFFECT plugin (IS_SYNTH FALSE), not a synth — audio passes through
- Never use buffer.clear() on the input in processBlock — it destroys the input audio
- Never auto-copy VST3 to Program Files (no admin); Cort copies manually
- Test via Standalone first; FL Studio requires closing/reopening to rescan VST3
- Drive chain runs at 4x oversampling (juce::dsp::Oversampling, polyphase IIR)
  wrapped around the whole engine in PluginProcessor; latency is reported to the
  host. Quality is fixed at 4x for now (user-selectable Eco/Normal/High modes TBD)
- Deploy with ./deploy.ps1 (NOT a raw Copy-Item — that nests the bundle and leaves
  a stale binary); UI shows a version + build-time stamp bottom-right to confirm
