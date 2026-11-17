"""Read classic spell identities and guild weights from the recorded references."""
import json
import pathlib
import re
from rule_html import Document

ROOT = pathlib.Path(__file__).resolve().parents[1]

def read_json(path):
    return json.loads(re.sub(r',\s*([}\]])', r'\1', re.sub(r'//[^\n]*', '', path.read_text())))

def normalize(value):
    return re.sub('[^a-z0-9]', '', value.lower())

def catalog():
    facts = {}
    document = Document((ROOT / 'references/rules/wiki/List_of_spells.html').read_text())
    for table in list(document.root.walk('table'))[:3]:
        for row in table.walk('tr'):
            cells = [cell for cell in row.children if cell.tag == 'td']
            if len(cells) < 4 or not cells[2].text().isdigit():
                continue
            links = list(cells[0].walk('a'))
            if not links:
                continue
            page = links[0].attrs['href'].split('/index.php/')[-1]
            school = 15 if cells[1].text() == 'All' else 0
            for link in cells[1].walk('a'):
                for word, mask in [('Air', 1), ('Fire', 2), ('Water', 4), ('Earth', 8)]:
                    if word in link.attrs.get('title', ''):
                        school |= mask
            costs = list(map(int, re.findall(r'\d+', cells[3].text())))
            facts[normalize(cells[0].text())] = {
                'page': page, 'englishName': cells[0].text(), 'school': school,
                'level': int(cells[2].text()), 'cost': costs[0],
                'skilledCost': costs[-1],
            }
    # Use the slot-weight table, not the derived whole-guild probabilities.
    tables = list(Document((ROOT / 'references/rules/wiki/Mage_Guild.html').read_text()).root.walk('table'))
    weights = {}
    for row in tables[9].walk('tr'):
        cells = [cell for cell in row.children if cell.tag == 'td']
        if len(cells) != 15 or not cells[2].text().isdigit():
            continue
        weights[normalize(cells[0].text())] = [int(cell.text()) if cell.text().isdigit() else 0 for cell in cells[3:12]]
    aliases = {'titanBolt': 'titanslightningbolt'}
    for school in ['Air', 'Fire', 'Water', 'Earth']:
        aliases['protect' + school] = normalize('Protection from ' + school)
        aliases[school.lower() + 'Elemental'] = normalize('Summon ' + school + ' Elemental')
    result = {}
    for category in ['adventure', 'offensive', 'timed', 'other']:
        for key, definition in read_json(ROOT / 'references/rules/vcmi/config/spells' / (category + '.json')).items():
            if 'index' not in definition:
                continue
            name = aliases.get(key, normalize(key))
            fact = dict(facts[name])
            assert fact['school'], key
            fact.update(key=key, originalId=definition['index'], adventure=category == 'adventure',
                        guildWeights=weights.get(name, [0] * 9))
            result[key] = fact
    assert sorted(row['originalId'] for row in result.values()) == list(range(70))
    return result

if __name__ == '__main__':
    for key, fact in sorted(catalog().items(), key=lambda item: item[1]['originalId']):
        print(key, fact['page'], fact['level'], fact['school'], fact['guildWeights'])
