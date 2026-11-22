note lengths are in beats# Choco Arpegio — Engine Logic & Parameters Spec

Source of truth for implementing Choco Arpegio. Read this before touching the engine, params, or UI.

- Repo: https://github.com/zhirnovcodes/choko-arpegio
- Sibling projects to copy structure/patterns from:
  - `C:\Work\BananaChords` — same repo layout (`core/domain`, `core/engine`, `hosts`, `apps`, `params.json`, `build-wasm`, pnpm workspace). Its **drift** / time-table logic (`core/domain/Sequences.cpp`, `buildTimeTable`) is the model for **swing** here.
  - `C:\Work\Beatality` — its **chaos** system (`core/domain/Chaos.*`, `ChaosRanges.h`, `ChaosSettings.h`) is the model for chaos here.
  - `C:\Work\IvanDAW\ivan-daw\docs\plugin-dev` — plugin architecture, web host, VST host, CI/CD, IvanDAW integration, checklists. Follow it.

---

## 1. What the plugin is

A MIDI arpeggiator / note generator.

- **Input:** MIDI notes (a held chord).
- **Output:** MIDI notes (one note at a time, a generated sequence over the held chord).
- Timing is driven by the host (DAW) playhead in beats.
- All randomness comes from pre-generated **sequences**. The audio/MIDI thread only *reads* sequences; it never rolls random numbers per frame (except chaos rolls, which regenerate sequences/params at defined trigger points).

---

## 2. Units and conventions

- **Time unit = beats** (quarter notes). `1` = one beat, `1/4` = a sixteenth note.
- Every length param marked "only pow of 2" is stored as an **integer exponent** `e`, value = `2^e` beats (same approach as Beatality `fxTimeMin`/`fxTimeMax`). The UI displays it as a fraction (`1/16`, `1/8`, … `16`).
- **Grid quantum `Q`** — smallest time step used for swing shifts and the time table. Use `Q = 1/192` beat (64 × 3, `kQuantaPerBeat = 192`), so every straight length (2^n beats) and every triplet of one (1/3, 1/6, 1/12 … beat = 1/8t, 1/16t, 1/32t …) is a whole number of quanta. All lengths/borders are integers in quanta. This satisfies "moves should be done in fractions, not free time values".
- If a `min` param ends up above its `max` (user or chaos), **swap them** when generating (Beatality behaviour).
- Random values: `unit()` = `[0, 1)`, `signedUnit()` = `[-1, 1)`. Use a seeded RNG (copy `Rng.h` from BananaChords). Chaos uses its own RNG instance so chaos rolls don't disturb sequence RNG state.

---

## 3. Sequences (generation)

Generated on **Shuffle**, on load, and when a param listed in the "Sequence changed" column changes (see §6). Fixed max sizes (no allocation on the audio thread).

Let `N` = number of elements in the `times` sequence.

### 3.1 `times` — note slot lengths
Splits the loop into slots; each slot holds one note.

- Inputs: `loop`, `noteLengthMin`, `noteLengthMax`.
- Each element is a length in **beats**, a **power of two** in `[noteLengthMin, noteLengthMax]` (ranges in the params table, §6). Roll the exponent uniformly between the min and max exponents.
- **Sum of all elements == `loop`** (exactly).
- Example: `loop = 4`, `min = 1/4`, `max = 1` → `1/2, 1, 1/4, 1/4, 1, 1`.
- Generation: fill the loop left to right; each roll is limited to lengths that still fit the remaining space (`<= remaining`). Since `loop` and all lengths are powers of two, the remainder is always a multiple of `noteLengthMin` and the loop always fills exactly.
- Edge cases:
  - `noteLengthMin > loop` → a single element equal to `loop`.
  - `noteLengthMax > loop` → effective max is `loop`.
- Max `N` = `loop_max / noteLengthMin_min` = `32 / (1/16)` = 512. Size arrays for this.

### 3.2 `swing` — pairwise shift values
- Size `N - 1`, one value per neighbouring slot pair `(i, i+1)`.
- Each value is `signedUnit()` in `[-1, 1)`.
- Scaled at time-table build by the **swing** param (`[0, 1]`): effective `d = swing[i] * swingParam`.
- This is the same as BananaChords **drift** (`generateDrift`), renamed.

