#pragma once
#include "Biquad.h"
#include "Waveshaper.h"
#include "PickupStage.h"
#include "Transformer.h"
#include <algorithm>
#include <cmath>

struct DistortionParams
{
    int   type     = 0;         // Waveshaper::Mode
    float drive    = 0.5f;      // 0..1
    float bass     = 0.0f;      // dB, -12..+12
    float mid      = 0.0f;      // dB, -12..+12
    float treble   = 0.0f;      // dB, -12..+12
    float presence = 0.5f;      // 0..1 -> post high-shelf brightness
    float tight    = 80.0f;     // Hz, pre-drive high-pass (low-cut)
    float highCut  = 20000.0f;  // Hz, post-chain low-pass (high-cut); 20k ~= open
    float mix      = 1.0f;      // 0..1 dry/wet
    float output   = 1.0f;      // 0..2 wet makeup gain, applied pre-mix
    float level    = 1.0f;      // linear master trim, applied post-mix
    bool  gate     = false;     // input noise gate on/off
    bool  raw      = false;     // CHARACTER: false=Smooth (darker), true=Raw (brighter/hotter)
    bool  loose    = false;     // TIGHT/LOOSE: false=Tight (focused), true=Loose (fatter lows)
    float dynamics = 0.5f;      // 0..1 amp "feel": power-amp sag, pick attack, speaker compression
    float feedback = 0.0f;      // 0..1 synthetic feedback amount (bloom + regenerative grit)
    float fbRatio  = 1.0f;      // feedback harmonic: 1=unison, 1.5=fifth, 2=octave
    int   pickupType = 0;       // PickupStage::Type (Single/Humbucker/Active/Bass)
    float pickupLoad = 0.6f;    // 0..1 amp-input load / brightness
    int   cabType    = 0;       // 0=1x12 Open, 1=2x12, 2=4x12 British, 3=4x12 Modern
    bool  cabBypass  = false;   // true when an external IR replaces the algorithmic cab
};

// Stereo-linked noise gate placed ahead of the drive stage, so hiss is silenced
// before the waveshaper amplifies it. Hysteresis (separate open/close thresholds)
// keeps it from chattering on signals hovering right at the threshold, and the
// gain ramp (fast attack, slow release) keeps transitions click-free.
class NoiseGate
{
public:
    void prepare(double sampleRate)
    {
        fs         = sampleRate;
        envRelCoef = std::exp(-1.0f / (float) (0.010 * fs)); // 10 ms envelope decay
        attCoef    = 1.0f - std::exp(-1.0f / (float) (0.002 * fs)); //   2 ms open ramp
        relCoef    = 1.0f - std::exp(-1.0f / (float) (0.120 * fs)); // 120 ms close ramp
        clear();
    }

    void clear() { env = 0.0f; gainState = 1.0f; open = true; }

    void setEnabled(bool e) { enabled = e; }

    // linkedAbs = max(|L|, |R|); returns the gain to apply to both channels.
    float processGain(float linkedAbs)
    {
        if (! enabled)
            return 1.0f;

        env = std::max(linkedAbs, env * envRelCoef);

        if (open) { if (env < closeThresh) open = false; }
        else      { if (env > openThresh)  open = true;  }

        const float target = open ? 1.0f : 0.0f;
        const float coef   = (target > gainState) ? attCoef : relCoef;
        gainState += (target - gainState) * coef;
        return gainState;
    }

private:
    double fs = 44100.0;
    bool   enabled = false;
    bool   open = true;
    float  env = 0.0f, gainState = 1.0f;
    float  envRelCoef = 0.0f, attCoef = 1.0f, relCoef = 1.0f;
    // Set to sit above a typical input line-noise floor but below real playing,
    // so high-gain presets don't amplify hiss into a "feedback" screech between
    // notes. (A user-adjustable threshold is the proper long-term control.)
    static constexpr float openThresh  = 0.0089f; // ~ -41 dBFS
    static constexpr float closeThresh = 0.0032f; // ~ -50 dBFS
};

