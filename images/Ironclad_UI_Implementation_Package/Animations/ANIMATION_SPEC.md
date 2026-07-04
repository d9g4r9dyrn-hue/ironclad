# Animation Specification

Hover glow: 120 ms ease-out, opacity 0 → 35%, glow radius 4 → 12 px.
Button press: 70 ms ease-in, y offset +1 px, inner shadow +20%, glow reduced 20%.
Knob rotation: immediate parameter response with 80 ms smoothing for pointer angle only.
Slider movement: immediate handle tracking, red fill interpolates over 50 ms.
LED pulse: 1.2 s sine opacity modulation between 70% and 100% for active status LEDs.
Meter decay: attack 5 ms, release 250 ms, peak hold 700 ms.
Preset transition: 160 ms crossfade on preset name and screen graph.
Tab transition: 100 ms selected-outline fade.
Oversampling selection: 120 ms red fill crossfade; disable click spam until DSP mode switch completes.