### 3.3 `sustain` — sounding length of each note
- Size `N`.
- If **legato** is on: `sustain[i] = slot length after swing` (note fills its slot, no gap). Compute at time-table build, not stored as random.
- Else: `sustain[i]` = random power of two in `[sustainMin, sustainMax]` (roll an integer exponent uniformly between the two exponents).
- Sustain is clamped to the slot length (after swing) — a note never overlaps the next slot.

### 3.4 `timeTable` — playhead → slot index or SILENCE
Built (not random) from `times`, `swing` × `swingParam`, `sustain`, `legato`.

1. Slot lengths in quanta: `len[i] = times[i] / Q`. Borders: `border[0] = 0`, `border[i+1] = border[i] + len[i]`.
2. Apply swing to every inner border `i` (between slot `i-1` and slot `i`), with `d = swing[i-1] * swingParam`:
   - `d > 0`: first slot grows to the right by `d * len[i] / 3` (up to a third of the second slot).
   - `d < 0`: second slot starts earlier by `|d| * len[i-1] / 3` (up to a third of the first slot).
   - `d = 0`: no move.
   - Snap the shift to the nearest musical length not above the limit: straight (2^j beats: 1/64, 1/32 … beat) or triplet (2^j / 3 beats: 1/8t, 1/16t …); 0 when below half a quantum (`swingShift` in `TimeTable.cpp`). E.g. next to a 1-beat note, Swing 1 moves the border 1/3 beat (an 8th triplet), Swing 0.5 moves it 1/6 beat (a 16th triplet).
   - Clamp so every slot keeps at least 1 quantum (forward pass then backward pass — copy BananaChords).
3. Sounding part of slot `i`: `[border[i], border[i] + min(sustain[i], border[i+1] - border[i]))`. The rest of the slot is **SILENCE**.
4. Cell size = gcd of all borders and all sound-end points (in quanta); `cellCount = loopQuanta / cellSize`. Each cell stores a slot index or `SILENCE` (e.g. `0xFFFF`).
   - Cell count ≈ `loop / smallest-resulting-segment` ("min chord length with applied swing").
5. A slot whose sounding length rounds to < 1 quantum is entirely SILENCE.

Lookup: `cell = floor(positionInLoopQuanta / cellSize)` → slot index or SILENCE. `positionInLoop` wraps by `loop`.

### 3.5 `patterns` — arpeggio algorithms over the loop
Plain random notes sounded too random, so notes come from arpeggio **algorithms** (`core/domain/Algorithms.*`).
The algorithm list and the choice are hidden from the user.

1. Roll a pattern length: a whole number of beats in `[2, 8]`.
2. `count = ceil(loop / pattern length)` (at least 1, at most 16). Each pattern is an `AlgorithmData`:
   - `startNote` — `unit()`: the algorithm's base note,
   - `randomOffset` — `signedUnit()`: scaled by the **RNG** param at play time,
   - `algorithm` — index of a random algorithm,
   - `notesCount` — one of `1, 2, 3, 4, 6, 8`: how many neighbouring chord notes the algorithm walks over.
3. A slot belongs to the pattern its start (before swing) falls in.

Seeded by `kSeedNotes`; regenerated with the other sequences (§6.1).

**Algorithms.** Interface `ArpegioAlgorithm`: `start(AlgorithmParams{baseNote, notesCount, rng})` and
`float next(int chordSize)` → note value in `[0, 1]`. The engine makes one instance of each at start.
Each walks a window of `min(notesCount, chordSize)` chord notes; its lowest note is the one `baseNote` picks,
moved down so the window fits inside the chord. `next` returns the middle of the chosen note's band.

