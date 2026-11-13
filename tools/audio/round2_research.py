# SPDX-License-Identifier: MIT
"""Serialized, cached Round 2 research; enforce a 300 MB response-byte budget."""
import argparse
import hashlib
import json
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / 'build-probe/audio-src'
LOG = CACHE / 'round2-requests.json'
USER_AGENT = 'zoo-simulator-audio-fetch/1.0'
LIMIT = 300_000_000
TAXA = [
    ('elephant', 'Loxodonta africana', 'elephant trumpet rumble vocalization'),
    ('tiger', 'Panthera tigris', 'tiger chuff prusten vocalization'),
    ('wolf', 'Canis lupus', 'wolf growl'),
    ('polar_bear', 'Ursus maritimus', 'polar bear vocalization chuff cub'),
    ('meerkat', 'Suricata suricatta', 'meerkat call growl alarm'),
    ('ostrich', 'Struthio camelus', 'ostrich boom vocalization hiss'),
    ('kangaroo', 'Osphranter rufus', 'kangaroo vocalization cough cluck'),
]


def fetch(url, label, suffix='.json', retry=False):
    CACHE.mkdir(parents=True, exist_ok=True)
    records = json.loads(LOG.read_text()) if LOG.exists() else []
    for record in reversed(records):
        if record['url'] == url and not retry:
            return record
    time.sleep(5)
    path = CACHE / ('round2-' + hashlib.sha256(url.encode()).hexdigest()[:20] + suffix)
    record = {'label': label, 'url': url, 'cache_file': path.name, 'bytes': 0}
    remaining = LIMIT - sum(item['bytes'] for item in records)
    if remaining <= 0:
        raise RuntimeError('Round 2 download budget exhausted')
    request = urllib.request.Request(url, headers={'User-Agent': USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=35) as response:
            if int(response.headers.get('Content-Length', '0')) > remaining:
                raise RuntimeError('Response would exceed Round 2 download budget')
            data = response.read(remaining + 1)
            record['bytes'] = len(data)
            if len(data) > remaining:
                raise RuntimeError('Response exceeds Round 2 download budget')
            path.write_bytes(data)
            record['status'] = response.status
            record['sha256'] = hashlib.sha256(data).hexdigest()
    except (urllib.error.URLError, TimeoutError) as error:
        record['status'] = str(error)
        if isinstance(error, urllib.error.HTTPError):
            data = error.read(min(remaining, 100_000))
            record['bytes'] = len(data)
            path.write_bytes(data)
            if error.code == 429:
                time.sleep(30)
    records.append(record)
    LOG.write_text(json.dumps(records, indent=2, ensure_ascii=False) + '\n')
    print(label, record['status'], record['bytes'], path.name, flush=True)
    return record


def get_json(base, params, label):
    record = fetch(base + '?' + urllib.parse.urlencode(params), label)
    if record['status'] != 200:
        return {}
    try:
        return json.loads((CACHE / record['cache_file']).read_text())
    except (ValueError, UnicodeDecodeError):
        return {}


def run(stage):
    for name, scientific, terms in TAXA:
        if stage == 'inat':
            data = get_json('https://api.inaturalist.org/v1/observations', {
                'taxon_name': scientific, 'sounds': 'true', 'sound_license': 'cc0,cc-by',
                'per_page': 50}, name + ': iNaturalist')
            print('TOTAL', name, data.get('total_results'), flush=True)
        elif stage == 'gbif':
            match = get_json('https://api.gbif.org/v1/species/match', {'name': scientific}, name + ': GBIF taxon')
            key = match.get('usageKey')
            if key:
                data = get_json('https://api.gbif.org/v1/occurrence/search', {
                    'taxonKey': key, 'mediaType': 'Sound', 'limit': 50}, name + ': GBIF sound')
                print('TOTAL', name, data.get('count'), flush=True)
        elif stage in ('zenodo', 'dryad'):
            # OR retains alternative call words instead of requiring every word in a record.
            words = terms.split()
            animal = 'polar bear' if name == 'polar_bear' else words[0]
            call_words = words[2:] if name == 'polar_bear' else words[1:]
            query = '(' + scientific + ') OR (' + animal + ' AND (' + ' OR '.join(call_words) + '))'
            if stage == 'zenodo':
                get_json('https://zenodo.org/api/records', {'q': query, 'size': 25}, name + ': Zenodo ' + query)
            else:
                get_json('https://datadryad.org/api/v2/search', {'q': query, 'per_page': 50}, name + ': Dryad ' + query)
        elif stage == 'archive':
            animal = name.replace('_', ' ')
            query = f'("{scientific}" OR "{animal}") AND mediatype:audio'
            get_json('https://archive.org/advancedsearch.php', {
                'q': query, 'fl[]': 'identifier,title,licenseurl,description', 'rows': 50,
                'output': 'json'}, name + ': Internet Archive ' + query)
        elif stage == 'commons':
            query = f'"{scientific}" (filetype:audio OR filetype:video)'
            get_json('https://commons.wikimedia.org/w/api.php', {
                'action': 'query', 'list': 'search', 'srnamespace': 6, 'srsearch': query,
                'srlimit': 50, 'format': 'json'}, name + ': Commons ' + query)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', choices=['inat', 'gbif', 'zenodo', 'dryad', 'archive', 'commons'])
    run(parser.parse_args().stage)
