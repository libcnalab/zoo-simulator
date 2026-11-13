# SPDX-License-Identifier: MIT
"""Measure decoded audio; write AUDIT.md and proposed (unapplied) GAINS.md."""
import json
import math
import re

import numpy as np

from build_audio import CACHE, DEST, HERE, ffmpeg, probe


def db(power):
    return 10 * math.log10(power) if power > 0 else -math.inf


def window_powers(samples, frames):
    """Non-overlapping windows, including a correctly weighted partial last window."""
    return np.array([float(np.mean(samples[start:start + frames] ** 2))
                     for start in range(0, len(samples), frames)])


def quiet_floor(samples, rate):
    powers = window_powers(samples, round(rate * 0.05))
    count = max(1, math.ceil(len(powers) * 0.1))
    return db(float(np.mean(np.sort(powers)[:count])))


def edge_silence(samples, rate):
    # One-millisecond RMS windows avoid counting ordinary waveform zero crossings.
    frames = max(1, round(rate * 0.001))
    powers = window_powers(samples, frames)
    active = np.flatnonzero(powers >= 10 ** (-50 / 10))
    if len(active) == 0:
        return len(samples) / rate * 1000, len(samples) / rate * 1000
    head = int(active[0]) * frames / rate * 1000
    tail = max(0, len(samples) - (int(active[-1]) + 1) * frames) / rate * 1000
    return head, tail


def band_shares(samples, rate):
    # Sum channel powers, not waveforms: opposite-polarity stereo must not cancel.
    spectrum = np.fft.rfft(samples, axis=0)
    powers = np.sum(np.abs(spectrum) ** 2, axis=1)
    weights = np.full(len(powers), 2.0)
    weights[0] = 1.0
    if len(samples) % 2 == 0:
        weights[-1] = 1.0
    powers *= weights
    total = float(np.sum(powers))
    if total == 0:
        return 0.0, 0.0
    frequencies = np.fft.rfftfreq(len(samples), 1 / rate)
    return (100 * float(np.sum(powers[frequencies < 120])) / total,
            100 * float(np.sum(powers[frequencies > 6000])) / total)


def loop_delta(samples, rate):
    half = round(rate * 0.05)
    seam = np.concatenate((samples[-half:], samples[:half]))
    level = db(float(np.mean(seam ** 2)))
    comparisons = [samples[2 * half:-2 * half], samples[-3 * half:-half], samples[half:3 * half]]
    return max(abs(level - db(float(np.mean(part ** 2)))) for part in comparisons)


def loudness(path, seconds):
    # EBU integration needs 400 ms. Padding is disclosed and used only for short steps/calls.
    filters = 'apad=whole_dur=0.4,' if seconds < 0.4 else ''
    result = ffmpeg(['-i', str(path), '-af', filters + 'ebur128=peak=true',
                     '-f', 'null', '-']).stderr.decode()
    summary = result[result.rfind('Summary:'):]
    integrated = re.search(r'I:\s+([-\d.inf]+) LUFS', summary)
    lra = re.search(r'LRA:\s+([-\d.inf]+) LU', summary)
    true_peak = re.search(r'Peak:\s+([-\d.inf]+) dBFS', summary)
    if not all((integrated, lra, true_peak)):
        raise ValueError('Missing ebur128 summary: ' + str(path))
    return float(integrated[1]), float(lra[1]), float(true_peak[1])


def measure(entry):
    path = DEST / entry['file']
    stream = probe(path)['streams'][0]
    rate = int(stream['sample_rate'])
    channels = int(stream['channels'])
    raw = ffmpeg(['-v', 'error', '-i', str(path), '-map', '0:a:0', '-c:a', 'pcm_f32le',
                  '-f', 'f32le', '-']).stdout
    samples = np.frombuffer(raw, dtype='<f4').astype(np.float64).reshape(-1, channels)
    seconds = len(samples) / rate
    integrated, lra, true_peak = loudness(path, seconds)
    head, tail = edge_silence(samples, rate)
    low, high = band_shares(samples, rate)
    return {'file': entry['file'], 'name': entry['name'], 'kind': entry['kind'],
            'species': entry.get('species'), 'seconds': seconds, 'true_peak': true_peak,
            'lufs': integrated, 'lra': lra, 'noise_floor': quiet_floor(samples, rate),
            'below_120_percent': low, 'above_6000_percent': high,
            'head_ms': head, 'tail_ms': tail,
            'seam_delta': loop_delta(samples, rate) if entry['kind'] == 'ambience' else None,
            'padded_loudness': seconds < 0.4}


