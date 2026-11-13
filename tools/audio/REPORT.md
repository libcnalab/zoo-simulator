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

## Round 2 — 2026-11-13

Part 1 searched the required source families and retains all nine omissions.
No source with uncertain species, call type or licence was promoted to an authentic
recording. See [RESEARCH.md](RESEARCH.md) for exact queries, retrieved counts,
per-candidate decisions, video audio-stream checks and explicit HTTP/access limits.
Nobody listened to the files.

| Name | Where searched / outcome |
|---|---|
| `elephant_trumpet` | iNaturalist's licensed Loxodonta africana file has no confirmed trumpet; Commons trumpet is Elephas maximus. Dryad's public dataset README explicitly identifies all 1,123 clips as manually identified rumbles (0.2 s buffers), so the 121 MB archive was not downloaded for a trumpet. Zenodo/Archive did not establish an eligible trumpet. |
| `tiger_chuff` | GBIF recordings are NC/proprietary; Commons has growls and a BY 2.0 yowling video, not an eligible chuff. Dryad/Zenodo chuff/prusten searches and Archive found no verified chuff file. |
| `wolf_growl` | iNaturalist/GBIF contain howls, mixed or unspecified calls and domestic dogs. PD Commons rallying has sustained overlapping harmonic calls, no confident isolated growl. Dryad/Zenodo/Archive do not establish an eligible growl. |
| `polar_bear_growl` | GBIF media are NC-SA; Commons play-fight videos checked have no audio. Generic sound effect lacks authentic-recording provenance. No suitable dataset/Archive/federal item verified. |
| `polar_bear_chuff` | Same searches as growl, including cub/nursing terms; nursing video has no audio. No source-identified chuff verified. Federal access limitations remain explicit. |
| `meerkat_growl` | No eligible iNaturalist observations; GBIF is NC-SA. Zenodo recruitment/speaker tracks and Archive pup distress are not identified growls. Commons tussling audio cannot establish an isolated growl. |
| `ostrich_grunt` | GBIF SA candidate is wing shaking; all 44 xeno-canto entries have NC/ND restrictions, including the explicit booms. One Commons courtship video is digitally silent; another is dominated by continuous noise. No eligible boom established in datasets/Archive. |
| `ostrich_hiss` | Same searches, plus hiss queries. XC46725 identifies the foreground hiss as a python; no eligible ostrich hiss established. |
| `kangaroo_grunt` | No eligible Osphranter rufus/Macropus rufus sound observations or GBIF sound media. Zenodo/Dryad cough/cluck/vocalization queries do not provide red-kangaroo calls. Both Commons red-kangaroo videos inspected have no audio stream (8.008 s original; 36.036 s small derivative of the larger original). |

### Round 2 new takes

