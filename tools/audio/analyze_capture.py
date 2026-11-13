#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Figures of what the game sent to the audio device.

Record a run with SDL's file driver (no sound card or listener needed, nothing plays):

    cd build
    SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=out.raw \
        timeout 40 ./zoo-simulator --stand habitat:lion --frames 1000000
    python3 ../tools/audio/analyze_capture.py out.raw [--wav out.wav]

The capture is interleaved signed 16 bit little endian, stereo, 44100 Hz (the game's mixer
format; SDL prints it when the driver starts). This measures level, balance, clipping and
silence. It does not judge whether a lion sounds like a lion: only a listener can.
"""
import argparse
import math
import sys
import wave

import numpy as np

RATE = 44100


def db(value: float) -> float:
    return 20.0 * math.log10(value) if value > 1e-9 else -180.0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("capture")
    parser.add_argument("--wav", help="also write the capture as a WAV file for listening")
    parser.add_argument("--window", type=float, default=1.0, help="seconds per row of the table")
    args = parser.parse_args()

    raw = np.fromfile(args.capture, dtype="<i2")
    raw = raw[: len(raw) // 2 * 2].reshape(-1, 2)
    if len(raw) == 0:
        print("empty capture")
        return 1
    if args.wav:
        with wave.open(args.wav, "wb") as out:
            out.setnchannels(2)
            out.setsampwidth(2)
            out.setframerate(RATE)
            out.writeframes(raw.tobytes())
    samples = raw.astype(np.float64) / 32768.0
    seconds = len(samples) / RATE
    peak = float(np.max(np.abs(samples)))
    clipped = int(np.sum(np.abs(raw.astype(np.int32)) >= 32767))
    rms = np.sqrt(np.mean(samples**2, axis=0))
    print(f"length {seconds:.1f} s, peak {db(peak):.1f} dBFS, samples at full scale {clipped}")
    print(f"level  left {db(rms[0]):.1f} dBFS, right {db(rms[1]):.1f} dBFS, "
          f"right minus left {db(rms[1]) - db(rms[0]):+.1f} dB")

    # Where the energy is: a hiss-only or rumble-only mix shows here.
    mono = samples.mean(axis=1)
    spectrum = np.abs(np.fft.rfft(mono * np.hanning(len(mono)))) ** 2
    frequencies = np.fft.rfftfreq(len(mono), 1.0 / RATE)
    total = float(spectrum.sum()) or 1.0
    bands = [(20, 200), (200, 1000), (1000, 4000), (4000, 20000)]
    shares = [float(spectrum[(frequencies >= low) & (frequencies < high)].sum()) / total
              for low, high in bands]
    print("energy " + ", ".join(f"{low}-{high} Hz {share * 100:.0f} %"
                                for (low, high), share in zip(bands, shares)))

    window = max(1, int(args.window * RATE))
    quiet = 0
    print(" time   left  right  (dBFS, RMS per window)")
    for start in range(0, len(samples) - window + 1, window):
        part = samples[start:start + window]
        left, right = (db(float(v)) for v in np.sqrt(np.mean(part**2, axis=0)))
        quiet += 1 if max(left, right) < -60.0 else 0
        bar = "#" * max(0, int((max(left, right) + 60.0) / 2.0))
        print(f"{start / RATE:5.1f}  {left:6.1f} {right:6.1f}  {bar}")
    print(f"windows below -60 dBFS: {quiet}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