// Power-amp "feel": stereo-linked dynamics that make the drive respond to how
// hard you play, instead of a static gain. Two envelopes of the input:
//   * a slow one models power-supply SAG - sustained energy dips the drive a
//     touch, then blooms back as the note settles (the springy tube feel);
//   * a fast one minus the slow one is the PICK transient - a brief bite added
//     right at the attack. Returns a multiplier applied to the drive gain.
class AmpDynamics
{
public:
    void prepare(double fs)
    {
        slowAtk = 1.0f - std::exp(-1.0f / (float) (0.005 * fs));  //   5 ms
        slowRel = 1.0f - std::exp(-1.0f / (float) (0.240 * fs));  // 240 ms bloom
        fastAtk = 1.0f - std::exp(-1.0f / (float) (0.0008 * fs)); // 0.8 ms
        fastRel = 1.0f - std::exp(-1.0f / (float) (0.030 * fs));  //  30 ms
        clear();
    }
    void clear() { slowEnv = fastEnv = 0.0f; }
    void setAmount(float a) { amount = a; }

    float process(float linkedAbs)
    {
        const float a = linkedAbs;
        slowEnv += (a > slowEnv ? slowAtk : slowRel) * (a - slowEnv);
        fastEnv += (a > fastEnv ? fastAtk : fastRel) * (a - fastEnv);
        const float transient = std::max(0.0f, fastEnv - slowEnv);
        const float sag  = 1.0f / (1.0f + amount * 2.6f * slowEnv);   // compress + bloom
        const float pick = 1.0f + amount * 2.2f * transient;          // attack bite
        return sag * pick;
    }
private:
    float slowEnv = 0, fastEnv = 0, amount = 0.5f;
    float slowAtk = 0, slowRel = 0, fastAtk = 1, fastRel = 1;
};

// Speaker cone compression: a real cone stops moving linearly as it's driven
// hard, gently compressing peaks. Stereo-linked, program-dependent soft squeeze
// applied to the wet signal after the cabinet stage.
class SpeakerComp
{
public:
    void prepare(double fs)
    {
        atk = 1.0f - std::exp(-1.0f / (float) (0.003 * fs));  //  3 ms
        rel = 1.0f - std::exp(-1.0f / (float) (0.120 * fs));  // 120 ms
        clear();
    }
    void clear() { env = 0.0f; }
    void setAmount(float a) { amount = a; }

    float process(float linkedAbs)
    {
        env += (linkedAbs > env ? atk : rel) * (linkedAbs - env);
        const float over = env > thresh ? env - thresh : 0.0f;
        return 1.0f / (1.0f + amount * 1.6f * over);
    }
private:
    float env = 0, amount = 0.5f, atk = 1, rel = 1;
    static constexpr float thresh = 0.5f;
};

// Synthetic feedback (hybrid). There is no acoustic loop in a plugin, so feedback
// is generated two ways and injected into the amp input (so it distorts and sings
// like the real thing):
//   * BLOOM - a deterministic oscillator at a chosen harmonic of the tracked note
//     (unison/5th/octave) that blooms in slowly while a note is held, the way a
//     string feeds back off a loud cab. Musical and always in key.
//   * REGEN - a resonant band-pass on the wet output, tuned to the same harmonic,
//     fed back through the amp for organic grit and a room-like resonance.
// A tanh limiter on the injection plus the waveshaper's own clipping keep the
// loop from ever running away - it sustains, it doesn't explode.
class FeedbackModule
{
public:
    void prepare(double sampleRate)
    {
        fs       = sampleRate;
        bloomAtk = 1.0f - std::exp(-1.0f / (float) (0.25 * fs));  // feedback builds fairly quick
        bloomRel = 1.0f - std::exp(-1.0f / (float) (0.035 * fs)); // ~35 ms: the frozen peak rings out
                                                                  // (climb is already frozen on mute, so no new harmonics)
        inAtk    = 1.0f - std::exp(-1.0f / (float) (0.002 * fs));
        inRel    = 1.0f - std::exp(-1.0f / (float) (0.008 * fs)); // fast: muting freezes the climb + cuts now
        climbInc = (float) (kNumHarm - 1) / (float) (kClimbSec * fs);  // full-speed rate; scaled by amount
        reson.setSampleRate(fs);
        relatch();
        clear();
    }
    void clear()
    {
        bloomEnv = 0.0f; inEnv = 0.0f; sustaining = false; prevSustaining = false; climb = 0.0f;
        for (auto& p : hPhase) p = 0.0;
        reson.clear();
    }

