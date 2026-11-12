# SPDX-License-Identifier: MIT
"""Cache Commons discovery and licence metadata; one request every five seconds."""
import hashlib
import json
from pathlib import Path
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / 'build-probe/audio-src'
CACHE.mkdir(parents=True, exist_ok=True)


def request(params):
    url = 'https://commons.wikimedia.org/w/api.php?' + urllib.parse.urlencode(params)
    path = CACHE / ('api-' + hashlib.sha256(url.encode()).hexdigest()[:16] + '.json')
    if path.exists():
        return json.loads(path.read_text())
    time.sleep(5)
    req = urllib.request.Request(url, headers={'User-Agent': 'zoo-simulator-audio-fetch/1.0'})
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=45) as response:
                data = response.read()
            break
        except urllib.error.HTTPError as error:
            if error.code not in (429, 502, 503) or attempt == 3:
                raise
            time.sleep(30 * (attempt + 1))
    path.write_bytes(data)
    return json.loads(data)


def plain(value):
    return re.sub('<[^>]+>', '', str(value))


def search(query):
    data = request(dict(action='query', list='search', srnamespace=6, srsearch=query,
                        srlimit=40, format='json'))
    for item in data.get('query', {}).get('search', []):
        print(item['title'], '|', plain(item['snippet']))


def metadata(title):
    data = request(dict(action='query', titles=title, prop='imageinfo',
                        iiprop='url|extmetadata|size', format='json'))
    for page in data.get('query', {}).get('pages', {}).values():
        for info in page.get('imageinfo', []):
            print(json.dumps({'title':title, 'url':info['url'], 'page':info['descriptionurl'],
                              'metadata':{key:plain(value['value']) for key,value in
                                          info.get('extmetadata', {}).items()}}, ensure_ascii=False))


if __name__ == '__main__':
    for arg in sys.argv[2:]:
        print('\nQUERY', arg, flush=True)
        (metadata if sys.argv[1] == 'metadata' else search)(arg)
