# Audio

What a visitor of Greenhill Zoo hears, how it is mixed, what was measured and what is missing.
The game's sound is `src/game/Soundscape.{hpp,cpp}` on cna-engine's `SoundSystem`; the
recordings, their sources and licences are `assets/audio/` (`SOURCES.md`, `sounds.json`,
`mix.json`) and `tools/audio/` (`sources.json`, `licence-evidence.json`, `RESEARCH.md`).

**Nobody has listened to any of it.** The loudspeaker stays muted on this machine: every run
uses SDL's `dummy` driver, or its `disk` driver, which writes the samples the game would have
played to a file. Everything below is measured (loudness, peaks, spectrograms looked at as
pictures, the game's own event log). Nothing here says how anything sounds.

## How it was measured

```sh
python3 tools/audio/capture_runs.py --set NAME              # one real-time game run per scenario
python3 tools/audio/measure_capture.py --set NAME           # figures, spectrograms, SUMMARY.md
python3 tools/audio/mix_model.py                            # level against distance, by the model
cd tools/audio && python3 -m unittest                       # 62 tests: mix model, measuring code
```

`capture_runs.py` starts the built game with `SDL_AUDIODRIVER=disk`
(`SDL_AUDIO_DISK_OUTPUT_FILE`, stereo 16 bit 44.1 kHz) and `ZOO_SOUND_LOG`, the soundscape's
event log (each bed, loop, call, step and a state line every 0.25 s with the effective level,
pan and distance of every looping source). Seed 7, real time (the audio device is paced by the
wall clock), one run at a time, a run's seconds counted from its first sound. Scenarios: a
stand for each of the fifteen species (60 s), the entrance among the 800 simulated visitors
(45 s), the wolves at 06, 12, 18 and 21 h and the entrance at 06, 12 and 21 h, and four walks
in `tools/audio/scripts/`. `--run-dir` runs another binary (the "before" one) beside its own
`assets/`. Captures and pictures stay in the git-ignored `build-probe/audio/<set>/`.

`measure_capture.py` reports per capture: integrated loudness, loudness range and true peak
(ffmpeg `ebur128`), clipped samples, right minus left, silences (under -60 dBFS for 0.3 s),
level steps (a 10 ms window 18 dB over the one before: a start without a fade), the voices,
the calls (by cue, per minute, the same take twice in a row, calls at once, the largest pan),
each isolated call's measured level against what the log says was asked for, and the steps
(interval, how far they stand out of the bed).

`mix_model.py` is the mix in Python; `test_mix_model.py` reads the constants out of
`Soundscape.cpp` and the visitor's stride and speeds out of `Walker.{hpp,cpp}` and fails when
they differ from the model, so the tables here cannot drift from the game unnoticed.

## What the game plays

- **Two beds everywhere.** The day's birdsong (`park_day`) at 0.85 times the sky's daylight,
  so it fades out at dusk and is gone at night, and leaves in the wind (`wind_trees`) at
  0.25 plus the wind. Both come and go at 1.2 per second (0.6 s to half): nothing starts or
  stops in one step. Before, both were started at full level on the first frame and never
  changed with the hour.
- **Loops that follow something.** The nearest water within 13 tiles, the nearest fountain
  within 45 m (new: `fountain`), and the murmur of the two 10 m squares of people heard
  loudest (at least three people in a square; level the square root of people over 30, since
  twice the people are twice the power). Before, the two fullest squares within 48 m played,
  at 0.3 plus people over 20, however far they were.
- **Calls tied to what the animal does.** An animal calls out when it starts `Vocalize`
  (standing): the call of its species that carries farthest. It makes one of its quiet sounds
  (never a roar, howl or trumpet) when it reaches a group mate (`Socialize`, standing), starts
  to `Play`, or, for flamingos only, starts to feed (the recording is of a feeding flock);
  then at most one quiet sound zoo-wide every 4 s, half of such moments, and an animal keeps
  quiet 25 s after it was heard. Before, besides `Vocalize`, a timer every 7 to 18 s picked
  any awake animal within 90 m and played any of its calls, a roar three times in ten.
- **Carry by kind of call** (a habitat is 30 to 60 m across, the zoo 320 m): quiet (growl,
  grunt, chirp, huff, snort) full level within 8 m, heard to 60 m; mid (bray, whinny, bark,
  call, rumble) 10 m and 110 m; loud (roar, howl, trumpet) 14 m and 220 m; half the pressure
  with each doubling of distance (rolloff 1), faded out over the last fifth of the reach.
- **No hard pans.** The engine pans a source abeam wholly into one ear and CNA's pan law then
  silences the other; every source is swung towards the front until it is at most 0.6 to one
  side (8 dB between the ears), at its true distance, and a call stays where its animal stood
  while the head turns.
- **No take twice in a row**, a few percent of variation: pitch within ±0.05 octave (3.5 %)
  and level within 0.7 dB for calls, 1.1 dB for steps.
- **Footsteps**: one on every stride of the body (`Walker.cpp`: 1.5 m, the same stride that
  sways the eyes), about 2.9 a second at the walk (4.4 m/s) and 5.9 at the run (8.8 m/s), by
  the surface under the feet (paved, gravel, grass), the takes of one surface levelled to its
  quietest (`mix.json`), a walking step 3.1 dB under a running one. Before, the steps' category
  was 0.45, under the birdsong bed, and no capture found them.
- **The mix** (`assets/audio/mix.json`, written by `tools/audio/audit_audio.py`): every call
  take is attenuated to its call's target loudness (`tools/audio/GAINS.md`) raised by 9 LU, so
  that a roar at its reference distance stands about 14 LU over the beds. The engine clamps
  every level to 1: the mix can only lower a take.

Level against distance, dB of the take's own level (`-` = not heard; "before" is the mix of `f8d0adf`):

| Source | 5 m | 10 m | 20 m | 30 m | 45 m | 60 m | 80 m | 110 m | 150 m | 200 m |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| quiet call (growl, grunt, chirp), before | -3.4 | -8.5 | -13.6 | -16.6 | - | - | - | - | - | - |
| quiet call, now | 0.0 | -1.9 | -8.0 | -11.5 | -15.0 | - | - | - | - | - |
| mid call (bray), before | 0.0 | -3.4 | -8.5 | -11.5 | -14.5 | -16.6 | -23.9 | - | - | - |
| mid call (rumble, call), before | -0.1 | -5.3 | -10.4 | -13.4 | -16.4 | -21.4 | - | - | - | - |
| mid call, now | 0.0 | 0.0 | -6.0 | -9.5 | -13.1 | -15.6 | -18.1 | - | - | - |
| loud call (roar), before | 0.0 | 0.0 | -3.8 | -6.8 | -9.8 | -11.9 | -14.1 | -16.4 | -23.3 | - |
| loud call, now | 0.0 | 0.0 | -3.1 | -6.6 | -10.1 | -12.6 | -15.1 | -17.9 | -20.6 | -29.9 |
| pond or pool water | -6.0 | -12.0 | -18.1 | - | - | - | - | - | - | - |
| fountain | -1.9 | -8.0 | -14.0 | -17.5 | - | - | - | - | - | - |
| crowd of 30 in a square | 0.0 | -5.4 | -10.8 | -14.0 | -18.0 | - | - | - | - | - |

The murmur against the number of people:

| People in the square | 2 | 3 | 6 | 12 | 17 | 30 | 60 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Level, dB of the recording | - | -10.0 | -7.0 | -4.0 | -2.5 | 0.0 | 0.0 |

### Captured output

The final disk-driver set is `build-probe/audio/merged-smoke/`; every row's spectrogram is the
named PNG in that directory and was generated from the same raw stereo stream as its figures.
All 27 were visually inspected in three contact sheets: the daytime beds remain continuous,
animal calls appear as discrete events, the 18 h wolf chorus is strong but not cut off, and the
21 h captures are visibly sparser. This is a visual check only, not a listening test.

| Capture | s | LUFS | True peak | Clipped | Voices max | Calls | Same take twice | Max pan | Spectrogram |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `approach-fountain` | 39 | -29.2 | -8.1 dBFS | 0 | 8 | 2 | 0 | 0.60 | `approach-fountain.png` |
| `entrance` | 55 | -30.7 | -11.7 dBFS | 0 | 6 | 4 | 0 | 0.57 | `entrance.png` |
| `habitat-brown_bear` | 71 | -29.4 | -11.6 dBFS | 0 | 6 | 2 | 0 | 0.60 | `habitat-brown_bear.png` |
| `habitat-camel` | 66 | -30.8 | -11.7 dBFS | 0 | 5 | 0 | 0 | 0.60 | `habitat-camel.png` |
| `habitat-elephant` | 67 | -29.2 | -6.4 dBFS | 0 | 6 | 3 | 0 | 0.60 | `habitat-elephant.png` |
| `habitat-flamingo` | 69 | -26.2 | -6.7 dBFS | 0 | 6 | 3 | 0 | 0.60 | `habitat-flamingo.png` |
| `habitat-giraffe` | 67 | -29.7 | -11.1 dBFS | 0 | 6 | 4 | 0 | 0.60 | `habitat-giraffe.png` |
| `habitat-kangaroo` | 66 | -29.8 | -11.5 dBFS | 0 | 6 | 7 | 0 | 0.60 | `habitat-kangaroo.png` |
| `habitat-lion` | 68 | -31.1 | -12.3 dBFS | 0 | 5 | 0 | 0 | 0.60 | `habitat-lion.png` |
| `habitat-meerkat` | 70 | -28.5 | -9.1 dBFS | 0 | 7 | 5 | 0 | 0.58 | `habitat-meerkat.png` |
| `habitat-ostrich` | 68 | -27.1 | -10.2 dBFS | 0 | 6 | 5 | 0 | 0.60 | `habitat-ostrich.png` |
| `habitat-penguin` | 66 | -29.7 | -11.7 dBFS | 0 | 6 | 3 | 0 | 0.60 | `habitat-penguin.png` |
| `habitat-polar_bear` | 66 | -31.2 | -11.9 dBFS | 0 | 5 | 0 | 0 | 0.60 | `habitat-polar_bear.png` |
| `habitat-rhino` | 68 | -29.8 | -11.2 dBFS | 0 | 6 | 3 | 0 | 0.60 | `habitat-rhino.png` |
| `habitat-tiger` | 66 | -29.8 | -11.7 dBFS | 0 | 5 | 0 | 0 | 0.60 | `habitat-tiger.png` |
| `habitat-wolf` | 68 | -31.3 | -12.0 dBFS | 0 | 5 | 0 | 0 | 0.60 | `habitat-wolf.png` |
| `habitat-zebra` | 66 | -30.2 | -10.7 dBFS | 0 | 6 | 6 | 0 | 0.60 | `habitat-zebra.png` |
| `hour-06-entrance` | 54 | -30.9 | -11.9 dBFS | 0 | 4 | 4 | 0 | 0.51 | `hour-06-entrance.png` |
| `hour-06-wolf` | 67 | -31.3 | -12.2 dBFS | 0 | 3 | 0 | 0 | 0.10 | `hour-06-wolf.png` |
| `hour-12-entrance` | 57 | -30.9 | -11.5 dBFS | 0 | 6 | 5 | 0 | 0.51 | `hour-12-entrance.png` |
| `hour-12-wolf` | 68 | -31.3 | -12.1 dBFS | 0 | 5 | 0 | 0 | 0.57 | `hour-12-wolf.png` |
| `hour-18-wolf` | 56 | -12.1 | -1.2 dBFS | 0 | 6 | 11 | 0 | 0.58 | `hour-18-wolf.png` |
| `hour-21-entrance` | 56 | -35.6 | -19.2 dBFS | 0 | 2 | 0 | 0 | 0.00 | `hour-21-entrance.png` |
| `hour-21-wolf` | 70 | -38.4 | -20.1 dBFS | 0 | 2 | 0 | 0 | 0.16 | `hour-21-wolf.png` |
| `run-east` | 36 | -30.4 | -11.6 dBFS | 0 | 7 | 3 | 0 | 0.56 | `run-east.png` |
| `turn-penguin` | 52 | -30.4 | -11.7 dBFS | 0 | 6 | 3 | 0 | 0.60 | `turn-penguin.png` |
| `walk-east` | 56 | -30.5 | -8.8 dBFS | 0 | 7 | 3 | 0 | 0.56 | `walk-east.png` |

Across the older 25 captures, 81 calls in 1,036 s included nine immediate take repeats and pans
up to 1.0 (one ear); the current 27 have 76 calls in 1,678 s, no immediate repeat and pan at most
0.60. Neither set clipped. Ordinary daytime stands are -26.2 to -31.3 LUFS; after birdsong fades,
the 21 h captures are -35.6 and -38.4 LUFS. The intended close wolf chorus at 18 h is the one
outlier: -12.1 LUFS, true peak -1.2 dBFS without clipping, followed by the lions about 105 m away.
The level-step detector found three 10 ms signal transients: two in the initial bear capture and
one on a footstep, not when a looping bed or follower was switched on. Calls in the dusk capture,
including its three simultaneous voices, did not trip it.

## The species: present, absent, substitute

What each species declares (`data/species/*.json` `sounds`) against what the library holds.
"Heard when" is the game's cue (see above).

| Species | In the library (takes) | Declared, no recording | Substitute or caveat | Heard when |
|---|---|---|---|---|
| Lion | `lion_roar` (2), `lion_growl` (2) | — | `lion_growl` is the low grunts ending a roaring bout (same species) | roar: the pride leader's chorus 17-20 h and the answers; growl: meeting, play |
| Tiger | `tiger_growl` (3) | `tiger_chuff` | — | `Vocalize`, meeting, play |
| Wolf | `wolf_howl` (3) | `wolf_growl` | — | the pack's chorus 17-20 h |
| Brown bear | `brown_bear_growl` (1), `brown_bear_huff` (2) | — | the growl is a short low event of an NPS grizzly recording; that it is a growl is uncertain (`SOURCES.md`) | `Vocalize`, meeting, play |
| Polar bear | — | `polar_bear_growl`, `polar_bear_chuff` | none found under an allowed licence with field provenance (`RESEARCH.md`) | silent |
| Elephant | `elephant_rumble` (3), `elephant_trumpet` (1) | — | **the trumpet is an Asian elephant's** (labelled species substitute) | trumpet: `Vocalize`; rumble: meeting, play |
| Rhino | `rhino_snort` (2), `rhino_grunt` (1) | — | white rhino fight calls; subtype provisional | `Vocalize`, meeting, play |
| Giraffe | `giraffe_grunt` (1) | — | giraffes are mostly silent; the nocturnal hum is not in the library | `Vocalize`, meeting, play |
| Zebra | `zebra_bark` (1), `zebra_whinny` (1) | — | the "whinny" is a plains-zebra bray (same species, call type substituted) | `Vocalize`, meeting, play |
| Camel | `camel_grunt` (2) | — | Bactrian camel, the zoo's species; CC BY-SA 3.0 | `Vocalize`, meeting, play |
| Kangaroo | — | `kangaroo_grunt` | none found (`RESEARCH.md`); kangaroos are mostly silent | silent |
| Ostrich | — | `ostrich_hiss`, `ostrich_grunt` | booms found only under NC/ND licences (`RESEARCH.md`) | silent |
| Meerkat | `meerkat_chirp` (3) | `meerkat_growl` | the chirps are alarm calls; CC BY-SA 3.0 | `Vocalize`, meeting, play |
| Flamingo | `flamingo_call` (3) | — | a feeding flock calling | `Vocalize`, meeting, play, **starting to feed** (new) |
| Penguin | `penguin_bray` (3) | — | take 2 is a contact call, not a bray (labelled) | `Vocalize`, meeting, play |

Eight declared names have no recording; the ninth originally missing name, `elephant_trumpet`,
is now a clearly labelled Asian-elephant substitute. Two earlier rounds searched Wikimedia Commons,
Freesound, xeno-canto, GBIF, iNaturalist, Zenodo/Dryad, the Internet Archive and US federal
archives for them (`tools/audio/RESEARCH.md`, "Names intentionally not delivered" and Round 2);
this round did not search again. Polar bears, kangaroos, ostriches and giraffes are mostly
silent in a real zoo by day, so their silence here is closer to the truth than an invented voice
would be. Non-vocal sounds (splashes at the penguin pool, hooves, wing flaps, ear flaps, chewing)
and the keepers' work (a bucket, a rake on gravel, a hose) have no recording: none was added,
because no identified source was prepared and the capture could not yet show where they would
be heard.

## Recordings changed or added in this audit

All three are rebuilt by `tools/audio/build_audio.py` from the cached source with the cut and
filters recorded in `tools/audio/sources.json`; a rebuild of the three groups
(`--only park_day,fountain,brown_bear_growl`) gave decoded PCM identical to the files here
for all 54 files (`check_rebuild.py`: 54 identical, 0 changed). Spectrograms (built from the
files with ffmpeg `showspectrumpic`, looked at): `build-probe/g7c-pics/*-before.png`,
`*-after.png`, and the evidence sheet `docs/evidence/audio-g7-2026-11-19.jpg`.

| File | What changed | Source, author, licence |
|---|---|---|
| `ambience/fountain.ogg` (new) | A town fountain's falling water, steady part 24-44 s of the source, mono, high-pass 100 Hz, loudness -23 LUFS, a 3 s cyclic crossfade, 17 s loop. Spectrogram: steady broadband noise from about 100 Hz to the source's 12.5 kHz MP3 limit, no seam to be seen. Played at the zoo's two fountains, full level within 4 m, heard to 45 m. | [Fountain in toulouse.ogg](https://commons.wikimedia.org/wiki/File:Fountain_in_toulouse.ogg) (Place de la Trinité, Toulouse), aldor, PDSounds.org record 579, transferred to Commons by Fæ; **public domain** (PD-author); SHA-256 `480b9da2...` in `licence-evidence.json` |
| `ambience/park_day.ogg` | Same cut of the same birdsong; the low-frequency rumble below 120 Hz, 47 % of the loop's energy (measured), now cut by a fourth-order high-pass at 120 Hz (two second-order `highpass=f=120` stages: FFmpeg's has two poles at most), 7 % left. That rumble lay where the lion's roar and the elephant's rumble have their energy. Re-levelled to -23 LUFS, which raised the rest 1 to 2 dB; the bed's level in the game went from 1.0 to 0.85 to keep the birds where they were. Spectrogram: the band under 200 Hz drops by some 10-15 dB, the bird syllables at 2-10 kHz and the pulsed ~450 Hz series at 25-33 s are unchanged. | unchanged: richwise, Freesound 516524, CC0 |
| `animals/brown_bear_growl.ogg` | Cut 4.6-5.9 s instead of 12.1-13.8 s of the same NPS recording: its longest low event (0.3 s, strongest at 241 Hz, 62 % of its energy at 120-500 Hz). The old cut was the low noise floor with two short broadband clicks (spectrogram); the new one has one voiced event at 0.52 s (200-450 Hz) and a weaker one at 0.73 s. Integrated loudness -30.9 → -27.3 LUFS at the same -3 dBFS peak. That it is a growl remains uncertain without listening (`SOURCES.md` says so). | unchanged: NPS / Dan Bergum, Yellowstone sound library, public domain |