def target(name):
    if name in ('lion_roar', 'elephant_trumpet', 'wolf_howl'):
        return -23.0, 'loud call'
    if name in ('meerkat_chirp', 'penguin_bray'):
        return -35.0, 'small/social call'
    if 'growl' in name or 'grunt' in name or 'huff' in name or 'snort' in name:
        return -31.0, '8 LU below loud calls'
    if name == 'elephant_rumble':
        return -27.0, 'low rumble, below the loud-call reference'
    return -29.0, 'other call, 6 LU below loud calls'


def write_gains(rows):
    groups = {}
    for row in rows:
        if row['kind'] == 'call':
            groups.setdefault(row['name'], []).append(row)
    lines = ['# Proposed call gains', '',
             'Measurements only; nobody listened. The files are not changed. The game applies the mix written to '
             '`assets/audio/mix.json` (per take: this table\'s target raised by 9 LU, attenuation only). '
             'One gain per call name matches the current manifest grouping. Measured LUFS is the '
             'equal-take energy mean of each file’s gated integrated loudness, not a concatenated-program '
             'measurement. The range exposes variation that a single per-name gain cannot fix.', '',
             'Targets: lion roar, elephant trumpet and wolf howl −23 LUFS; growls/grunts/huffs/snorts '
             '−31 LUFS (8 LU lower); meerkat chirp and penguin bray −35 LUFS. These are mix choices, '
             'not measured animal sound-pressure levels. The elephant trumpet is the labelled Asian-elephant substitute. '
             'Positive gain is capped to leave at least 1 dB of measured true-peak headroom.', '',
             '| Name | Measured mean LUFS | Take range LUFS | Target LUFS | Proposed gain dB | Basis |',
             '|---|---:|---|---:|---:|---|']
    for name, takes in sorted(groups.items()):
        average = db(sum(10 ** (row['lufs'] / 10) for row in takes) / len(takes))
        desired, reason = target(name)
        gain = min(desired - average, -1 - max(row['true_peak'] for row in takes))
        limits = f"{min(row['lufs'] for row in takes):.1f} to {max(row['lufs'] for row in takes):.1f}"
        lines.append(f'| `{name}` | {average:.2f} | {limits} | {desired:.1f} | {gain:+.2f} | {reason} |')
    lines += ['', 'A listening review is still owed, particularly for the preserved contact-call penguin '
              'variant; until then the mix rests on loudness measurements alone.']
    (HERE / 'GAINS.md').write_text('\n'.join(lines) + '\n')


MIX_OFFSET_LU = 9.0


def write_mix(rows):
    """The adopted mix: one gain per call file, read by the game (src/game/Soundscape.cpp).

    Each take is brought to its call's target loudness (the table in GAINS.md) raised by
    MIX_OFFSET_LU, so that a roar at its reference distance stands about 14 LU above the ambience
    beds (-23 LUFS at 0.55). Gains only attenuate: a take quieter than its target stays as it is.
    """
    gains = {}
    for row in rows:
        if row['kind'] != 'call':
            continue
        desired, _ = target(row['name'])
        gains[row['file']] = round(min(0.0, desired + MIX_OFFSET_LU - row['lufs']), 1)
    record = {
        'note': 'Written by tools/audio/audit_audio.py from measured loudness; nobody listened. '
                'gains_db: decibels applied to a call take when it is played (0 = as recorded).',
        'offset_lu': MIX_OFFSET_LU,
        'gains_db': dict(sorted(gains.items())),
    }
    (HERE.parent.parent / 'assets' / 'audio' / 'mix.json').write_text(json.dumps(record, indent=2) + '\n')