    void setAmount(float a) { amount = a; }
    void setRatio (float r) { ratio = r; }

    // Called once per block from the base-rate pitch tracker.
    void setTarget(float freqHz, float conf)
    {
        voiced = (freqHz > 0.0f && conf > 0.6f);
        if (voiced) pendingFreq = freqHz;
    }

    // Per sample: prevWet = last output (mono), inLevel = |dry amp input|.
    float process(float prevWet, float inLevel)
    {
        if (amount <= 0.0f) return 0.0f;

        inEnv += (inLevel > inEnv ? inAtk : inRel) * (inLevel - inEnv);
        // Feedback is a loop-gain threshold: it engages while the note is STRONG and
        // cuts as the string amplitude falls (not lingering into the quiet tail),
        // so it stops climbing/evolving once you release or mute. Hysteresis keeps
        // it from flickering while it's clearly sounding.
        if      (inEnv > 0.040f) sustaining = true;
        else if (inEnv < 0.020f) sustaining = false;

        // Latch the pitch + reset the climb ONLY at a note onset (mute -> sounding),
        // never mid-note or during the release. That way the tail just fades the
        // existing harmonic stack instead of snapping to a lone pure harmonic (which
        // the amp gain turned into a loud odd tone at the very end).
        if (sustaining && ! prevSustaining) { climb = 0.0f; if (voiced) relatch(); }
        prevSustaining = sustaining;

        const float target = (sustaining && pendingFreq > 0.0f) ? amount : 0.0f;
        bloomEnv += (target > bloomEnv ? bloomAtk : bloomRel) * (target - bloomEnv);

        // Climb while sustaining. A moving raised window over the harmonic ladder
        // slides UP as the note rings, so the EMPHASIS rises in pitch (fundamental
        // -> octave -> 12th -> 2 octaves) while adjacent rungs overlap/crossfade -
        // it laddders upward without ever bending or landing on a lone pure tone.
        // Climb rate scales with FEEDBACK amount: heavy feedback (standing at the
        // amp) climbs hard and fast while the note sustains; light feedback creeps.
        if (bloomEnv > 0.08f && sustaining)
            climb = std::min(climb + amount * climbInc, (float) (kNumHarm - 1));

        float sum = 0.0f, wsum = 0.0f;
        for (int k = 0; k < kNumHarm; ++k)
        {
            hPhase[k] += hInc[k];
            if (hPhase[k] >= twoPi) hPhase[k] -= twoPi;
            const float w = std::max(0.0f, 1.0f - std::abs(climb - (float) k) / kWindow) * kHarmAmp[k];
            sum  += w * std::sin((float) hPhase[k]);
            wsum += w;
        }
        const float osc = wsum > 1.0e-6f ? sum / wsum : 0.0f;   // weighted avg -> bounded

        const float bloomSig = bloomEnv * kBloomInject * osc;
        const float regenSig = kRegenGain * bloomEnv * reson.process(prevWet);

        // Soft-limit the injection so the amp loop sustains instead of exploding.
        return std::tanh((bloomSig + regenSig) * 4.0f) * 0.25f;
    }

private:
    // Latch the harmonic ladder (base*1,2,3,4) to the current note + voice.
    void relatch()
    {
        const float base = std::clamp(pendingFreq * ratio, 20.0f, (float) (fs * 0.45));
        const float top  = std::min((float) (fs * 0.45), 3800.0f);   // cap the top rung so it never gets shrill
        for (int k = 0; k < kNumHarm; ++k)
        {
            hFreq[k] = std::min(base * (float) (k + 1), top);
            hInc[k]  = twoPi * (double) hFreq[k] / fs;
        }
        reson.setBandPass(hFreq[0], kResonQ);   // fixed for the note (no mid-note retune)
    }

