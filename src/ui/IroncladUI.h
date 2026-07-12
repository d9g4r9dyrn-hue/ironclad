#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include <map>

// ============================================================================
//  Ironclad image-backed UI toolkit
//
//  Everything here works in the artwork's canonical source-pixel coordinate
//  system (1774 x 887). The editor computes a single uniform letterbox scale
//  and every layer / control is positioned from source-pixel rectangles taken
//  straight from the UI package manifest, so the skin stays pixel-accurate at
//  any window size.
//
//  Header-only on purpose: no new .cpp means no CMakeLists churn.
// ============================================================================

namespace ic
{
    // Canonical design space matches the mockup faceplate proportions (~2.65:1).
    constexpr float kBaseW = 1536.0f;
    constexpr float kBaseH = 576.0f;

    // ---- design colours (from the component-sheet palette) -----------------
    namespace Colours
    {
        const juce::Colour glowRed    { 0xffff2a2a };   // GLOW RED
        const juce::Colour darkRed     { 0xffc4121b };   // DARK RED
        const juce::Colour brushedRed { 0xff8b1119 };   // BRUSHED RED
        const juce::Colour blackMetal { 0xff0d0d0f };   // BLACK METAL
        const juce::Colour darkGray    { 0xff1a1a1d };   // DARK GRAY
        const juce::Colour gunmetal    { 0xff2b2b31 };   // GUNMETAL
        const juce::Colour chrome      { 0xffc7c7cc };   // CHROME
        const juce::Colour label      { 0xffe0e0e4 };   // LIGHT SILVER
        const juce::Colour screenBg   { 0xff0d0d0f };
        const juce::Colour muted       { 0xff9a9a9f };
    }

    // ------------------------------------------------------------------------
    //  Uniform scale + centred letterbox transform, shared by paint() and
    //  every control's bounds so nothing drifts relative to the artwork.
    // ------------------------------------------------------------------------
    struct Viewport
    {
        float scale = 1.0f;
        float offX  = 0.0f;
        float offY  = 0.0f;

        void update (int w, int h)
        {
            scale = juce::jmin ((float) w / kBaseW, (float) h / kBaseH);
            offX  = ((float) w - kBaseW * scale) * 0.5f;
            offY  = ((float) h - kBaseH * scale) * 0.5f;
        }

        juce::Rectangle<int> map (float x, float y, float w, float h) const
        {
            return { juce::roundToInt (offX + x * scale),
                     juce::roundToInt (offY + y * scale),
                     juce::roundToInt (w * scale),
                     juce::roundToInt (h * scale) };
        }

        juce::Rectangle<int> map (const juce::Rectangle<float>& r) const
        {
            return map (r.getX(), r.getY(), r.getWidth(), r.getHeight());
        }

        // Design-space -> screen-space transform, for painting juce::Path shapes
        // (angled panels) that map() can't express as a rectangle.
        juce::AffineTransform getTransform() const
        {
            return juce::AffineTransform::scale (scale).translated (offX, offY);
        }
    };

    // ------------------------------------------------------------------------
    //  Serves each PNG straight from BinaryData (the artwork is compiled into
    //  the plugin), so there is NO filesystem access at all: the skin loads
    //  identically from the build tree, Program Files\Common Files\VST3, or a
    //  customer's machine. Lookup is by the file's basename, which is unique
    //  across the package. Loaded images are cached on first use.
    // ------------------------------------------------------------------------
    class AssetManager
    {
    public:
        AssetManager() = default;

        bool isValid() const noexcept { return BinaryData::namedResourceListSize > 0; }

