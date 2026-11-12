# SPDX-License-Identifier: MIT
"""Rebuild the checked, attributed Vorbis library using Python's stdlib and FFmpeg."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
CACHE = ROOT / 'build-probe/audio-src'
DEST = ROOT / 'assets/audio'
USER_AGENT = 'zoo-simulator-audio-fetch/1.0'


def run(args):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def ffmpeg(args):
    return run(['ffmpeg', '-hide_banner', '-nostdin', '-threads', '1',
                '-filter_threads', '1', '-filter_complex_threads', '1', '-y'] + args)


def probe(path):
    return json.loads(run(['ffprobe', '-v', 'error', '-show_streams', '-show_format',
                           '-of', 'json', str(path)]).stdout)


def peak(path, filters=None):
    args = ['-i', str(path), '-af', (filters + ',' if filters else '') + 'volumedetect',
            '-f', 'null', '-']
    output = ffmpeg(args).stderr.decode()
    match = re.search(r'max_volume: ([-\d.]+) dB', output)
    if not match:
        raise ValueError('No measurable peak: ' + str(path))
    return float(match[1])


def download(entry):
    path = CACHE / entry['cache_file']
    if not path.exists():
        for attempt in range(4):
            time.sleep(5 if attempt == 0 else 30 * attempt)
            request = urllib.request.Request(entry['download_url'], headers={'User-Agent': USER_AGENT})
            try:
                with urllib.request.urlopen(request, timeout=60) as response:
                    data = response.read()
                digest = hashlib.sha256(data).hexdigest()
                if digest != entry['source_sha256']:
                    raise ValueError('Source changed: ' + entry['source_page'])
                path.write_bytes(data)
                break
            except urllib.error.HTTPError as error:
                if error.code not in (429, 502, 503) or attempt == 3:
                    raise
    if hashlib.sha256(path.read_bytes()).hexdigest() != entry['source_sha256']:
        raise ValueError('Cached source checksum mismatch: ' + str(path))
    return path


def prepare(entry, source, work):
    start = entry['cut_start']
    end = entry['cut_end']
    channels = entry['channels']
    base = (f'atrim=start={start}:end={end},asetpts=PTS-STARTPTS,'
            f'aresample=44100,aformat=channel_layouts={"mono" if channels == 1 else "stereo"}')
    if entry['filters']:
        base += ',' + ','.join(entry['filters'])
    base += ',aresample=44100'
    if entry['kind'] == 'ambience':
        crossfade = entry['crossfade_seconds']
        length = end - start - crossfade
        graph = (f'[0:a]{base},asplit=3[main][head][tail];'
                 f'[main]atrim=start={crossfade}:end={length},asetpts=PTS-STARTPTS[mid];'
                 f'[head]atrim=end={crossfade},asetpts=PTS-STARTPTS[h];'
                 f'[tail]atrim=start={length},asetpts=PTS-STARTPTS[t];'
                 f'[t]afade=t=out:st=0:d={crossfade}[tf];'
                 f'[h]afade=t=in:st=0:d={crossfade}[hf];'
                 '[tf][hf]amix=inputs=2:duration=longest:normalize=0[blend];'
                 '[mid][blend]concat=n=2:v=0:a=1[out]')
        ffmpeg(['-i', str(source), '-filter_complex', graph, '-map', '[out]',
                '-c:a', 'pcm_f32le', str(work)])
    else:
        duration = end - start
        base += f',afade=t=in:d=0.015,afade=t=out:st={duration - 0.060}:d=0.060'
        ffmpeg(['-i', str(source), '-af', base, '-c:a', 'pcm_f32le', str(work)])


def encode(entry, work, output):
    if entry['kind'] == 'ambience':
        analysis = ffmpeg(['-i', str(work), '-af', 'loudnorm=I=-23:TP=-1:LRA=7:print_format=json',
                           '-f', 'null', '-']).stderr.decode()
        measured = json.loads(analysis[analysis.rfind('{'):analysis.rfind('}') + 1])
        # A constant gain preserves the cyclic waveform and its dynamics exactly.
        gain = min(-23.0 - float(measured['input_i']), -1.5 - peak(work))
    else:
        gain = -3.0 - peak(work)
    lower_gain = None
    upper_gain = None
    best_error = math.inf
    best_path = CACHE / 'audio-best.ogg'
    for attempt in range(24):
        ffmpeg(['-i', str(work), '-af', f'volume={gain:.6f}dB', '-ar', '44100',
                '-ac', str(entry['channels']), '-map_metadata', '-1', '-c:a', 'libvorbis',
                '-q:a', '4', '-fflags', '+bitexact', '-flags:a', '+bitexact', str(output)])
        measured_peak = peak(output)
        target = -3.0 if entry['kind'] != 'ambience' else -1.5
        if entry['kind'] == 'ambience' and measured_peak <= -1.0:
            return
        error = abs(measured_peak - target)
        if error < best_error:
            best_error = error
            shutil.copyfile(output, best_path)
        if error <= 0.11:
            return
        if measured_peak < target:
            lower_gain = gain if lower_gain is None else max(lower_gain, gain)
        else:
            upper_gain = gain if upper_gain is None else min(upper_gain, gain)
        if lower_gain is not None and upper_gain is not None:
            gain = (lower_gain + upper_gain) / 2
        else:
            gain += target - measured_peak
    shutil.copyfile(best_path, output)
    if peak(output) > -1.0:
        raise ValueError('Encoded peak exceeds -1 dBFS: ' + str(output))
    if entry['kind'] != 'ambience' and best_error > 0.15:
        raise ValueError('Decoded normalization did not converge: ' + str(output))


def write_records(entries):
    groups = {}
    rows = []
    for entry in entries:
        path = DEST / entry['file']
        duration = float(probe(path)['format']['duration'])
        group = groups.setdefault(entry['name'], {'name': entry['name'], 'kind': entry['kind'],
                                                  'files': [], 'seconds': []})
        group['files'].append(entry['file'])
        group['seconds'].append(duration)
        if entry['kind'] == 'call':
            group['species'] = entry['species']
        if entry['kind'] == 'ambience':
            group['loop'] = True
        licence = entry['licence']
        if entry.get('licence_url'):
            licence = f'[{licence}]({entry["licence_url"]})'
        changes = (f'Cut {entry["cut_start"]}–{entry["cut_end"]} s; '
                   f'{"mono" if entry["channels"] == 1 else "stereo"}; ')
        changes += '; '.join(entry['filters']) + '; ' if entry['filters'] else ''
        if entry['kind'] == 'ambience':
            changes += (f'cyclic tail/head crossfade {entry["crossfade_seconds"]} s; '
                        'constant loudness gain targeting -23 LUFS; ')
        else:
            changes += '15 ms fade in, 60 ms fade out; decoded peak normalized to -3 dBFS; '
        changes += '44100 Hz Ogg Vorbis quality 4'
        values = [f'`{entry["file"]}`', entry['heard'],
                  f'[{entry["title"]}]({entry["source_page"]})', entry['author'], licence, changes]
        rows.append('| ' + ' | '.join(value.replace('|', '&#124;').replace('\n', ' ') for value in values) + ' |')
    manifest = {'version': 1, 'sounds': list(groups.values())}
    (DEST / 'sounds.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n')
    heading = ('# Audio sources\n\n'
               'Per-file attribution and modifications. Source licences apply to the recordings and their '
               'edited derivatives; repository scripts are MIT. No recording is relabelled as MIT.\n\n'
               'The source page and its licence metadata were checked without signing in. '
               'Exact source hashes, cuts and filters are in `tools/audio/sources.json`; '
               'the checked metadata is preserved in `tools/audio/licence-evidence.json`.\n\n'
               '| File | What is heard / substitution | Source title and page | Author | Licence | Changes |\n'
               '|---|---|---|---|---|---|\n')
    (DEST / 'SOURCES.md').write_text(heading + '\n'.join(rows) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--only', help='Rebuild comma-separated sound groups (records still cover every file)')
    args = parser.parse_args()
    CACHE.mkdir(parents=True, exist_ok=True)
    entries = json.loads((HERE / 'sources.json').read_text())['files']
    for entry in entries:
        if args.only and entry['name'] not in args.only.split(','):
            continue
        source = download(entry)
        output = DEST / entry['file']
        output.parent.mkdir(parents=True, exist_ok=True)
        work = CACHE / 'audio-prepared.wav'
        prepare(entry, source, work)
        encode(entry, work, output)
        print(entry['file'], flush=True)
    write_records(entries)


if __name__ == '__main__':
    main()
