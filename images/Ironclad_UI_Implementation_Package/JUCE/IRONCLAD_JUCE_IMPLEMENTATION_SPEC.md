# IRONCLAD UI Implementation Specification for JUCE VST3

Source artwork: `Reference/source_concept_actual_1774x887.png`.
Actual source dimensions: **1774 × 887**. Use these as the canonical pixel coordinate system for pixel-perfect matching.

## Scaling
Use a single uniform scale factor from the canonical source size:

```cpp
constexpr float kBaseW = 1774.0f;
constexpr float kBaseH = 887.0f;
float scale = std::min(getWidth() / kBaseW, getHeight() / kBaseH);
AffineTransform t = AffineTransform::scale(scale).translated((getWidth()-kBaseW*scale)*0.5f, (getHeight()-kBaseH*scale)*0.5f);
```

Render at 75%, 100%, 125%, 150%, and 200% by scaling from this coordinate system. Do not move controls independently at different sizes.

## Component hierarchy

```cpp
IroncladEditor
 ├─ FrameComponent
 ├─ ArmorComponent left/right
 ├─ RedPanelComponent left/right
 ├─ ScreenBezelComponent
 ├─ ScreenComponent
 │   ├─ TransferCurveComponent
 │   ├─ SpectrumComponent
 │   ├─ MeterComponent input/output
 │   ├─ PresetComponent
 │   └─ ToolbarComponent
 ├─ IroncladKnob drive/tone/bass/mid/presence/output
 ├─ IroncladSlider lowCut/highCut/mix/level
 ├─ IroncladSwitch character/tightLoose/gate
 ├─ LedComponent strips and dots
 └─ Label/value text components
```

## Painting order
1. Main background / outer frame.
2. Side armor and side LED strips.
3. Black metal header, mesh grilles, top slashes.
4. Red left/right brushed panels.
5. Center bevel and glass display.
6. Bottom vent trim and nameplate.
7. Knobs/sliders/switches/buttons/meters.
8. Text, icons, tick marks.
9. Glow/reflection overlays.

## Asset loading
Use a centralized `AssetManager` with `juce::ImageCache::getFromMemory()` or BinaryData. Cache all PNGs. Do not load files in `paint()`.

## Custom LookAndFeel
Implement `IroncladLookAndFeel` for sliders and buttons. Use image-backed drawing first, then vector fallback using `Path`, `ColourGradient`, and `DropShadow`.

## Hit testing
Use rectangular component bounds from `Layout/ironclad_layout_actual_source_pixels.json`, but for knobs use circular hit testing around `center` and `radius`.

## Repaint strategy
Meters, FFT, and transfer curve repaint at 30–60 fps. Static panels should use `CachedComponentImage` and repaint only on scale changes.

## GPU-friendly drawing
Prefer pre-rendered PNGs for static chrome, brushed panels, mesh, armor, and heavy glow. Use `juce::Path` only for live curve and meter segments. OpenGL acceleration is optional.
