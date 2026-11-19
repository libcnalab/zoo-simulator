# Audio audit — Round 2

These are measurements, and nobody has listened to the files. They do not certify call identity, absence of speech, perceived background quality or an appropriate final mix.

Decoded float PCM at the native 44100 Hz is measured without gain changes. True peak, integrated loudness and loudness range come from FFmpeg `ebur128=peak=true` (true peak is referenced to full scale; commonly written dBTP). LRA is often zero for short clips and is not a useful dynamic-range claim there. Clips shorter than 400 ms are zero-padded to 400 ms for loudness only and marked †; this is an explicitly padded measurement, not native-duration integrated loudness.

Noise floor is 10 log10 of the arithmetic mean power of the quietest ceil(10%) of non-overlapping 50 ms windows, including the final partial window. Each window averages all its samples/channels; the selected windows have equal weight. This is a quiet-window proxy that includes call energy and fades, not an isolated noise estimate. A gap at most 25 dB flags review; it does not prove hiss. Frequency shares use the whole-file rectangular real FFT, including DC, correct one-sided power weights, and summed channel energies. Cutoffs are strictly below 120 Hz and above 6 kHz. Head/tail silence uses consecutive 1 ms RMS windows below −50 dBFS (44 frames, approximately 0.998 ms). Loop seam delta compares the centered 100 ms boundary RMS with the two adjacent 100 ms windows and the loop interior, reporting the largest absolute difference.