- `elephant_rumble_2.ogg`: Loxodonta africana, natural bee-response rumble in PLOS Audio S1,
  [original record](https://plos.figshare.com/articles/dataset/Bee_Threat_Elicits_Alarm_Call_in_African_Elephants/143732), CC BY 4.0; cut 0.13–4.39 s.
- `elephant_rumble_3.ogg`: Loxodonta africana, original-speed oral rumble in PLOS Movie S3,
  [original record](https://plos.figshare.com/articles/dataset/Visualizing_Sound_Emission_of_Elephant_Vocalizations_Evidence_for_Two_Rumble_Production_Types__/117289), CC BY 4.0; cut 0.30–2.62 s.
- `penguin_bray.ogg` now contains a real Spheniscus demersus ecstatic-display bray excerpt,
  [PLOS Video S3](https://plos.figshare.com/articles/dataset/The_Vocal_Repertoire_of_the_African_Penguin_Spheniscus_demersus_Structure_and_Function_of_Calls/1122424), CC BY 4.0; cut 6.00–11.85 s.
- `penguin_bray_2.ogg` retains the old short contact call, explicitly labelled as such.
- `penguin_bray_3.ogg`: a distinct ecstatic-display bray excerpt with three long syllables,
  [Nature Supplementary Video S1](https://www.nature.com/articles/srep17255), CC BY 4.0; cut 3.40–9.40 s.

Brown-bear growl, giraffe grunt, rhino grunt, zebra bark and zebra whinny remain single-take.
Original recordings and alternative-source queries were tried; call uncertainty, absent audio,
background noise and the zebra archive's size prevented additional accepted takes.
The exact decisions are in RESEARCH.md. No ambiguous event was relabelled to meet a take count.

### Round 2 measured audit and acceptance results

[AUDIT.md](AUDIT.md) contains all requested metrics for all 52 Ogg files. Its first paragraph
states that these are measurements and nobody listened. The initial audit found two files
with excess silent margins; both were trimmed through sources.json:

| File | Old cut s | New cut s | Final head / tail silence ms |
|---|---|---|---|
| `animals/elephant_rumble.ogg` | 0.95–5.55 | 1.1475–5.2672 | 19.95 / 31.00 |
| `animals/penguin_bray_2.ogg` (old take 1) | 0.25–1.35 | 0.4804–1.1011 | 22.95 / 18.12 |

Final audit: **52 rows, zero head/tail silence flags over 80 ms, eight quiet-floor review
flags**. The latter are water, camel grunt take 1, elephant rumble take 1, flamingo call
take 2, giraffe grunt and all three wolf howls. Eight prepared alternative passages were
measured; none resolved the threshold without changing natural call coverage. The exact
comparisons and retained-background decisions are in AUDIT.md. No noise reduction or new
Part 3 frequency filter was applied. The quiet-window proxy includes sustained call energy;
it must not be described as proof of audible hiss.

Species spreads over 4 LU: brown bear **7.1 LU**, elephant **6.4 LU**, lion **11.4 LU**,
penguin **4.1 LU**. [GAINS.md](GAINS.md) proposes one gain for each of the 16 delivered call
names, with loud-call targets, growls/grunts 8 LU lower, and quieter penguin/meerkat targets.
These gains are unapplied. Within-name take differences remain visible in that table.
Loop seam deltas remain below 3 dB. Nobody listened to the files.

The final library has **52 files, 2,453,628 bytes for the whole assets/audio directory**,
well below 12,000,000 bytes. There are four additional Ogg paths, plus the replacement of
penguin take 1. Of the original 48 paths, 46 retain identical decoded signed-16-bit PCM;
only `elephant_rumble.ogg` (silence trim) and `penguin_bray.ogg` (requested true bray
replacement) differ. The original penguin audio was first preserved byte-for-byte in decoded
PCM at `_2`, then its silent margins were trimmed in Part 3. No other old audio changed.

All 52 source hashes match the cached inputs and an attribution/licence-evidence record.
The nine absent names remain absent from the manifest. Soundscape's name-to-files manifest
contract is unchanged. No C++, data/species, other assets or sibling checkout was modified.
The only verifier duration exception is the documented short contact-call variant (minimum
0.5 s after trimming); all other bounds are unchanged. No C++ API change is recommended.
The proposed per-name gain table is the handoff for later integration, not a runtime change.

Research made **300 serialized requests**, with **137,679,189 response bytes recorded**,
using the required User-Agent and five-second pauses. No 184.6 MB zebra archive was downloaded:
it would exceed the remaining cumulative budget. HTTP access failures and bounded result
pages are recorded as limits, never as proof that a source contains no recording.

### Round 2 commands actually run

No CMake, C++ build, CTest, git commit, branch operation, push or publishing command was run.
Before each audio-build command group the environment was:

```sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv ASAN_OPTIONS=detect_leaks=0
```

Baseline and Part 1:

```sh
python3 tools/audio/check_rebuild.py --snapshot
python3 tools/audio/build_audio.py > build-probe/audio-src/round2-part1-build.log
python3 tools/audio/check_rebuild.py
python3 tools/audio/verify_audio.py > build-probe/audio-src/round2-part1-verify.log
```

Result: exit 0; **48 decoded PCM hashes identical, zero differences; 48 files, 340 per-file
assertions, zero failures**, plus manifest/species/inventory checks. The original reference
was preserved as `round2-original-pcm.json` for the final old-file comparison.

Part 2:

```sh
python3 tools/audio/build_audio.py > build-probe/audio-src/round2-part2-build.log
python3 tools/audio/check_rebuild.py --snapshot
python3 tools/audio/build_audio.py > build-probe/audio-src/round2-part2-rebuild.log
python3 tools/audio/check_rebuild.py
python3 tools/audio/verify_audio.py > build-probe/audio-src/round2-part2-verify.log
```

Result: exit 0; **52 decoded PCM hashes identical, zero differences; 52 files, 368 per-file
assertions, zero failures**; directory then 2,457,190 bytes.

Part 3 investigation and trim:

```sh
python3 tools/audio/test_audit_audio.py
python3 tools/audio/audit_audio.py > build-probe/audio-src/round2-audit-initial.log
python3 tools/audio/audit_passages.py
python3 tools/audio/build_audio.py --only elephant_rumble,penguin_bray > build-probe/audio-src/round2-trim-build.log
python3 tools/audio/audit_passages.py > build-probe/audio-src/round2-passage-comparisons.log
python3 tools/audio/test_audit_audio.py
```

The first analytic suite passed **8 tests**. The first audit completed its measurements but
failed strict JSON export on a legitimate negative-infinite quiet floor. The exporter was
corrected to preserve non-finite dB values as strings; rerunning the same audit command
succeeded (52 rows, 2 edge flags, 7 quiet-floor flags before trimming). Its JSON was copied
to `round2-audit-before-trim.json`. The passage comparison was extended from seven to eight
alternatives after trimming exposed the elephant flag. The final analytic suite passed
**9 tests, zero failures**. Both passage-comparison runs and the group trim build exited 0.
The trimmed files were also measured directly to check their edge silence before the final build.

Final acceptance run:

```sh
python3 tools/audio/build_audio.py > build-probe/audio-src/round2-final-build.log
python3 tools/audio/check_rebuild.py --snapshot
python3 tools/audio/build_audio.py > build-probe/audio-src/round2-final-rebuild.log
python3 tools/audio/check_rebuild.py
python3 tools/audio/verify_audio.py > build-probe/audio-src/round2-final-verify.log
python3 tools/audio/audit_audio.py > build-probe/audio-src/round2-final-audit.log
```

All exited 0: **52 decoded PCM hashes identical, zero differences; 52 files, 368 per-file
assertions, zero failures; 52 audit rows, zero edge-silence flags, eight explicitly retained
quiet-floor review flags**. Analytic audit tests: **9 passed**. All 15 Python files parse;
all 52 source hashes and licence records match. `git diff --check` initially found a trailing
blank line in the generated research ledger; the writer was corrected and the final check passed.

### Round 2 files and limitations

[FILE_INVENTORY.md](FILE_INVENTORY.md) lists every created/modified reviewable file and all
present ignored cache artifacts. Only the audio row of assets/ATTRIBUTION.md was extended;
its non-audio rows are unchanged. Work is left uncommitted for the lead.

The nine missing names remain unfulfilled as recordings, which the brief explicitly allows
after documented unsuccessful searches. Five requested single-take names remain single-take
for the reasons above. The new penguin displays are honest excerpts, and the contact-call
variant is explicitly identified. The audit uses already-installed NumPy; no dependency was
installed. Shorter-than-400-ms footsteps use disclosed zero padding solely for EBU measurement,
not asset editing. No subjective listening, speech-absence certification or final mix approval
is claimed. Nobody listened to the files.
