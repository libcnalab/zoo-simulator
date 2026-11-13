# SPDX-License-Identifier: MIT
"""Retry transient failures and close remaining targeted Round 2 searches."""
import json
from pathlib import Path
from urllib.parse import urlencode, urlparse
from round2_research import CACHE, LOG, fetch, get_json


def main():
    fetch('https://datadryad.org/api/v2/files/4964066/download', 'elephant: Dryad acoustic dataset README', '.md')
    fetch('https://datadryad.org/api/v2/files/4964058/download', 'elephant: Dryad acoustic dataset labels', '.zip')
    wanted = {'File:Rallying.ogg', 'File:Play fight of polar bears.wmv.ogv',
              'File:Cub polar bear is nursing 2.ogv', 'File:Erdmaennchen leipzig raufen.ogv',
              'File:Struisvogel paringsdans libtheora.ogg', 'File:Ostrich mating.webm'}
    for record in json.loads(LOG.read_text()):
        if not record['label'].startswith('Commons candidate batch') or record['status'] != 200:
            continue
        data = json.loads((CACHE / record['cache_file']).read_text())
        for page in data.get('query', {}).get('pages', {}).values():
            if page['title'] in wanted:
                for info in page.get('imageinfo', []):
                    metadata = info.get('extmetadata', {})
                    licence = metadata.get('LicenseShortName', {}).get('value', '')
                    credit = str(metadata.get('Credit', {})).lower()
                    eligible = licence in ['CC0', 'Public domain', 'CC BY 3.0', 'CC BY 4.0', 'CC BY-SA 3.0', 'CC BY-SA 4.0']
                    if info['size'] <= 40_000_000 and eligible and 'youtube' not in credit:
                        fetch(info['url'], 'Part 1 media candidate ' + page['title'], Path(urlparse(info['url']).path).suffix)
    queries = ['elephant AND (trumpet OR rumble OR vocalization OR acoustic)',
               'tiger AND (chuff OR prusten OR vocalization)', 'wolf AND growl',
               '"polar bear" AND (vocalization OR chuff OR growl)',
               'meerkat AND (call OR growl OR alarm)', 'ostrich AND (boom OR vocalization OR hiss)',
               'kangaroo AND (vocalization OR cough OR cluck)']
    for query in queries:
        get_json('https://zenodo.org/api/records', {'q': query, 'size': 25}, 'Zenodo Boolean size25 ' + query)
    fetch('https://xeno-canto.org/species/Struthio-camelus?pg=2', 'ostrich: xeno-canto page 2', '.html')
    fetch('https://www.fws.gov/search/videos?search_api_fulltext=polar%20bear', 'polar_bear: FWS video search', '.html')
    for domain in ['fws.gov', 'noaa.gov', 'usgs.gov']:
        query = 'site:' + domain + ' "polar bear" vocalization sound'
        fetch('https://www.bing.com/search?' + urlencode({'format':'rss','q':query}),
              'polar_bear: federal web index ' + query, '.xml')
    for record in json.loads(LOG.read_text()):
        if '429' in str(record['status']) or '504' in str(record['status']):
            fetch(record['url'], record['label'] + ' retry', retry=True)


if __name__ == '__main__':
    main()
