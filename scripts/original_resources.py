"""Verified original data catalog: Complete overrides RoE, never the reverse."""
import hashlib
import json
import pathlib
from h3_resources import Lod

WINDOWS_SIZE = 98265058
WINDOWS_SHA256 = '83cbb6319386d6deba65273f53a60395fda8cce74cc24177b2ac3560ded9c315'
MAC_FILES = {
    'h3bitmap.lod': (24721482, '6242e1399f650ae72fa99806f9330a9d'),
    'h3sprite.lod': (49501869, '3c9c7637d1aee5dd0fb8c2b91d58c217'),
    'h3ab_spr.lod': (7402663, 'e79f491203c701d81646315ff6019bfb'),
    'heroes3.snd': (12018808, 'd032957a7dd6b1a9c2e3a62b7ed7f73d'),
    'video.vid': (5853304, 'b8853018665fa515a2d472a10f374eee'),
}

def windows_files(root):
    """Return only individually verified media from the verified demo archive."""
    record = root / 'references/windows-demo/source.json'
    if not record.exists():
        return {}
    metadata = json.loads(record.read_text())
    if not metadata.get('extracted'):
        return {}
    archive = root / 'references/windows-demo/h3demo.exe'
    if archive.stat().st_size != WINDOWS_SIZE or hashlib.sha256(archive.read_bytes()).hexdigest() != WINDOWS_SHA256:
        raise ValueError('Windows demo archive checksum mismatch')
    result = {}
    for entry in metadata['files']:
        relative = pathlib.PurePosixPath(entry['file'])
        if relative.is_absolute() or '..' in relative.parts or not str(relative).startswith('references/windows-demo/extracted/Program_Files/'):
            raise ValueError('Unsafe Windows data path')
        path = root / relative
        if path.suffix.lower() not in {'.lod', '.snd', '.vid', '.h3m', '.mp3', '.txt'}:
            raise ValueError('Unexpected Windows demo file type')
        data = path.read_bytes()
        if len(data) != entry['size'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise ValueError('Windows demo data checksum mismatch: ' + str(relative))
        key = path.name.lower()
        if key in result:
            raise ValueError('Duplicate Windows data filename')
        result[key] = path
    if len(result) != 20:
        raise ValueError('Incomplete Windows data inventory')
    return result

class OriginalResources:
    def __init__(self, root):
        self.root = pathlib.Path(root)
        self.entries, self.archives, self.inventory = {}, {}, []
        self.windows = windows_files(self.root)
        sources = []
        for key in ('h3bitmap.lod', 'h3sprite.lod'):
            if key in self.windows:
                sources.append(('windows/' + key, self.windows[key], 'windows-roe-demo'))
        base = self.root / 'references/demo/extracted/Heroes III Demo/data'
        for name, (size, expected_md5) in MAC_FILES.items():
            path = base / name
            data = path.read_bytes()
            if len(data) != size or hashlib.md5(data).hexdigest() != expected_md5:
                raise ValueError('Mac Complete demo checksum mismatch: ' + name)
            self.archives[name] = {'size': size, 'md5': expected_md5, 'sha256': hashlib.sha256(data).hexdigest()}
        sources.extend((name, base / name, 'mac-complete-demo') for name in ('h3bitmap.lod', 'h3sprite.lod', 'h3ab_spr.lod'))
        for name, path, edition in sources:
            lod = Lod(path)
            self.archives[name] = {**self.archives.get(name, {}), 'size': len(lod.data),
                                   'sha256': hashlib.sha256(lod.data).hexdigest(),
                                   'file': str(path.relative_to(self.root)), 'edition': edition}
            for key, entry in lod.entries.items():
                data = lod.read(key)
                provenance = {'archive': name, 'name': entry.name, 'size': len(data),
                              'sha256': hashlib.sha256(data).hexdigest(), 'edition': edition}
                self.inventory.append(provenance)
                self.entries[key] = (lod, provenance)

    def read(self, name):
        lod, _ = self.entries[name.lower()]
        return lod.read(name)

    def source(self, name):
        return self.entries[name.lower()][1]
