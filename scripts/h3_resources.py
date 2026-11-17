"""Independent, bounded readers for classic H3 LOD / PCX / DEF data.

Only data is decoded. No game executable or third-party parser is loaded.
PNG output uses the standard library and retains the original indexed pixels.
Format references and the actual demo checksums are in references/demo/.
"""
from dataclasses import dataclass
import pathlib
import re
import struct
import zlib


class FormatError(ValueError):
    pass


class Snd:
    """Bounded original SND directory. Mac names store stem/NUL/extension."""
    def __init__(self, path):
        self.data = pathlib.Path(path).read_bytes()
        count, = numbers(self.data, 0, 1)
        end = 4 + count * 48
        if not 0 < count <= 16384 or end > len(self.data):
            raise FormatError('Invalid SND directory')
        self.entries = {}
        for index in range(count):
            at = 4 + index * 48
            raw = take(self.data, at, 40)
            parts = [part for part in raw.split(b'\0') if part]
            if len(parts) not in (1, 2):
                raise FormatError('Invalid SND filename')
            try:
                name = b'.'.join(parts).decode('ascii')
            except UnicodeDecodeError as error:
                raise FormatError('Invalid SND filename') from error
            if not re.fullmatch(r'[A-Za-z0-9_-]+\.wav', name, re.I):
                raise FormatError('Unsafe or unsupported SND filename')
            offset, size = numbers(self.data, at + 40, 2)
            if offset < end or not 0 < size <= 16777216 or offset + size > len(self.data):
                raise FormatError('Invalid SND file range')
            key = name.lower()
            if key in self.entries:
                raise FormatError('Duplicate SND filename')
            self.entries[key] = (offset, size)

    def read(self, name):
        offset, size = self.entries[name.lower()]
        return take(self.data, offset, size)


def take(data, offset, size):
    if offset < 0 or size < 0 or offset + size > len(data):
        raise FormatError(f'Truncated resource at {offset}, need {size}, have {len(data)}')
    return data[offset:offset + size]


def numbers(data, offset, count, code='I'):
    size = struct.calcsize('<' + code * count)
    return struct.unpack('<' + code * count, take(data, offset, size))


def dimensions(width, height):
    if not 0 < width <= 8192 or not 0 < height <= 8192 or width * height > 16777216:
        raise FormatError(f'Invalid image dimensions {width} x {height}')


@dataclass(frozen=True)
class Entry:
    name: str
    offset: int
    size: int
    kind: int
    compressed: int


class Lod:
    def __init__(self, path):
        self.path = pathlib.Path(path)
        self.data = self.path.read_bytes()
        if take(self.data, 0, 4) != b'LOD\0':
            raise FormatError('Not an H3 LOD archive')
        self.version, count = numbers(self.data, 4, 2)
        if count > 100000:
            raise FormatError('Excessive archive directory')
        take(self.data, 92, count * 32)
        self.entries = {}
        for index in range(count):
            start = 92 + index * 32
            name = take(self.data, start, 16).split(b'\0')[0].decode('ascii')
            if not name or '/' in name or '\\' in name or name in ('.', '..'):
                raise FormatError('Invalid archive entry name')
            offset, size, kind, compressed = numbers(self.data, start + 16, 4)
            if offset < 92 + count * 32 or size > 268435456:
                raise FormatError(f'Invalid archive entry: {name}')
            take(self.data, offset, compressed or size)
            key = name.lower()
            if key in self.entries:
                raise FormatError(f'Duplicate archive entry: {name}')
            self.entries[key] = Entry(name, offset, size, kind, compressed)

    def read(self, name):
        entry = self.entries[name.lower()]
        data = take(self.data, entry.offset, entry.compressed or entry.size)
        if entry.compressed:
            stream = zlib.decompressobj()
            data = stream.decompress(data, entry.size + 1)
            if not stream.eof or stream.unused_data or stream.unconsumed_tail:
                raise FormatError(f'Invalid compressed stream: {entry.name}')
        if len(data) != entry.size:
            raise FormatError(f'Archive size mismatch: {entry.name}')
        return data


def chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))


def png(width, height, pixels, palette=None, alpha=None):
    dimensions(width, height)
    channels = 1 if palette is not None else 3
    if len(pixels) != width * height * channels:
        raise FormatError('PNG pixel buffer size mismatch')
    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 3 if palette is not None else 2, 0, 0, 0))
    if palette is not None:
        if len(palette) != 768:
            raise FormatError('Invalid palette')
        out += chunk(b'PLTE', palette)
        if alpha is not None:
            out += chunk(b'tRNS', bytes(alpha))
    stride = width * channels
    rows = b''.join(b'\0' + pixels[y * stride:(y + 1) * stride] for y in range(height))
    return out + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b'')


