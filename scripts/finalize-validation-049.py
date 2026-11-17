#!/usr/bin/env python3
"""Archive 0.49 map service evidence without copying the user's save."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TMP = Path('/private/tmp/heroesiii-0.49-user')
OUT = ROOT / 'references/validation/0.49'


def read(path):
    return json.loads(path.read_text())


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(2 ** 20), b''):
            h.update(block)
    return h.hexdigest()


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')


original = read(TMP / 'source-checksum.json')
assert digest(Path(original['source'])) == original['sha256'], 'Original save changed'
builds = {'native': 'ui-final', 'distribution': 'ui-release', 'native-sanitized': 'ui-sanitized'}
screenshots, checks = {}, {}
for build, folder in builds.items():
    log = ROOT / 'build' / build / 'Testing/Temporary/LastTest.log'
    assert log.read_text().count('Test Passed.') == 12
    target = OUT / build
    shutil.copytree(TMP / folder, target, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('test-saves'))
    shutil.copy2(log, target / 'ctest.log')
    audit = read(target / 'map-services-ui-audit.json')
    assert [r['sourceClass'] for r in audit['tests']] == [97, 92, 42]
    assert not audit['originalSaveWritten']
    checks[build] = {'ctestSuitesPassed': 12, 'uiAudit': audit}
    screenshots[build] = {p.name: digest(p) for p in target.glob('*.png')}
assert len(screenshots['native']) == 10
assert screenshots['native'] == screenshots['distribution'] == screenshots['native-sanitized']
rules = ROOT / 'references/object-rules-0.49'
sources = []
for name, remote in {
    'MiscObjects.cpp': 'lib/mapObjects/MiscObjects.cpp',
    'lighthouse.json': 'config/objects/lighthouse.json',
    'SThievesGuildInfo.h': 'lib/gameState/SThievesGuildInfo.h',
    'GUIClasses.cpp': 'client/windows/GUIClasses.cpp',
    'CGameState.cpp': 'lib/gameState/CGameState.cpp',
}.items():
    sources.append({'file': name, 'url': 'https://raw.githubusercontent.com/vcmi/vcmi/develop/' + remote,
                    'sha256': digest(rules / name)})
write(rules / 'sources.json', sources)
package = read(ROOT / 'dist/macos-package.json')
assert package['version'] == '0.49.0'
for name, record in package['files'].items():
    assert digest(ROOT / 'dist' / name) == record['sha256']
sha_file = ROOT / 'dist/SHA256SUMS'
lines = sha_file.read_text().splitlines() if sha_file.exists() else []
for name, record in package['files'].items():
    lines = [line for line in lines if not line.endswith('  ' + name)]
    lines.append(record['sha256'] + '  ' + name)
sha_file.write_text('\n'.join(lines) + '\n')
site_audit = read(ROOT / 'data/native/map-site-audit-0.49.json')
record = {'version': '0.49.0', 'originalSaveUnchanged': True,
          'originalSaveSHA256': original['sha256'], 'builds': checks,
          'screenshots': screenshots, 'identicalAcrossThreeBuilds': True,
          'siteDispatchAudit': {k: site_audit[k] for k in
                                ['maps', 'genericFallbackClasses', 'genericFallbackInstances']},
          'package': package, 'ruleSources': sources,
          'manualCheck': {'privateProgressCopy': True, 'sourceClass': 92,
                          'troopsBefore': 2, 'troopsAfter': 1, 'experienceGained': 40,
                          'repeatVisitProtected': True, 'returnedToAdventure': True},
          'scope': 'Three reported object rules; no full-map completion claim'}
write(ROOT / 'data/native/validation-0.49.json', record)
write(OUT / 'validation.json', record)
print(json.dumps({'version': record['version'], 'suitesPerBuild': 12,
                  'screenshotsPerBuild': 10, 'identicalScreenshots': True,
                  'originalSaveUnchanged': True}, ensure_ascii=False))
