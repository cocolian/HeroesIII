"""Generate classic trade lots from the recorded original-game rate table.

Use the integer table, including its non-uniform rounding. Do not calculate
prices with a floating-point efficiency approximation or import engine code.
"""
import hashlib
import json
import pathlib
import re
from rule_html import Document
from h3_resources import Lod

root = pathlib.Path(__file__).resolve().parents[1]
path = root / 'references/rules/wiki/Marketplace.html'
document = Document(path.read_text())
rows = []
for tr in document.root.walk('tr'):
    cells = [node for node in tr.children if node.tag in ('td', 'th')]
    if len(cells) != 9 or not re.fullmatch(r'[1-9]\+?', cells[0].text()):
        continue
    rates = []
    for cell in cells[1:]:
        match = re.fullmatch(r'(\d+) → (\d+)', cell.text())
        assert match, cell.text()
        rates.append({'give': int(match[1]), 'receive': int(match[2])})
    rows.append({'marketplaces': int(cells[0].text()[0]), 'rates': rates})
assert [row['marketplaces'] for row in rows] == list(range(1, 10))
post = root / 'references/rules/wiki/Trading_Post.html'
assert 'five' in Document(post.read_text()).root.text()
lod = Lod(root / 'references/demo/extracted/Heroes III Demo/data/h3bitmap.lod')
objects = lod.read('objects.txt')
post_template = next(line for line in objects.decode('cp1252').splitlines() if line.lower().startswith('avxpost0.def '))
assert post_template.split()[-4:] == ['99', '0', '0', '0']
record = {
    'edition': 'Heroes III Complete / Shadow of Death, classic resource trade',
    'sources': [{'file': str(p.relative_to(root)), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                for p in (path, post)],
    'categories': ['goldToBasic', 'goldToRare', 'basicToGold', 'basicToBasic', 'basicToRare',
                   'rareToGold', 'rareToBasic', 'rareToRare'],
    'basicResources': ['wood', 'ore'], 'rareResources': ['mercury', 'sulfur', 'crystal', 'gems'],
    'rows': rows, 'maximumEffectiveMarkets': 9, 'tradingPostMarkets': 5,
    'tradingPostTemplate': {'archive': 'h3bitmap.lod', 'entry': 'objects.txt',
                            'entrySha256': hashlib.sha256(objects).hexdigest(), 'record': post_template},
    'remaining': ['multiple owned towns and player resource transfers', 'all market services and original executable comparison'],
}
(root / 'data/native/market.json').write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n')
compiled = ['// Generated classic integer trade lots; see data/native/market.json.']
for row in rows:
    compiled.append('std::array<TradeRate,8>{{' + ','.join('TradeRate{%d,%d}' % (r['give'], r['receive']) for r in row['rates']) + '}},')
(root / 'native/generated/market.inc').write_text('\n'.join(compiled) + '\n')
print('Generated 9 classic market rate rows, 8 exchange categories; verified original Trading Post template.')
