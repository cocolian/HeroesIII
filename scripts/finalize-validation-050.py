#!/usr/bin/env python3
"""Archive location checks without storing the user's original progress."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TMP = Path('/private/tmp/heroesiii-0.50-user')
OUT = ROOT / 'references/validation/0.50'


def read(path):
    return json.loads(path.read_text())


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(2 ** 20), b''):
            h.update(block)
    return h.hexdigest()


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


original = read(TMP / 'source-checksum.json')
assert digest(Path(original['source'])) == original['sha256'], 'Original progress changed'
checks, screenshots = {}, {}
folders = {'native': 'ui-final', 'distribution': 'ui-release',
           'native-sanitized': 'ui-sanitized'}
for build, folder in folders.items():
    log = ROOT / 'build' / build / 'Testing/Temporary/LastTest.log'
    assert log.read_text().count('Test Passed.') == 13, build
    assert '343 extended location checks passed across all 41 classes' in log.read_text()
    target = OUT / build
    shutil.copytree(TMP / folder, target, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('test-saves'))
    shutil.copy2(log, target / 'ctest.log')
    audit = read(target / 'locations-ui-audit.json')
    assert audit['classes'] == 41 and not audit['originalProgressWritten']
    assert len(audit['tests']) == 41
    checks[build] = {'ctestSuitesPassed': 13, 'locationChecks': 343, 'uiAudit': audit}
    screenshots[build] = {p.name: digest(p) for p in target.glob('*.png')}
assert len(screenshots['native']) == 106
assert screenshots['native'] == screenshots['distribution'] == screenshots['native-sanitized']

rules = ROOT / 'references/object-rules-0.50'
sources = read(rules / 'sources.json')
for source in sources['files']:
    assert digest(rules / source['path']) == source['sha256'], source['path']
proof = read(rules / 'numeric-facts.json')
assert digest(ROOT / 'data/native/adventure-locations.json') == proof['outputSHA256']
proof['sources'] = sources
write(rules / 'numeric-facts.json', proof)

campaign_dir = Path('/private/tmp/heroesiii-0.50-video-final')
campaign = read(campaign_dir / 'campaign-audit.json')
assert campaign['completed'] and campaign['reloads'] == 0 and campaign['normalGameplayOnly']
(OUT / 'video-campaign').mkdir(parents=True, exist_ok=True)
for name in ['campaign-audit.json', 'actions.jsonl', 'final.json', 'first-siege.json']:
    source = campaign_dir / name
    if source.exists():
        shutil.copy2(source, OUT / 'video-campaign' / name)
failures = {}
for name in ['campaign', 'campaign-final', 'campaign-fixed', 'campaign-verified', 'seven-final']:
    source = Path('/private/tmp/heroesiii-0.50-' + name + '.log')
    if source.exists():
        shutil.copy2(source, OUT / (name + '.log'))
        failures[name] = source.read_text().splitlines()[-1]

package = read(ROOT / 'dist/macos-package.json')
assert package['version'] == '0.50.0'
sha_file = ROOT / 'dist/SHA256SUMS'
lines = sha_file.read_text().splitlines() if sha_file.exists() else []
for name, entry in package['files'].items():
    assert digest(ROOT / 'dist' / name) == entry['sha256']
    lines = [line for line in lines if not line.endswith('  ' + name)]
    lines.append(entry['sha256'] + '  ' + name)
sha_file.write_text('\n'.join(lines) + '\n')
bundle = read(Path('/private/tmp/heroesiii-0.50-bundle.json'))
assert bundle['version'] == '0.50.0' and bundle['strictSignatureValid']
audit = read(ROOT / 'data/native/map-site-audit-0.50.json')
assert audit['maps'] == 15 and audit['genericFallbackClasses'] == 0
record = {'version': '0.50.0', 'originalSaveUnchanged': True,
          'originalSaveSHA256': original['sha256'], 'builds': checks,
          'screenshots': screenshots, 'identicalAcrossThreeBuilds': True,
          'ruleSources': sources, 'numericFacts': proof,
          'siteDispatchAudit': {key: audit[key] for key in
                                ['maps', 'genericFallbackClasses', 'genericFallbackInstances']},
          'campaign': {key: campaign[key] for key in
                       ['map', 'completed', 'day', 'moves', 'visits', 'reloads', 'normalGameplayOnly']},
          'sevenLakesPassed': False, 'sevenLakesFailedAttempts': failures,
          'bundle': bundle, 'package': package,
          'scope': '41 classic location handlers; six secondary-skill systems and full original UI fidelity remain incomplete'}
write(ROOT / 'data/native/validation-0.50.json', record)
write(OUT / 'validation.json', record)
print(json.dumps({'version': '0.50.0', 'classes': 41, 'locationChecks': 343,
                  'suitesPerBuild': 13, 'screenshotsPerBuild': 106,
                  'identicalScreenshots': True, 'originalSaveUnchanged': True,
                  'completedMap': campaign['map'], 'day': campaign['day']}, ensure_ascii=False))
