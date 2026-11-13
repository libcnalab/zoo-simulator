# SPDX-License-Identifier: MIT
"""Part 2 discovery after inspecting the original single-take sources."""
from round2_research import fetch, get_json


def main():
    for name, species, query in [
        ('brown_bear', 'Ursus arctos', '"brown bear" AND growl'),
        ('elephant', 'Loxodonta africana', '"elephant" AND rumble'),
        ('giraffe', 'Giraffa camelopardalis', 'giraffe AND grunt'),
        ('penguin', 'Spheniscus demersus', '"African penguin" AND (bray OR ecstatic OR vocal)'),
        ('rhino', 'Ceratotherium simum', '"white rhinoceros" AND vocal'),
        ('zebra', 'Equus quagga', 'zebra AND (bray OR bark OR whinny)'),
    ]:
        get_json('https://api.inaturalist.org/v1/observations', {
            'taxon_name': species, 'sounds': 'true', 'sound_license': 'cc0,cc-by',
            'per_page': 50}, 'Part 2 ' + name + ': iNaturalist')
        get_json('https://commons.wikimedia.org/w/api.php', {
            'action': 'query', 'list': 'search', 'srnamespace': 6,
            'srsearch': species + ' filetype:audio', 'srlimit': 50, 'format': 'json'},
            'Part 2 ' + name + ': Commons audio')
        get_json('https://zenodo.org/api/records', {'q': query, 'size': 25},
                 'Part 2 ' + name + ': Zenodo ' + query)
    get_json('https://commons.wikimedia.org/w/api.php', {
        'action': 'query', 'list': 'search', 'srnamespace': 6,
        'srsearch': 'African penguin ecstatic', 'srlimit': 50, 'format': 'json'},
        'Part 2 penguin: Commons ecstatic display')
    get_json('https://api.figshare.com/v2/articles', {'resource_doi':'10.1371/journal.pone.0010346'},
             'Part 2 elephant: original PLOS supplementary records')
    fetch('https://www.nature.com/articles/srep17255', 'Part 2 penguin: original publisher supplement descriptions', '.html')


if __name__ == '__main__':
    main()
