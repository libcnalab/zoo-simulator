# Audio verification report

All files are Vorbis, 44100 Hz. Peaks are decoded sample peaks from FFmpeg `volumedetect`. Loop checks decode two copies with `-stream_loop 1` and use `astats` RMS for the centered 100 ms joint, its adjacent 100 ms windows, and the rest of the loop. The reported delta is the largest of those three comparisons. Every delta must be at most 3 dB. Every file also has a 100 ms `astats` RMS envelope in `build-probe/audio-src/envelopes.json`. Measurements cannot establish absence of intelligible speech or confirm call type; see `RESEARCH.md` for listening-review uncertainties.

| File | Seconds | Channels | Peak dBFS | LUFS | Joint maximum delta | Source | Licence |
|---|---:|---:|---:|---:|---|---|---|
| animals/lion_roar.ogg | 2.200000 | 1 | -2.9 | — | — | Lion raring-sound1TamilNadu178.ogg | Public domain |
| animals/lion_roar_2.ogg | 2.100000 | 1 | -3.1 | — | — | Lion raring-sound1TamilNadu178.ogg | Public domain |
| animals/lion_growl.ogg | 3.000000 | 1 | -3.1 | — | — | Lionroar.wav | CC BY 4.0 |
| animals/lion_growl_2.ogg | 3.400000 | 1 | -2.9 | — | — | Lionroar.wav | CC BY 4.0 |
| animals/tiger_growl.ogg | 2.800000 | 1 | -3.0 | — | — | 439280 schots angry-tiger.wav | CC0 |
| animals/tiger_growl_2.ogg | 2.900000 | 1 | -2.9 | — | — | 439280 schots angry-tiger.wav | CC0 |
| animals/tiger_growl_3.ogg | 3.600000 | 1 | -2.9 | — | — | 439280 schots angry-tiger.wav | CC0 |
| animals/wolf_howl.ogg | 8.400000 | 1 | -3.1 | — | — | Wolf howls.ogg | Public domain |
| animals/wolf_howl_2.ogg | 9.800000 | 1 | -3.1 | — | — | Wolf howls.ogg | Public domain |
| animals/wolf_howl_3.ogg | 9.700000 | 1 | -3.0 | — | — | Wolf howls.ogg | Public domain |
| animals/brown_bear_growl.ogg | 1.700000 | 1 | -3.1 | — | — | Yellowstone sound library - Grizzly Bear vocalizations - 001.mp3 | Public domain |
| animals/brown_bear_huff.ogg | 1.600000 | 1 | -3.0 | — | — | Yellowstone sound library - Grizzly Bear vocalizations - 001.mp3 | Public domain |
| animals/brown_bear_huff_2.ogg | 1.600000 | 1 | -3.1 | — | — | Yellowstone sound library - Grizzly Bear vocalizations - 001.mp3 | Public domain |
| animals/zebra_bark.ogg | 1.590000 | 1 | -3.0 | — | — | Zebras - Grant's Zebra Braying | CC0 |
| animals/zebra_whinny.ogg | 1.590000 | 1 | -3.1 | — | — | Zebras - Grant's Zebra Braying | CC0 |
| footsteps/paved_1.ogg | 0.400000 | 1 | -3.1 | — | — | kroki-po-chodniku.WAV | CC0 |
| footsteps/paved_2.ogg | 0.400000 | 1 | -3.1 | — | — | kroki-po-chodniku.WAV | CC0 |
| footsteps/paved_3.ogg | 0.400000 | 1 | -3.1 | — | — | kroki-po-chodniku.WAV | CC0 |
| footsteps/paved_4.ogg | 0.400000 | 1 | -3.1 | — | — | kroki-po-chodniku.WAV | CC0 |
| footsteps/paved_5.ogg | 0.400000 | 1 | -2.9 | — | — | kroki-po-chodniku.WAV | CC0 |
| footsteps/gravel_1.ogg | 0.400000 | 1 | -3.1 | — | — | Footsteps_Boots_Gritty_Ground_(Gravel).wav | CC0 |
| footsteps/gravel_2.ogg | 0.400000 | 1 | -3.1 | — | — | Footsteps_Boots_Gritty_Ground_(Gravel).wav | CC0 |
| footsteps/gravel_3.ogg | 0.400000 | 1 | -3.0 | — | — | Footsteps_Boots_Gritty_Ground_(Gravel).wav | CC0 |
| footsteps/gravel_4.ogg | 0.400000 | 1 | -2.9 | — | — | Footsteps_Boots_Gritty_Ground_(Gravel).wav | CC0 |
| footsteps/gravel_5.ogg | 0.400000 | 1 | -3.0 | — | — | Footsteps_Boots_Gritty_Ground_(Gravel).wav | CC0 |
| footsteps/grass_1.ogg | 0.400000 | 1 | -2.9 | — | — | FOLEY_Footsteps_Grass_001.wav | CC0 |
| footsteps/grass_2.ogg | 0.400000 | 1 | -2.9 | — | — | FOLEY_Footsteps_Grass_001.wav | CC0 |
| footsteps/grass_3.ogg | 0.400000 | 1 | -3.0 | — | — | FOLEY_Footsteps_Grass_001.wav | CC0 |
| footsteps/grass_4.ogg | 0.400000 | 1 | -2.9 | — | — | FOLEY_Footsteps_Grass_001.wav | CC0 |
| footsteps/grass_5.ogg | 0.400000 | 1 | -2.9 | — | — | FOLEY_Footsteps_Grass_001.wav | CC0 |
| ambience/park_day.ogg | 36.000000 | 2 | -4.1 | -23.09 | 1.747 dB (-26.49 joint / -25.66 rest) | UK early morning urban birdsong in Spring | CC0 |
| ambience/crowd.ogg | 25.000000 | 1 | -2.4 | -23.12 | 1.376 dB (-21.72 joint / -22.69 rest) | Mumbling crowd - people talking background sound | CC BY 4.0 |
| ambience/water.ogg | 15.000000 | 1 | -6.5 | -23.24 | 2.640 dB (-22.56 joint / -24.54 rest) | Water lapping on lake Pukaki,South Island,New Zealand.mp3 | CC0 |
| ambience/wind_trees.ogg | 20.000000 | 2 | -5.0 | -23.08 | 0.881 dB (-29.89 joint / -29.01 rest) | windinleaves.wav | CC0 |
| animals/giraffe_grunt.ogg | 1.400000 | 1 | -3.1 | — | — | Nocturnal humming vocalizations: adding a piece to the puzzle of giraffe vocal communication — Additional file 2 (Grunt, Fig. 1a) | CC BY 4.0 |
| animals/penguin_bray.ogg | 1.100000 | 1 | -3.0 | — | — | Vocal-individuality-cues-in-the-African-penguin-(Spheniscus-demersus)-a-source-filter-theory-srep17255-s2.oga | CC BY 4.0 |
| animals/meerkat_chirp.ogg | 2.300000 | 1 | -3.0 | — | — | Erdmaennchen.ogg | CC BY-SA 3.0 |
| animals/meerkat_chirp_2.ogg | 2.310000 | 1 | -3.0 | — | — | Erdmaennchen.ogg | CC BY-SA 3.0 |
| animals/meerkat_chirp_3.ogg | 2.300000 | 1 | -2.9 | — | — | Erdmaennchen.ogg | CC BY-SA 3.0 |
| animals/flamingo_call.ogg | 3.500000 | 1 | -3.1 | — | — | 2019209_flamingos.wav | CC BY 4.0 |
| animals/flamingo_call_2.ogg | 3.600000 | 1 | -2.9 | — | — | 2019209_flamingos.wav | CC BY 4.0 |
| animals/flamingo_call_3.ogg | 3.500000 | 1 | -2.9 | — | — | 2019209_flamingos.wav | CC BY 4.0 |
| animals/rhino_grunt.ogg | 3.800000 | 1 | -3.1 | — | — | white Rhino   fighting | CC0 |
| animals/rhino_snort.ogg | 1.800000 | 1 | -3.1 | — | — | white Rhino   fighting | CC0 |
| animals/rhino_snort_2.ogg | 1.900000 | 1 | -3.1 | — | — | white Rhino   fighting | CC0 |
| animals/camel_grunt.ogg | 2.560000 | 1 | -3.0 | — | — | Cri du chameau.wav | CC BY-SA 3.0 |
| animals/camel_grunt_2.ogg | 2.650000 | 1 | -3.0 | — | — | Cri du chameau.wav | CC BY-SA 3.0 |
| animals/elephant_rumble.ogg | 4.600000 | 1 | -2.9 | — | — | Bee Threat Elicits Alarm Call in African Elephants — Audio S3 (control rumbles) | CC BY 4.0 |

48 files; 340 per-file assertions passed; 0 failures. Manifest, species contracts and inventory checks passed. Total audio directory: 2,286,725 bytes.
