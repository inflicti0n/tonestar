# Vocals mode: research and a first map

Guitar stays the default. This is a note so we can pick a vocal star and a vocal FX ring the same way we picked the guitar ones: jobs, not genres, not a plugin list.

The vocal chart does not have to be 6 pulls and 8 handles. That count is a guitar accident. Vocals get as many as it takes to cover the common cases, and no spare copies.

Cab goes away. Input, In/Out, mute, Devices, presets drawer, tape, and looper stay. Vocal presets and guitar presets should not mix.

ToneStar is a standalone box you sing through, not a DAW. Live and practice matter more than Melodyne-style editing.

---

## What people actually want

Across Nectar, Auto-Tune, Neural DSP Mantra, UA Topline, CLA Vocals, Boss VE-500, and TC Helicon VoiceLive, the jobs repeat. Genre pages change the *settings*, not the jobs.

**Make the cheap take usable**

- Hide the room and the fan. Gate, high-pass, sometimes a denoise. Bedroom mics are noisy.
- Stop the pops and the S's. Plosives and sibilance are the first things people hate after they add air or compression.
- Even the level. Whisper to belt without riding a fader. Serial compression (fast then slow) is the usual answer. "Vocal rider" is the same job with a different name.
- Hear the words. Presence around 2 to 5 kHz. Air above 8 kHz if it does not get lispy.

**Sound like a record, not a hallway**

- A plate or a small room that sits *behind* the voice. Ducked reverb is the modern default. Long wet halls are for ballads and backgrounds.
- Slap or a short throw delay, often tempo-synced, often ducked. Guitar-style dotted wash is less common on a lead vocal.
- A double. One extra voice, slightly late and detuned, is how pop and rock get wide without a chorus pedal.

**Fix pitch, or make pitch the sound**

- Light tune so a take sits. This is the default ask.
- Hard tune as a style (Cher, T-Pain, modern rap). People want a knob from "nobody notices" to "that is the effect."
- Key and scale. Without them, tune and harmony guess wrong.

**Make one voice into more voices**

- Unison double.
- 3rd / 5th harmony. Live boxes (VoiceLive, VE-500) often steal the key from the guitar. We do not have that yet.
- A choir stack. Useful, easy to make cheap. Needs humanize (timing, level, formant) or it sounds like a chip.

**Give the voice a character**

- Saturation / grit so it holds against guitars. Rock and metal screams live here.
- Telephone / radio / lo-fi. Very vocal. Almost nobody puts a guitar through a phone filter as a *main* move.
- Formant. Darker, younger, monster, "not my body." Little AlterBoy made this a default ask.
- Vocoder / robot. Fun, not the center. VoiceLive and VE-500 both ship it because people expect it.

**Sit in a band**

- Cut through distorted guitars (mid push, clamp, less reverb).
- Stay dry and in your face for rap.
- Stay soft and dark for jazz / bedroom / soul.

CLA Vocals is the shortest version of this whole list: bass, treble, compress, reverb, delay, pitch-width. Six faders. That is how little "vocal software" has to be if the defaults are good.

Nectar's assistant is the long version of the same list: Shape (tone), Intensity (dynamics), FX (space), Width, Voices, Backer.

---

## How they build the chain

Order is boring and settled. We should not invent a new one.

1. Gate / high-pass / cleanup
2. Pitch correction (if any)
3. Subtractive EQ
4. De-ess
5. Compression (often two stages)
6. Saturation
7. Tonal EQ (body, presence, air)
8. Double / harmony / formant
9. Delay and reverb on the side, ducked

Corrective work is serial. Space is parallel. That maps cleanly onto ToneStar: the star is the "amp" (steps 1 to 7), the ring is the rest.

---

## What replaces the guitar amp

The guitar star rewrites one amp. Center is already playable.

The vocal "amp" is a channel strip, not a tube stack and not a cab.

Always on, even at the center (all pulls at zero):

| Stage | Why |
| --- | --- |
| High-pass | 80 to 120 Hz for sung voice, higher for screams. Kills rumble and proximity mud. |
| Light gate | Bedroom noise. Easy to get wrong if it chatters. |
| De-ess | Always. Air and compression make S's worse. |
| Gentle leveler | 2 to 4 dB. The take should already feel even. |
| Soft presence | A little 3 kHz so a dry vocal is not a pillow. |

No cab. No IR. No binaural-of-a-speaker unless we later add a "headphone vocal" extra. User said cab disappears. Leave the hole. The star can sit lower or we keep the empty space. That is a layout question, not a sound one.