def pcx(data):
    size, width, height = numbers(data, 0, 3)
    dimensions(width, height)
    if size == width * height:
        pixels = take(data, 12, size)
        palette = take(data, 12 + size, 768)
        if len(data) != 12 + size + 768:
            raise FormatError('Unexpected indexed PCX trailing data')
        return width, height, png(width, height, pixels, palette)
    if size == width * height * 3 and len(data) == 12 + size:
        # H3 24-bit PCX stores BGR, top row first.
        bgr = take(data, 12, size)
        rgb = bytearray(size)
        rgb[0::3], rgb[1::3], rgb[2::3] = bgr[2::3], bgr[1::3], bgr[0::3]
        return width, height, png(width, height, rgb)
    raise FormatError('Unsupported H3 PCX size')


@dataclass(frozen=True)
class Frame:
    name: str
    offset: int
    size: int
    format: int
    full_width: int
    full_height: int
    width: int
    height: int
    left: int
    top: int
    header_size: int = 32


class Def:
    def __init__(self, data):
        self.data = data
        self.kind, self.width, self.height, count = numbers(data, 0, 4)
        if count > 1024:
            raise FormatError('Excessive animation groups')
        self.palette = take(data, 16, 768)
        self.groups = {}
        position = 784
        for _ in range(count):
            group, frames, _, _ = numbers(data, position, 4)
            if group in self.groups or frames > 10000:
                raise FormatError('Invalid animation group')
            position += 16
            names = take(data, position, frames * 13)
            position += frames * 13
            offsets = numbers(data, position, frames)
            position += frames * 4
            sequence = []
            for index, offset in enumerate(offsets):
                size, encoding, fw, fh = numbers(data, offset, 4)
                dimensions(fw, fh)
                first = numbers(data, offset + 16, 1)[0]
                header_size = 32
                # Original SGTWMTA/B use a compact format-1 header followed
                # immediately by the full-height row table (no crop rectangle).
                if encoding == 1 and first == fh * 4 and first > fw:
                    width, height, left, top, header_size = fw, fh, 0, 0, 16
                else:
                    width, height, left, top = numbers(data, offset + 16, 4)
                dimensions(width, height)
                if left + width > fw or top + height > fh or encoding not in (0, 1, 2, 3):
                    raise FormatError('Invalid DEF frame geometry or encoding')
                take(data, offset + header_size, size)
                name = names[index * 13:(index + 1) * 13].split(b'\0')[0].decode('ascii')
                sequence.append(Frame(name, offset, size, encoding, fw, fh, width, height, left, top, header_size))
            self.groups[group] = sequence

    def pixels(self, frame):
        body = take(self.data, frame.offset + frame.header_size, frame.size)
        w, h = frame.width, frame.height
        if frame.format == 0:
            return take(body, 0, w * h)
        pixels = bytearray(w * h)

        def decode(position, length, compact):
            result = bytearray()
            while len(result) < length:
                tag = take(body, position, 1)[0]
                position += 1
                if compact:
                    color, run = tag >> 5, (tag & 31) + 1
                    literal = color == 7
                else:
                    color, run = tag, take(body, position, 1)[0] + 1
                    position += 1
                    literal = color == 255
                if len(result) + run > length:
                    raise FormatError('DEF run crosses row/block boundary')
                if literal:
                    result.extend(take(body, position, run))
                    position += run
                else:
                    result.extend(bytes([color]) * run)
            return result, position

        if frame.format == 1:
            offsets = numbers(body, 0, h)
            for y, offset in enumerate(offsets):
                if offset < 4 * h:
                    raise FormatError('DEF row points into offset table')
                pixels[y * w:(y + 1) * w], _ = decode(offset, w, False)
        elif frame.format == 2:
            position = numbers(body, 0, 1, 'H')[0]
            if position < 2:
                raise FormatError('DEF compact data points into offset')
            for y in range(h):
                pixels[y * w:(y + 1) * w], position = decode(position, w, True)
        else:
            if w % 32:
                raise FormatError('DEF format 3 requires 32-pixel blocks')
            blocks = w // 32
            offsets = numbers(body, 0, h * blocks, 'H')
            for y in range(h):
                for block in range(blocks):
                    offset = offsets[y * blocks + block]
                    if offset < 2 * h * blocks:
                        raise FormatError('DEF block points into offset table')
                    start = y * w + block * 32
                    pixels[start:start + 32], _ = decode(offset, 32, True)
        return bytes(pixels)

    def display_palette(self):
        palette = bytearray(self.palette)
        alpha = bytearray([255] * 256)
        alpha[0] = 0
        # Marker indices are not ordinary RGB colors. Keep original pixels
        # intact; resolve shadow/selection markers only in the output palette.
        # Exact original compositing and player-color replacement need an
        # independent executable comparison (documented in parity.md).
        if self.palette[:3] == b'\0\xff\xff':
            for index, opacity in enumerate((0, 64, 64, 128, 128, 0, 128, 64)):
                palette[index * 3:index * 3 + 3] = b'\0\0\0'
                alpha[index] = opacity
        return palette, alpha

    def frame_png(self, frame):
        palette, alpha = self.display_palette()
        return png(frame.width, frame.height, self.pixels(frame), palette, alpha)
