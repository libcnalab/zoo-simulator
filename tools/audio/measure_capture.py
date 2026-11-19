#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Measures the captures of capture_runs.py: what the game sent to the audio device.

    python3 tools/audio/measure_capture.py --set before [--only NAME,...] [--no-pictures]

For every build-probe/audio/<set>/<name>.raw (signed 16 bit little endian, stereo, 44100 Hz:
what SDL's disk driver writes for this game) and its event log <name>.log it writes
<name>.json (every figure), <name>.png (spectrogram, left over right) and one table,
build-probe/audio/<set>/SUMMARY.md.

Nothing here judges how anything sounds. It measures level (EBU R 128 through ffmpeg), peaks,
clipping, balance, silence, steps in level, and compares the level and the left/right
difference of each animal call with what the game's own log says it asked for.
"""
import argparse
import json
import math
import pathlib
import re
import subprocess
import sys

import numpy as np

from mix_model import pan_gains

ROOT = pathlib.Path(__file__).resolve().parents[2]
RATE = 44100
FULL_SCALE = 32768.0


# ---- pure helpers (tools/audio/test_measure_capture.py) -------------------------------------

def db(power: float) -> float:
    """Decibels of a power (mean square); -inf for silence."""
    return 10.0 * math.log10(power) if power > 0.0 else -math.inf


def window_powers(samples: np.ndarray, frames: int) -> np.ndarray:
    """Mean square of consecutive whole windows of `frames` samples, per channel: (n, channels)."""
    count = len(samples) // frames
    if count == 0:
        return np.zeros((0, samples.shape[1]))
    return np.mean(samples[:count * frames].reshape(count, frames, -1) ** 2, axis=1)


def first_sound(raw: np.ndarray) -> int:
    """Index of the first frame that is not digital silence (len(raw) when there is none)."""
    loud = np.flatnonzero(np.any(raw != 0, axis=1))
    return int(loud[0]) if len(loud) else len(raw)


def clipped(raw: np.ndarray) -> int:
    """Samples at either end of the 16 bit range."""
    return int(np.sum((raw >= 32767) | (raw <= -32768)))


def silences(samples: np.ndarray, rate: int, floor_db: float = -60.0, window: float = 0.1,
             least: float = 0.3) -> list[tuple[float, float]]:
    """(start, length) in seconds of every stretch of at least `least` seconds below `floor_db`."""
    frames = max(1, round(rate * window))
    quiet = np.max(window_powers(samples, frames), axis=1) < 10.0 ** (floor_db / 10.0)
    out, start = [], None
    for index, flag in enumerate(list(quiet) + [False]):
        if flag and start is None:
            start = index
        elif not flag and start is not None:
            if (index - start) * window >= least - 1e-9:
                out.append((start * window, (index - start) * window))
            start = None
    return out


def level_steps(samples: np.ndarray, rate: int, window: float = 0.01, jump_db: float = 18.0,
                audible_db: float = -50.0) -> list[tuple[float, float]]:
    """(time, jump in dB) where the level of one 10 ms window stands `jump_db` or more above the
    window before it and above `audible_db`: a start without a fade (or a click). A sound that
    fades in over tens of milliseconds, as every take of the library does, is not reported."""
    powers = np.max(window_powers(samples, max(1, round(rate * window))), axis=1)
    floor = 10.0 ** (-90.0 / 10.0)
    out = []
    for index in range(1, len(powers)):
        if powers[index] < 10.0 ** (audible_db / 10.0):
            continue
        jump = db(powers[index]) - db(max(powers[index - 1], floor))
        if jump >= jump_db:
            out.append((index * window, jump))
    return out


def following(marks: list[float], causes: list[float], lead: float = 0.05, lag: float = 0.35) -> int:
    """How many of the marks come within `lag` seconds after (or `lead` before) one of the causes."""
    return sum(1 for mark in marks if any(-lead <= mark - cause <= lag for cause in causes))


def repeats(takes: list[str]) -> int:
    """How many times an item of the sequence is the same as the one before it."""
    return sum(1 for before, after in zip(takes, takes[1:]) if before == after)


def parse_log(text: str) -> list[dict]:
    """The soundscape's event log: tab-separated `time kind fields...`, fields `key=value` or bare."""
    events = []
    for line in text.splitlines():
        parts = line.split("\t")
        if len(parts) < 2:
            continue
        try:
            event = {"t": float(parts[0]), "kind": parts[1], "bare": []}
        except ValueError:
            continue
        for field in parts[2:]:
            key, sep, value = field.partition("=")
            if not sep:
                event["bare"].append(field)
                continue
            try:
                event[key] = float(value)
            except ValueError:
                event[key] = value
        events.append(event)
    return events


FOLLOWERS_OLD = ("water", "people0", "people1")
FOLLOWERS = ("water", "fountain", "people0", "people1")


def follower_triples(event: dict) -> dict[str, tuple[float, float, float]]:
    """(effective volume, pan, distance in metres) of each looping source of a `state` line.
    The log wrote them bare and in the order water, crowd, crowd before it named them."""
    out = {}
    for name in FOLLOWERS:
        value = event.get(name)
        if isinstance(value, str) and value.count(",") == 2:
            out[name] = tuple(float(part) for part in value.split(","))
    if not out and len(event.get("bare", [])) >= 3 * len(FOLLOWERS_OLD):
        for index, name in enumerate(FOLLOWERS_OLD):
            out[name] = tuple(float(part) for part in event["bare"][3 * index:3 * index + 3])
    return out


def follower_figures(states: list[dict], heard: float = 0.003) -> dict[str, dict]:
    """Per looping source: the share of the time it is heard at all, its highest level, how far to
    one side it is ever put while heard, and how near it comes."""
    out: dict[str, dict] = {}
    for name in FOLLOWERS:
        rows = [triple[name] for triple in map(follower_triples, states) if name in triple]
        loud = [row for row in rows if row[0] > heard]
        if not rows:
            continue
        out[name] = {"heard_share": round(len(loud) / len(rows), 2),
                     "volume_max": round(max((row[0] for row in rows), default=0.0), 4),
                     "pan_abs_max": round(max((abs(row[1]) for row in loud), default=0.0), 3),
                     "distance_min": round(min((row[2] for row in loud), default=float("nan")), 1)}
    return out


def overlap_count(spans: list[tuple[float, float]]) -> int:
    """The most spans (start, end) that are open at one moment."""
    edges = sorted([(start, 1) for start, _ in spans] + [(end, -1) for _, end in spans])
    most = now = 0
    for _, step in edges:
        now += step
        most = max(most, now)
    return most


# ---- measuring one capture -----------------------------------------------------------------

def step_envelope(samples: np.ndarray, times: list[float], rate: int, window: float = 0.01,
                  before: float = 0.15, after: float = 0.45) -> dict | None:
    """The mean power around the given times, in windows of `window` seconds from `before` seconds
    ahead of each to `after` seconds past it; how far its highest window after the times stands
    over the median of the windows ahead of them, and when."""
    frames = int(rate * window)
    ahead, behind = round(before / window), round(after / window)
    mono = np.mean(samples ** 2, axis=1) if samples.ndim == 2 else samples ** 2
    rows = []
    for t in times:
        at = int(t * rate)
        if at - ahead * frames < 0 or at + behind * frames > len(mono):
            continue
        rows.append(mono[at - ahead * frames:at + behind * frames].reshape(-1, frames).mean(axis=1))
    if not rows:
        return None
    mean = np.mean(np.array(rows), axis=0)
    floor = float(np.median(mean[:ahead]))
    peak = int(np.argmax(mean[ahead:]))
    return {"epochs": len(rows), "envelope_db": [round(db(float(p)), 1) for p in mean],
            "stand_out_db": round(db(float(mean[ahead + peak])) - db(floor), 1),
            "peak_after_ms": round(peak * window * 1000)}


def ebur128(raw: np.ndarray) -> dict:
    result = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-f", "s16le", "-ar", str(RATE), "-ac", "2",
                             "-i", "-", "-af", "ebur128=peak=true", "-f", "null", "-"],
                            input=raw.astype("<i2").tobytes(), capture_output=True, check=True).stderr.decode()
    summary = result[result.rfind("Summary:"):]

    def number(pattern: str) -> float:
        found = re.search(pattern, summary)
        if not found:
            return float("nan")
        return -math.inf if "inf" in found[1] else float(found[1])
    return {"lufs": number(r"I:\s+([-\d.inf]+) LUFS"), "lra": number(r"LRA:\s+([-\d.inf]+) LU"),
            "true_peak": number(r"Peak:\s+([-\d.inf]+) dBFS")}


