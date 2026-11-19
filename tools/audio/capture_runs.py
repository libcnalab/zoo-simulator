#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Records what the game would have played, without playing it.

Each scenario is one run of the game with SDL's disk audio driver (the samples go to a file,
never to a sound card) and the soundscape's own event log (ZOO_SOUND_LOG: which voice, which
take, what gain, pan and distance, when). Run from the repository root:

    python3 tools/audio/capture_runs.py --set before            # every scenario
    python3 tools/audio/capture_runs.py --set after --only habitat-lion,walk

Output: build-probe/audio/<set>/<scenario>.{raw,log,out}; measure with measure_capture.py.
`--run-dir DIR` runs `DIR/zoo-simulator` from DIR instead of build/ (a "before" binary beside
the assets/ and data/ it is to read, for a comparison).
The game runs in real time (not --lockstep): the audio device is paced by the wall clock,
so only a real-time run records what a visitor would have heard. One run at a time, and none
while less than 6 GB of memory is available (the machine is shared).
"""
import argparse
import os
import pathlib
import signal
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPECIES = ["lion", "tiger", "wolf", "brown_bear", "polar_bear", "elephant", "rhino", "giraffe", "zebra",
           "camel", "kangaroo", "ostrich", "meerkat", "flamingo", "penguin"]
SEED = "7"
# A minute at each habitat is long enough to include routine changes while keeping every
# scenario within the audit's 30-to-60-second capture window.
HABITAT_SECONDS = 60


def scenarios(scripts: pathlib.Path) -> dict[str, dict]:
    """name -> {args, seconds of sound wanted}."""
    out: dict[str, dict] = {}
    for species in SPECIES:
        out[f"habitat-{species}"] = {"args": ["--stand", f"habitat:{species}"], "seconds": HABITAT_SECONDS}
    out["entrance"] = {"args": [], "seconds": 45}
    for hour in (6, 12, 18, 21):
        wolf_args = ["--stand", "habitat:wolf", "--hour", str(hour)]
        if hour == 18:
            # Fill just past the six-game-hour chorus interval. The command-line hour changes
            # calendar time but not the motion clock used for call spacing; the ordinary 600-tick
            # fill has already consumed the first dusk call before Soundscape starts observing.
            wolf_args += ["--ticks", "3601"]
        out[f"hour-{hour:02d}-wolf"] = {"args": wolf_args, "seconds": HABITAT_SECONDS}
        if hour != 18:
            out[f"hour-{hour:02d}-entrance"] = {"args": ["--hour", str(hour)], "seconds": 45}
    for script in sorted(scripts.glob("*.script")):
        first = script.read_text().splitlines()[0]
        # "# args: --stand habitat:lion; seconds: 60"
        args, seconds = [], 60
        if first.startswith("# args:"):
            head, _, tail = first[len("# args:"):].partition("; seconds:")
            args = head.split()
            seconds = int(tail) if tail.strip() else 60
        out[script.stem] = {"args": args + ["--script", str(script)], "seconds": seconds}
    return out


def available_gb() -> float:
    for line in pathlib.Path("/proc/meminfo").read_text().splitlines():
        if line.startswith("MemAvailable:"):
            return int(line.split()[1]) / 1048576.0
    return 0.0


def run(name: str, spec: dict, out: pathlib.Path, load_limit: float, where: pathlib.Path) -> bool:
    while available_gb() < 6.0:
        print(f"{name}: {available_gb():.1f} GB available, waiting", flush=True)
        time.sleep(60)
    raw, log, text = (out / f"{name}.{ext}" for ext in ("raw", "log", "out"))
    for path in (raw, log, text):
        path.unlink(missing_ok=True)
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIODRIVER="disk",
               SDL_AUDIO_DISK_OUTPUT_FILE=str(raw), ZOO_SOUND_LOG=str(log))
    command = ["./zoo-simulator", "--seed", SEED, "--frames", "10000000", *spec["args"]]
    with text.open("w") as sink:
        sink.write(" ".join(command) + "\n")
        sink.flush()
        game = subprocess.Popen(command, cwd=where, env=env, stdout=sink, stderr=subprocess.STDOUT)
        # Loading takes 11 s on a quiet machine and twice that on a busy one: the seconds wanted
        # are counted from the first sound (the log's "beds" line), not from the start.
        waited = 0.0
        while waited < load_limit and game.poll() is None and not (log.exists() and "\tbeds\t" in log.read_text()):
            time.sleep(0.25)
            waited += 0.25
        if game.poll() is None:
            try:
                game.wait(timeout=spec["seconds"] if waited < load_limit else 0.1)
            except subprocess.TimeoutExpired:
                pass
        game.send_signal(signal.SIGTERM)
        try:
            code = game.wait(timeout=20)
        except subprocess.TimeoutExpired:
            game.kill()
            code = game.wait()
    size = raw.stat().st_size if raw.exists() else 0
    print(f"{name}: exit {code}, loaded in {waited:.0f} s, {size / 176400:.1f} s captured", flush=True)
    return size > 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--set", required=True, help="name of the output directory under build-probe/audio/")
    parser.add_argument("--only", help="comma-separated scenario names (or prefixes ending in *)")
    parser.add_argument("--load-limit", type=float, default=120.0,
                        help="seconds to wait for the game's first sound before giving a run up")
    parser.add_argument("--run-dir", type=pathlib.Path, default=ROOT / "build",
                        help="directory holding the zoo-simulator to run, and its assets/ and data/")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()
    known = scenarios(ROOT / "tools" / "audio" / "scripts")
    if args.list:
        print("\n".join(known))
        return 0
    wanted = list(known)
    if args.only:
        wanted = []
        for pattern in args.only.split(","):
            hits = [n for n in known if n == pattern or (pattern.endswith("*") and n.startswith(pattern[:-1]))]
            if not hits:
                print(f"no scenario matches {pattern}")
                return 2
            wanted += hits
    out = ROOT / "build-probe" / "audio" / args.set
    out.mkdir(parents=True, exist_ok=True)
    failed = [name for name in wanted if not run(name, known[name], out, args.load_limit, args.run_dir.resolve())]
    if failed:
        print("nothing captured: " + ", ".join(failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
