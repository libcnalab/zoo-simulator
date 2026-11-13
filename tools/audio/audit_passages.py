# SPDX-License-Identifier: MIT
"""Compare unused/shorter passages for quiet-floor flags without changing delivered assets."""
import json

import numpy as np

from audit_audio import db, quiet_floor
from build_audio import CACHE, HERE, ffmpeg, prepare


def main():
    entries = json.loads((HERE / 'sources.json').read_text())['files']
    candidates = [
        ('animals/elephant_rumble.ogg', 1.4, 5.1, 'Shorter sustained rumble; removes natural onset/decay.'),
        ('animals/camel_grunt.ogg', 0.1, 2.35, 'Shorter first call; loses its natural ending.'),
        ('animals/flamingo_call_2.ogg', 32.0, 35.6, 'Unused late flock passage; same continuous background.'),
        ('animals/giraffe_grunt.ogg', 0.65, 1.25, 'Single short grunt core; discards natural onset/decay.'),
        ('animals/wolf_howl.ogg', 1.5, 7.5, 'Shorter early chorus; overlapping tonal calls remain continuous.'),
        ('animals/wolf_howl_2.ogg', 10.0, 17.0, 'Shorter middle chorus; no isolated quiet background.'),
        ('animals/wolf_howl_3.ogg', 20.0, 27.0, 'Shorter late chorus; no isolated quiet background.'),
        ('ambience/water.ogg', 5.0, 23.0, 'Later water passage; water noise is the intended loop content.'),
    ]
    rows = []
    for filename, start, end, decision in candidates:
        entry = next(item for item in entries if item['file'] == filename).copy()
        measurements = []
        for cut_start, cut_end in [(entry['cut_start'], entry['cut_end']), (start, end)]:
            entry.update(cut_start=cut_start, cut_end=cut_end)
            work = CACHE / 'round2-passage-comparison.wav'
            prepare(entry, CACHE / entry['cache_file'], work)
            raw = ffmpeg(['-v', 'error', '-i', str(work), '-f', 'f32le', '-']).stdout
            samples = np.frombuffer(raw, dtype='<f4').astype(np.float64).reshape(-1, entry['channels'])
            sample_peak = db(float(np.max(samples ** 2)))
            measurements.append(sample_peak - quiet_floor(samples, 44100))
        row = {'file': filename, 'candidate_start': start, 'candidate_end': end,
               'current_sample_peak_floor_gap': measurements[0],
               'candidate_sample_peak_floor_gap': measurements[1], 'decision': decision}
        rows.append(row)
        print(filename, f'{measurements[0]:.2f} -> {measurements[1]:.2f} dB', decision)
    (CACHE / 'round2-passage-comparisons.json').write_text(json.dumps(rows, indent=2) + '\n')


if __name__ == '__main__':
    main()