def spectrogram(raw: np.ndarray, picture: pathlib.Path) -> None:
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "s16le", "-ar", str(RATE),
                    "-ac", "2", "-i", "-", "-lavfi",
                    "showspectrumpic=s=1500x560:mode=separate:legend=1:scale=log:fscale=log:drange=100",
                    str(picture)], input=raw.astype("<i2").tobytes(), check=True)


def take_levels() -> dict[str, dict]:
    """Mean-square level (dBFS, mono mix) and length of every take of the library."""
    cache = ROOT / "build-probe" / "audio" / "take-levels.json"
    manifest = json.loads((ROOT / "assets" / "audio" / "sounds.json").read_text())
    stamp = max((ROOT / "assets" / "audio" / f).stat().st_mtime
                for entry in manifest["sounds"] for f in entry["files"])
    if cache.exists() and cache.stat().st_mtime > stamp:
        return json.loads(cache.read_text())
    levels = {}
    for entry in manifest["sounds"]:
        for file in entry["files"]:
            pcm = subprocess.run(["ffmpeg", "-v", "error", "-i", str(ROOT / "assets" / "audio" / file),
                                  "-f", "f32le", "-"], capture_output=True, check=True).stdout
            channels = 2 if entry["name"] in ("park_day", "wind_trees") else 1
            data = np.frombuffer(pcm, dtype="<f4").astype(np.float64)
            levels[file] = {"name": entry["name"], "kind": entry["kind"], "seconds": len(data) / channels / RATE,
                            "power_db": db(float(np.mean(data ** 2)))}
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text(json.dumps(levels, indent=1))
    return levels


