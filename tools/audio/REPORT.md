# A1 sound-library delivery report

Delivered **48 Ogg files**: 29 animal-call files representing 16 requested call names,
4 ambience loops, and 15 distinct recorded footsteps. Total `assets/audio/` size:
**2,286,725 bytes** (2.287 MB; below 12 MB). Nothing was committed or published.

## 1. Every created or modified file

[FILE_INVENTORY.md](FILE_INVENTORY.md) lists every delivered/script/document path and every
ignored source/cache/intermediate path individually. There are 62 new reviewable files and one
modified existing file, `assets/ATTRIBUTION.md`, to which four rows were appended. The old rows
were checked byte-for-byte against HEAD and remain unchanged. No C++ file or external directory
was changed. Python import bytecode was generated only inside the owned `tools/audio/` tree;
it is ignored, disposable and not a delivery artifact.

The asset records are [`assets/audio/sounds.json`](../../assets/audio/sounds.json) and
[`assets/audio/SOURCES.md`](../../assets/audio/SOURCES.md). The latter has one row per Ogg,
including the original page URL, complete supplied author credit, licence URL and edits.
`sources.json` holds every download URL, original checksum, cut, channels and filters;
`licence-evidence.json` preserves the checked metadata. `build_audio.py` and `README.md`
provide the rebuild command; `verify_audio.py` and `check_rebuild.py` provide the checks.

## 2. Implementation and limitations

Every delivered file is Vorbis quality 4 at 44100 Hz. One-shots have 15 ms / 60 ms fades
and decoded peaks within 0.1 dB of -3 dBFS at the measurement precision. All peaks are below
-1 dBFS. Park and wind are stereo; all other files are mono. Loops use cyclic tail/head
crossfades, loudness control before the blend where needed, and static final loudness gain.
Water additionally uses transient compression. High-pass is 60 Hz; elephant rumble has no
high-pass. No animal or footstep was synthesized, and no noise-reduction filter was applied.

Meerkat chirps and camel grunts retain **CC BY-SA 3.0** under the permitted fallback.
All other assets are Public Domain, CC0 or CC BY 4.0. See RESEARCH.md for the fallback searches,
licence/source discrepancies and every rejected candidate. Its dedicated uncertainty table
identifies tiger reverberation; grizzly and rhino call classification; the short penguin call;
camel attribution/call naming; crowd intelligibility; park background; and quiet paved/grass
footsteps. **No recording was listened to.** Source spectrograms/waveforms were inspected,
and every final file has a measured 100 ms envelope in the ignored cache. These measurements
cannot prove the absence of speech or replace a listening review.