Pitch correction is *not* a good always-on default at hard settings. A little is fine. Hard tune is a ring handle.

---

## How many (this is the actual question)

Count independent jobs. If two pulls do the same work, one of them is decoration. If a common use case has no pull, we are short.

### What other products put on the front

They do not agree on 6 and 8. They agree on a smaller set.

| Product | Tone on the front | FX on the front | Notes |
| --- | --- | --- | --- |
| CLA Vocals | 3 (bass, treble, compress) | 3 (reverb, delay, pitch-width) | Mix strip. No tune. No harmony. Too small for us. |
| Nectar Assistant | 2 (Shape, Intensity) | 4 (FX blob, Width, Voices, Backer) | FX is verb+delay+mod in one. Backer is AI. We skip that. |
| VoiceLive 3 | 1 (Adaptive Tone / Enhance) | 11 blocks, **6 on the floor** | Floor: µMod, Delay, Reverb, Double, Harmony, plus HIT. HardTune is a remap. They say it out loud: 11 effects, 6 switches. |
| VE-500 | 1 (Enhance) | Pitch, Harmony, 4 FX slots, 2 space | Pedalboard slots, not kinds. Radio / lo-fi live in those slots. |
| Mantra | strip (gate, de-ess, comps, sat) | Tune, harmonies, doubler, delay, verb | Same kinds as below. |
| UA Topline | 1 strip | Tune+formant, de-ess/gate, space/mod | Formant rides with tune. |

The live number that matters is VoiceLive's: they *have* eleven vocal blocks and still only put six under the foot. The six they protect are double, harmony, delay, reverb, modulation, and a HIT. Hard tune and transducer (phone) are in the box but not worth a dedicated switch by default.

Nectar's assistant is even coarser: two tone macros, then space, width, and extra voices.

So the market's "how many can a person aim at" is about **2 to 3 tone macros** and **5 to 6 effects**. ToneStar's star is richer than a macro, so we can go a bit above 2 tone pulls. We should not go to 6 just to keep a hex.

### Tone: which jobs will not fold

| Job | Independent? | If we drop it |
| --- | --- | --- |
| **Clear** (more natural than center) | No. Center *is* this. | Nothing. A good center is a usable voice. |
| **Crush** (how slammed) | Yes. | No pop, no rap. |
| **Grit** (harmonics) | Yes. Not the same as crush. You can be loud and clean, or dirty and dynamic. | No rock, no scream character. |
| **Tight** (HPF, less low-mid, closer) | Yes. Not the same as crush. Jazz can be close without being slammed. | Rap and screams get muddy. |
| **Cut** (2 to 5 kHz forward) | Yes. | Will not sit on guitars. |
| **Warm** (body, less air) | Yes, and not just "Cut at zero." Zero Cut means no extra presence. Warm *adds* chest and takes air. R&B wants both body and some presence. | Ballad / soul / jazz go thin. |
| Body / Scream as extra pulls | No. Grit+Tight is the scream. Warm is the chest. | Do not add a 6th or 7th. |

**Star count: 5.** Crush, Grit, Tight, Cut, Warm. Center is Clear.

4 is too few (you must kill Tight or Warm, and a use case dies).
6 is a spare Clear that fights the center.
7 is genre spokes. Do not.

### FX: which jobs will not fold

| Job | Independent? | If we drop it |
| --- | --- | --- |
| **Tune** | Yes. Nothing else does pitch-to-scale. | We are not vocal software. |
| **Double** | Yes. The record-width move. | Pop and rock sound like one dry take. |
| **Width / chorus** | No. CLA and a lot of mixes treat this as the same fader as double (stereo / wide / spreader). | Fold into Double at the high end. |
| **Echo** | Yes. Slap and throws are not reverb. | Country and pop lose the word-repeat. |
| **Bloom** | Yes. Space is not delay. | Everything sits in a dead booth. |
| **Stack** (harmony) | Yes. Intervals, not unisons. | VoiceLive's whole reason for existing. |
| **Phone** (radio / lo-fi / telephone) | Yes. A filter, not a space. VoiceLive calls it Transducer. VE-500 ships Radio / Lo-fi. Rap and electronic use it as a *part*, not a toy. | We miss a common production case. Worth the 6th handle. |
| **Formant** | Half. Independent processor, uncommon as a session default. | Right-click Tune (gender / monster). Not a handle. |
| **Vocoder / robot** | No for v1. | Right-click Phone later. |
| **Sweep / Pulse / Talk** | No. Not what people open vocal software for. | Gone. |
| **Squeeze** | No. That is Crush on the star. | Gone. |
| **Choir / Stutter / Rhythmic** | No. VoiceLive has them and still leaves them off the floor. | Stack at the high end can choir a little. |

