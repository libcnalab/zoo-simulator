# SPDX-License-Identifier: MIT
"""Snapshot or compare SHA-256 hashes of every file's decoded 16-bit PCM."""
import argparse
import json

from build_audio import CACHE, DEST, ffmpeg


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', action='store_true', help='Record the current decoded library')
    args = parser.parse_args()
    reference = CACHE / 'rebuild-reference-pcm.json'
    hashes = {}
    for path in sorted(DEST.rglob('*.ogg')):
        result = ffmpeg(['-v', 'error', '-i', str(path), '-map', '0:a:0',
                         '-c:a', 'pcm_s16le', '-f', 'hash', '-hash', 'sha256', '-'])
        hashes[str(path.relative_to(DEST))] = result.stdout.decode().strip()
    if args.snapshot:
        CACHE.mkdir(parents=True, exist_ok=True)
        reference.write_text(json.dumps(hashes, indent=2) + '\n')
        print(f'Snapshotted {len(hashes)} decoded PCM hashes')
        return
    previous = json.loads(reference.read_text())
    changed = sorted(path for path in set(previous) | set(hashes)
                     if previous.get(path) != hashes.get(path))
    result = {'files': len(hashes), 'identical': len(hashes) - len(changed), 'changed': changed}
    (CACHE / 'rebuild-result.json').write_text(json.dumps(result, indent=2) + '\n')
    if changed:
        raise SystemExit('Decoded PCM changed: ' + ', '.join(changed))
    print(f'{len(hashes)} decoded PCM hashes identical; 0 differences')


if __name__ == '__main__':
    main()