        // Path is relative to the UI package root, e.g. "Knobs/output_knob_core_2x.png";
        // only the basename is matched against the embedded resources.
        const juce::Image& get (const juce::String& relativePath)
        {
            auto it = cache.find (relativePath);
            if (it != cache.end())
                return it->second;

            juce::Image img;
            const auto wanted = relativePath.fromLastOccurrenceOf ("/", false, false);
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                if (wanted == BinaryData::originalFilenames[i])
                {
                    int size = 0;
                    if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
                        data != nullptr && size > 0)
                        img = juce::ImageFileFormat::loadFrom (data, (size_t) size);
                    break;
                }
            }
            return cache.emplace (relativePath, std::move (img)).first->second;
        }

    private:
        std::map<juce::String, juce::Image> cache;
    };

    // ------------------------------------------------------------------------
    //  Rotary knob rendered from the artwork's "core" sprite.
    //
    //  All six knobs are the SAME physical dial, so they share ONE sprite
    //  (output_knob_core — the others are the identical dial but cropped
    //  off-centre, which made per-sprite rendering look lopsided). To get a
    //  clean, round, value-tracking knob we:
    //    1. align the cap's optical centre (sprite centre + kCapOff) to the
    //       control-box centre,
    //    2. rotate the cap about that centre through the +/-135 degree sweep
    //       (the baked pointer reads straight up at mid travel), and
    //    3. clip to the cap circle so no red corners / tick stubs leak.
    //  The tick ring is drawn here (static) so the artwork's baked ticks are
    //  never used; labels/values stay with the editor.
    // ------------------------------------------------------------------------
    class ImageKnob : public juce::Slider
    {
    public:
        ImageKnob()
        {
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        }

        void setCore (const juce::Image& img) { core = img; repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            const auto centre  = b.getCentre();
            const float boxHalf = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

            paintTicks (g, centre, boxHalf);

            if (! core.isValid())
                return;

            // Cap sprite occupies ~108/152 of the control box in the artwork.
            const float side = boxHalf * 2.0f * (108.0f / 152.0f);
            const float s    = side / (float) core.getWidth();

            // Cap optical centre inside the 216-px core sprite.
            const float capCx = (core.getWidth()  * 0.5f + kCapOffX) * s;
            const float capCy = (core.getHeight() * 0.5f + kCapOffY) * s;

            const float prop  = (float) valueToProportionOfLength (getValue());
            const float angle = juce::jmap (prop, 0.0f, 1.0f, kMinAngle, kMaxAngle);

            const float clipR = side * 0.5f * kCapClip;

            // Opaque metallic body so the panel texture never shows through the
            // (partly transparent) cap sprite, plus a soft drop shadow for depth.
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (centre.x - clipR, centre.y - clipR + boxHalf * 0.06f,
                           clipR * 2.0f, clipR * 2.0f);
            juce::ColourGradient body (juce::Colour (0xff232327), centre.x, centre.y - clipR,
                                       juce::Colour (0xff070708), centre.x, centre.y + clipR, false);
            g.setGradientFill (body);
            g.fillEllipse (centre.x - clipR, centre.y - clipR, clipR * 2.0f, clipR * 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawEllipse (centre.x - clipR, centre.y - clipR, clipR * 2.0f, clipR * 2.0f,
                           juce::jmax (1.0f, boxHalf * 0.03f));

            juce::Path clip;
            clip.addEllipse (centre.x - clipR, centre.y - clipR, clipR * 2.0f, clipR * 2.0f);

            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip);

            // Scale core -> screen, drop the cap centre onto the control centre,
            // then rotate the whole cap about that centre.
            auto t = juce::AffineTransform::scale (s)
                        .translated (centre.x - capCx, centre.y - capCy)
                        .rotated (angle, centre.x, centre.y);
            g.drawImageTransformed (core, t);
        }

    private:
        // Subtle amp-style tick ring: short, thin, low-contrast marks hugging the
        // knob (the mockup uses restrained ticks, not prominent clock-hands).
        void paintTicks (juce::Graphics& g, juce::Point<float> c, float half)
        {
            constexpr int   n   = 11;
            const float r1 = half * 0.90f, r2 = half * 1.0f;
            const float w  = juce::jmax (1.0f, half * 0.018f);

            g.setColour (juce::Colours::white.withAlpha (0.42f));
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) (n - 1);
                const float a = juce::jmap (t, 0.0f, 1.0f, kMinAngle, kMaxAngle);
                const float dx = std::sin (a), dy = -std::cos (a); // 0 = straight up
                g.drawLine (c.x + dx * r1, c.y + dy * r1,
                            c.x + dx * r2, c.y + dy * r2, w);
            }
        }

        static constexpr float kMinAngle = -2.35619449f; // -135 degrees
        static constexpr float kMaxAngle =  2.35619449f; //  135 degrees
        static constexpr float kCapOffX  =  7.0f;   // cap-centre offset in 216-px sprite
        static constexpr float kCapOffY  = 20.0f;
        static constexpr float kCapClip  =  0.84f;  // clip radius as fraction of cap half

        juce::Image core;
    };

    // ------------------------------------------------------------------------
    //  Vertical slider drawn entirely in code: a dark channel, a red-glow fill
    //  below the handle, a tick ladder, and a chrome handle. The artwork's
    //  slider sprites bake in their own red panel + a static handle (and the
    //  track-markers mockup bakes in a second handle), so they can't be
    //  composited cleanly — this vector slider matches the skin without the
    //  ghosting, mirroring how the knobs sidestep the baked mockups.
    // ------------------------------------------------------------------------
    class ImageSlider : public juce::Slider
    {
    public:
        ImageSlider()
        {
            setSliderStyle (juce::Slider::LinearVertical);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        }

        // Kept for editor call-site compatibility; the sprite is unused now.
        void setHandle (const juce::Image&) {}

        void paint (juce::Graphics& g) override
        {
            auto  b       = getLocalBounds().toFloat();
            const float cx = b.getCentreX();
            const float trackW = juce::jmax (5.0f, b.getWidth() * 0.34f);
            const float margin = b.getHeight() * 0.045f;
            const float top = b.getY() + margin, bot = b.getBottom() - margin;

            const float prop = (float) valueToProportionOfLength (getValue());
            const float hy   = juce::jmap (prop, 0.0f, 1.0f, bot, top);

            // --- dark recessed channel ---
            juce::Rectangle<float> track (cx - trackW * 0.5f, top, trackW, bot - top);
            g.setColour (juce::Colour (0xff090909));
            g.fillRoundedRectangle (track, trackW * 0.5f);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.drawRoundedRectangle (track.reduced (0.5f), trackW * 0.5f, 1.0f);

            // --- small red tick marks behind the track ---
            const int   nt = 9;
            const float tickX = trackW * 0.5f + b.getWidth() * 0.10f;
            g.setColour (Colours::glowRed.withAlpha (0.28f));
            for (int i = 0; i < nt; ++i)
            {
                const float y = juce::jmap ((float) i / (nt - 1), top + 2.0f, bot - 2.0f);
                g.drawLine (cx - tickX - b.getWidth() * 0.06f, y, cx - tickX, y, 1.0f);
                g.drawLine (cx + tickX, y, cx + tickX + b.getWidth() * 0.06f, y, 1.0f);
            }

            // --- fine red illuminated centre line (brighter below the cap) ---
            const float lineW = juce::jmax (1.5f, trackW * 0.22f);
            g.setColour (Colours::glowRed.withAlpha (0.20f));
            g.fillRoundedRectangle (cx - lineW * 0.5f, top, lineW, bot - top, lineW * 0.5f);
            juce::ColourGradient glow (Colours::glowRed.withAlpha (0.9f), cx, bot,
                                       Colours::glowRed.withAlpha (0.25f), cx, hy, false);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (cx - lineW * 0.5f, hy, lineW, bot - hy, lineW * 0.5f);

            // --- chrome/black slider cap ---
            const float hw = b.getWidth() * 0.86f;
            const float hh = juce::jmax (12.0f, b.getHeight() * 0.058f);
            juce::Rectangle<float> hr (cx - hw * 0.5f, hy - hh * 0.5f, hw, hh);
            juce::ColourGradient hg (juce::Colour (0xff26262a), cx, hr.getY(),
                                     juce::Colour (0xff0c0c0e), cx, hr.getBottom(), false);
            hg.addColour (0.44, juce::Colour (0xffd0d0d6));
            hg.addColour (0.56, juce::Colour (0xff7f7f86));
            g.setGradientFill (hg);
            g.fillRoundedRectangle (hr, hh * 0.24f);
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.drawRoundedRectangle (hr, hh * 0.24f, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRect (juce::Rectangle<float> (hr.getX() + 3.0f, hy - 0.5f, hw - 6.0f, 1.5f));
        }
    };

    // ------------------------------------------------------------------------
    //  Two-position toggle drawn in code: a pill track with a chrome handle on
    //  the active side, a red glow under the active half, and a label on each
    //  side (active = red, inactive = grey). Behaves as a juce::Button so it
    //  toggles on click and binds through a ButtonAttachment. Replaces the
    //  *_switch_full mockups, which bake in a static handle position.
    // ------------------------------------------------------------------------
    class ToggleSwitch : public juce::Button
    {
    public:
        ToggleSwitch (juce::String leftLabel, juce::String rightLabel)
            : juce::Button ({}), left (std::move (leftLabel)), right (std::move (rightLabel))
        {
            setClickingTogglesState (true);
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool /*down*/) override
        {
            auto b = getLocalBounds().toFloat();
            const bool on = getToggleState();               // false = left, true = right

            // Pill sized from the component height, centred; labels flank it.
            const float pillH = juce::jmin (b.getHeight() * 0.60f, b.getWidth() * 0.16f);
            const float pillW = pillH * 2.3f;
            juce::Rectangle<float> pill (b.getCentreX() - pillW * 0.5f,
                                         b.getCentreY() - pillH * 0.5f, pillW, pillH);

            // track + red glow under the active half
            g.setColour (juce::Colour (0xff0e0e10));
            g.fillRoundedRectangle (pill, pillH * 0.5f);
            auto activeHalf = on ? pill.withTrimmedLeft (pillW * 0.5f)
                                 : pill.withTrimmedRight (pillW * 0.5f);
            g.setColour (Colours::glowRed.withAlpha (0.85f));
            g.fillRoundedRectangle (activeHalf.reduced (pillH * 0.14f), pillH * 0.32f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (pill.reduced (0.5f), pillH * 0.5f, 1.0f);

            // chrome handle on the active side
            const float hw = pillW * 0.52f, hh = pillH * 1.18f;
            juce::Rectangle<float> hr (on ? pill.getRight() - hw : pill.getX(),
                                       b.getCentreY() - hh * 0.5f, hw, hh);
            juce::ColourGradient hg (juce::Colour (0xff2b2b2f), hr.getX(), hr.getY(),
                                     juce::Colour (0xff101012), hr.getX(), hr.getBottom(), false);
            hg.addColour (0.46, juce::Colour (0xffd6d6db));
            hg.addColour (0.54, juce::Colour (0xff8a8a90));
            g.setGradientFill (hg);
            g.fillRoundedRectangle (hr, hh * 0.28f);
            g.setColour (juce::Colours::black.withAlpha (highlighted ? 0.4f : 0.7f));
            g.drawRoundedRectangle (hr, hh * 0.28f, 1.2f);

            // side labels
            const float fh = juce::jmax (10.0f, b.getHeight() * 0.40f);
            g.setFont (juce::Font (fh, juce::Font::bold));
            const int pillL = juce::roundToInt (pill.getX());
            const int pillR = juce::roundToInt (pill.getRight());
            g.setColour (on ? Colours::muted : Colours::glowRed);
            g.drawText (left,  juce::Rectangle<int> (0, 0, pillL - 6, (int) b.getHeight()),
                        juce::Justification::centredRight, false);
            g.setColour (on ? Colours::glowRed : Colours::muted);
            g.drawText (right, juce::Rectangle<int> (pillR + 6, 0,
                                                     (int) b.getWidth() - pillR - 6, (int) b.getHeight()),
                        juce::Justification::centredLeft, false);
        }

    private:
        juce::String left, right;
    };
}