    Biquad reson;
    static constexpr int kNumHarm = 3;   // climb f0 -> 2f0 -> 3f0 (top stays musical, not shrill)
    double fs = 44100.0;
    double hPhase[kNumHarm] { 0, 0, 0 };
    double hInc  [kNumHarm] { 0, 0, 0 };
    float  hFreq [kNumHarm] { 0, 0, 0 };
    float  amount = 0.0f, ratio = 1.0f, pendingFreq = 110.0f;
    float  bloomEnv = 0, inEnv = 0, climb = 0, climbInc = 0;
    float  bloomAtk = 0, bloomRel = 0, inAtk = 1, inRel = 1;
    bool   voiced = false, sustaining = false, prevSustaining = false;
    static constexpr float  kHarmAmp[kNumHarm] { 1.0f, 1.0f, 0.9f }; // very slight top taper
    static constexpr float  kWindow      = 1.6f;    // harmonics within this many rungs overlap
    static constexpr float  kBloomInject = 0.06f;
    static constexpr float  kRegenGain   = 0.04f;   // just a touch of grit; too high self-oscillates
    static constexpr float  kResonQ      = 4.0f;    // lower Q -> ring decays faster in the tail
    static constexpr float  kClimbSec    = 0.6f;    // full-speed climb time (at FEEDBACK = max)
    static constexpr double twoPi        = 6.283185307179586;
};

