# Acoustics: cab, room, binaural

The last stage after Bloom. Always on. Pose is locked: the amp sits on the midline, slightly down, about 1.2 m in front. No orbit, no mic locker, no ninth FX vertex.

```
Bloom (mono) → speaker / cab → floor and early room → binaural off: L = R
                                              ↘ binaural on: SADIE HRTF 0° / −15°
```

The star still answers "what amp am I standing in." FX answers "what is happening to the guitar and around that amp." Acoustics answers "what speaker and space is it in."

## Three layers

### 1. Speaker and cab (mono)

One measured 12-inch cone, then Size and Back as recipes around it.

The cone is a trimmed (~32 ms) mono fold of Jester Dyne Brutal **Cookie Monster**: Celestion Vintage 30 + Shure SM57, 48 kHz, CC0. Mix is 0.72 at Combo and 0.80 at Stack. Size and Back filters do the rest of the box. The Brutal pack is all one oversized 4×12, so there is no honest Combo or Stack IR to add.

**Size** 0 → 1 lands Combo (1×12) / Twin (2×12) / Stack (4×12). More low-mid coupling, darker breakup, more beaming (narrower highs). Not a speaker-count knob.

**Back** 0 → 1 lands Open / Closed. Closed: less rear-leak highs, stronger lows, more directional.

Star `cabHz` stays a mild fizz lid so the amp still plays if this stage is bypassed in a debug build. Do not add a second full cab on the star.

### 2. Near room (still mono)

Not Bloom. Floor bounce ~1-3 ms (duller), a few early taps, short dark tail 100-250 ms. Size raises floor and weight a little. Closed Back reduces "room in the cab." Kept narrow so HRTF hears one source.

Bloom stays a pedal (spring / plate / pad / Shimmer). This room must not become a second hall.

### 3. Binaural

Off: copy the mono cab into L and R (today’s dry-phone behaviour).

On: convolve L / R with the extracted SADIE II KEMAR D2 pair at **azimuth 0°, elevation −15°**, measured at 1.2 m. Headphones hear a cab in front of you. Not "3D," not an orbit.

Makeup is a prepare-time 5×5 Size×Back LUT (silent pink through cab + room). The audio thread bilinear-interpolates and smooths (~25 ms). Floor 0.15, peak target −6 dBFS, ceiling 1.2. Never probe the IR on the audio thread. HRTF makeup is still a single prepare probe (floor 0.4). Cab / binaural must not become a second Out.

## UI

Setup, not a ring job. One cab object under the star field, above In / Out. The silhouette is the control.

- **Wheel:** continuous Size (Combo / Twin / Stack)
- **Click:** snap to the next Size land (Combo 0.15 → Twin 0.50 → Stack 0.85 → Combo)
- **Right-click:** toggle Back (Open 0.25 / Closed 0.75)
- **Binaural:** next to Mute / Devices. Tooltip: "headphones hear a cab in front of you."

Combo is a 1×12 combo (panel + one speaker). Twin is a wide 2×12. Stack is a head on a 4×12. Open peeks a glowing back and the cones show through; Closed is a sealed dark baffle. Caption under the object names the two lands. **Open** is nova, **Closed** is dim.

Defaults: Size 0.15 (combo), Back 0.25 (slightly open), binaural off.

## Persist and slug

Settings keys: `cabSize`, `cabBack`, `binaural`.

Slug v3 is **24 characters** (18 bytes, same alphabet + scramble):

| Bytes | Payload |
| --- | --- |
| 0-5 | star |
| 6-13 | FX |
| 14 | flags: bit 0 shimmer, bit 1 binaural |
| 15 | Size |
| 16 | Back |
| 17 | reserved 0 |

Decode by length: 8 = v1 star (cab defaults), 20 = v2 (cab defaults, binaural off), 24 = v3.

## Assets

| File | License | What we ship |
| --- | --- | --- |
| `assets/irs/even_v30_sm57.wav` | CC0, Jester Dyne Brutal Pack | Trimmed Cookie Monster (V30 + SM57) |
| `assets/hrtf/sadie_d2_front_{L,R}.wav` | Apache 2.0, University of York | SADIE II D2, az 0°, el −15°, 1.2 m |
| `assets/hrtf/NOTICE` + `LICENSE.txt` | Apache 2.0 | Attribution required by SADIE |

Do not ship the whole Brutal zip or the SADIE database.
