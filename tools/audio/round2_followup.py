# SPDX-License-Identifier: MIT
"""Additional source-family, multilingual and paginated queries for Round 2."""
import json
from round2_research import CACHE, LOG, TAXA, fetch, get_json


def commons(query, label):
    return get_json('https://commons.wikimedia.org/w/api.php', {
        'action': 'query', 'list': 'search', 'srnamespace': 6, 'srsearch': query,
        'srlimit': 50, 'format': 'json'}, label + ': Commons ' + query)


def main():
    focused = {
        'elephant': 'elephant AND (trumpet OR rumble OR vocalization OR acoustic)',
        'tiger': 'tiger AND (chuff OR prusten OR vocalization OR acoustic)',
        'wolf': 'wolf AND (growl OR vocalization)',
        'polar_bear': '"polar bear" AND (vocalization OR chuff OR cub OR sound)',
        'meerkat': 'meerkat AND (call OR growl OR alarm)',
        'ostrich': 'ostrich AND (boom OR vocalization OR hiss)',
        'kangaroo': '(kangaroo OR "Macropus rufus") AND (vocalization OR cough OR cluck)',
    }
    for name, query in focused.items():
        get_json('https://zenodo.org/api/records', {'q': query, 'size': 50}, name + ': Zenodo focused ' + query)
        get_json('https://datadryad.org/api/v2/search', {'q': name.replace('_', ' ') + ' vocalization', 'per_page': 50}, name + ': Dryad simple vocalization')
    for name, query in focused.items():
        get_json('https://archive.org/advancedsearch.php', {'q': '(' + query + ') AND mediatype:audio', 'fl[]': 'identifier,title,licenseurl,description', 'rows': 50, 'output': 'json'}, name + ': Internet Archive focused ' + query)
    for query in ['elephant trumpet', 'elephant rumble', 'tiger chuff', 'tiger prusten', 'wolf growl', 'polar bear chuff', 'polar bear cub', 'meerkat call', 'meerkat growl', 'meerkat alarm', 'ostrich boom', 'ostrich hiss', 'kangaroo cough', 'kangaroo cluck']:
        get_json('https://datadryad.org/api/v2/search', {'q': query, 'per_page': 50}, 'Dryad simple ' + query)
    for record in json.loads(LOG.read_text()):
        if ': GBIF sound' in record['label'] and record['status'] == 200:
            data = json.loads((CACHE / record['cache_file']).read_text())
            for offset in range(50, data['count'], 300):
                url = record['url'].replace('limit=50', 'limit=300') + f'&offset={offset}'
                fetch(url, record['label'] + f' offset {offset}')
    for scientific in ['Macropus rufus', 'Struthio camelus']:
        get_json('https://api.inaturalist.org/v1/observations', {
            'taxon_name': scientific, 'sounds': 'true', 'sound_license': 'cc-by-sa',
            'per_page': 50}, scientific + ': iNaturalist SA fallback')
    for name, scientific, _ in TAXA:
        for media in ['audio', 'video']:
            commons(scientific + ' filetype:' + media, name)
        for category in [f'Audio files of {scientific}', f'Sounds of {scientific}']:
            get_json('https://commons.wikimedia.org/w/api.php', {
                'action': 'query', 'list': 'categorymembers', 'cmtitle': 'Category:' + category,
                'cmlimit': 100, 'format': 'json'}, name + ': Commons category ' + category)
    languages = {
        'elephant': ['Elefant Trompeten', 'éléphant', 'elefante', 'olifant'],
        'tiger': ['Tiger prusten', 'tigre', 'tijger'],
        'wolf': ['Wolf knurren', 'loup', 'lobo'],
        'polar_bear': ['Eisbär', 'ours polaire', 'oso polar', 'ijsbeer'],
        'meerkat': ['Erdmännchen', 'suricate', 'suricata', 'stokstaartje'],
        'ostrich': ['Strauß Ruf', 'autruche', 'avestruz', 'struisvogel'],
        'kangaroo': ['Macropus rufus', 'Riesenkänguru', 'kangourou roux', 'canguro rojo'],
    }
    for name, queries in languages.items():
        for query in queries:
            commons(query + ' filetype:audio', name)
    get_json('https://xeno-canto.org/api/2/recordings', {'query': 'Struthio camelus'}, 'ostrich: xeno-canto API')
    fetch('https://xeno-canto.org/species/Struthio-camelus', 'ostrich: xeno-canto species', '.html')
    federal = [
        ('FWS NDL', 'https://digitalmedia.fws.gov/digital/api/search/collection/natdiglib/searchterm/polar%20bear/field/all/mode/all/conn/and/maxRecords/100'),
        ('FWS', 'https://www.fws.gov/search?search_api_fulltext=polar%20bear%20sound'),
        ('NOAA', 'https://www.noaa.gov/search?search_api_fulltext=polar%20bear%20vocalization'),
        ('USGS', 'https://www.usgs.gov/search?keywords=polar%20bear%20vocalization'),
    ]
    for label, url in federal:
        fetch(url, 'polar_bear: ' + label, '.html')


if __name__ == '__main__':
    main()
