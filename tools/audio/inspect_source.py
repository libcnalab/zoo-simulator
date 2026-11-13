# SPDX-License-Identifier: MIT
"""Render a source waveform and log-frequency spectrogram without listening."""
import argparse
from pathlib import Path

from build_audio import CACHE, ffmpeg


def inspect(path, output, start=None, end=None):
    prefix = ''
    if start is not None:
        prefix = f'atrim=start={start}:end={end},asetpts=PTS-STARTPTS,'
    graph = (f'[0:a]{prefix}asplit=2[a][b];'
             '[a]showwavespic=s=1684x260:colors=white[w];'
             '[b]showspectrumpic=s=1400x600:legend=1:scale=log:fscale=log,scale=1684:-1[s];'
             '[w][s]vstack[out]')
    ffmpeg(['-i', str(path), '-filter_complex', graph, '-map', '[out]', '-frames:v', '1', str(output)])
    print(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('label')
    parser.add_argument('--start', type=float)
    parser.add_argument('--end', type=float)
    args = parser.parse_args()
    if (args.start is None) != (args.end is None):
        parser.error('--start and --end must be supplied together')
    if Path(args.label).name != args.label:
        parser.error('label must be a filename component')
    inspect(args.source, CACHE / ('round2-' + args.label + '-inspection.png'), args.start, args.end)


if __name__ == '__main__':
    main()