**Ring count: 6.** Tune, Double, Echo, Bloom, Stack, Phone.

5 (drop Phone) still covers "sing a song." It fails "radio verse / lo-fi hook," which is a real case, not a gimmick.
7 (add Formant) starts the toy shelf. VoiceLive's extra five blocks live there. We should not.

### Use cases vs this count

If a row has a hole, the count is wrong.

| Use case | Star | Ring | Hole? |
| --- | --- | --- | --- |
| Bedroom / hear myself | Center | none | No |
| Jazz / folk / choir | Warm | small Bloom | No |
| Country | Cut | Echo slap | No |
| Pop radio | Crush + Cut | Double, Echo, Bloom | No |
| Rap / dry pop | Crush + Tight | Tune optional, Phone optional | No |
| R&B / soul | Warm + Crush | Double, Bloom | No |
| Rock | Grit + Cut | Double | No |
| Metal scream | Grit + Tight | none or tiny Bloom | No |
| Metal clean | Cut | Double, Bloom | No |
| Hard-tune pop | Crush + Tight | Tune up, Double | No |
| Stacked chorus | Crush + Cut | Stack, Double | No |
| Radio / telephone verse | Tight | Phone | No |
| Spoken / podcast | Tight | none | No |
| Formant / monster | any | Tune right-click | Covered without a handle |
| Vocoder | any | not v1 | Acceptable miss |

No row needs a 6th star pull or a 7th handle.

### The number

**5 pulls on the star. 6 handles on the ring.**

Guitar is 6 + 8. Vocals are 5 + 6. That is 11 aimed controls instead of 14, and the two we dropped (Clear, Width/Form/Pulse/Sweep/Talk/Squeeze) were copies or guitar leftovers.

Layout follows the count. A five-point star is fine. A six-handle ring around it is fine. Do not add a dummy sixth spoke so it still looks like the guitar hex.

---

## Star archetypes

Each vertex is a job. Pull one or stack a few. They rewrite one strip. Center is already a usable voice (the old Clear).

| Pull | You want | What it actually does |
| --- | --- | --- |
| **Grit** | Bark. Hold against guitars. | Saturation, mid push, a bit of 1176-style clamp. Rock, and the scream's teeth. |
| **Crush** | In your face. Even, loud, modern. | Stacked compression, louder makeup. Pop chorus, rap lead. |
| **Tight** | Close and dry. No hallway. | Higher HPF, faster release, less low-mid, gate a bit hungrier. |
| **Cut** | Sit in the band. | Forward 2 to 5 kHz, dip mud 250 to 400 Hz. |
| **Warm** | Darker and smoother. | Body 150 to 300 Hz, less air, slower optical-style compress, softer clip. |

### Pairs that should work

- Crush + Tight = modern rap / dry pop
- Warm (alone, or + a little Crush) = jazz, folk, bedroom, soul
- Grit + Cut = rock that cuts a dense mix
- Crush + Cut = radio pop
- Grit + Tight = metal scream
- Tight (alone) = spoken, podcast, tight ad-lib

### Genre is a *recipe*, not a spoke

| If they say | Start with |
| --- | --- |
| Pop | Crush + Cut, then Double, Bloom, Echo |
| Rap / hip-hop | Crush + Tight, optional Tune, optional Phone |
| Rock | Grit + Cut, Double |
| R&B / soul | Warm + Crush, Double, Bloom |
| Country | Cut, Echo slap |
| Metal scream | Grit + Tight |
| Metal clean | Cut, Double |
| Jazz / choir | Warm, small Bloom |
| Hard-tune pop | Crush + Tight, Tune up |

No Scream spoke. No Clear spoke. If a pair is weak, fix the LUT.

### Names

- Grit, not Crunch or Drive. Drive sounds like a pedal.
- Crush, not Heavy. Heavy is a guitar lie.
- Tight / Cut / Warm can keep those words. The jobs match.

---

## FX ring

Six handles. Not eight. Guitar leftovers (Squeeze, Talk, Shift, Sweep, Pulse, Width-as-chorus) do not get a seat.

**On the dry voice**

