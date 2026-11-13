# SPDX-License-Identifier: MIT
"""Targeted dataset and media-record follow-up after broad Round 2 searches."""
import json
from round2_research import CACHE, LOG, fetch, get_json


def main():
    queries = ['elephant trumpet audio', 'elephant rumble', 'tiger chuff', 'tiger prusten',
               'wolf growl', 'polar bear vocalization', 'polar bear chuff', 'polar bear cub',
               'meerkat growl', 'meerkat alarm', 'ostrich boom', 'ostrich vocalization',
               'kangaroo vocalization', 'kangaroo cough', 'kangaroo cluck']
    for query in queries:
        get_json('https://zenodo.org/api/records', {'q': query, 'size': 25}, 'Zenodo simple ' + query)
    for version in [464258, 374863, 370902, 3509, 309216, 18482, 26683, 5734, 245197, 231113, 90766]:
        get_json(f'https://datadryad.org/api/v2/versions/{version}/files', {'per_page': 100}, f'Dryad candidate version {version} files')
    for terms in ['elephant trumpet', 'tiger chuff', 'wolf growl', 'polar bear growl',
                  'polar bear chuff', 'meerkat growl', 'ostrich boom', 'ostrich hiss', 'kangaroo grunt']:
        for media in ['audio', 'video']:
            get_json('https://commons.wikimedia.org/w/api.php', {
                'action': 'query', 'list': 'search', 'srnamespace': 6,
                'srsearch': terms + ' filetype:' + media, 'srlimit': 50, 'format': 'json'},
                terms + ': Commons targeted ' + media)
    # Species-query media descriptions are checked at each individual item's page/API.
    titles = []
    for record in json.loads(LOG.read_text()):
        if ': Commons ' not in record['label'] or record['status'] != 200:
            continue
        data = json.loads((CACHE / record['cache_file']).read_text())
        for hit in data.get('query', {}).get('search', []):
            title = hit['title']
            if any(word in title.lower() for word in ['en-us-', 'en-au-', 'en-uk-', 'de-', 'fr-', 'es-', 'nl-', 'll-q', 'pronunciation', 'familiaris', 'antti luode']):
                continue
            if title not in titles:
                titles.append(title)
    print('Commons candidate metadata:', len(titles), flush=True)
    for offset in range(0, len(titles), 15):
        batch = titles[offset:offset + 15]
        get_json('https://commons.wikimedia.org/w/api.php', {
            'action': 'query', 'titles': '|'.join(batch), 'prop': 'imageinfo',
            'iiprop': 'url|extmetadata|size', 'format': 'json'}, 'Commons candidate batch ' + str(offset // 15 + 1))


if __name__ == '__main__':
    main()
