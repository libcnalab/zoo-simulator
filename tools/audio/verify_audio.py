# SPDX-License-Identifier: MIT
"""Check every file, decoded peak, 100 ms envelope, loudness and cyclic boundary."""
import json
import re

from build_audio import CACHE, DEST, HERE, ROOT, ffmpeg, peak, probe


def rms_level(path, start, end, repeat=False):
    args = ['-stream_loop', '1'] if repeat else []
    args += ['-i', str(path), '-af',
             f'atrim=start={start:.9f}:end={end:.9f},astats=metadata=0:reset=0',
             '-f', 'null', '-']
    result = ffmpeg(args).stderr.decode()
    values = re.findall(r'RMS level dB: ([-\d.]+)', result)
    return float(values[-1])


def envelope(path):
    result = ffmpeg(['-i', str(path), '-af',
                     'asetnsamples=n=4410:p=0,astats=metadata=1:reset=1,'
                     'ametadata=print:key=lavfi.astats.Overall.RMS_level',
                     '-f', 'null', '-']).stderr.decode()
    return [float(value) for value in re.findall(r'lavfi.astats.Overall.RMS_level=([-\d.inf]+)', result)]


def integrated(path):
    result = ffmpeg(['-i', str(path), '-af', 'loudnorm=I=-23:TP=-1:LRA=7:print_format=json',
                     '-f', 'null', '-']).stderr.decode()
    data = json.loads(result[result.rfind('{'):result.rfind('}') + 1])
    return float(data['input_i'])


def main():
    sources = json.loads((HERE / 'sources.json').read_text())['files']
    for entry in sources:
        assert entry['licence'] in ('Public domain', 'CC0', 'CC BY 3.0', 'CC BY 4.0',
                                    'CC BY-SA 3.0', 'CC BY-SA 4.0')
        assert entry['author'] and entry['source_page'] and entry['source_sha256']
        if entry['licence'].startswith('CC BY-SA'):
            assert entry.get('sharealike_reason')
    manifest = json.loads((DEST / 'sounds.json').read_text())
    assert manifest['version'] == 1
    named = [file for group in manifest['sounds'] for file in group['files']]
    actual = sorted(str(path.relative_to(DEST)) for path in DEST.rglob('*.ogg'))
    assert len(named) == len(set(named))
    assert sorted(named) == actual == sorted(entry['file'] for entry in sources)
    assert len({group['name'] for group in manifest['sounds']}) == len(manifest['sounds'])
    for group in manifest['sounds']:
        assert group['kind'] in ('call', 'ambience', 'footstep')
        assert len(group['files']) == len(group['seconds'])
        for file, duration in zip(group['files'], group['seconds']):
            assert abs(duration - float(probe(DEST / file)['format']['duration'])) < 0.000001
        if group['kind'] == 'call':
            species = json.loads((ROOT / 'data/species' / (group['species'] + '.json')).read_text())
            assert group['name'] in species['sounds']
    rows = []
    envelopes = {}
    checks = 0
    for entry in sources:
        path = DEST / entry['file']
        data = probe(path)
        stream = data['streams'][0]
        duration = float(data['format']['duration'])
        measured_peak = peak(path)
        assert stream['codec_name'] == 'vorbis', path
        assert stream['sample_rate'] == '44100', path
        assert stream['channels'] == entry['channels'], path
        assert measured_peak <= -1.0, (path, measured_peak)
        minimum, maximum = (1, 10 if entry['name'] in ('wolf_howl', 'elephant_rumble') else 6)
        if entry['kind'] == 'footstep':
            minimum, maximum = 0.15, 0.45
        if entry['kind'] == 'ambience':
            minimum, maximum = {'park_day': (30, 45), 'crowd': (20, 30),
                                'water': (10, 20), 'wind_trees': (15, 30)}[entry['name']]
        assert minimum - 0.001 <= duration <= maximum + 0.001, (path, duration)
        checks += 5
        if entry['kind'] != 'ambience':
            assert abs(measured_peak + 3) <= 0.15, (path, measured_peak)
            checks += 1
        levels = envelope(path)
        assert levels and max(levels) > -45, path
        checks += 1
        envelopes[entry['file']] = levels
        joint = '—'
        loudness = '—'
        if entry['kind'] == 'ambience':
            loudness = integrated(path)
            assert abs(loudness + 23) <= 1, (path, loudness)
            middle = rms_level(path, duration - 0.05, duration + 0.05, True)
            rest = rms_level(path, 0.1, duration - 0.1)
            before = rms_level(path, duration - 0.15, duration - 0.05)
            after = rms_level(path, 0.05, 0.15)
            delta = max(abs(middle - rest), abs(middle - before), abs(middle - after))
            joint = f'{delta:.3f} dB ({middle:.2f} joint / {rest:.2f} rest)'
            assert delta <= 3, (path, joint)
            checks += 2
        rows.append([entry['file'], f'{duration:.6f}', str(stream['channels']), f'{measured_peak:.1f}',
                     str(loudness), joint, entry['title'], entry['licence']])
        print(entry['file'], 'OK', flush=True)
    total = sum(path.stat().st_size for path in DEST.rglob('*') if path.is_file())
    assert total < 12_000_000, total
    (CACHE / 'envelopes.json').write_text(json.dumps(envelopes, indent=2) + '\n')
    report = ('# Audio verification report\n\n'
              'All files are Vorbis, 44100 Hz. Peaks are decoded sample peaks from FFmpeg `volumedetect`. '
              'Loop checks decode two copies with `-stream_loop 1` and use `astats` RMS for the centered '
              '100 ms joint, its adjacent 100 ms windows, and the rest of the loop. The reported delta '
              'is the largest of those three comparisons. Every delta must be at most 3 dB. '
              'Every file also has a 100 ms `astats` RMS envelope in '
              '`build-probe/audio-src/envelopes.json`. Measurements cannot establish absence of intelligible '
              'speech or confirm call type; see `RESEARCH.md` for listening-review uncertainties.\n\n'
              '| File | Seconds | Channels | Peak dBFS | LUFS | Joint maximum delta | Source | Licence |\n'
              '|---|---:|---:|---:|---:|---|---|---|\n')
    report += '\n'.join('| ' + ' | '.join(row) + ' |' for row in rows)
    report += f'\n\n{len(rows)} files; {checks} per-file assertions passed; 0 failures. '
    report += f'Manifest, species contracts and inventory checks passed. Total audio directory: {total:,} bytes.\n'
    (HERE / 'VERIFICATION.md').write_text(report)
    print(f'{len(rows)} files; {checks} assertions passed; 0 failures; {total} bytes')


if __name__ == '__main__':
    main()
