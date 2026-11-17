"""Bounded, read-only SoD terrain decoder; no map objects or gameplay imported.

Layout reference: alexanderbelous/h3mtxt/doc/h3m_specification.MD.
Only the documented header and seven-byte tile records are interpreted.
"""
import gzip
import io
import struct
from pathlib import Path


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, length):
        if length < 0 or self.pos + length > len(self.data):
            raise ValueError('Truncated H3M at byte %d' % self.pos)
        result = self.data[self.pos:self.pos+length]
        self.pos += length
        return result

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return struct.unpack('<H', self.take(2))[0]

    def u32(self):
        return struct.unpack('<I', self.take(4))[0]

    def count(self, limit):
        n = self.u32()
        if n > limit:
            raise ValueError('H3M count exceeds limit')
        return n

    def string(self):
        return self.take(self.count(65536))


def read_terrain(path):
    data = Path(path).read_bytes()
    if len(data) > 16*1024*1024:
        raise ValueError('Oversized H3M')
    if data[:2] == b'\x1f\x8b':
        with gzip.GzipFile(fileobj=io.BytesIO(data)) as stream:
            data = stream.read(64*1024*1024+1)
    if len(data) > 64*1024*1024:
        raise ValueError('Oversized H3M expansion')
    r = Reader(data)
    if r.u32() != 28:
        raise ValueError('Only classic Shadow of Death version 28 is supported')
    r.take(1)
    size = r.u32()
    if size not in (36, 72, 108, 144):
        raise ValueError('Unsupported map size')
    levels = 2 if r.u8() else 1
    name, description = r.string(), r.string()
    difficulty = r.u8()
    r.take(1)
    for _ in range(8):
        human, computer = r.u8(), r.u8()
        if not (human or computer):
            r.take(13)
            continue
        r.take(5)  # behavior, custom alignments, alignments, random alignment
        if r.u8():
            r.take(5)
        r.take(1)  # random hero
        if r.u8() != 255:
            r.take(1)
            r.string()
        r.take(1)  # nonspecific placeholders
        for _ in range(r.count(156)):
            r.take(1)
            r.string()
    victory = r.u8()
    if victory != 255:
        r.take(2)
        lengths = {0:2, 1:6, 2:5, 3:5, 4:3, 5:3, 6:3, 7:3,
                   8:0, 9:0, 10:4, 11:0, 12:4}
        if victory not in lengths:
            raise ValueError('Unknown victory condition')
        r.take(lengths[victory])
    loss = r.u8()
    if loss != 255:
        if loss not in (0, 1, 2):
            raise ValueError('Unknown loss condition')
        r.take(2 if loss == 2 else 3)
    if r.u8():
        r.take(8)
    r.take(20)  # allowed heroes
    r.take(r.count(156))
    for _ in range(r.u8()):
        r.take(2)
        r.string()
        r.take(1)
    r.take(31+18+9+4)  # reserved, disabled artifacts/spells/skills
    for _ in range(r.count(4096)):
        r.string()
        r.string()
    for _ in range(156):
        if not r.u8():
            continue
        if r.u8():
            r.take(4)
        if r.u8():
            r.take(2*r.count(28))
        if r.u8():
            r.take(38)
            r.take(2*r.u16())
        if r.u8():
            r.string()
        r.take(1)  # gender
        if r.u8():
            r.take(9)
        if r.u8():
            r.take(4)
    offset = r.pos
    tiles = [tuple(r.take(7)) for _ in range(size*size*levels)]
    for terrain, view, river, river_view, road, road_view, flags in tiles:
        if terrain > 9 or river > 4 or road > 3 or river_view > 12 or road_view > 16:
            raise ValueError('Invalid terrain/road/river record')
        limits = [46, 24, 79, 79, 79, 79, 79, 79, 33, 48]
        if view >= limits[terrain]:
            raise ValueError('Invalid original terrain frame')
    # Sanity-check the next section without interpreting object instances.
    templates = r.count(4096)
    return dict(size=size, levels=levels, name=name, description=description,
                difficulty=difficulty, tiles=tiles, terrainOffset=offset,
                objectTemplateCount=templates)


if __name__ == '__main__':
    import sys
    for path in sys.argv[1:]:
        result = read_terrain(path)
        print(path, result['size'], result['levels'], result['terrainOffset'],
              result['objectTemplateCount'])