| Handle | Land, low to high | Why it is here |
| --- | --- | --- |
| **Tune** | polish to noticeable to hard | The number one vocal-only ask. Needs a key. |
| **Double** | fatten to ADT to a wide stack | Width lives here at the top. Not a second handle. |

**After the strip**

| Handle | Land, low to high | Why it is here |
| --- | --- | --- |
| **Echo** | slap to 1/8 to a dotted throw | Duck it. Sync to the tape BPM. |
| **Bloom** | room to plate to hall | Duck it. No shimmer by default. |
| **Stack** | 3rd below to 3rd+5th to a small choir | Harmony. Needs a key. |
| **Phone** | dull lo-fi to radio to telephone | The one extra production case that is not space and not pitch. |

No shimmer. No formant knob. Vocoder on Phone later, if we care.

---

## What should not go on the star or the ring

These show up in every "best vocal plugin" list. They are the wrong shape for ToneStar.

| Thing | Why skip, or park it |
| --- | --- |
| Melodyne / graph tune | Offline editor. We are live. |
| VocAlign | Needs two tracks. We have one input. |
| RX / Clarity denoise | Heavy, latency, and a different product. A gate is enough at first. |
| AI voice / Backer / clone | Creepy, huge, not us. |
| Vocoder as a handle | Cool. Third-tier. Right-click Phone later. |
| Manual clip gain / vocal rider graph | We have Crush and In. |
| Mic modeling | Marketing. A high-pass and a shelf do 90% of it. |

---

## Presets and the slug

Guitar slugs describe star + FX + shimmer + binaural + cab.

Vocal slugs should describe star + FX + key, and must not load on a guitar session as if they were a cab+amp.

Simple rule: the slug carries a mode bit. Guitar codes ignore vocal fields. Vocal codes ignore cab. No binaural field on a vocal slug. The drawer has two lists, or one list with a guitar / vocal mark. Do not show a Tight Stack guitar preset when you are in vocals.

Share codes staying one string is still good. People already paste those.

---

## Latency

Tune and Stack will add delay. Guitar mode can stay as it is.

For vocals we should pick a budget and stick to it. Live boxes advertise "low latency" because singers cannot perform 30 ms late. If Tune at "polish" needs a smaller window than hard tune, that is allowed. Hard tune can cost more.

This is a build constraint, not a taste one.

---

## Locked

