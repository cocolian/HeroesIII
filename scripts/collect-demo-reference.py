"""Download and verify the public Complete demo archive as a data reference.

The archive is never executed. Expected size and SHA1 come from Debian's
game-data-packager 85.1 metadata; extraction and coverage are separate steps.
"""
import hashlib
import json
import pathlib
import subprocess
import argparse
import shutil

arguments = argparse.ArgumentParser(description=__doc__)
arguments.add_argument('--extract', action='store_true', help='Extract only data, music, map and readme; never the application')
options = arguments.parse_args()

root = pathlib.Path(__file__).resolve().parents[1]
folder = root / 'references/demo'
folder.mkdir(parents=True, exist_ok=True)
archive = folder / 'heroes_3_complete_demo.sit'
url = 'https://archive.org/download/heroes_3_complete_demo/heroes_3_complete_demo.sit'
expected_size = 110524563
expected_sha1 = '7c970cec5706af93f2c17c851ee82b2de54f5d53'

def matches(path):
    return path.exists() and path.stat().st_size == expected_size and hashlib.sha1(path.read_bytes()).hexdigest() == expected_sha1

if not matches(archive):
    partial = archive.with_suffix('.sit.part')
    subprocess.run(['curl', '-L', '--fail', '--retry', '1', '--connect-timeout', '20',
                    '--max-time', '300', '--output', str(partial), url], check=True)
    if not matches(partial):
        raise RuntimeError('Demo archive does not match the recorded Debian reference')
    partial.replace(archive)

previous = json.loads((folder / 'source.json').read_text()) if (folder / 'source.json').exists() else {}
record = {
    **previous,
    'edition': 'Official Macintosh Heroes III Complete demo (public archive)',
    'url': url,
    'reference': 'https://sources.debian.org/data/contrib/g/game-data-packager/85.1/data/heroes3.yaml',
    'file': str(archive.relative_to(root)),
    'size': archive.stat().st_size,
    'sha1': expected_sha1,
    'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
    'verifiedAgainstReference': True,
    'executed': False,
    'extracted': previous.get('extracted', False),
    'coverage': previous.get('coverage', 'Not yet inspected; this is a demo subset, not proof of complete game assets.'),
}
if options.extract:
    if not shutil.which('unar') or not shutil.which('lsar'):
        raise RuntimeError('Install the unar data extractor first (macOS: brew install unar)')
    listing = json.loads(subprocess.check_output(['lsar', '-j', str(archive)]))
    selected = []
    for item in listing['lsarContents']:
        name = item['XADFileName']
        if item.get('XADIsDirectory') or item.get('XADIsResourceFork'):
            continue
        if name.startswith(('Heroes III Demo/data/', 'Heroes III Demo/maps/', 'Heroes III Demo/music/')) or name == 'Heroes III Demo/Read Me':
            selected.append(name)
    # Select exact data filenames. Resource forks sharing a selected data name
    # are written as .rsrc metadata, avoiding unar's skip-fork open failure.
    # Force reuse of this generated extraction directory. Force-skip would
    # skip its existing top-level folder, including newly selected music.
    subprocess.run(['unar', '-o', str(folder / 'extracted'), '-D', '-k', 'visible', '-f', str(archive), *selected], check=True)
    extracted_root = folder / 'extracted'
    for name in selected:
        if not (extracted_root / name).is_file():
            raise RuntimeError('Expected data file was not extracted: ' + name)
    record['extracted'] = True
    record['executableExtracted'] = False
(folder / 'source.json').write_text(json.dumps(record, indent=2) + '\n')
print('Verified public demo archive:', record['size'], 'bytes; SHA256:', record['sha256'])
