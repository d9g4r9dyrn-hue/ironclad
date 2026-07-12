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
  (resonance + cone-breakup + roll-off, skipped when an IR is loaded) -> high-cut ->
  TAIL HUSH (per-channel: a level-follower blends toward a ~3.6 kHz low-pass as a note
  decays, so the amplified noise floor doesn't hiss in the sustain; bright while playing,
  ~55% darkening in the quiet tail - does NOT cut sustain like a gate).
  Drive is split across preamp/power (`preStaticGain`=gain^0.72, power=^0.28).
  Also houses the amp-feel and feedback DSP:
    - **NoiseGate** — stereo-linked, pre-drive (gates line noise before the gain)
    - **AmpDynamics** — power-amp sag + pick-attack bite (stereo-linked drive mod)
    - **SpeakerComp** — program-dependent cone compression, post-cab
    - **FeedbackModule** — hybrid synthetic feedback: a harmonic-ladder bloom that
      CLIMBS (moving overlapping window, climb rate ∝ FEEDBACK) + regen resonator,
      injected into the amp input; engages while the note is strong, cuts on mute
      (fast), FX carry the tail. tanh-limited + clipped so it never runs away
- **dsp/GuitarSource** — GUITAR SOURCE MODEL at the FRONT of the per-channel chain
  (before pickup/tightHP). Seven archetype profiles (S-Type SC, Single-Cut HB, Modern
  HB, T-Type SC, Semi-Hollow, P-90, Extended-Range) each baking: source output level,
  pre-distortion HP (tightness), low-mid bell (body/scoop), pickup resonance peak,
  presence + top shelves, an optional body mode (semi-hollow bloom), and a feedback-
  sensitivity multiplier. GTR_MODEL (0..1) blends the character in from neutral;
  GTR_OUTPUT/GTR_BODY/GTR_BRIGHT are user trims. Because it changes gain staging and
  the bass hitting the clipper, the same preset REACTS differently per guitar. Feedback
  sensitivity feeds the FeedbackModule (semi-hollows bloom more, tight moderns less).
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
- **dsp/Delay, dsp/Reverb, dsp/Chorus, dsp/Flanger, dsp/Phaser, dsp/Compressor** —
  post-amp FX/bus, BASE rate in PluginProcessor, order chorus -> flanger -> phaser ->
  delay -> reverb -> compressor. Flanger = short modulated delay (~1.8 ms centre, up
  to ~4.5 ms swing) with regeneration. Phaser = 6-stage swept all-pass cascade (log
  250 Hz–2.2 kHz) with feedback; both L/R LFOs 90° apart. Reverb = juce::dsp::Reverb +
  Schroeder dispersion for Spring. Delay tempo-syncs (getHostBpm). Compressor is a
  soft-knee comp/limiter (ratio to 20:1) on the output bus.
- **UI (PluginEditor)** — rebuilt to match the mockup (images/ironclad_background.png):
  one wide hardware chassis in a **1536x576 design space** (ic::kBaseW/H), uniformly
  letterboxed by ic::Viewport (NEVER setFixedAspectRatio - it hangs FL). paintChassis()
  draws the shell: angled red panels (makePanelPath + paintRedPanel clips the brushed-red
  texture to the path), top mesh vent, side armour, bottom logo plate (paintWordmark),
  flanking vents. LEFT panel = six tone knobs (DRIVE/TONE/BASS/MID/PRESENCE/OUTPUT,
  labels above, no persistent values). RIGHT panel = DISTORTION CHARACTER + four sliders
  (LOW CUT/HIGH CUT/MIX/LEVEL) + TIGHT/LOOSE + GATE. CENTRE screen has a global preset
  row (arrows + a hamburger presetMenuBtn -> showPresetMenu category submenus) and a
  5-icon nav bar (navBtns) switching pages via updateNavVisibility() + paintNavContent():
    - Distortion: mode stepper, big DRIVE%, oversampling.
    - Effects: a 7-way sub-selector (fxSelBtns: DYN/CHO/FLG/PHS/DLY/RVB/CMP) that shows
      that effect's controls (dynamics+feedback+voice / chorus / flanger / phaser /
      delay / reverb / comp).
    - Analysis: the live transfer curve.
    - Cabinet: guitar-type dropdown (icon PopupMenu) + GTR_* knobs + pickup/cab pills + IR.
    - Settings: placeholder.
  Every control stays instantiated + attached; pages just show/hide + reposition them.
  Pill hit-zones are transparent TextButtons; paintNavContent draws the pill visuals from
  each button's live bounds. guitar_types_2_cut.png (1024x1536, 7 rows, white bg flood-
  filled to transparent) is embedded and sliced per-type via getClippedImage in the ctor
  (gtrIconSrc/gtrPhotoSrc). TODO: animate the input/output meters (currently static) from
  processor level atomics; finish switch/typography polish; DPI-test in FL.

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
GUITARTYPE (S-Type SC/Single-Cut HB/Modern HB/T-Type SC/Semi-Hollow/P-90/Extended
Range), GTR_MODEL (character depth), GTR_OUTPUT (source level dB), GTR_BODY, GTR_BRIGHT.
PICKUP (Single/Humbucker/Active/Bass), PU_LOAD (amp-input load), CAB (1x12/2x12/
4x12 British/4x12 Modern), IR_ON.
FX: DLY_ON/SYNC/DIV/TIME/FB/TONE/MIX/PING, RVB_ON/MODE/SIZE/DAMP/MIX, CHO_ON/RATE/
DEPTH/MIX, FLG_ON/RATE/DEPTH/FB/MIX, PHS_ON/RATE/DEPTH/FB/MIX, CMP_ON/THRESH/RATIO/
ATTACK/RELEASE/MAKEUP/MIX.
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
