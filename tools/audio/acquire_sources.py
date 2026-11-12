# SPDX-License-Identifier: MIT
"""Research helper: preserve Commons/Freesound metadata and cache verified media sequentially."""
import hashlib
import html
import json
from pathlib import Path
import re
import sys
import time
import urllib.error
import urllib.request

from research import CACHE, request

HERE = Path(__file__).resolve().parent
EVIDENCE = HERE / 'licence-evidence.json'


def fetch(url, path):
    if path.exists():
        return path.read_bytes()
    for attempt in range(4):
        time.sleep(5 if attempt == 0 else 30 * attempt)
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'zoo-simulator-audio-fetch/1.0'})
            with urllib.request.urlopen(req, timeout=60) as response:
                data = response.read()
            path.write_bytes(data)
            return data
        except urllib.error.HTTPError as error:
            if error.code not in (429, 502, 503) or attempt == 3:
                raise


def text(value):
    return html.unescape(re.sub('<[^>]+>', ' ', str(value))).strip()


def commons(title):
    data = request(dict(action='query', titles=title, prop='imageinfo',
                        iiprop='url|extmetadata|size', format='json'))
    info = next(iter(data['query']['pages'].values()))['imageinfo'][0]
    meta = {key: text(value['value']) for key, value in info['extmetadata'].items()}
    return {'title': title[5:], 'source_page': info['descriptionurl'],
            'download_url': info['url'].split('?')[0], 'author': meta.get('Artist', ''),
            'licence': meta['LicenseShortName'], 'licence_url': meta.get('LicenseUrl', ''),
            'metadata': meta}


def freesound(url):
    sound_id = url.rstrip('/').split('/')[-1]
    page = fetch(url, CACHE / f'freesound-{sound_id}.html').decode()
    title = html.unescape(re.search(r'property="og:audio:title" content="([^"]+)"', page)[1])
    media = html.unescape(re.search(r'data-static-file-url="([^"]+)"', page)[1])
    licence_match = re.search(r'<a title="Go to the full license text" href="([^"]+)"[^>]*>([^<]+)', page)
    licence = text(licence_match[2])
    licence = {'Creative Commons 0': 'CC0', 'Attribution 4.0': 'CC BY 4.0',
               'Attribution 3.0': 'CC BY 3.0'}.get(licence, licence)
    description = page.split('id="soundDescriptionSection"')[1].split('</div>')[0]
    return {'title': title, 'source_page': url, 'download_url': media,
            'author': url.split('/people/')[1].split('/')[0], 'licence': licence,
            'licence_url': licence_match[1], 'metadata': {'description': text(description),
                                                        'download_note': 'Public high-quality preview linked by source page'}}


def main():
    records = json.loads(EVIDENCE.read_text()) if EVIDENCE.exists() else {}
    for arg in sys.argv[1:]:
        if arg in records:
            print('cached', arg, flush=True)
            continue
        record = commons(arg) if arg.startswith('File:') else freesound(arg)
        print(record['title'], record['licence'], record['metadata'], flush=True)
        if record['licence'] not in ('Public domain', 'CC0', 'CC BY 3.0', 'CC BY 4.0', 'CC BY-SA 3.0', 'CC BY-SA 4.0'):
            print('REJECTED LICENCE', flush=True)
            continue
        suffix = Path(record['download_url']).suffix
        cache_file = 'source-' + hashlib.sha256(arg.encode()).hexdigest()[:16] + suffix
        data = fetch(record['download_url'], CACHE / cache_file)
        record['cache_file'] = cache_file
        record['source_sha256'] = hashlib.sha256(data).hexdigest()
        records[arg] = record
        EVIDENCE.write_text(json.dumps(records, indent=2, ensure_ascii=False) + '\n')


if __name__ == '__main__':
    main()
