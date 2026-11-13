# Rebuilding the zoo audio library

Requires Python 3 (standard library only), `ffmpeg` with `libvorbis`, and `ffprobe`.
Run from the repository root:

```sh
python3 tools/audio/build_audio.py
python3 tools/audio/verify_audio.py
```

Downloads and intermediate files stay in ignored `build-probe/audio-src/`. Existing downloads
are reused only after verifying their SHA-256 hashes. Requests use
`zoo-simulator-audio-fetch/1.0`, run sequentially, and pause five seconds between downloads;
transient HTTP failures have longer backoff. No login, API key, dependencies, CMake or compilation
is needed. `--only SOUND_NAME` rebuilds an individual group (or comma-separated groups); the other outputs must already exist.

`sources.json` has one entry per delivered file: immutable source checksum, public download URL,
source page, author, licence URL, original cut times, channels and FFmpeg filters. The public
high-quality Freesound previews are used where the original download requires login. Their
individual sound pages expose the licence and preview URL without authentication.
`licence-evidence.json` preserves the source metadata read during research. The helper scripts
`research.py` and `acquire_sources.py` support future source discovery; they are not needed to rebuild.

The builder uses 15 ms / 60 ms fades for one-shots and adjusts gain against the *decoded Vorbis*
peak, targeting -3 dBFS. Ambiences overlap the source tail with its head using a linear crossfade
and rotate that blend to the end of the loop. Loudness control is applied before the blend where needed; water also uses transient compression.
Static gain measured with `loudnorm` then targets -23 LUFS without changing the cyclic boundary. All outputs use 44100 Hz Vorbis quality 4.
The manifest durations come from `ffprobe`, never from the intended cut length.

The verifier checks the complete disk/source/manifest inventory, species IDs and names, Vorbis
codec, sample rate, channels, durations, decoded peaks, loop loudness and 100 ms boundary RMS.
It writes the per-file results to `tools/audio/VERIFICATION.md`, and 100 ms envelopes to
`build-probe/audio-src/envelopes.json`. See `RESEARCH.md` for omissions, rejected sources,
ShareAlike fallback reasoning and the limitations of review without listening.

Rebuilding with the same source bytes and FFmpeg version reproduces the decoded library.
Vorbis containers can have different stream serial numbers; compare decoded PCM hashes rather
than assuming byte-identical Ogg containers across FFmpeg releases.

To check an existing library's reproducibility with the same FFmpeg version:

```sh
python3 tools/audio/check_rebuild.py --snapshot
python3 tools/audio/build_audio.py
python3 tools/audio/check_rebuild.py
```

The reference and comparison result stay in `build-probe/audio-src/`. The comparison includes
the complete file inventory and SHA-256 of decoded signed 16-bit PCM, independent of Ogg
container serial numbers.

## Round 2 measurements and research

The builder and verifier still use Python's standard library. The additional measured audit
uses the already available NumPy (tested with 2.5.2); no package was downloaded or installed.
Run after building:

```sh
python3 tools/audio/test_audit_audio.py
python3 tools/audio/audit_audio.py
```

The audit writes `AUDIT.md`, `GAINS.md` and ignored `audit-measurements.json`. Its exact metric
definitions, short-clip loudness padding and limits are stated in AUDIT.md. Digital silence
has a `-inf` noise floor (represented as a string in the machine-readable JSON). The editable
`audit-decisions.md` is included in the generated audit so measured tables can be regenerated
without losing the documented passage checks and trimming decisions. Gain proposals are not
applied to audio or the game.

Round 2 research helpers preserve exact requests and evidence in `build-probe/audio-src/`.
`round2_research.py` serializes requests with five-second pauses and the required User-Agent,
counts response bytes against a decimal 300 MB cap, and reuses cached successes/failures unless
explicitly retried. `round2_ledger.py` regenerates only its marked section in RESEARCH.md.
`inspect_source.py SOURCE LABEL [--start SECONDS --end SECONDS]` writes a combined waveform
and logarithmic spectrogram in that cache. These scripts do not play audio. Nobody listened.

The nine missing call names remain omitted; no manifest entry points to a substitute species.
Penguin take 1 is now a documented ecstatic-display bray excerpt, take 2 is the explicitly
labelled old contact call, and take 3 is a separate display excerpt. Elephant rumble has three
distinct natural recordings. See the Round 2 sections of REPORT.md and RESEARCH.md.
