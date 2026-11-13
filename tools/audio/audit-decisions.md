## Source-cut corrections and background review

The initial 52-file audit found two files with margins over 80 ms. Only these source cuts
changed during Part 3; the builder reapplied its existing fades and decoded-peak normalization.

| File | Previous source cut s | Final source cut s | Initial head / tail ms | Final head / tail ms |
|---|---|---|---|---|
| `animals/elephant_rumble.ogg` | 0.95–5.55 | 1.1475–5.2672 | 217.51 / 302.77 | 19.95 / 31.00 |
| `animals/penguin_bray_2.ogg` | 0.25–1.35 | 0.4804–1.1011 | 250.43 / 268.89 | 22.95 / 18.12 |

The penguin contact call is naturally short: the verifier permits 0.5 s specifically for
this documented variant, while preserving the other existing duration bounds. It is not
padded back to one second. Removing the elephant's silent margins makes its quiet-floor
proxy rise: those windows now contain the sustained rumble instead of digital silence.
This is a measurement consequence, not added noise.

The complete camel, wolf, flamingo and water source waveforms/spectrograms were inspected
again (`round2-audit-*-inspection.png` in the cache); the complete giraffe and elephant
sources were inspected in Part 2. Camel and wolf show sustained tonal/harmonic calls with
little quiet space. The flamingo source contains continuous flock calls and background
throughout, including its late passage. Giraffe has only one short grunt with low-frequency
background; trimming to the core does not separate that background. Water is deliberately
continuous noise. These observations cannot certify perceptual cleanliness.

`python3 tools/audio/audit_passages.py` actually prepared and measured these alternatives.
The comparison below uses sample-peak minus quiet-floor on the prepared float WAV, so it is
gain-independent but is not exactly the decoded true-peak metric in the main table. The
same existing filters/fades were used; no candidate replaced a delivered file.

| File | Alternative source cut s | Current gap dB | Alternative gap dB | Decision |
|---|---|---:|---:|---|
| `animals/elephant_rumble.ogg` | 1.4–5.1 | 21.33 | 19.27 | Shorter sustained rumble; removes natural onset/decay. |
| `animals/camel_grunt.ogg` | 0.1–2.35 | 18.67 | 9.53 | Shorter first call; loses its natural ending. |
| `animals/flamingo_call_2.ogg` | 32.0–35.6 | 22.19 | 24.25 | Unused late flock passage; same continuous background. |
| `animals/giraffe_grunt.ogg` | 0.65–1.25 | 24.58 | 24.72 | Single short grunt core; discards natural onset/decay. |
| `animals/wolf_howl.ogg` | 1.5–7.5 | 18.00 | 16.87 | Shorter early chorus; overlapping tonal calls remain continuous. |
| `animals/wolf_howl_2.ogg` | 10.0–17.0 | 15.46 | 17.63 | Shorter middle chorus; no isolated quiet background. |
| `animals/wolf_howl_3.ogg` | 20.0–27.0 | 17.69 | 17.72 | Shorter late chorus; no isolated quiet background. |
| `ambience/water.ogg` | 5.0–23.0 | 22.41 | 19.83 | Later water passage; water noise is the intended loop content. |

None of the alternatives resolves the threshold. The narrower animal cuts lose natural
call structure without demonstrating a cleaner recording; the late flamingo passage still
fails. Better elephant recordings were added as distinct takes, not used to erase the
original call. The required source searches also failed to establish a stronger second
giraffe grunt. Background flags remain plainly listed rather than being hidden with
silence, gating, denoising or an aggressive filter. No new Part 3 frequency filter was applied.
The existing high-pass filters, including the 60 Hz penguin filter, remain recorded in
sources.json. A1's water dynamics processing is preserved, not introduced in this round.

Loudness ranges for very short files can be zero or unstable (including startup artefacts
around three seconds); the table reports FFmpeg's measurements verbatim. Per-name gain
proposals use integrated loudness, not those short-clip LRA values. The 4 LU species flags
and residual within-name variation require mix review; the proposed gains cannot equalize
different takes within a name. Nobody listened to the files.
