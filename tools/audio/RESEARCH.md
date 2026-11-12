# Audio research and review notes

Research date: 2026-11-12. No audio was listened to. Selection used the source's species/call
identification, spectrograms, waveform envelopes, and decoded measurements. This is a bounded
search of Commons, NPS/FWS and publicly readable Freesound pages, not proof that an unavailable
recording does not exist anywhere. No animal calls were synthesized. No footsteps were synthesized.

The final inventory and per-file licence/cut records are in `sources.json` and
[`assets/audio/SOURCES.md`](../../assets/audio/SOURCES.md). The measured format, peak, loudness,
and loop-boundary table is in [VERIFICATION.md](VERIFICATION.md).

## Names intentionally not delivered

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

## Substitutions and ShareAlike exceptions

- `lion_growl.ogg` and `_2`: low grunts from the end of a species-identified lion roaring bout.
- `zebra_whinny.ogg`: a plains-zebra bray, explicitly described by the recordist as distinct
  from horse neighing; the same-species bray is the stand-in for this requested name.
- `penguin_bray.ogg`: a short African-penguin call from the supplementary recording. It is
  not an entire multi-syllable ecstatic-display song; exact call subtype remains unverified.
- `meerkat_chirp.ogg`, `_2`, `_3`: **CC BY-SA 3.0 fallback**. Commons `Suricata`/meerkat
  searches and Freesound searches found no verified CC0/PD/CC BY recording of meerkat calls.
  Human vocalizations, pronunciation files, and calls of other mongoose species were excluded.
  These three edited recordings remain CC BY-SA 3.0.
- `camel_grunt.ogg`, `_2`: **CC BY-SA 3.0 fallback**. The Commons description (including
  Russian identification) and category identify Bactrian camel. The CC BY Freesound candidate
  `camel.flac` does not identify a camel species; an unanswered comment asks that question.
  No species-verified CC0/PD/CC BY Bactrian-camel call was found. Both edited recordings remain
  CC BY-SA 3.0, with the complete author list supplied by Commons preserved.

The repository does not change the licences of edited recordings to MIT. The MIT statement
covers repository scripts and records. See the linked CC licence for each recording's terms.

## Credit and licence discrepancies resolved

