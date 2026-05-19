# Music Composer

A step-based melody editor for any UniGeek board with a speaker. Songs are stored on disk as standard Nokia [RTTTL](https://en.wikipedia.org/wiki/Ring_Tone_Text_Transfer_Language) ringtone strings, so what you write here is portable to any RTTTL-compatible buzzer, synth, or web player.

> [!note]
> Hidden on boards without sound output (no buzzer and no I2S). The Games menu only shows this entry when `DEVICE_HAS_SOUND` is true for your board.

## Top Menu

When you open Music Composer you get a list with three entries:

| Item | What it does |
|------|--------------|
| **New** | Start an empty song at 120 BPM. |
| **Open File** | Pick a saved `.rtttl` from `/unigeek/music/` via the file browser. |
| **Built-in Demos** | Load one of the 7 bundled public-domain melodies (see below). |

## Built-in Demos

| Slug | Source |
|------|--------|
| Twinkle | Twinkle, Twinkle, Little Star |
| OdeToJoy | Beethoven — Ode to Joy |
| FrereJacques | Frère Jacques |
| JingleBells | Jingle Bells |
| MaryLamb | Mary Had a Little Lamb |
| AlsAct | Always (lullaby) |
| Beep | Short ascending-arpeggio test tone |

Loading a demo drops you straight into the editor with the song parsed in — edit it, then **Save** to write a copy under `/unigeek/music/`. The originals are read-only (compiled into firmware).

## Song Model

A song is a sequence of up to **64 steps**. Each step is one of:

- A **note** — a MIDI pitch in the range **C2 (36) … C7 (96)** plus a length code.
- A **rest** — silence with a length code (`midi = 0`).

The length code is the RTTTL denominator — `1`, `2`, `4`, `8`, `16`, or `32` — and divides the whole-note duration set by the song's BPM. The whole song shares one BPM in the range **40 … 300**.

## Editor

The editor is mode-aware so it works on every input shape UniGeek supports:

### Pitch and length

- **UP / DOWN** — shift the current step up or down by one semitone. Every shift auditions the new pitch via a short non-blocking `tone()` so you hear what you picked.
- Pitch wraps at C2 / C7. Past the top, the next press toggles the step to a rest; from a rest, UP picks the previous pitch.

### Cursor (varies by board)

- **4-way boards** (Cardputer, T-Lora Pager, etc.) — **LEFT / RIGHT** moves the cursor between steps directly.
- **2/3-button boards** (M5StickC, T-Display, etc.) — the cursor never moves under the d-pad; instead use the **Move step…** entry in the action menu for a number picker, plus **Prev step / Next step** for single-step navigation.

The detection is runtime via `Uni.Nav->is4Way()`, so behaviour follows the active navigation mode (e.g. M5StickC with the encoder HAT is treated as 4-way).

### Action menu (PRESS)

PRESS in the editor opens the action list:

| Action | Effect |
|--------|--------|
| **Play** | Non-blocking playback from step 1; cancels on BACK or PRESS. |
| **Octave +1 / −1** | Shift the current step by 12 semitones, clamped to C2…C7. |
| **Toggle rest** | Swap the current step between a rest and the last pitch it held. |
| **Length** | Cycle the length code `1 → 2 → 4 → 8 → 16 → 32 → 1`. |
| **Insert step** | Insert a copy of the current step after the cursor (if `stepCount < 64`). |
| **Delete step** | Remove the current step; cursor stays at the same index. |
| **BPM…** | Number picker, 40 … 300. |
| **Rename…** | Edit the song name (drives the RTTTL header and the filename if you save). |
| **Save** | Write `<name>.rtttl` to `/unigeek/music/`. First save uses the song name; subsequent saves overwrite the same file. |
| **Clear** | Wipe all steps and start fresh; confirmation required. |
| **Move step…** | (3-button only) Jump to any step by index. |

> [!tip]
> The editor sets `dirty = true` on every action-menu entry, so even popups that paint over the whole screen always restore cleanly when you return.

## Playback

`Play` walks the step list from index 0. For each step it computes the duration as:

```
durationMs = (60000 / bpm) * (4 / lenCode)
```

…and calls `Uni.Speaker->tone(freq, durationMs)`. Rests call `tone(0, durationMs)` to insert silence. Playback drives entirely from `onUpdate()` so the UI stays responsive — the highlight bar follows the playing step.

Playback works on every speaker backend:

- **LEDC buzzer** (M5StickC Plus 1.1) — single-channel square waves.
- **I2S DAC** (Cardputer, T-Lora Pager, T-Embed CC1101, etc.) — sine-LUT mono tone.
- **ES8311 codec** (Cardputer ADV, CoreS3, StickC S3) — same trapezoidal LUT.

No WAV files needed; everything is generated from RTTTL on the fly.

## Storage

- Working directory: `/unigeek/music/` on SD if available, otherwise LittleFS.
- File format: standard RTTTL — `name:d=<defLen>,o=<defOctave>,b=<bpm>:<note>,<note>,…`
- Files are pure ASCII; you can edit them on a desktop or paste from any RTTTL collection on the web, then drop them onto the SD via the Web File Manager.

> [!warn]
> The serializer always writes octave `5` as the default and respects `MIDI_MIN` (C2) as the floor. Imports outside the C2…C7 range are clamped on load — extreme bass / treble notes from third-party ringtones may shift octave during round-trip.

## Tips

- For chord-feeling melodies on a mono buzzer, use short rests between notes to give your ear an attack envelope.
- BPM affects every step proportionally; tune your tempo last so length codes stay readable.
- Save early so the action menu's BPM picker and Length cycle have a place to write — there's no autosave, and BACK from the editor returns to the top menu without prompting.
