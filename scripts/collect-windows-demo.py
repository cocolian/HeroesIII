"""Download and verify the public 1999 Windows demo without executing it."""
import hashlib
import json
import pathlib
import subprocess
import argparse

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--extract', action='store_true', help='Extract only cabinets and whitelisted data/media files')
options = parser.parse_args()

root = pathlib.Path(__file__).resolve().parents[1]
folder = root / 'references/windows-demo'
folder.mkdir(parents=True, exist_ok=True)
archive = folder / 'h3demo.exe'
url = 'https://raw.githubusercontent.com/GeorgeK1ng/heroes3demo/main/h3demo.exe'
expected_size = 98265058
expected_sha256 = '83cbb6319386d6deba65273f53a60395fda8cce74cc24177b2ac3560ded9c315'

def verified(path):
    return (path.exists() and path.stat().st_size == expected_size
            and hashlib.sha256(path.read_bytes()).hexdigest() == expected_sha256)

if not verified(archive):
    temporary = archive.with_suffix('.download')
    subprocess.run(['curl', '--fail', '--location', '--retry', '2', '--max-time', '120',
                    '--max-filesize', '120000000', '--output', str(temporary), url], check=True)
    if not verified(temporary):
        raise ValueError('Windows demo size/SHA256 did not match the independent public record')
    temporary.replace(archive)

record = json.loads((folder / 'source.json').read_text()) if (folder / 'source.json').exists() else {}
record.update({'edition': 'Official Windows Heroes III Restoration of Erathia demo, 1999',
          'url': url, 'mirrorPage': 'https://github.com/GeorgeK1ng/heroes3demo',
          'reference': 'https://github.com/vcmi/vcmi/issues/2126',
          'referencePurpose': 'Independent archive size/SHA256 and data-only extraction notes',
          'file': str(archive.relative_to(root)), 'size': expected_size,
          'sha256': expected_sha256, 'verifiedAgainstReference': True,
          'executed': False, 'gameCodeImported': False})
if options.extract:
    cabinets = folder / 'cabinet'
    subprocess.run(['7zz', 'e', '-y', '-o' + str(cabinets), str(archive),
                    '/Disk1/data1.cab', '/Disk1/data1.hdr'], check=True)
    if sorted(path.name for path in cabinets.iterdir()) != ['data1.cab', 'data1.hdr']:
        raise ValueError('Unexpected installer file extracted')
    cabinet = cabinets / 'data1.cab'
    listing = subprocess.check_output(['unshield', 'l', str(cabinet)], text=True)
    (folder / 'cabinet-list.txt').write_text(listing)
    selected = []
    for line in listing.splitlines():
        parts = line.strip().split(None, 1)
        if len(parts) != 2 or not parts[0].isdigit():
            continue
        name = parts[1].rsplit('\\', 1)[-1]
        if name.lower() in ['h3bitmap.lod', 'h3sprite.lod', 'heroes3.snd', 'video.vid', 'h3demo.h3m', 'readme.txt'] or name.lower().endswith('.mp3'):
            selected.append(name)
    if len(selected) != 20:
        raise ValueError('Unexpected Windows demo data listing')
    extracted = folder / 'extracted'
    subprocess.run(['unshield', '-d', str(extracted), 'x', str(cabinet), *selected], check=True)
    inventory = []
    for path in sorted(extracted.rglob('*')):
        if path.is_file():
            if path.name not in selected:
                raise ValueError('Unexpected game code extracted: ' + str(path))
            inventory.append({'file': str(path.relative_to(root)), 'size': path.stat().st_size,
                              'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    if len(inventory) != len(selected):
        raise ValueError('Incomplete Windows demo data extraction')
    record.update({'extracted': True, 'executableExtracted': False,
                   'dataExtractedFromVerifiedArchive': True, 'files': inventory,
                   'extractors': {'7zz': '26.06 (Homebrew)', 'unshield': '1.6.2 (Homebrew)'},
                   'dataChecksumSource': 'Self-recorded after extraction from the independently verified archive'})
(folder / 'source.json').write_text(json.dumps(record, indent=2) + '\n')
print('Verified Windows demo:', expected_size, 'bytes;', expected_sha256)
