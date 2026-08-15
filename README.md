# ToneStar

Every amp UI looks like a faceplate. Gain, Bass, Mid, Treble, Presence, Master. Those are ingredients. When you want a heavy rhythm sound you already know the recipe: more drive, more saturation, tighter lows, less boxy low-mid, a bit of clank. Doing that on six knobs is slow, and you usually end up with a different amp than the one you started with. Big modelers keep the same strip, then add a cab locker and a Time / Rate knob on every pedal.

ToneStar starts from the job. The star has six pulls: Clean, Crunch, Heavy, Tight, Cut, Warm. Each one moves a bundle of internals together. Heavy is a whole land (extra stages, compression, scoop, clank). Tight is the high-pass and the faster feel. Stack them and they rewrite one amp.

FX and cab work the same way. Echo goes from slapback to a wash. Cab is Size and Back. In is how hard you hit the amp. Out is loudness.

This started as a Focusrite ASIO monitor so guitar could be heard in headphones. It is a standalone Windows app now.

## Signal path

```
guitar → In → processPre → star (amp) → processPost → cab / room / HRTF → Out → Tape → Looper → mute → click
```

- **In** hits the amp. **Out** is loudness.
- Tape records the guitar after Out.
- What you hear is live guitar, unmuted tape lanes, and the looper. Mute silences the speakers; the playhead keeps walking.
- The metronome is mixed onto the device buffer last.

## Star

Center is already a playable amp. Each vertex is 0-1 and stays on its spoke. Pulling one adds a job.

Composition lives in ToneCompose.h. Tight stays a feel control. Warm owns the top shelf above 6 kHz.

| Pull | Job | Recipe |
| --- | --- | --- |
| **Clean** | Hear the guitar | Less drive, more headroom and pick |
| **Crunch** | Edge of breakup | Mid gain, mid push, sag |
| **Heavy** | Chug / saturate | More stages, compression, low-mid scoop, 1-2 kHz clank |
| **Tight** | Faster, less bloom | High-pass, less low-end hang |
| **Cut** | Sit in the band | 0.5-3 kHz forward |
| **Warm** | Darker, smoother | Softer clip, a bit of 250-500 Hz |

Shift-drag for fine moves. Double-click zeros that job.

## FX ring

All handles start at 0. Each one is how much of that land. Pre listens to the clean string. Post sits on the finished amp.

**Pre** (before the star):

| Pull | Land |
| --- | --- |
| **Squeeze** | Comp: glue → sustain → squash |
| **Talk** | Auto-wah: mild attack quack → full vocal sweep |
| **Shift** | −12 → 12-string → synth stack |

**Post** (after the star):

| Pull | Land |
| --- | --- |
| **Echo** | Slapback → dotted rhythm → shoegaze wash |
| **Bloom** | Spring → plate → cloud. Right-click the label for Shimmer (octave-up tail) |
| **Width** | Chorus: slow thicken → deeper |
| **Sweep** | Phaser → flange |
| **Pulse** | Trem → chop |

Shimmer is on Bloom. Same handle, octave-up on the tail.

## Cab

Always on, under the star. Wheel or click **Size** (Combo / Twin / Stack). Right-click **Back** (Open / Closed). **Binaural** uses a SADIE II D2 pair at azimuth 0°, elevation −15° so headphones hear a cab in front. More in docs/acoustics.md.

## Tape, looper, presets

Gestures are in docs/controls.md.

- **Tape:** eight-lane linear practice tape after Out. Clips live in `Documents/ToneStar/tape`.
- **Looper:** phrase looper after Out and Tape. Space is the pedal while ToneStar is focused.
- **Presets:** store the share slug (star, FX, shimmer, binaural, cab). In, Out, mute, and devices stay as they are.

Settings save under `%APPDATA%\ToneStar\`.

## Build

Windows 10/11, Visual Studio 2022 with the CMake workload. The CMake target is `ToneStar`.

```bat
build.bat
```

That configures and builds Release to `build/ToneStar_artefacts/Release/ToneStar.exe`. Close the app first (the exe locks).

```bat
package.bat
```

Same build, then writes `dist/ToneStar.zip`. No install.

ASIO duplex: pick the same interface as input and output. Turn Direct Monitor **off** or you will hear double.

```
cmake -S . -B build
cmake --build build --config Release --target ToneStar
```

The exe lands at `build/ToneStar_artefacts/Release/ToneStar.exe`. JUCE 8.0.15 is fetched at configure time.

## Licence

Copyright 2026 ToneStar authors. GNU AGPLv3. See [`LICENSE`](LICENSE) and [`licenses/THIRD_PARTY.md`](licenses/THIRD_PARTY.md) for JUCE, Steinberg ASIO, SIL OFL fonts, SADIE Apache 2.0 HRTF, and the CC0 cab IR. Official Windows builds may be sold as convenience; anyone can compile the same source.