Names not delivered (per the brief's explicit omission rule):

| Name | Reason |
|---|---|
| `tiger_chuff` | A genuine CC0 tiger growl/snarl recording was found, but no source-identified chuff suitable for a confident cut was found. A growl was not relabelled as a chuff. |
| `wolf_growl` | Verified PD wolf howls were found; no licensed, source-identified wolf growl was located in the searched sources. |
| `polar_bear_growl` | No recording with verifiable field-recording provenance was located. A Commons CC0 item only describes itself as a sound effect; the readable Freesound hits were human voices or designed monsters. |
| `polar_bear_chuff` | Same provenance problem as above; no verified polar-bear chuff was located. |
| `elephant_trumpet` | The clearly identified CC0 Commons trumpet is an Asian elephant (`Elephas maximus`), whereas the data requires African elephant (`Loxodonta africana`). Other hits were human/instrument imitations, AI generation, or unidentified elephant species. |
| `meerkat_growl` | Verified meerkat alarm chirps were found, but a distinct growl was not identified. |
| `ostrich_grunt` | Commons scientific-name/audio searches and Freesound searches found pronunciations, generic pack descriptions, or references to other archives without a verified reusable adult grunt recording. |
| `ostrich_hiss` | No individual adult ostrich hiss file with a verified acceptable licence and downloadable recording was located in those searches. |
| `kangaroo_grunt` | Searches for kangaroo / `Osphranter rufus` yielded speech, music, or unrelated effects, not a verified red-kangaroo recording. |

The permitted same-species substitutions are lion grunts for growls, zebra brays for whinny,
and a short African-penguin call for full bray. Their records explicitly say so.

## 3. Commands actually run and results

No CMake, compiler, game build, CTest or GoogleTest command was run, as specifically required
by this asset-only task. No C++ file was created or owned, so clang-format was not applicable.

Final commands:

```sh
python3 tools/audio/build_audio.py
python3 tools/audio/check_rebuild.py
python3 tools/audio/verify_audio.py
git diff --check
```

The finalized builder was run twice across the entire 48-file library, both with exit status 0.
The comparison baseline was saved by inline Python using FFmpeg SHA-256 over decoded
`pcm_s16le` after the first finalized build. `check_rebuild.py` compared the second build:
**48 decoded PCM hashes identical; 0 differences**. The first exploratory comparison against
assets made by earlier normalization code had 21 differences; the final unchanged-script
comparison above is the reproducibility result.

Final verification: **48 files, 340 per-file assertions, 0 failures**, plus passing JSON,
manifest/inventory, source-licence and species-contract checks. Python AST syntax, JSON syntax,
SPDX headers and unchanged original attribution rows also passed. FFmpeg version:
`7.1.5-0+deb13u1`; Python `3.13.5`.

Earlier targeted builder invocations used these exact selections while adding or correcting assets:

```sh
python3 tools/audio/build_audio.py --only wolf_howl
python3 tools/audio/build_audio.py --only footstep_paved,footstep_gravel,footstep_grass
python3 tools/audio/build_audio.py --only brown_bear_huff,park_day,crowd,water,wind_trees
python3 tools/audio/build_audio.py --only park_day,crowd,water,wind_trees
python3 tools/audio/build_audio.py --only water,wind_trees
python3 tools/audio/build_audio.py --only giraffe_grunt,penguin_bray,meerkat_chirp,flamingo_call,wind_trees
python3 tools/audio/build_audio.py --only giraffe_grunt,penguin_bray,meerkat_chirp,flamingo_call
python3 tools/audio/build_audio.py --only rhino_grunt,rhino_snort
python3 tools/audio/build_audio.py --only camel_grunt
python3 tools/audio/build_audio.py --only elephant_rumble
```

The invocation including giraffe and wind initially failed decoded peak convergence for giraffe;
the gain search was corrected and the subsequent invocation succeeded. Earlier verifier runs
caught wolf peak -3.2 dBFS, grizzly-huff peak -2.4 dBFS, park loudness -28.03 LUFS, and water
loudness -25.48 LUFS. Those failed gates were fixed. The subsequent 45-file gate passed
(reported 274 selected per-file assertions before the activity assertions were added to the
counter); both later 48-file verification runs passed. Nothing above is a claimed C++ test result.

The verifier runs `ffprobe` and FFmpeg `volumedetect` for every delivered file. For each loop
it decodes two copies with `-stream_loop 1`, measures `astats` RMS in the centered 100 ms
joint, adjacent windows and the rest, and checks the maximum difference is at most 3 dB.
The exact final table follows; source links, authors, licence links and cuts are in SOURCES.md.

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

## 4. Deviations, assumptions and API recommendations

The nine missing names and same-species substitutions are explicit allowances in the brief.
High-quality public Freesound previews were used where original downloads require login;
those URLs and hashes are recorded. Adult walking was inferred from the shoe/boot walking
descriptions; performer ages are not supplied. ShareAlike applies to the five edited meerkat
and camel files, as marked in their source records. The loop overlap uses explicit fades and
`amix` after an exact-length `acrossfade` attempt lost the overlap; the resulting duration and
seam measurements passed. An early discovery request overlapped a metadata lookup and received
HTTP 429; subsequent acquisition used serialized requests, five-second spacing and backoff.

**No C++ API changes are proposed.** The lead should handle the nine absent optional names
and review the explicitly uncertain recordings before final soundscape integration. All work
is left as uncommitted working-tree changes for the lead.
