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
