# Ironclad

A VST3 / Standalone **distortion** plugin built in C++ with JUCE — amp-style tone shaping with a
bold, hardware-inspired interface.

Product page: https://corticorp.com/music/plugins/Ironclad/

## Highlights

- **Amp-style voicings** — four distinct characters, from clean grit to full saturation.
- **Focused tone stack** — drive and tone shaping tuned for guitars and broader audio processing.
- **Hardware-inspired UI** — a polished standalone experience, not just a plugin slot.

## Build

Requires CMake 3.22+ and a C++20 toolchain. JUCE 8.0.4 is fetched via `FetchContent`.

```sh
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Targets: **VST3** and **Standalone**.

## Layout

```
src/     processor + editor
```

---

© CortiCorp, LLC. All rights reserved.
