# ToneStar

Amp UIs copy hardware: Gain, Bass, Mid, Treble, Presence, Master. That is honest. It is the wrong first interface.

Nobody sits down and thinks "nudge 2.5 kHz and add a clip stage." They think heavier. Tighter. Still in the mix. A heavy rhythm tone is never one knob. You raise gain, add saturation, tighten the low end, scoop the boxy low-mid, and push the clank. Six knobs later you have a different amp, not the job you asked for. Modelers make this worse. They hand you the same strip, then a cab locker, then a pedalboard with Time and Rate on every box.

ToneStar is a recipe mixer. The star is six jobs: Clean, Crunch, Heavy, Tight, Cut, Warm. Pull a vertex and a bundle of internals move the way a player would. "Make it heavy" is never just more gain. Tight adds no drive. Combinations rewrite one chassis; they do not blend six amps. FX works the same way. Echo is slap to wash, not a Time knob. The cab is Size and Back, not a mic locker.

Center is already a playable amp. In hits it. Out is volume. It started as a Focusrite ASIO guitar monitor. It is a standalone Windows app now.

## Signal path

```
guitar → In → processPre → star (amp) → processPost → cab / room / HRTF → Out → Tape → Looper → mute → click
```

- **In** hits the amp. **Out** is the only volume.
- Tape writes the finished guitar after Out. Never the click, never other tracks, never the looper.
- The hear path mixes live guitar + unmuted tape lanes + the looper. Mute silences the room; the playhead keeps walking.
- The metronome is mixed onto the device buffer last.

## Star

Center is already a playable amp. Each vertex is 0-1 and spoke-constrained. Pulling a vertex *adds a job*, not a single parameter.

Composition lives in ToneCompose.h. Gain is not a raw sum. Tight adds no drive. Warm owns the >6 kHz shelf.

| Pull | Job | What the recipe does |
| --- | --- | --- |
| **Clean** | Hear the guitar | Less drive/sat, more headroom and pick |
| **Crunch** | Edge of breakup | Mid gain, mid push, sag, not squash |
| **Heavy** | Chug / saturate | More stages + compression, low-mid scoop, 1-2 kHz clank |
| **Tight** | Not flubby | HPF, less bloom, faster. **No extra gain** |
| **Cut** | Sit in the band | 0.5-3 kHz forward. Not ice-pick treble |
| **Warm** | Stop the fizz | Darker highs, smoother clip, a bit of 250-500 Hz |

Shift-drag is fine. Double-click zeros that job.

## FX ring

All handles start at 0. Lands are amount, not exposed Time / Rate knobs. Pre needs a clean string. Post needs the finished amp.

**Pre** (before the star):

| Pull | Land |
| --- | --- |
| **Squeeze** | Comp glue → sustain → squash. Not a secret drive |
| **Talk** | Auto-wah. Mild attack quack → full vocal sweep |
| **Shift** | −12 → 12-string → synth stack |

**Post** (after the star):

| Pull | Land |
| --- | --- |
| **Echo** | Slapback → dotted rhythm → shoegaze wash |
| **Bloom** | Spring → plate → cloud. Right-click the label for Shimmer (octave-up tail) |
| **Width** | Chorus. Slow thicken → deeper |
| **Sweep** | Phaser → flange |
| **Pulse** | Trem → chop |

Shimmer is not a ninth pull. Same Bloom handle, octave-up on the tail.

## Cab

Always on. Not on the FX ring. Wheel or click **Size** (Combo / Twin / Stack). Right-click **Back** (Open / Closed). **Binaural** uses a SADIE II D2 pair at azimuth 0°, elevation −15° so headphones hear a cab in front. More in docs/acoustics.md.

## Tape, looper, presets

Gestures are in docs/controls.md.

- **Tape:** eight-lane linear practice tape after Out. Not a second looper. Clips live in `Documents/ToneStar/tape`.
- **Looper:** phrase looper after Out and Tape. Space is the pedal only while ToneStar is focused.
- **Presets:** store the share slug only (star, FX, shimmer, binaural, cab). In / Out / mute / devices stay as they are.

Settings save under `%APPDATA%\ToneStar\`. There is no migration from older Constellation folders.

## Build

Windows 10/11, Visual Studio 2022 with the CMake workload. The CMake target is still `GuitarMonitor`; the product name is ToneStar.

```bat
package.bat
```

That configures, builds Release, and writes `dist/ToneStar.zip` with `ToneStar.exe`. No install. Close the app before rebuilding (the exe locks).

ASIO duplex: pick the same interface as input and output. Turn Direct Monitor **off** or you will hear double.

```
cmake -S . -B build
cmake --build build --config Release --target GuitarMonitor
```

The exe lands at `build/GuitarMonitor_artefacts/Release/ToneStar.exe`. JUCE 8.0.15 is fetched at configure time.

## Licence

Copyright 2026 ToneStar authors. GNU AGPLv3. See [`LICENSE`](LICENSE) and [`licenses/THIRD_PARTY.md`](licenses/THIRD_PARTY.md) for JUCE, Steinberg ASIO, SIL OFL fonts, SADIE Apache 2.0 HRTF, and the CC0 cab IR. Official Windows builds may be sold as convenience; anyone can compile the same source.
