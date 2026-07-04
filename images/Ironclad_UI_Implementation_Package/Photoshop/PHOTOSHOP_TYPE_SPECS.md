# Ironclad Photoshop Type Specs

Use the top-level reference assets as the primary visual reference:
- images/ironclad_background.png
- images/ironclad_components.png

These two files define the intended overall look, while the implementation package contains the detailed layout and Photoshop layer notes for the control placement and typography.

## Global type direction
- Font family: use a condensed, high-contrast industrial sans-serif with a slightly technical / metal aesthetic.
- Weight: mostly medium to bold, with selective light accents for secondary labels.
- Letter spacing: slightly tight for logo and screen labels; medium for control labels.
- Case: uppercase for logo and major labels; title case for secondary labels and preset names.
- Rendering: crisp, slightly sharpened text with subtle stroke or soft glow where the artwork calls for it.

## Logo / brand text
- Purpose: top-center branding and nameplate treatment.
- Style: bold uppercase, condensed, sharp edges.
- Color: warm off-white / light gray #E0E0E4.
- Size: approximately 36–44 px at source art scale.
- Tracking: -4% to -6%.
- Use: reserve for the main Ironclad wordmark and bottom nameplate.

## Control labels
- Purpose: knob and slider names such as Drive, Tone, Bass, Mid, Presence, Output, Mix, Tight.
- Style: small uppercase or title case labels, aligned to control centers.
- Color: light gray / off-white #E0E0E4.
- Size: approximately 14–16 px at source art scale.
- Weight: medium bold.
- Tracking: 0% to +2%.
- Alignment: centered over or just above the control.

## Screen / display labels
- Purpose: preset name, mode labels, meter labels, toolbar labels, and value annotations.
- Style: tight, readable, slightly technical.
- Color: bright red accent #FF2A2A for active or highlighted values; off-white #E0E0E4 for neutral labels.
- Size: approximately 12–18 px depending on area and hierarchy.
- Weight: medium to semibold.
- Tracking: 0% to +1%.
- Use: keep text legible over the dark glass / screen bezel background.

## Value readout text
- Purpose: numeric values and preset title text on the center display.
- Style: compact, high-contrast, slightly monospaced or evenly spaced for readability.
- Color: bright red accent #FF2A2A for emphasized values; neutral white for secondary readouts.
- Size: approximately 18–28 px depending on the target region.
- Weight: semibold to bold.
- Tracking: 0% to +1%.
- Use: prefer simple, clean digits and short text strings.

## Button / switch labels
- Purpose: mode buttons, preset navigation, toggle labels, and utility actions.
- Style: short uppercase labels with sharp geometry.
- Color: off-white #E0E0E4 for default; red accent #FF2A2A for active/selected state.
- Size: approximately 11–14 px.
- Weight: semibold.
- Tracking: 0% to +1%.
- Use: keep labels compact and centered inside the button area.

## Notes for implementation
- Match the Photoshop artwork first; do not invent a new font family if the source art is already clear.
- For fallback rendering in JUCE, use a bold condensed sans-serif and adjust tracking/weight to stay visually close to the artwork.
- Keep all type aligned to the source layout coordinates rather than independently repositioning text at runtime.
- Preserve contrast against dark chrome, brushed panels, and the center glass surface.