def write_audit(rows):
    lines = ['# Audio audit — Round 2', '',
             'These are measurements, and nobody has listened to the files. They do not certify call '
             'identity, absence of speech, perceived background quality or an appropriate final mix.', '',
             'Decoded float PCM at the native 44100 Hz is measured without gain changes. True peak, '
             'integrated loudness and loudness range come from FFmpeg `ebur128=peak=true` '
             '(true peak is referenced to full scale; commonly written dBTP). LRA is often zero for '
             'short clips and is not a useful dynamic-range claim there. Clips shorter than 400 ms '
             'are zero-padded to 400 ms for loudness only and marked †; this is an explicitly padded '
             'measurement, not native-duration integrated loudness.', '',
             'Noise floor is 10 log10 of the arithmetic mean power of the quietest ceil(10%) of '
             'non-overlapping 50 ms windows, including the final partial window. Each window averages '
             'all its samples/channels; the selected windows have equal weight. This is a quiet-window '
             'proxy that includes call energy and fades, not an isolated noise estimate. A gap at most '
             '25 dB flags review; it does not prove hiss. Frequency shares use the whole-file rectangular '
             'real FFT, including DC, correct one-sided power weights, and summed channel energies. '
             'Cutoffs are strictly below 120 Hz and above 6 kHz. Head/tail silence uses consecutive '
             '1 ms RMS windows below −50 dBFS (44 frames, approximately 0.998 ms). Loop seam delta '
             'compares the centered 100 ms boundary RMS with the two adjacent 100 ms windows and '
             'the loop interior, reporting the largest absolute difference.', '',
             '| Ogg | Duration s | True peak dBFS | LUFS | LRA LU | Quiet floor dBFS | <120 Hz % | >6 kHz % | Head ms | Tail ms | Seam Δ dB |',
             '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
    for row in rows:
        seam = '—' if row['seam_delta'] is None else f"{row['seam_delta']:.3f}"
        dagger = '†' if row['padded_loudness'] else ''
        lines.append(f"| `{row['file']}` | {row['seconds']:.6f} | {row['true_peak']:.1f} | "
                     f"{row['lufs']:.1f}{dagger} | {row['lra']:.1f} | {row['noise_floor']:.2f} | "
                     f"{row['below_120_percent']:.2f} | {row['above_6000_percent']:.2f} | "
                     f"{row['head_ms']:.2f} | {row['tail_ms']:.2f} | {seam} |")
    lines += ['', '## Head/tail silence over 80 ms', '']
    silence = [row for row in rows if max(row['head_ms'], row['tail_ms']) > 80]
    for row in silence:
        lines.append(f"- `{row['file']}`: head {row['head_ms']:.2f} ms, tail {row['tail_ms']:.2f} ms.")
    if not silence:
        lines.append('None after the source-cut corrections recorded below.')
    lines += ['', '## Quiet floor within 25 dB of true peak', '',
              '| File | Peak − quiet floor dB |', '|---|---:|']
    noisy = [row for row in rows if row['true_peak'] - row['noise_floor'] <= 25]
    for row in noisy:
        lines.append(f"| `{row['file']}` | {row['true_peak'] - row['noise_floor']:.2f} |")
    lines += ['', '## Species loudness spread', '',
              'Range across every delivered take and call name of a species; flags are strictly over 4 LU. '
              'Peak normalization does not equalize this measure. See [GAINS.md](GAINS.md) for per-name proposals.', '',
              '| Species | Minimum LUFS | Maximum LUFS | Spread LU | Over 4 LU |', '|---|---:|---:|---:|---|']
    species = {}
    for row in rows:
        if row['species']:
            species.setdefault(row['species'], []).append(row['lufs'])
    for name, values in sorted(species.items()):
        spread = max(values) - min(values)
        lines.append(f'| {name} | {min(values):.1f} | {max(values):.1f} | {spread:.1f} | '
                     f'{"yes" if spread > 4 else "no"} |')
    decisions = HERE / 'audit-decisions.md'
    if decisions.exists():
        lines += ['', decisions.read_text().rstrip()]
    lines += ['', f'{len(rows)} files measured; {len(silence)} edge-silence flags; '
              f'{len(noisy)} quiet-floor review flags.']
    (HERE / 'AUDIT.md').write_text('\n'.join(lines) + '\n')
    print(lines[-1])


def main():
    entries = json.loads((HERE / 'sources.json').read_text())['files']
    rows = []
    for entry in sorted(entries, key=lambda item: item['file']):
        rows.append(measure(entry))
        print(entry['file'], 'measured', flush=True)
    CACHE.mkdir(parents=True, exist_ok=True)
    # Digital silence has a legitimate -infinite dB floor; keep JSON standards-compliant.
    serializable = [{key: str(value) if isinstance(value, float) and not math.isfinite(value) else value
                     for key, value in row.items()} for row in rows]
    (CACHE / 'audit-measurements.json').write_text(json.dumps(serializable, indent=2, allow_nan=False) + '\n')
    write_audit(rows)
    write_gains(rows)
    write_mix(rows)


if __name__ == '__main__':
    main()