// Amp-style distortion chain, per channel:
//   tight low-cut -> input gain -> waveshaper -> DC block -> tone stack
//   (bass/mid/treble) -> presence -> cabinet low-pass -> high-cut
// then wet makeup gain, dry/wet mix, and master level. A stereo-linked noise
// gate sits in front of the whole thing. Stereo is two independent channel chains.
class IroncladEngine
{
public:
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;
        for (auto& c : chans)
        {
            c.pickup.setSampleRate(sr);
            c.tightHP.setSampleRate(sr);
            c.focus.setSampleRate(sr);
            c.preEmph.setSampleRate(sr);
            c.interHP.setSampleRate(sr);
            c.postEmph.setSampleRate(sr);
            c.bass.setSampleRate(sr);
            c.mid.setSampleRate(sr);
            c.treble.setSampleRate(sr);
            c.presence.setSampleRate(sr);
            c.cabRes.setSampleRate(sr);
            c.cabBreak.setSampleRate(sr);
            c.cab.setSampleRate(sr);
            c.highCut.setSampleRate(sr);
            c.xfmr.prepare(sr);
        }
        // Fast follower driving the dynamic (level-dependent) transfer curve.
        dynAtk = 1.0f - std::exp(-1.0f / (float) (0.001 * sr));  //  1 ms
        dynRel = 1.0f - std::exp(-1.0f / (float) (0.050 * sr));  // 50 ms
        gate.prepare(sr);
        ampDyn.prepare(sr);
        spkComp.prepare(sr);
        feedback.prepare(sr);
        clear();
    }

    // Aim the feedback bloom at the note the base-rate pitch tracker found.
    void setFeedbackTarget(float freqHz, float confidence)
    {
        feedback.setTarget(freqHz, confidence);
    }

    void clear()
    {
        for (auto& c : chans)
        {
            c.pickup.clear();
            c.tightHP.clear();
            c.focus.clear();
            c.preEmph.clear();
            c.interHP.clear();
            c.postEmph.clear();
            c.bass.clear();
            c.mid.clear();
            c.treble.clear();
            c.presence.clear();
            c.cabRes.clear();
            c.cabBreak.clear();
            c.cab.clear();
            c.highCut.clear();
            c.xfmr.clear();
            c.dc.clear();
            c.dynEnv = 0.0f;
        }
        gate.clear();
        ampDyn.clear();
        spkComp.clear();
        feedback.clear();
        prevWetMono = 0.0f;
    }

    void setParameters(const DistortionParams& p)
    {
        params = p;

        const float tight = std::clamp(p.tight, 20.0f, 500.0f);
        const float hiCut = std::clamp(p.highCut, 1000.0f, 20000.0f);
        // Presence knob (0..1) maps to a 0..+9 dB high shelf around 4.5 kHz.
        const float presDb = p.presence * 9.0f;
        // TIGHT/LOOSE adds a pre-drive low shelf: Tight is flat/focused, Loose
        // pushes more low end into the waveshaper for a fatter, looser feel.
        const float focusDb = p.loose ? 3.5f : 0.0f;
        // Cabinet voicing per type: a low resonance (thump), a cone-BREAKUP peak in
        // the upper mids (the speaker "honk" that a plain low-pass can't give), and
        // a roll-off. CHARACTER (Raw) opens the roll-off for more edge.
        struct CabV { float roll, resHz, resDb, brkHz, brkDb; };
        static const CabV cabs[4] = {
            { 8000.0f,  95.0f, 3.0f, 2800.0f, 2.0f },   // 1x12 Open Back
            { 7000.0f, 100.0f, 4.0f, 2500.0f, 3.0f },   // 2x12 Combo
            { 6000.0f, 105.0f, 5.0f, 2200.0f, 4.0f },   // 4x12 British
            { 6800.0f, 100.0f, 4.0f, 3000.0f, 3.0f },   // 4x12 Modern
        };
        const CabV& cv = cabs[std::clamp(p.cabType, 0, 3)];
        const float cabHz = cv.roll + (p.raw ? 1000.0f : 0.0f);

        for (auto& c : chans)
        {
            c.pickup.setParameters(p.pickupType, p.pickupLoad);
            c.tightHP.setHighPass(tight, 0.707f);
            c.focus.setLowShelf(180.0f, focusDb);
            // Pre-emphasis brightens what hits the clipper (so distortion is more
            // articulate); a highpass couples the two stages (tube coupling cap,
            // tightens the lows before the power stage); post-emphasis tilts most of
            // the pre-emphasis back, leaving a touch of extra presence in the grind.
            c.preEmph.setHighShelf(1200.0f, 4.5f);
            c.interHP.setHighPass(150.0f, 0.707f);
            c.postEmph.setHighShelf(1200.0f, -3.0f);
            // Output transformer pushes harder in Raw (more saturation + hysteresis).
            c.xfmr.setAmount(p.raw ? 0.45f : 0.30f);
            c.bass.setLowShelf(120.0f, p.bass);
            c.mid.setPeak(750.0f, 0.7f, p.mid);
            c.treble.setHighShelf(3000.0f, p.treble);
            c.presence.setHighShelf(4500.0f, presDb);
            // Cab body resonance (thump) + cone-breakup honk + roll-off.
            c.cabRes.setPeak(cv.resHz, 1.1f, cv.resDb);
            c.cabBreak.setPeak(cv.brkHz, 1.6f, cv.brkDb);
            c.cab.setLowPass(cabHz, 0.707f);
            // User high-cut sits after the cab for a final treble tailoring.
            c.highCut.setLowPass(hiCut, 0.707f);
        }

        gate.setEnabled(p.gate);
        // Amp feel: sag/pick and speaker compression scale with the DYNAMICS macro.
        ampDyn.setAmount(p.dynamics);
        spkComp.setAmount(p.dynamics);
        feedback.setAmount(p.feedback);
        feedback.setRatio(p.fbRatio);
        // Raw runs the waveshaper a little hotter for more grind.
        gain = Waveshaper::driveGain(p.type, p.drive) * (p.raw ? 1.2f : 1.0f);
        // Split the drive across a preamp stage and a power-amp stage (product ~=
        // gain, so total distortion stays in the same ballpark but now comes from
        // two cascaded stages, which is what a real amp does).
        preStaticGain   = std::pow(gain, 0.72f);
        powerStaticGain = std::pow(gain, 0.28f);
    }

    void processBlock(float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            // Stereo-linked front: gate the dry input.
            const float g    = gate.processGain(std::max(std::abs(left[i]), std::abs(right[i])));
            const float dryL = left[i]  * g;
            const float dryR = right[i] * g;

            // Inject synthetic feedback into the amp INPUT only (not the dry-mix
            // path), so it distorts through the whole amp like real feedback.
            const float fb   = feedback.process(prevWetMono, std::max(std::abs(dryL), std::abs(dryR)));
            const float inL  = dryL + fb;
            const float inR  = dryR + fb;

            // Sag/pick track the DRY input (not the injected feedback), so feedback
            // and sag can't pump each other into a wobble that cuts the tone out.
            // The multiplier feeds the preamp drive inside processWet (gain is split
            // there across the preamp/power stages).
            const float dyn = ampDyn.process(std::max(std::abs(dryL), std::abs(dryR)));

            float wl = processWet(chans[0], inL, dyn);
            float wr = processWet(chans[1], inR, dyn);

            // Stereo-linked speaker compression on the wet signal after the cab.
            const float cg = spkComp.process(std::max(std::abs(wl), std::abs(wr)));
            wl *= cg;
            wr *= cg;
            prevWetMono = 0.5f * (wl + wr);   // feeds the regenerative resonator

            // Wet makeup, dry/wet mix (dry stays clean), master level, and a base
            // gain-staging trim so the bounded-but-boosted amp output sits at a sane
            // level instead of slamming 0 dBFS (the tone stack can add a lot of gain).
            left[i]  = (dryL * (1.0f - params.mix) + wl * params.output * params.mix) * params.level * kOutTrim;
            right[i] = (dryR * (1.0f - params.mix) + wr * params.output * params.mix) * params.level * kOutTrim;
        }
    }