CATEGORY = {"animals": 1.0}


def measure(raw_path: pathlib.Path, pictures: bool) -> dict:
    raw = np.fromfile(raw_path, dtype="<i2")
    raw = raw[: len(raw) // 2 * 2].reshape(-1, 2)
    start = first_sound(raw)
    sound = raw[start:]
    samples = sound.astype(np.float64) / FULL_SCALE
    out: dict = {"name": raw_path.stem, "load_seconds": start / RATE, "seconds": len(sound) / RATE}
    if len(sound) < RATE:
        out["error"] = "less than a second of sound"
        return out
    out.update(ebur128(sound))
    out["sample_peak"] = db(float(np.max(np.abs(samples))) ** 2)
    out["clipped"] = clipped(sound)
    channel = np.mean(samples ** 2, axis=0)
    out["left_db"], out["right_db"] = db(float(channel[0])), db(float(channel[1]))
    out["right_minus_left"] = out["right_db"] - out["left_db"]
    out["silences"] = silences(samples, RATE)
    steps = level_steps(samples, RATE)
    out["level_steps"] = [(round(t, 2), round(jump, 1)) for t, jump in steps][:40]
    out["level_steps_count"] = len(steps)
    # The first tenth of a second against the second second: a bed that starts at full level.
    out["start_step_db"] = db(float(np.mean(samples[: RATE // 10] ** 2))) - db(float(np.mean(samples[RATE:2 * RATE] ** 2)))
    second = np.max(window_powers(samples, RATE), axis=1)
    out["per_second_db"] = [round(db(float(p)), 1) for p in second]
    if pictures:
        spectrogram(sound, raw_path.with_suffix(".png"))

    log_path = raw_path.with_suffix(".log")
    if not log_path.exists():
        return out
    events = parse_log(log_path.read_text())
    beds = [e for e in events if e["kind"] == "beds"]
    if not beds:
        return out
    # The capture's clock: its first sound is the beds' start.
    offset = beds[0]["t"]
    states = [e for e in events if e["kind"] == "state"]
    calls = [e for e in events if e["kind"] == "call"]
    footsteps = [e for e in events if e["kind"] == "step"]
    minutes = out["seconds"] / 60.0
    levels = take_levels()
    out["voices_max"] = int(max((e.get("voices", 0) for e in states), default=0))
    out["audible_max"] = int(max((e.get("audible", 0) for e in states), default=0))
    out["hour"] = states[0].get("hour") if states else None
    out["followers"] = follower_figures(states)
    for bed in ("air", "leaves", "daylight", "wind"):
        values = [e[bed] for e in states if isinstance(e.get(bed), float)]
        if values:
            out[bed] = {"first": round(values[0], 3), "mean": round(float(np.mean(values)), 3),
                        "last": round(values[-1], 3)}
    groups = [max(int(part) for part in e["groups"].split(",")) for e in states if isinstance(e.get("groups"), str)]
    out["group_max"] = max(groups, default=None)
    # What the animals did against what was heard of it.
    out["vocalize_arrivals"] = sum(1 for e in events if e["kind"] == "vocalize")
    out["lively_onsets"] = sum(1 for e in events if e["kind"] == "lively")
    reasons: dict[str, int] = {}
    for e in events:
        if e["kind"] == "nocall":
            reason = next((b for b in e["bare"] if b in ("out-of-reach", "none-in-library")), "other")
            cue = e["bare"][1] if len(e["bare"]) > 1 else "?"
            reasons[f"{cue}: {reason}"] = reasons.get(f"{cue}: {reason}", 0) + 1
    out["nocall"] = reasons

    call_rows = []
    for call in calls:
        name, take, why = (call["bare"] + ["", "", ""])[:3]
        length = levels.get(take, {}).get("seconds", 2.0) / 2.0 ** call.get("pitch", 0.0)
        call_rows.append({"t": call["t"] - offset, "name": name, "take": take, "why": why, "seconds": length,
                          "away": call.get("away"), "gain": call.get("gain"), "spatial": call.get("spatial"),
                          "pan": call.get("pan"), "voice": call.get("voice"), "level": call.get("level", 1.0)})
    heard = [row for row in call_rows if row["voice"]]
    out["calls"] = len(heard)
    out["calls_unheard"] = len(call_rows) - len(heard)
    out["calls_per_minute"] = len(heard) / minutes if minutes else 0.0
    out["calls_by_why"] = {why: sum(1 for r in heard if r["why"] == why) for why in sorted({r["why"] for r in heard})}
    by_name: dict[str, list[str]] = {}
    for row in heard:
        by_name.setdefault(row["name"], []).append(row["take"])
    out["calls_by_name"] = {name: len(takes) for name, takes in sorted(by_name.items())}
    # A sound the library has one take of cannot but repeat it: counted apart.
    several = {name for name in by_name if sum(1 for level in levels.values() if level["name"] == name) > 1}
    out["same_take_in_a_row"] = sum(repeats(takes) for name, takes in by_name.items() if name in several)
    out["only_take_again"] = sum(repeats(takes) for name, takes in by_name.items() if name not in several)
    out["calls_at_once_max"] = overlap_count([(r["t"], r["t"] + r["seconds"]) for r in heard])
    out["call_pan_abs_max"] = round(max((abs(r["pan"]) for r in heard), default=0.0), 3)
    out["pan_abs_max"] = round(max([out["call_pan_abs_max"]] +
                                   [f["pan_abs_max"] for f in out["followers"].values()]), 3)

    # Each call against what was asked for: its level over the bed, and left against right.
    master = states[0].get("master", 0.8) if states else 0.8
    tenth = RATE // 20
    powers = window_powers(samples, tenth)  # 50 ms windows
    for row in heard:
        begin, end = int(row["t"] * 20), int((row["t"] + row["seconds"]) * 20)
        if begin < 30 or end >= len(powers) or row["take"] not in levels:
            continue
        others = [o for o in heard if o is not row and o["t"] < row["t"] + row["seconds"]
                  and o["t"] + o["seconds"] > row["t"] - 1.5]
        if others:
            continue
        before = np.median(powers[begin - 30:begin], axis=0)
        during = np.mean(powers[begin:end], axis=0)
        net = np.maximum(during - before, 1e-12)
        amplitude = row["gain"] * row["spatial"] * CATEGORY["animals"] * master * row["level"]
        if amplitude <= 0.0:
            continue
        left, right = pan_gains(row["pan"])
        expected = [levels[row["take"]]["power_db"] + 20.0 * math.log10(max(amplitude * g, 1e-6))
                    for g in (left, right)]
        row["over_bed_db"] = round(db(float(np.sum(during))) - db(float(np.sum(before))), 1)
        loud = 0 if expected[0] >= expected[1] else 1
        row["measured_db"] = round(db(float(net[loud])), 1)
        row["expected_db"] = round(expected[loud], 1)
        row["measured_r_minus_l"] = round(db(float(net[1])) - db(float(net[0])), 1)
        row["expected_r_minus_l"] = round(expected[1] - expected[0], 1)
    out["call_rows"] = [{k: (round(v, 3) if isinstance(v, float) else v) for k, v in row.items()} for row in call_rows]
    clear = [r for r in heard if r.get("over_bed_db", 0.0) >= 3.0]
    if clear:
        errors = [r["measured_db"] - r["expected_db"] for r in clear]
        out["call_level_error_db"] = {"calls": len(clear), "median": round(float(np.median(errors)), 1),
                                      "worst": round(float(max(errors, key=abs)), 1)}

    if footsteps:
        times = [e["t"] - offset for e in footsteps]
        gaps = np.diff(times)
        surfaces = sorted({e["bare"][0].split("/")[-1].rsplit("_", 1)[0] for e in footsteps if e["bare"]})
        takes = [e["bare"][0] for e in footsteps if e["bare"]]
        out["steps"] = {"count": len(times), "surfaces": surfaces, "same_take_in_a_row": repeats(takes),
                        "interval_median": round(float(np.median(gaps)), 3) if len(gaps) else None,
                        "interval_min": round(float(np.min(gaps)), 3) if len(gaps) else None,
                        "interval_max": round(float(np.max(gaps)), 3) if len(gaps) else None}
        # Does a step stand out of the bed? One step is lost in the bed's own movement, the mean
        # of all of them around their logged times is not.
        envelope = step_envelope(samples, times, RATE)
        if envelope:
            out["steps"]["envelope_db"] = envelope["envelope_db"]
            out["steps"]["stand_out_db"] = envelope["stand_out_db"]
            out["steps"]["peak_after_ms"] = envelope["peak_after_ms"]
        # A heel on paving rises faster than any fade: such a level step is the step itself.
        out["level_steps_at_footsteps"] = following([t for t, _ in steps], times)
        pitches = [e["pitch"] for e in footsteps if "pitch" in e]
        out["steps"]["pitch_abs_max"] = round(max(map(abs, pitches)), 4) if pitches else None
        out["steps"]["per_surface"] = {surface: sum(1 for take in takes if f"/{surface}_" in take or
                                                    take.endswith(f"/{surface}.ogg")) for surface in surfaces}
    return out


def table(rows: list[dict]) -> str:
    def cell(value, digits=1):
        if value is None:
            return "-"
        if isinstance(value, float):
            return "-inf" if value == -math.inf else f"{value:.{digits}f}"
        return str(value)
    lines = ["| Capture | s | LUFS | LRA | True peak dBFS | Clipped | R-L dB | Silences | Level steps | Start step dB "
             "| Voices max | Calls | per min | by cue | Vocalize arrivals | Same take twice | Only take again "
             "| Calls at once | Max abs pan | Call level error dB (n) |",
             "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---:|---|"]
    for row in rows:
        if "error" in row:
            lines.append(f"| {row['name']} | {row['seconds']:.0f} | {row['error']} |")
            continue
        error = row.get("call_level_error_db")
        why = ", ".join(f"{k} {v}" for k, v in row.get("calls_by_why", {}).items()) or "-"
        lines.append("| " + " | ".join([
            row["name"], cell(row["seconds"], 0), cell(row["lufs"]), cell(row["lra"]), cell(row["true_peak"]),
            cell(row["clipped"]), cell(row["right_minus_left"]), cell(len(row["silences"])),
            cell(row["level_steps_count"]), cell(row["start_step_db"]), cell(row.get("voices_max")),
            cell(row.get("calls")), cell(row.get("calls_per_minute")), why, cell(row.get("vocalize_arrivals")),
            cell(row.get("same_take_in_a_row")), cell(row.get("only_take_again")),
            cell(row.get("calls_at_once_max")),
            cell(row.get("pan_abs_max"), 2),
            f"{error['median']:+.1f} median, {error['worst']:+.1f} worst ({error['calls']})" if error else "-",
        ]) + " |")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--set", required=True)
    parser.add_argument("--only")
    parser.add_argument("--no-pictures", action="store_true")
    args = parser.parse_args()
    directory = ROOT / "build-probe" / "audio" / args.set
    wanted = set(args.only.split(",")) if args.only else None
    rows = []
    for raw_path in sorted(directory.glob("*.raw")):
        if wanted and raw_path.stem not in wanted:
            continue
        row = measure(raw_path, not args.no_pictures)
        raw_path.with_suffix(".json").write_text(json.dumps(row, indent=1, default=str) + "\n")
        rows.append(row)
        print(raw_path.stem, "measured", flush=True)
    if not wanted:
        (directory / "SUMMARY.md").write_text(table(rows))
    print(table(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
