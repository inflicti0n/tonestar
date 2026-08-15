# ToneStar

<p align="center">
  <img src="screenshots/overall.png" alt="ToneStar" width="520">
</p>

<p align="center">
  <strong>A Windows guitar amp that starts from the sound you want, not a row of knobs.</strong>
</p>

Six pulls on a star. An FX ring around it. A cab under it. Share a tone with a code. Record eight tape lanes. Loop a phrase. Export a 24-bit WAV.

No faceplate. No plugin host. Just the guitar, in.

---

## The whole rig

<p align="center">
  <img src="screenshots/complete_overall.png" alt="ToneStar with tape, presets, and looper open" width="900">
</p>

Open the drawers and the window grows around the star.

- **Tape** on the left — eight-lane practice recorder with a metronome.
- **Star** in the middle — amp, FX, cab, share code.
- **Presets** on the right — saved tones, one double-click away.
- **Looper** underneath — a phrase pedal after the tape.

**BPM metronome** lives on the tape strip. Turn the click on, set the tempo, and both tape and looper can lock to it.

**Binaural** is the extra for headphones. Flip it on and the cab sits in front of you, slightly down, instead of a flat mono-in-both-ears dump.

**Two loopers.** Simple is one phrase, like a Ditto. Advanced is two phrases, like an RC-500. Right-click the Looper title to switch.

Every click and gesture is in [docs/controls.md](docs/controls.md).

---

## Starmap

<p align="center">
  <img src="screenshots/starmap.png" alt="The ToneStar starmap" width="480">
</p>

Center is already a playable amp. Each vertex is a job. Pull one, or stack a few — they rewrite one amp together.

| Pull | You want |
| --- | --- |
| **Clean** | Hear the guitar. Headroom, pick, sparkle. |
| **Crunch** | Edge of breakup. Mids, sag, classic grind. |
| **Heavy** | Chug. Extra stages, compression, palm-mute clank. |
| **Tight** | Faster, less bloom. High-pass, less hang. |
| **Cut** | Sit in the band. Forward 0.5–3 kHz. |
| **Warm** | Darker and smoother. Softer clip, a bit of body. |

Shift-drag for fine moves. Double-click a spoke to zero it.

**Pairs that work:** Heavy + Tight = modern rhythm. Clean + Warm = jazz / bedroom. Crunch + Cut = it cuts a mix.

---

## FX ring

The eight handles around the star are how much of that land. All start at zero. The live line on the ring is the mix — it follows the handles and moves with the guitar.

**Before the amp** (listens to the clean string):

- **Squeeze** — glue → sustain → squash
- **Talk** — mild attack quack → full vocal sweep
- **Shift** — fat low octave → 12-string → synth stack

**After the amp** (sits on the finished tone):

- **Echo** — slapback → dotted rhythm → shoegaze wash
- **Bloom** — kiss of spring → plate → cloud
- **Width** — slow thicken → deeper chorus
- **Sweep** — phaser → flange
- **Pulse** — soft throb → hard trem chop

Right-click the **Bloom** label for **Shimmer**. Same handle, octave-up on the tail.

---

## Presets

<p align="center">
  <img src="screenshots/preset.png" alt="Share a ToneStar preset with a code" width="480">
</p>

Easily share a preset via a code. Copy the slug, send it, paste it, hit **Apply** — the starmap, FX, shimmer, binaural, and cab snap into place.

Store and load as many presets as you want. Open the drawer, hit **+** to save the current tone, **double-click** to load one instantly. Right-click to rename. X to delete.

In, Out, mute, and your audio device stay as they are. The code is the sound, not the room.

---

## Tape

<p align="center">
  <img src="screenshots/tape-track.png" alt="Eight-lane tape recorder" width="720">
</p>

Eight-lane linear practice tape, after Out. Click a track to arm it. Hit record. Play it back. Quantization is supported — **Q** waits for the next beat to start and closes on a beat, and moves and trims snap to the grid.

Loop a range with the **Loop** button, then record over a track while the others play. Mute lanes you do not want in the mix. Each track has **L/R pan** and its own **volume**.

Rename a lane. Drag a clip. Crop either edge — the WAV stays whole. The click and BPM sit at the bottom of the strip.

Clips live in `Documents/ToneStar/tape`.

---

## Export

<p align="center">
  <img src="screenshots/export.png" alt="Export a 24-bit WAV" width="360">
</p>

Seamlessly export to **24-bit stereo WAV**. Same mix as playback — unmuted lanes, pans, levels.

Pick the range you want (**Start** and **Length** in seconds). One click on **Export WAV** and you are done.

---

## Looper

The looper sits after Out and Tape. Live guitar always passes through. Mute silences the speakers; the playhead keeps walking. **Space** is the pedal while ToneStar is focused.

**Simple** (default) — one phrase, Ditto / RC-1 style.

- Tap: empty → record → play → overdub → play
- Double-tap: stop (or discard if you are still on the first take)
- Hold: undo / redo the last layer, or clear when stopped
- **Q** — free length, or wait for the beat

**Advanced** — right-click the Looper title. Two phrases, like an RC-500. Each has rec/play, stop, and level. The first closed phrase is master; the second locks to its length and waits for the downbeat.

---

## Cabs

The cab is always on, under the star. Three sizes. Open or closed back.

<p align="center">
  <img src="screenshots/cab_combo.png" alt="Combo cab" height="120">
  &nbsp;&nbsp;
  <img src="screenshots/cab_twin.png" alt="Twin cab" height="120">
  &nbsp;&nbsp;
  <img src="screenshots/cab_stack.png" alt="Stack cab" height="120">
</p>

<p align="center">
  <strong>Combo</strong> &nbsp;·&nbsp; <strong>Twin</strong> &nbsp;·&nbsp; <strong>Stack</strong>
</p>

Wheel or click **Size** to step Combo → Twin → Stack. Right-click **Back** for **Open** vs **Closed** — open air versus a sealed box.

**Binaural** uses a SADIE II HRTF so headphones hear that cab in front of you, not in the middle of your head.

---

## In, Out, devices

<p align="center">
  <img src="screenshots/asio.png" alt="ASIO device setup" width="420">
</p>

**In** is how hard you hit the amp. **Out** is loudness. **Mute** silences the speakers without stopping tape or the looper.

**Devices** picks the interface. Use the same device for input and output (ASIO duplex). Turn Direct Monitor **off** or you will hear double.

---

## Look

Ctrl+Shift+P opens a hidden plasma tune panel — colors and motion for the star fluid and the FX ring. Save it, or leave it alone. The amp does not care.

---

## Build

Windows 10/11, Visual Studio 2022 with the CMake workload.

```bat
build.bat
```

Release exe: `build/ToneStar_artefacts/Release/ToneStar.exe`. Close the app first (the exe locks).

```bat
package.bat
```

Same build, then `dist/ToneStar.zip`. No install.

```
cmake -S . -B build
cmake --build build --config Release --target ToneStar
```

JUCE 8.0.15 is fetched at configure time.

## Licence

Copyright 2026 ToneStar authors. GNU AGPLv3. See [`LICENSE`](LICENSE) and [`licenses/THIRD_PARTY.md`](licenses/THIRD_PARTY.md) for JUCE, Steinberg ASIO, SIL OFL fonts, SADIE Apache 2.0 HRTF, and the CC0 cab IR. Official Windows builds may be sold as convenience; anyone can compile the same source.
