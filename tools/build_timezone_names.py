"""Regenerate the checked-in CLDR timezone labels; runtime remains offline."""
from pathlib import Path
import hashlib
import json
import unicodedata
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
REVISION = '91c267402229a59e3ef2774544f001bf959e8809'
BASE = f'https://raw.githubusercontent.com/unicode-org/cldr-json/{REVISION}/'

def main():
    target = ROOT / 'assets/Timezones'
    target.mkdir(exist_ok=True)
    inputs = {}
    def fetch(path):
        data = urllib.request.urlopen(BASE + path, timeout=30).read()
        inputs[path] = hashlib.sha256(data).hexdigest()
        return data
    aliases = json.loads(fetch('cldr-json/cldr-bcp47/bcp47/timezone.json'))['keyword']['u']['tz']
    result = {}
    for language in ('en', 'ru'):
        tree = json.loads(fetch(f'cldr-json/cldr-dates-full/main/{language}/timeZoneNames.json'))['main'][language]['dates']['timeZoneNames']['zone']
        cities = {}
        def flatten(node, prefix=''):
            if 'exemplarCity' in node:
                cities[prefix] = node['exemplarCity']
            for key, value in node.items():
                if isinstance(value, dict):
                    flatten(value, f'{prefix}/{key}' if prefix else key)
        flatten(tree)
        for record in aliases.values():
            if not isinstance(record, dict):
                continue
            ids = record.get('_alias', '').split()
            label = next((cities[x] for x in ids if x in cities), None)
            if language == 'en' and not label and ids:
                label = ids[0].rsplit('/', 1)[-1].replace('_', ' ')
            if label:
                for identifier in ids:
                    cities.setdefault(identifier, label)
        countries = json.loads(fetch(f'cldr-json/cldr-localenames-full/main/{language}/territories.json'))['main'][language]['localeDisplayNames']['territories']
        countries = {key: value for key, value in countries.items() if len(key) == 2}
        labels = list(cities.values()) + list(countries.values())
        def sort_key(label):
            return ''.join(c for c in unicodedata.normalize('NFD', label.casefold().replace('ё', 'е')) if not unicodedata.combining(c))
        result[language] = {'cities': cities, 'countries': countries, 'sort': {label: sort_key(label) for label in labels}}
    (target / 'names.json').write_text(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2) + '\n')
    (target / 'LICENSE').write_bytes(fetch('LICENSE'))
    provenance = {'project': 'Unicode CLDR JSON', 'revision': REVISION, 'base_url': BASE, 'inputs_sha256': inputs, 'output_sha256': hashlib.sha256((target / 'names.json').read_bytes()).hexdigest(), 'generator': 'tools/build_timezone_names.py'}
    (target / 'sources.json').write_text(json.dumps(provenance, indent=2) + '\n')

if __name__ == '__main__':
    main()