- The Commons grizzly MP3 has an `Artist` field naming Jennifer Jerrett, but its description
  says Dan Bergum recorded the bear. The [original NPS item](https://www.nps.gov/media/video/view.htm?id=48C181D5-1DD8-B71B-0BE5FF0F921CAA4F)
  credits **NPS/Dan Bergum** and explicitly explains public-domain use for NPS-credited media.
  The delivered records credit Dan Bergum and preserve the discrepancy. Cut times refer to the
  downloaded 65.135417 s Commons MP3, not the longer original NPS item.
- The Commons elephant-rumble Ogg is tagged **CC BY 2.5**, outside this brief's allowed versions.
  It was not downloaded or used. The [current publisher PLOS Figshare record](https://plos.figshare.com/articles/dataset/Bee_Threat_Elicits_Alarm_Call_in_African_Elephants/143732)
  and its [own API metadata](https://api.figshare.com/v2/articles/143732) explicitly identify
  **CC BY 4.0** and list original `Audio_S3.wav` (file ID 424086). That original WAV, its source
  checksum, the publisher's licence object, and the listed authors are the delivered source.
- The Commons giraffe **hum** derivative has CC BY-SA 4.0 metadata. It was not used. The
  [original publisher's article](https://link.springer.com/article/10.1186/s13104-015-1394-3)
  licenses the work CC BY 4.0 and supplies a separate, explicitly identified **grunt** in
  Additional file 2. The library uses that original MOV's audio, not the Commons derivative.

## Sources excluded for their licence or the fallback rule

| Candidate | Verified licence / reason for exclusion |
|---|---|
| [Commons elephant bee-study Ogg](https://commons.wikimedia.org/wiki/File:Bee-Threat-Elicits-Alarm-Call-in-African-Elephants-pone.0010346.s003.ogg) | CC BY 2.5 is outside the allowed versions; original publisher WAV used under the publisher's current CC BY 4.0 metadata instead. |
| [Robinhood76, 02240 elephant trumpets longer.wav](https://freesound.org/people/Robinhood76/sounds/107825/) | CC BY-NC 4.0; prohibited. Also an instrument imitation rather than an elephant recording. |
| [Akacie, Autumn Forest Wind 73723](https://freesound.org/people/Akacie/sounds/73723/) | CC BY-NC 4.0; prohibited. Similar titles by this author have different licences: do not infer a licence from another file. |
| [Akacie, Autumn Forest Wind 73717](https://freesound.org/people/Akacie/sounds/73717/) | CC BY-NC 4.0; prohibited. |
| [Commons Lion Mad](https://commons.wikimedia.org/wiki/File:Lion_Mad.ogg) | CC BY-SA 4.0; PD lion recordings exist, so the fallback condition fails. The species description is also uncertain. |
| [Commons Giraffe Hum](https://commons.wikimedia.org/wiki/File:Giraffe_Hum.oga) | CC BY-SA 4.0; unnecessary because an original CC BY 4.0 grunt exists. |
| [Sharad Apte greater-flamingo recording](https://commons.wikimedia.org/wiki/File:Greater_flamingo_call_recorded_in_March_2005_at_Mayani,_Satara.wav) | CC BY-SA 4.0; downloaded as a candidate but never delivered because dobroide's species-identified CC BY 4.0 recording was subsequently found. |
| [XC432217 greater flamingo](https://commons.wikimedia.org/wiki/File:Phoenicopterus_roseus_-_Greater_Flamingo_XC432217.mp3) | CC BY-SA 4.0; unnecessary once the CC BY 4.0 source was found. |
| Pixabay tiger-growl search result; Sound Ideas commercial animal libraries | Proprietary/NC/all-rights-reserved sources are outside the brief and were not used. No YouTube material or game rips were used. |

## Other tempting sources excluded

| Candidate | Reason |
|---|---|
| [Commons Elephant voice - trumpeting](https://commons.wikimedia.org/wiki/File:Elephant_voice_-_trumpeting.ogg) | CC0 but explicitly categorized as Asian elephant; wrong species. |
| [InspectorJ Flamingo Calls, Ensemble, A](https://freesound.org/people/InspectorJ/sounds/400860/) | CC BY 4.0 but American flamingos, not greater flamingos; the source also mentions a child screaming. |
| [Erdie camel.flac](https://freesound.org/people/Erdie/sounds/57181/) | CC BY 4.0, but camel species unspecified. |
| [Commons Bear growl](https://commons.wikimedia.org/wiki/File:Bear_growl.ogg) | CC BY 3.0, but only “bear cub” is identified; brown-bear species cannot be established. |
| [Commons Polar bear sound effect](https://commons.wikimedia.org/wiki/File:Polar_bear_sound_effect.wav) | CC0, but no field-recording provenance or description establishing a real recorded animal, rather than a designed effect. |
| [Commons Rhinoceros sound effect](https://commons.wikimedia.org/wiki/File:Rhinoceros_sound_effect.wav) | CC0, but no rhino species or field-recording provenance. A better white-rhino field recording was found. |
| [Danjocross Angry Elephant](https://freesound.org/people/Danjocross/sounds/507467/) | CC0 but created by blowing into PVC pipe. |
| [aesqe monster_growl_01](https://freesound.org/people/aesqe/sounds/49127/) | CC BY 4.0 but processed human voice; not a polar bear. |
| [SFXMint elephant trumpet](https://sfxmint.com/sounds/elephant-trumpet) | CC0 but explicitly AI-generated, not a recording. |
| [Clagnar Polar Bear](https://freesound.org/people/Clagnar/sounds/588556/) | CC0 but a man speaking the words “Polar Bear”. |
| “Grévys zebra (Sound Effects)” Commons search hit | Description says it combines hippo and donkey; wrong species and designed audio. |

## Measurement-based review and uncertainties

Source spectrograms were generated with `showspectrumpic` and inspected in the tool viewer for:
PD lion, the CC BY lion bout, tiger, FWS wolves, grizzly, giraffe, penguin, meerkat, greater
flamingo, white rhino, camel and elephant. The elephant excerpt preserves the low-frequency
band. Lion bouts show repeated low-frequency pulses; wolf howls show sustained harmonic
bands; the giraffe source has a brief low grunt near 0.9 s. Meerkat cuts select distinct warning
pulses from the first half of the source, avoiding its apparent repeated second half.
Footstep waveforms were inspected; the very quiet paved and grass sources were also viewed
with display gain. The gravel file explicitly separates five walking takes from five running
takes, and only the walking portion is used. All final files also have 100 ms `astats` RMS
envelopes; none of the cuts is all silence.

No measurement can prove the absence of intelligible words or establish an animal call's
subtype. **Every recording remains unlistened-to.** The following files have additional
uncertainty requiring particular attention during the lead's listening review:

| Files | Specific uncertainty |
|---|---|
| `animals/tiger_growl.ogg`, `_2`, `_3` | Source explicitly warns of strong enclosure reverberation; spectrograms cannot rule out quiet background visitors. |
| `animals/brown_bear_growl.ogg` | Weak warning vocalization assigned to growl from the source's mixed huff/growl description; precise call type is uncertain. |
| `animals/brown_bear_huff.ogg`, `_2` | Mixed huffing/jaw-popping recording during capture; precise breath-versus-jaw-pop content needs listening. |
| `animals/rhino_grunt.ogg`, `animals/rhino_snort.ogg`, `_2` | Source identifies white rhinos fighting in a waterhole; exact grunt/snort classification and separation from splashes remain provisional. |
| `animals/penguin_bray.ogg` | Short call substituted for full bray; exact vocal subtype unverified. |
| `animals/camel_grunt.ogg`, `_2` | Recording is identified as Bactrian camel by Commons, but the original contributor list does not assign one individual recordist. Growl/grunt nomenclature is approximate. |
| `ambience/crowd.ogg` | Source describes indistinct open-air conversation; no foreground sentence or music can be excluded by spectrogram alone. |
| `ambience/park_day.ogg` | Source states no traffic; the absence of very distant human/urban sounds needs listening. |
| `footsteps/paved_1.ogg` through `_5`, `footsteps/grass_1.ogg` through `_5` | Sources are quiet; substantial gain exposes background texture. Cuts contain one dominant impact, but shoe/scuff realism needs listening. |

The intended compromises are recorded rather than hidden behind generic animal names. No C++
API or source file was changed. Missing names should be handled as absent optional assets;
adding a later recording needs a new per-file source/licence check.

Additional assumptions: walking shoe/boot source descriptions were treated as adult steps;
recordist ages are not documented. Call-type substitutions above are permitted same-species
substitutions, not claims that each requested label is scientifically exact. Early discovery
included an overlapping search and metadata lookup and received HTTP 429; acquisition was then
serialized with five-second spacing and longer HTTP-error backoff. No further parallel network
fetches were used. No source licence was inferred from a site's general reputation or from a
similarly named file.