private:
    struct Channel
    {
        PickupStage pickup;
        Biquad   tightHP, focus, preEmph, interHP, postEmph, bass, mid, treble, presence,
                 cabRes, cabBreak, cab, highCut;
        Transformer xfmr;
        DCBlocker dc;
        float    dynEnv = 0.0f;   // per-channel level follower for the dynamic transfer curve
    };

    // One channel through the amp: pre-drive filters -> dynamic drive gain ->
    // waveshaper -> DC block -> tone stack -> cabinet body/roll-off -> high-cut.
    // Returns the wet signal before makeup/mix/level (done stereo-linked outside).
    float processWet(Channel& c, float in, float dyn)
    {
        float x = c.pickup.process(in);   // pickup RLC resonance + amp-input load
        x = c.tightHP.process(x);
        x = c.focus.process(x);
        x = c.preEmph.process(x);         // pre-emphasis

        // --- Preamp stage: voiced by TYPE, with a DYNAMIC transfer curve ------
        // Follow the PLAYING level (pre-gain) and add a level-dependent bias into the
        // shaper, so the clip SHAPE shifts with how hard you pick (more even-harmonic
        // asymmetry when you dig in) - the curve changes, not just its input gain.
        const float a = std::abs(x);
        c.dynEnv += (a > c.dynEnv ? dynAtk : dynRel) * (a - c.dynEnv);
        const float bias = kDynBias * std::min(c.dynEnv / kDynRef, 1.0f);

        x *= preStaticGain * dyn;
        x = Waveshaper::shape(params.type, x + bias) - Waveshaper::shape(params.type, bias);

        // --- Interstage coupling + power-amp stage + output transformer ------
        x = c.interHP.process(x);
        x = Waveshaper::powerAmp(x * powerStaticGain);
        x = c.postEmph.process(x);        // post-emphasis
        x = c.xfmr.process(x);            // output transformer saturation + hysteresis

        x = c.dc.process(x);
        x = c.bass.process(x);
        x = c.mid.process(x);
        x = c.treble.process(x);
        x = c.presence.process(x);
        // Algorithmic cab (skipped when an external IR is taking over the cab).
        if (! params.cabBypass)
        {
            x = c.cabRes.process(x);
            x = c.cabBreak.process(x);
            x = c.cab.process(x);
        }
        x = c.highCut.process(x);
        return x;
    }

    Channel chans[2];
    NoiseGate gate;
    AmpDynamics ampDyn;
    SpeakerComp spkComp;
    FeedbackModule feedback;
    DistortionParams params;
    float  gain = 1.0f;
    float  preStaticGain = 1.0f, powerStaticGain = 1.0f;   // drive split preamp/power
    float  dynAtk = 0.0f, dynRel = 0.0f;                   // dynamic-curve follower coeffs
    float  prevWetMono = 0.0f;
    double sr   = 44100.0;
    static constexpr float kOutTrim  = 0.6f;   // base gain-staging trim (transformer also tames peaks)
    static constexpr float kDynBias  = 0.35f;  // max level-dependent bias (dynamic curve depth)
    static constexpr float kDynRef   = 0.30f;  // playing level at which the dynamic bias saturates
};