| Ogg | Duration s | True peak dBFS | LUFS | LRA LU | Quiet floor dBFS | <120 Hz % | >6 kHz % | Head ms | Tail ms | Seam Δ dB |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `ambience/crowd.ogg` | 25.000000 | -2.4 | -22.8 | 5.5 | -28.13 | 5.61 | 0.47 | 0.00 | 0.00 | 1.484 |
| `ambience/fountain.ogg` | 17.000000 | -2.9 | -23.0 | 1.0 | -28.28 | 0.30 | 21.07 | 0.00 | 7.62 | 1.295 |
| `ambience/park_day.ogg` | 36.000000 | -3.6 | -23.3 | 7.1 | -33.10 | 7.14 | 4.05 | 0.00 | 0.00 | 1.774 |
| `ambience/water.ogg` | 15.000000 | -6.5 | -23.3 | 1.4 | -29.73 | 0.32 | 5.54 | 0.00 | 0.00 | 2.591 |
| `ambience/wind_trees.ogg` | 20.000000 | -5.1 | -22.7 | 4.9 | -34.03 | 0.00 | 57.82 | 0.00 | 0.00 | 0.879 |
| `animals/brown_bear_growl.ogg` | 1.300000 | -2.9 | -27.3 | 0.0 | -50.87 | 0.29 | 0.11 | 10.98 | 37.87 | — |
| `animals/brown_bear_huff.ogg` | 1.600000 | -2.8 | -23.8 | 0.0 | -49.03 | 0.18 | 0.39 | 9.98 | 7.62 | — |
| `animals/brown_bear_huff_2.ogg` | 1.600000 | -3.1 | -30.1 | 0.0 | -55.00 | 0.08 | 0.15 | 27.94 | 47.53 | — |
| `animals/camel_grunt.ogg` | 2.560000 | -2.9 | -10.3 | 0.0 | -22.40 | 0.08 | 0.07 | 4.99 | 47.71 | — |
| `animals/camel_grunt_2.ogg` | 2.650000 | -2.9 | -11.0 | 0.0 | -32.58 | 0.01 | 0.18 | 7.98 | 48.91 | — |
| `animals/elephant_rumble.ogg` | 4.119705 | -3.0 | -24.7 | 0.9 | -24.32 | 97.63 | 0.00 | 19.95 | 31.00 | — |
| `animals/elephant_rumble_2.ogg` | 4.260000 | -2.9 | -21.8 | 2.1 | -27.94 | 89.46 | 0.00 | 7.98 | 70.52 | — |
| `animals/elephant_rumble_3.ogg` | 2.320000 | -3.0 | -18.3 | 0.0 | -33.78 | 10.89 | 0.06 | 2.00 | 23.22 | — |
| `animals/elephant_trumpet.ogg` | 1.270000 | -3.0 | -16.1 | 0.0 | -38.41 | 0.17 | 1.91 | 12.97 | 4.88 | — |
| `animals/flamingo_call.ogg` | 3.500000 | -3.0 | -18.8 | 0.8 | -29.61 | 0.10 | 0.05 | 1.00 | 0.00 | — |
| `animals/flamingo_call_2.ogg` | 3.600000 | -2.8 | -17.6 | 0.7 | -24.94 | 0.15 | 0.08 | 1.00 | 1.18 | — |
| `animals/flamingo_call_3.ogg` | 3.500000 | -2.9 | -18.5 | 0.5 | -28.60 | 0.08 | 0.06 | 1.00 | 1.95 | — |
| `animals/giraffe_grunt.ogg` | 1.400000 | -3.1 | -20.8 | 0.0 | -27.63 | 58.35 | 0.03 | 0.00 | 6.17 | — |
| `animals/lion_growl.ogg` | 3.000000 | -3.1 | -17.3 | 20.0 | -32.93 | 45.91 | 0.02 | 2.00 | 0.82 | — |
| `animals/lion_growl_2.ogg` | 3.400000 | -2.9 | -22.6 | 0.0 | -32.81 | 59.06 | 0.08 | 0.00 | 7.71 | — |
| `animals/lion_roar.ogg` | 2.200000 | -2.9 | -11.7 | 0.0 | -37.07 | 0.78 | 0.34 | 4.99 | 4.99 | — |
| `animals/lion_roar_2.ogg` | 2.100000 | -3.1 | -11.2 | 0.0 | -38.01 | 0.88 | 0.16 | 1.00 | 3.76 | — |
| `animals/meerkat_chirp.ogg` | 2.300000 | -3.0 | -23.5 | 0.0 | -39.64 | 0.01 | 0.43 | 1.00 | 8.21 | — |
| `animals/meerkat_chirp_2.ogg` | 2.310000 | -3.0 | -22.8 | 0.0 | -41.16 | 0.00 | 0.19 | 3.99 | 9.23 | — |
| `animals/meerkat_chirp_3.ogg` | 2.300000 | -2.9 | -23.4 | 0.0 | -42.91 | 0.01 | 0.16 | 4.99 | 10.20 | — |
| `animals/penguin_bray.ogg` | 5.850000 | -3.1 | -15.6 | 3.5 | -38.67 | 0.36 | 0.08 | 1.00 | 16.26 | — |
| `animals/penguin_bray_2.ogg` | 0.620748 | -3.0 | -11.5 | 0.0 | -37.97 | 0.09 | 0.00 | 22.95 | 18.12 | — |
| `animals/penguin_bray_3.ogg` | 6.000000 | -3.0 | -15.2 | 1.2 | -41.20 | 0.11 | 0.02 | 5.99 | 21.59 | — |
| `animals/rhino_grunt.ogg` | 3.800000 | -3.1 | -20.3 | 2.4 | -40.48 | 0.17 | 0.01 | 2.00 | 11.61 | — |
| `animals/rhino_snort.ogg` | 1.800000 | -3.1 | -19.0 | 0.0 | -58.67 | 0.01 | 0.00 | 3.99 | 10.07 | — |
| `animals/rhino_snort_2.ogg` | 1.900000 | -3.1 | -18.4 | 0.0 | -42.44 | 0.01 | 0.01 | 2.00 | 22.27 | — |
| `animals/tiger_growl.ogg` | 2.800000 | -2.9 | -17.8 | 0.0 | -32.36 | 0.08 | 1.00 | 3.99 | 8.34 | — |
| `animals/tiger_growl_2.ogg` | 2.900000 | -2.8 | -20.1 | 0.0 | -34.91 | 0.44 | 1.63 | 2.00 | 3.58 | — |
| `animals/tiger_growl_3.ogg` | 3.600000 | -2.9 | -18.2 | 0.9 | -32.06 | 0.21 | 0.40 | 2.00 | 8.16 | — |
| `animals/wolf_howl.ogg` | 8.400000 | -3.0 | -16.2 | 1.7 | -20.87 | 0.00 | 0.27 | 1.00 | 2.09 | — |
| `animals/wolf_howl_2.ogg` | 9.800000 | -3.1 | -14.0 | 1.7 | -18.18 | 0.00 | 0.03 | 1.00 | 1.27 | — |
| `animals/wolf_howl_3.ogg` | 9.700000 | -3.0 | -13.6 | 3.4 | -20.78 | 0.00 | 0.03 | 0.00 | 1.04 | — |
| `animals/zebra_bark.ogg` | 1.590000 | -3.0 | -21.2 | 0.0 | -31.89 | 0.41 | 0.00 | 2.99 | 1.61 | — |
| `animals/zebra_whinny.ogg` | 1.590000 | -3.1 | -19.9 | 0.0 | -33.38 | 0.54 | 0.00 | 2.00 | 12.59 | — |
| `footsteps/grass_1.ogg` | 0.397098 | -1.9 | -27.6† | 0.0 | -44.42 | 4.30 | 43.41 | 6.98 | 13.97 | — |
| `footsteps/grass_2.ogg` | 0.397098 | -2.7 | -24.7† | 0.0 | -42.08 | 2.57 | 38.75 | 3.99 | 11.97 | — |
| `footsteps/grass_3.ogg` | 0.397098 | -2.3 | -21.3† | 0.0 | -35.19 | 5.46 | 26.60 | 1.00 | 2.00 | — |
| `footsteps/grass_4.ogg` | 0.397098 | -2.4 | -25.2† | 0.0 | -40.73 | 8.97 | 25.07 | 2.99 | 7.98 | — |
| `footsteps/grass_5.ogg` | 0.397098 | -2.9 | -20.6† | 0.0 | -35.98 | 5.37 | 34.08 | 2.99 | 2.99 | — |
| `footsteps/gravel_1.ogg` | 0.397098 | -3.0 | -23.1† | 0.0 | -47.31 | 16.80 | 2.16 | 5.99 | 20.95 | — |
| `footsteps/gravel_2.ogg` | 0.397098 | -3.1 | -24.8† | 0.0 | -52.88 | 14.65 | 7.90 | 43.90 | 38.91 | — |
| `footsteps/gravel_3.ogg` | 0.397098 | -3.0 | -23.7† | 0.0 | -54.12 | 12.76 | 1.68 | 41.90 | 21.95 | — |
| `footsteps/gravel_4.ogg` | 0.397098 | -2.9 | -23.2† | 0.0 | -58.56 | 21.70 | 0.82 | 46.89 | 9.98 | — |
| `footsteps/gravel_5.ogg` | 0.397098 | -3.0 | -23.6† | 0.0 | -51.36 | 30.05 | 1.13 | 45.90 | 2.00 | — |
| `footsteps/paved_1.ogg` | 0.397098 | -3.1 | -27.1† | 0.0 | -36.80 | 12.20 | 2.02 | 0.00 | 2.99 | — |
| `footsteps/paved_2.ogg` | 0.397098 | -3.1 | -29.0† | 0.0 | -41.20 | 8.04 | 0.36 | 2.99 | 7.98 | — |
| `footsteps/paved_3.ogg` | 0.422313 | -3.1 | -26.8 | 0.0 | -72.79 | 10.48 | 1.58 | 2.00 | 25.22 | — |
| `footsteps/paved_4.ogg` | 0.397098 | -3.1 | -26.6† | 0.0 | -33.61 | 16.69 | 0.66 | 1.00 | 2.00 | — |
| `footsteps/paved_5.ogg` | 0.397098 | -2.9 | -28.6† | 0.0 | -40.28 | 7.44 | 0.70 | 2.00 | 5.99 | — |

