# ToneStar: what each pull does

Drag a spoke. **0 is off. 1 is the full recipe.** None of these are volume. **Out** is volume.

```
guitar → In → FX (clean) → STAR → FX (around the amp) → cab → Out
```

---

## Star: what amp am I standing in

Center is already a playable amp. Pulling a vertex *adds a job*.

```
              Clean
           /         \
        Warm           Crunch
           \         /
            Cut - Heavy
                \
               Tight
```

| Pull | You want | What happens |
| --- | --- | --- |
| **Clean** | Hear the guitar | Headroom, pick, sparkle. Less dirt. |
| **Crunch** | Edge of breakup | Classic rock grind. Mids, sag, not squash. |
| **Heavy** | Chug / saturate | More stages, more compression, palm-mute clank. |
| **Tight** | Not flubby | HPF, less bloom, faster. **Adds no gain.** |
| **Cut** | Sit in the band | Forward mids. **Not** ice-pick treble. |
| **Warm** | Stop the fizz | Darker highs, smoother clip, a bit of body. |

**Pairs that work:** Heavy + Tight = modern rhythm. Clean + Warm = jazz/bedroom. Crunch + Cut = it cuts a mix. Heavy + Warm = thick, not harsh.

---

## FX ring: what is happening around that amp

All start at 0. Three listen to the *clean* string. Five sit *after* the amp.

### Before the star (needs a clean guitar)

| Pull | You want | Low → high |
| --- | --- | --- |
| **Squeeze** | Even / sticky | Glue → sustain → squash. Not a secret drive. |
| **Talk** | Wah / quack | Mild attack quack → full vocal sweep. |
| **Shift** | Bigger / organ | Fat low octave → 12-string → synth stack. |

### After the star (needs the finished amp)

| Pull | You want | Low → high |
| --- | --- | --- |
| **Echo** | Slap / bounce / wash | Slapback → dotted rhythm → shoegaze wash. |
| **Bloom** | Spring / pad | Kiss of spring → plate → cloud. |
| **Width** | Two guitars / 80s | Slow thicken → deeper chorus. |
| **Sweep** | Swirl / jet | Phaser → flange. |
| **Pulse** | Chop / surf | Soft throb → hard trem chop. |

> **Shimmer** is not a ninth pull. Right-click the **Bloom** label. Same handle, octave-up on the tail.

---

## Cab: what speaker is it in

Always on. Not on the FX ring. Not a second Bloom. Poke the box.

| Gesture | You want | Lands |
| --- | --- | --- |
| **Wheel** | How big the box feels | Combo → Twin → Stack |
| **Click** | Jump to the next cabinet | Combo → Twin → Stack → Combo |
| **Right-click** | Open air vs wall | Open / Closed |
| **Binaural** | Headphones hear a cab in front | Off = L=R. On = in-front, slightly down. |

---

## Chrome

Void page, solid fills, no outlines. Space Grotesk on the title, Gaegu on the chrome.

| Control | Does |
| --- | --- |
| **In** | How hard you hit the amp. Not tone. |
| **Out** | How loud the room is. The only volume knob. |
| **Mute** | Circle. Lights flare when silent. Title on hover. |
| **Binaural** | Circle. Lights nova when the cab is in front. Title on hover. |
| **Debug** | Circle. Lights starlight while the peak log runs. Title on hover. |
| **Presets** | Circle, top left. Opens a drawer on the right. Title on hover. |
| **Advanced** | Circle, next to Presets. Opens a drawer on the left. Title on hover. |
| **Looper** | Circle, next to Advanced. Opens a strip under the window. Title on hover. |

## Tape (Advanced)

Grows left, three times the old strip. This is a **linear practice tape**, not a second looper. Record the finished guitar (after Out). Play the other lanes. The click never goes on a track. The looper is never baked into a track. Mute silences the room, not the tape.

Clips live in `Documents/ToneStar/tape` as `track0.wav` … `track7.wav`. The WAV stays whole. Dragging either edge only changes a virtual in or out point.

| Control | Does |
| --- | --- |
| **Play / Pause** | One button. Play when stopped or paused, pause while it is running |
| **Stop** | Returns the playhead to zero |
| **Record** | Writes the armed empty lane. Stop freezes the playhead. Will not replace a clip. Delete first |
| **Q** | Grid. Rec waits for the next beat and closes on a beat. Move and trim snap to beats |
| **Loop** | Lights a range on the ruler. Play wraps at the right edge. Drag either border to resize, drag the bar to move |
| **Folder** | Open the tape directory |
| **Export** | Bounce unmuted lanes into one 24-bit stereo WAV. Same mix as playback. Pick a start time and length in seconds |
| **Click lane** | Arm that lane. The whole row lights |
| **Click clip** | Select it (starlight border). X or Delete / Backspace removes it |
| **Mute** | Skip that lane in the mix. Does not change the file |
| **Level** | Vertical fader next to the name. How loud that lane is in playback and export |
| **Right-click name** | Rename. Click elsewhere to finish |
| **Drag clip** | Move it on the timeline. Does not zoom or rescale the grid |
| **Drag left / right edge** | Virtual crop. Right edge stays put when you crop the left. Never writes a new WAV |
| **Wheel on ruler or wave** | Zoom around the pointer. Shift+wheel pans |
| **Drag ruler** | Pan the view. Click the ruler to jump the playhead |
| **Playhead triangle** | Downward starlight handle on the ruler. Drag it to seek |
| **Metronome** | Enable dry clicks |
| **BPM** | Click to type, hold-drag, or scroll |
| **Sparkle** | Pulses on each click |

## Looper

Grows down. The phrase sits **after Out and Tape**. Live guitar always passes through. The click never enters a phrase. Mute silences the room; the playhead keeps walking. Tape writes guitar only, never the loop.

Default is **Simple**: one phrase, Ditto / RC-1.

| Gesture | Does |
| --- | --- |
| **Tap / Space down** | Empty → record. Recording → play. Play → overdub. Overdub → play. Stopped → play. Space only while ToneStar is focused |
| **Double-tap** | Stop (keep the phrase). During first record or while armed, discard |
| **Hold ~1.5 s** | Playing / overdub → undo or redo the last layer. Stopped / armed → clear |
| **Q** | Quantize. Off = free length. On = wait for the next beat to start, close on a later beat |
| **Level** | How loud the phrase is next to you. Not Out |

Right-click the **Looper** title for **Advanced** (Bloom → Shimmer). Two phrases, like an RC-500. Each has rec/play, stop, and level. The first closed phrase is master; the second locks to its length. Restart waits for the master’s downbeat.

## Presets

The drawer stores the **share slug** (star, FX, shimmer, binaural, cab). In / Out / mute / devices stay as they are.

| Gesture | Does |
| --- | --- |
| **Plus** | Save the current tone as `Preset #1`, then `#2`, and so on |
| **Double-click** | Load that preset now |
| **Right-click** | Rename as you type |
| **X** | Delete that preset |

Metal is often **just the star + Stack**. All FX at 0 is a valid sound.
