# SPDX-License-Identifier: MIT
"""Write the exact Round 2 request/search ledger into RESEARCH.md from cached evidence."""
from collections import Counter
import json
import re

from round2_research import CACHE, LOG, ROOT


def clean(value):
    return re.sub('<[^>]*>', '', str(value)).replace('|', '&#124;').replace('\n', ' ')


def describe(record):
    if record['status'] != 200:
        return str(record['status']) + '; access/query failure, not an empty collection.'
    try:
        data = json.loads((CACHE / record['cache_file']).read_text())
    except (ValueError, UnicodeDecodeError):
        return 'Media or HTML saved; see candidate decisions above. SHA-256: ' + record.get('sha256', '')
    if isinstance(data, list):
        return 'Publisher supplement records: ' + '; '.join(
            str(item.get('id')) + ' ' + item.get('title', '') for item in data)
    if 'hits' in data:
        hits = data['hits']
        titles = [item['metadata']['title'] for item in hits.get('hits', [])]
        return f"{hits.get('total')} matches; inspected first {len(titles)} record/file listings. " + '; '.join(titles)
    if '_embedded' in data:
        items = data['_embedded'].get('stash:datasets', data['_embedded'].get('stash:files', []))
        return f"{data.get('total', data.get('count', len(items)))} results. " + '; '.join(
            item.get('title', item.get('path', str(item.get('id')))) for item in items)
    if 'response' in data:
        docs = data['response'].get('docs', [])
        licensed = [item for item in docs if any(token in str(item.get('licenseurl', ''))
                    for token in ['/by/', '/publicdomain/'])]
        return (f"{data['response'].get('numFound')} matches; first {len(docs)} checked. "
                'Items claiming eligible licences: ' + ('; '.join(
                    item.get('title', item['identifier']) for item in licensed) or 'none') +
                '. No eligible species/call/recordist/place combination established; see candidate exclusions.')
    if 'query' in data:
        query = data['query']
        items = query.get('search', query.get('categorymembers', []))
        if 'pages' in query:
            return 'Item metadata/individual licence evidence: ' + '; '.join(
                page['title'] for page in query['pages'].values()) + '. See candidate decisions.'
        titles = [item['title'] for item in items]
        return (f"{query.get('searchinfo', {}).get('totalhits', len(titles))} matches; "
                f"{len(titles)} returned. " + ('; '.join(titles) or 'No matching items.') +
                (' Candidate provenance/call-type decisions above; pronunciations, music and wrong species excluded.' if titles else ''))
    if 'results' in data and 'total_results' in data:
        items = data['results']
        return (f"{data['total_results']} observations; {len(items)} on this page. " +
                '; '.join(str(item['id']) + ' ' + item.get('taxon', {}).get('name', '') +
                          ': ' + (item.get('description') or 'No call description')[:350] for item in items) +
                '. Species/call-type, recording-quality and licence decisions are in the candidate reviews above.'
                if items else '0 sound observations under the requested licence filter.')
    if 'results' in data and 'count' in data:
        counts = Counter(medium.get('license', 'unspecified') for item in data['results']
                         for medium in item.get('media', []) if medium.get('type') == 'Sound')
        return (f"{data['count']} occurrences; {len(data['results'])} on this page. Sound licences: " +
                '; '.join(f'{key}: {value}' for key, value in sorted(counts.items())) +
                '. NC/ND/proprietary media excluded; eligible media reviewed separately for species and call type.')
    if 'usageKey' in data:
        return f"Taxon key {data['usageKey']}: {data.get('scientificName')}; match {data.get('matchType')}."
    return 'API response cached; see candidate decisions.'


def main():
    path = ROOT / 'tools/audio/RESEARCH.md'
    start = '<!-- ROUND2-LEDGER-START -->'
    end = '<!-- ROUND2-LEDGER-END -->'
    records = json.loads(LOG.read_text())
    text = '\n' + start + '\n\n### Round 2 exact requests and search results\n\n'
    text += ('Search result counts are bounded discovery results, not proof of absence. '
             'Raw response filenames and hashes are retained in the ignored request ledger. '
             'Successful metadata fetches do not imply a recording was accepted.\n\n')
    text += '| Source / query or item | Status | Results / disposition |\n|---|---|---|\n'
    for record in records:
        text += f"| [{clean(record['label'])}]({record['url']}) | {clean(record['status'])} | {clean(describe(record))} |\n"
    text += f"\n{len(records)} serialized requests; {sum(item['bytes'] for item in records):,} response bytes recorded.\n\n{end}\n"
    old = path.read_text()
    if start in old:
        before, rest = old.split(start, 1)
        _, after = rest.split(end, 1)
        old = before.rstrip() + text + after
    else:
        old += text
    path.write_text(old.rstrip() + '\n')


if __name__ == '__main__':
    main()