## Head/tail silence over 80 ms

None after the source-cut corrections recorded below.

## Quiet floor within 25 dB of true peak

| File | Peak − quiet floor dB |
|---|---:|
| `ambience/water.ogg` | 23.23 |
| `animals/camel_grunt.ogg` | 19.50 |
| `animals/elephant_rumble.ogg` | 21.32 |
| `animals/flamingo_call_2.ogg` | 22.14 |
| `animals/giraffe_grunt.ogg` | 24.53 |
| `animals/wolf_howl.ogg` | 17.87 |
| `animals/wolf_howl_2.ogg` | 15.08 |
| `animals/wolf_howl_3.ogg` | 17.78 |

## Species loudness spread

Range across every delivered take and call name of a species; flags are strictly over 4 LU. Peak normalization does not equalize this measure. See [GAINS.md](GAINS.md) for per-name proposals.

| Species | Minimum LUFS | Maximum LUFS | Spread LU | Over 4 LU |
|---|---:|---:|---:|---|
| brown_bear | -30.1 | -23.8 | 6.3 | yes |
| camel | -11.0 | -10.3 | 0.7 | no |
| elephant | -24.7 | -16.1 | 8.6 | yes |
| flamingo | -18.8 | -17.6 | 1.2 | no |
| giraffe | -20.8 | -20.8 | 0.0 | no |
| lion | -22.6 | -11.2 | 11.4 | yes |
| meerkat | -23.5 | -22.8 | 0.7 | no |
| penguin | -15.6 | -11.5 | 4.1 | yes |
| rhino | -20.3 | -18.4 | 1.9 | no |
| tiger | -20.1 | -17.8 | 2.3 | no |
| wolf | -16.2 | -13.6 | 2.6 | no |
| zebra | -21.2 | -19.9 | 1.3 | no |

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

54 files measured; 0 edge-silence flags; 8 quiet-floor review flags.