| # | Algorithm | Order (C E G) |
|---|---|---|
| 0 | Up | C E G C E G |
| 1 | Down | G E C G E C |
| 2 | Up-Down (exclusive) | C E G E C E G |
| 3 | Up-Down (inclusive) | C E G G E C C E G |
| 4 | Down-Up | G E C E G E C |
| 5 | Converge | C G E (outside notes, moving inward) |
| 6 | Diverge | E C G (middle, moving outward) |
| 7 | Thumb Up | C E C G (lowest note between the others) |
| 8 | Pinky Up | G C G E (highest note between the others) |
| 9 | Alberti | C G E G (low, high, middle, high) |
| 10 | Random Walk | each step ±1 position, reflecting at the ends; seeded by `rng` |
| 11 | Leapfrog | 1 3 2 4 3 5 |
| 12 | Staircase | 1 2 3, 2 3 4, 3 4 5 |
| 13 | Trill | 1 2 1 2, 2 3 2 3 |
| 14 | Root Bounce | 1 2 1 3 1 4 (back to the chord's root) |

Window positions past the end wrap around inside the window (Leapfrog, Staircase, Trill).
Not included: **As Played** (the engine sorts the held notes and does not keep press order) and **Chord**
(the engine plays one note at a time).

---

## 4. Engine — one processing iteration (per block / per frame)

1. **Time → slot index**
   - Time source:
     - `retrig = false`: global DAW playhead (beats).
     - `retrig = true`: beats elapsed since the first note-on of the current held chord (sequence restarts on each new chord from silence).
   - `posInLoop = time mod loop`, convert to quanta, look up `timeTable` → slot index `i` or SILENCE.
2. **Slot index → MIDI note**
   - Held chord notes sorted ascending: `chord[0..k-1]`.
   - Find the slot's pattern. On the pattern's first slot: `algorithm.start(...)`; on each later slot: one more
     `algorithm.next(k)`. (A jump, a chord-size change or a grace re-pick replays the pattern from its first slot.)
   - `value = next(k) + randomOffset × RNG`; the pattern's first slot gets no offset. Wrap into `[0, 1)`
     (`value − floor(value)`: 1.5 → 0.5, −0.25 → 0.75), so the offset never piles notes onto the chord's ends.
   - `note = chord[floor(value * k)]` (clamp index to `k-1`).
   - No held notes or SILENCE → no note.
3. **Output diff vs previous frame**
   - Same note as last frame and same slot → keep sounding (no new events).
   - Different (note changed, slot changed, or went to SILENCE) → note-off for the old note, note-on for the new one.
   - **A new slot always restarts the note**, even if it has the same pitch as the previous slot (also in legato): note-off then note-on.
   - Note-off all on transport stop, loop jump/seek, and when all input notes are released.
4. **Output velocity**: pass-through — the velocity of the input chord note being played. No velocity sequence.
5. **Note range**: output notes are **only the notes of the held chord**, at their played pitches. No octave shifting or note-range expansion.

### 4.1 Quick-change protection
Implement this:
- The pitch for a slot is **latched at the slot start** (or first frame of the sounding part). Input chord changes mid-slot do **not** retrigger the current note; they take effect at the next slot start.
- Grace window: if the input chord changes within `1/32` beat after a slot start, re-latch (handles chords played slightly late, like a human hitting keys after the beat).
- This replaces any "change only every N beats" rule — slot boundaries already are the musical grid.
- All input notes arriving at the same frame are one chord: the engine picks the note once, after applying all of them (no on/off/on bursts while a chord is pressed).

---

## 5. Chaos

Same model as Beatality. Logic lives in the **engine**; when chaos changes a param, the engine writes the new value and **pushes it to the host** (so UI and DAW automation display reflect it).

- **Priority:** chaos > MIDI. If a chaos roll and a MIDI CC hit the same param in the same block, chaos wins. A later MIDI CC change overrides again normally.
- **Range blend:** each chaos-changeable param has a range at chaos 0 `[low0, high0]` and at chaos 1 `[low1, high1]`. With `chaosValue = v`:
  `low = lerp(low0, low1, v)`, `high = lerp(high0, high1, v)`, result = uniform roll in `[low, high]`, clamped to the param's limits.
  - Pow-of-2 params: blend and roll the **exponents**, round to a whole exponent (Beatality `chaosStep`).
- **Triggers** (`chaosTrigger`):
  - `none` — never rolls on its own.
  - `note` — rolls on each new input chord (note-on from silence).
  - `beat` — rolls every `chaosBeat` beats.
  - `loop` — rolls every `chaosLoop` loops (`chaosLoop × loop` beats; can be a fraction of a loop).
  - The **Chaos** button rolls in every mode.
  - Counting follows the sequence start: the transport start (play or seek), or with **Retrig** on also each new chord from silence. The start itself is not a roll. Changing the unit size restarts the count (Beatality `Chaos::tick` / `restart`).
- **Groups** (toggle on = chaos may change it):
  | Toggle | Changes | Then regenerates |
  |---|---|---|
  | chaos shuffle | runs Shuffle (new sequences, params untouched) | all |
  | chaos length | `noteLengthMin`, `noteLengthMax` | all |
  | chaos swing | `swing` | time table |
  | chaos sustain | `sustainMin`, `sustainMax` | sustain + time table |
  | chaos chaos | `chaosLoop`, `chaosBeat` | — |
  | chaos rng | `rng` | — |
- Toggles and enums (legato, retrig, trigger type) are never changed by chaos.

### 5.1 `core/domain/ChaosRanges.h`
Chaos ranges are `constexpr ChaosRange{low0, high0, low1, high1}` constants in `core/domain/ChaosRanges.h` (same as Beatality's `ChaosRanges.h`). Pow-of-2 params are given as **exponents** (n = 2^n beats, or loops for `chaosLoop`).

| Param | Chaos 0 | Chaos 1 | Constant (exponents) |
|---|---|---|---|
| `noteLengthMin` / `noteLengthMax` | 1/4 … 8 beats | 1/16 … 1 beat | `{-2, 3, -4, 0}` |
| `sustainMin` / `sustainMax` | 1/4 … 8 beats | 1/32 … 16 beats | `{-2, 3, -5, 4}` |
| `swing` | 0 … 0.2 | 0 … 1 | `{0, 0.2, 0, 1}` |
| `chaosLoop` | 1 … 8 loops | 1/8 … 1 loop | `{0, 3, -3, 0}` |
| `chaosBeat` | 2 … 32 beats | 1/8 … 2 beats | `{1, 5, -3, 1}` |
| `rng` | 0 … 0.22 | 0 … 1.2 (clamped to 1) | `{0, 0.22, 0, 1.2}` |

---

## 6. Parameters

`params.json` uses the same schema as BananaChords/Beatality (`id`, `label`, `kind`, `min`, `max`, `default`, `group`, `automatable`, optional `unit`, `choices`). "MIDI controlled" = `automatable: true` (mappable to host automation / MIDI CC).

Pow-of-2 params are `kind: "int"` exponents; the range column shows beats, the exponent column shows what is stored.

| # | id | Label | Group | Kind | Range (beats) | Exponent range | Default | MIDI | Sequences regenerated | Chaos 0 | Chaos 1 | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `loop` | Loop Length | main | int (pow2) | 1/4 … 32 | -2 … 5 | 4 (e=2) | yes | all | — | — | |
| 2 | `noteLengthMin` | Note Length Min | main | int (pow2) | 1/16 … 16 | -4 … 4 | 1/4 (e=-2) | yes | all | 1/4 … 8 | 1/16 … 1 | |
| 3 | `noteLengthMax` | Note Length Max | main | int (pow2) | 1/16 … 16 | -4 … 4 | 1/2 (e=-1) | yes | all | 1/4 … 8 | 1/16 … 1 | |
| 4 | `legato` | Legato | main | boolean | 0/1 | — | 0 | no | sustain, time table | — | — | When on, Sustain Min/Max are disabled in UI |
| 5 | `sustainMin` | Sustain Min | main | int (pow2) | 1/32 … 16 | -5 … 4 | 1/8 (e=-3) | yes | sustain, time table | 1/4 … 8 | 1/32 … 16 | |
| 6 | `sustainMax` | Sustain Max | main | int (pow2) | 1/32 … 16 | -5 … 4 | 1/4 (e=-2) | yes | sustain, time table | 1/4 … 8 | 1/32 … 16 | |
| 7 | `swing` | Swing | main | float | 0 … 1 | — | 0 | yes | time table (rebuild only) | 0 … 0.2 | 0 … 1 | Scales the swing sequence; like BananaChords drift |
| 8 | `retrig` | Retrig | main | boolean | 0/1 | — | 0 | no | — | — | — | Restart sequence from each new chord |
| 9 | `shuffle` | Shuffle | main buttons | trigger | — | — | — | yes | all | — | — | Re-rolls all sequences |
| 10 | `chaos` | Chaos | main buttons | trigger | — | — | — | yes | — | — | — | Runs one chaos roll |
| 11 | `chaosValue` | Chaos Value | chaos main | float | 0 … 1 | — | 0.5 | yes | — | — | — | Blends chaos ranges (§5) |
| 12 | `chaosTrigger` | Chaos Trigger | chaos main | choice | none, note, beat, loop | — | none | no | — | — | — | |
| 13 | `chaosLoop` | Chaos Loop | chaos main | int (pow2) | 1/8 … 8 loops | -3 … 3 | 1 (e=0) | yes | — | 1 … 8 | 1/8 … 1 | Visible only when trigger = loop |
| 14 | `chaosBeat` | Chaos Beat | chaos main | int (pow2) | 1/8 … 32 beats | -3 … 5 | 4 (e=2) | yes | — | 2 … 32 | 1/8 … 2 | Visible only when trigger = beat |
| 15 | `chaosShuffle` | Chaos Shuffle | chaos params | boolean | 0/1 | — | 1 | no | — | — | — | |
| 16 | `chaosLength` | Chaos Length | chaos params | boolean | 0/1 | — | 0 | no | — | — | — | |
| 17 | `chaosSwing` | Chaos Swing | chaos params | boolean | 0/1 | — | 0 | no | — | — | — | |
| 18 | `chaosSustain` | Chaos Sustain | chaos params | boolean | 0/1 | — | 0 | no | — | — | — | |
| 19 | `chaosChaos` | Chaos Chaos | chaos params | boolean | 0/1 | — | 0 | no | — | — | — | Chaos changes Chaos Loop / Chaos Beat |
| 20 | `rng` | RNG | main | float | 0 … 1 | — | 0 | yes | — (read at play time) | 0 … 0.22 | 0 … 1.2 | Scales each pattern's random note offset (§3.5) |
| 21 | `chaosRng` | Chaos RNG | chaos params | boolean | 0/1 | — | 0 | no | — | — | — | Chaos changes RNG |

Defaults are proposals — adjust freely.

### 6.1 Change → regeneration rules
- `loop`, `noteLengthMin`, `noteLengthMax`, `shuffle`: regenerate **all** sequences (N changes, so swing/sustain/notes must be resized).
- `legato`, `sustainMin`, `sustainMax`: regenerate **sustain**, rebuild **time table**. `times`, `swing`, `notes` unchanged.
- `swing`: rebuild **time table** only (the swing sequence itself stays).
- Regeneration must not allocate or block on the audio thread: build into a back buffer and swap atomically (or build on the audio thread with fixed-size arrays — sizes are bounded).

---

## 7. Implementation order

Commit and push after every step.

1. **Repo + base classes** — clone layout from BananaChords: `core/domain` (Rng, Sequences, TimeTable, Chaos, ChaosRanges), `core/engine` (Engine), `params.json`, CMake, pnpm workspace, unit tests for sequence generation and time-table lookup.
2. **Default-style UI + standalone web build** — WASM engine + simple UI exposing all params. Demo host: a fake DAW playhead that plays random 3–4-note chords; arpeggiator output drives a simple sine synth (copy the sine generator from IvanDAW).
3. **Designed UI** — apply the design docs (provided later). Rebuild web version.
4. **CI/CD + IvanDAW integration** — per `plugin-dev/05-ci-cd.md` and `06-ivandaw-integration.md`:
   `daw-plugin.yml` (PR artifact `choko-arpegio-daw`; on `master` release `daw-build-<n>` with `choko-arpegio-daw.zip`)
   and `core-tests.yml` (native core tests, until step 5 brings `mac.yml`). ivan-daw then pins the release in
   `plugins/choko-arpegio/package.json` and the arpeggiator is a node in the DAW.
5. **VST** (Windows) and **Mac** builds — JUCE per `plugin-dev/04-vst-host.md`; CI adds `mac.yml` (and the Windows job).

---

## 8. Open questions

None currently.