- Five-point star. Phone stays. Tune stays on the ring.
- No binaural in vocals. No shimmer in vocals.
- Top of the window: **ToneStar** stays the title. **GUITAR | VOCALS** sits after the drawer buttons on the left. One click. Swaps the star, hides the cab, swaps the preset list, leaves tape and looper alone. Mid-session flips keep rolling. No dialog.
- Key: only in vocals. Tiny control, top right, outside the star and the ring. Scroll or hold-and-drag to walk keys. Small up/down arrows next to it. Hide it in guitar.
- Key display is the normal musician spelling: `C`, `Cm`, `C#`, `C#m`, `D`, `Dm`, `Eb`, `Ebm`, and so on. Click the circle to flip major / minor. Accidentals are the usual ones (C#, Eb, F#, Ab, Bb), not a second name for the same key.

## Dry tape

A vocal lane does **not** print the wet chain. The WAV is the mic after channel pick, before In, star, FX, and de-ess. The vocal slug (star, ring, key) is stored on that lane. Play and export run that file through **that lane's** slug. Change Crush on the selected vocal track and the take updates. No re-sing.

Guitar lanes stay printed after amp and cab. No slug on a printed lane. Old sessions are all printed.

Through-chain clips show a small V disc. Selecting a vocal track does **not** flip GUITAR to VOCALS. The title toggle is the only mode switch. If you are already in VOCALS, selecting a vocal lane loads that slug. Live edits to the star, ring, or key write the slug back onto the selected through-chain lane.

Vocal lanes keep running their stored slugs in guitar mode, so you can play guitar on top of processed vocals. Do not send a vocal WAV through the amp.

Two vocal takes can disagree (verse Crush, chorus Warm). Each plays through its own stored slug.

Looper stays wet-after-tape. Do not change it.

In and Out sit on the vocal chain, so balance is part of what you can change after the take. That is why the tap is before In.

## Shimmer

Not a must. Skip it.

Shimmer is an octave-up cloud on a reverb tail. Guitarists use it as a wash. On a lead vocal it is a special effect (Enya, some shoegaze doubles), not how a song vocal is finished. A complete vocal path is tune, de-ess, level, tone, a double, a plate, a slap. Bloom already covers space. If we ever want sparkle on a voice, that is a later right-click, not v1.

## Formant (not a control)

Two different things share that word.

**Preserve**, inside Tune: when a note is pulled to the scale, keep the throat the same size so the singer still sounds like themselves. Without this, even light tune goes thin or chipmunky. This is just how Tune should work. Hidden. No knob.

**Shift**, as an effect: make the voice younger, older, male, female, monster. That is Little AlterBoy. Fun, not how you finish a song vocal. No point putting it on Tune. Not v1.

## Hidden de-ess

Yes. Keep it always on, and keep it hidden.

A song vocal without a de-ess is not finished. Cut, Crush, and any air shelf make S's worse. Every live box buries this inside Enhance. Every mix chain has one.

Hidden is fine if it is conservative and it moves with the star. More Cut / more air means a little more de-ess. A fixed "one singer" setting will lisp on a bright voice and miss a dull one. Do not put a knob on it unless people complain. If they do, a right-click on Cut is enough.

## The real bar: a song, in here, no other software

This is the product test. Not "a fun vocal FX mode." Someone should be able to track a song vocal on tape, export the WAV, and not need Nectar, Auto-Tune, or a DAW chain to make it listenable.

That means play and export are the same mix: dry vocal lanes through their stored slugs, printed guitar lanes as they were taped. What you hear is what the file becomes, not a second hidden chain. A vocal WAV by itself is the dry mic. Do not bounce those files as-is.

What "good enough" has to include, in the box:

| Need | How we cover it |
| --- | --- |
| Even level, whisper to belt | Center leveler + Crush |
| No harsh S's | Hidden de-ess |
| Words clear, not a hallway | Center strip + Tight / Cut / Warm |
| Pitch that sits, or hard-tune as a choice | Tune on the ring + Key |
| A record-wide lead | Double |
| A place to stand | Bloom and Echo, ducked, BPM from tape |
| Extra parts without a second singer | Stack + Key |
| A radio / lo-fi verse | Phone |
| Sit on a guitar track you already taped | Switch to VOCALS, arm another lane |
| A file you can send | Export WAV, same as now |

What we still will not be, and that is OK:

- Note-by-note Melodyne. Live tune only.
- Aligning two takes to each other. One input.
- Killing a bad room the way RX does. Sing closer, use Tight, live with a gate.
- Mixing a full band arrangement. Eight tape lanes plus a looper is the song.

Latency has to stay singable. If Tune at "polish" is late, people will not record through it, and the bar fails. Polish should be the low-latency side. Hard tune can cost more.

In and Out still matter. A cheap mic into a hot interface will clip before the star. The center strip should forgive a bit, not pretend gain does not exist.

---

---

## Sources (the ones this note is built from)

Not a reading list. These are the products and writeups that kept agreeing with each other.

**All-in-one vocal suites**

- iZotope Nectar 4: Pitch, Voices, Backer, Comp, De-ess, Delay, Dimension, EQ, Gate, Reverb, Saturation. Assistant intents: Shape, Intensity, FX, Width, Voices. Targets: sung / rap / dialogue.
- Neural DSP Mantra: tune, gate, de-ess, comps, saturation, harmonies, delay, Lexicon-style verbs, doubler.
- UA Topline: channel strip, tune + formant, de-ess, gate, reverb / delay / mod, key finder.
- Waves CLA Vocals: bass, treble, compress, reverb, delay, pitch-width. Three flavors each.
- Antares Auto-Tune + EFX: tune as the product, then a rack. Harmony Engine as the stack.
- Klevgrand Altitude: real-time tune, three-voice harmony, doubler, formant, zero-latency pitch.

**Live boxes (closest to us)**

- TC Helicon VoiceLive 3: harmony from guitar chords, double, hard tune, vocoder, reverb, delay, transducer. Footswitches for those, not for EQ.
- Boss VE-500: Enhance (comp, de-ess, EQ), pitch correct, harmony / vocoder, radio, lo-fi, distortion, then two verb/delay blocks.

**How people actually mix**

- Standard chain writeups (BandLab, Music Guy Mixing, Ghostnote rap chain, Sean Kim 2025): cleanup, then tone, then space on sends.
- Nail The Mix metal notes (Asking Alexandria, Wage War, Cradle of Filth): screams are HPF + saturate + clamp. Cleans are presence + air + careful de-ess. Space stays small.

**LANDR 2026 plugin roundup, in one line:** the category is still tune, de-ess, EQ, level, double. Alignment tools are for multi-track DAWs. We can ignore those.
