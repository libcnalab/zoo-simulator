# Proposed call gains

Measurements only; nobody listened. The files are not changed. The game applies the mix written to `assets/audio/mix.json` (per take: this table's target raised by 9 LU, attenuation only). One gain per call name matches the current manifest grouping. Measured LUFS is the equal-take energy mean of each file’s gated integrated loudness, not a concatenated-program measurement. The range exposes variation that a single per-name gain cannot fix.

Targets: lion roar, elephant trumpet and wolf howl −23 LUFS; growls/grunts/huffs/snorts −31 LUFS (8 LU lower); meerkat chirp and penguin bray −35 LUFS. These are mix choices, not measured animal sound-pressure levels. The elephant trumpet is the labelled Asian-elephant substitute. Positive gain is capped to leave at least 1 dB of measured true-peak headroom.

| Name | Measured mean LUFS | Take range LUFS | Target LUFS | Proposed gain dB | Basis |
|---|---:|---|---:|---:|---|
| `brown_bear_growl` | -27.30 | -27.3 to -27.3 | -31.0 | -3.70 | 8 LU below loud calls |
| `brown_bear_huff` | -25.90 | -30.1 to -23.8 | -31.0 | -5.10 | 8 LU below loud calls |
| `camel_grunt` | -10.64 | -11.0 to -10.3 | -31.0 | -20.36 | 8 LU below loud calls |
| `elephant_rumble` | -20.83 | -24.7 to -18.3 | -27.0 | -6.17 | low rumble, below the loud-call reference |
| `elephant_trumpet` | -16.10 | -16.1 to -16.1 | -23.0 | -6.90 | loud call |
| `flamingo_call` | -18.27 | -18.8 to -17.6 | -29.0 | -10.73 | other call, 6 LU below loud calls |
| `giraffe_grunt` | -20.80 | -20.8 to -20.8 | -31.0 | -10.20 | 8 LU below loud calls |
| `lion_growl` | -19.19 | -22.6 to -17.3 | -31.0 | -11.81 | 8 LU below loud calls |
| `lion_roar` | -11.44 | -11.7 to -11.2 | -23.0 | -11.56 | loud call |
| `meerkat_chirp` | -23.22 | -23.5 to -22.8 | -35.0 | -11.78 | small/social call |
| `penguin_bray` | -13.68 | -15.6 to -11.5 | -35.0 | -21.32 | small/social call |
| `rhino_grunt` | -20.30 | -20.3 to -20.3 | -31.0 | -10.70 | 8 LU below loud calls |
| `rhino_snort` | -18.69 | -19.0 to -18.4 | -31.0 | -12.31 | 8 LU below loud calls |
| `tiger_growl` | -18.59 | -20.1 to -17.8 | -31.0 | -12.41 | 8 LU below loud calls |
| `wolf_howl` | -14.46 | -16.2 to -13.6 | -23.0 | -8.54 | loud call |
| `zebra_bark` | -21.20 | -21.2 to -21.2 | -29.0 | -7.80 | other call, 6 LU below loud calls |
| `zebra_whinny` | -19.90 | -19.9 to -19.9 | -29.0 | -9.10 | other call, 6 LU below loud calls |

A listening review is still owed, particularly for the preserved contact-call penguin variant; until then the mix rests on loudness measurements alone.