The bear is still the quietest voice of the zoo, by measurement: the loudest 100 ms of the
growl and huffs, after their mix gains, are -21.8, -21.0 and -25.7 dBFS, against -13.7 to
-17.2 for the lion's, tiger's, rhino's and giraffe's growls and grunts. The mix cannot raise a
take (the engine clamps levels to 1), and a limiter does not help: limiting the growl's peak by
5 or 8 dB lowered its mean by 3.6 or 5.5 dB as well (`build-probe/g7c-bear/`), because the
event itself is short and dense, not a click on a quiet body. Without a longer bear call from
an allowed source the bear stays quieter; a real brown bear in a zoo is mostly silent, and its
huffs and jaw-pops are close-range sounds.

## What remains limited

Nobody has listened to the result, so the measurements can reject clipping, hard pans, abrupt
starts, repetition and implausible balance but cannot establish perceptual quality. The disk
driver also makes the off-screen game run slower than its simulated walking speed; step timing is
therefore checked against the walker's stride model as well as reported from the wall-clock
captures.

Eight declared vocalization names still have no licensed recording, and no identified source has
yet been prepared for splashes, hooves, wings, chewing or keepers' tools. The brown-bear cut is a
measured low event from the correct NPS grizzly source, but its publisher does not identify the
event's exact position; it remains explicitly uncertain rather than being presented as a proven
growl. Some one-minute habitat captures contain no call because animals now sound only when their
behaviour supplies a cue. That silence is intentional, not filled with a timer.
