#!/usr/bin/env python3
"""Read original RGBA atlases and record small regions that actually animate.

This analysis writes JSON only. It never changes or exports image pixels.
The renderer separately verifies all original frames against these regions.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parent.parent
HALLS = ('tbcshall.def', 'tbcshal2.def', 'tbcshal3.def', 'tbcshal4.def')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rgba_png(path):
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Not PNG: ' + str(path))
    compressed = bytearray()
    at = 8
    width = height = color = 0
    palette = alpha = b''
    while at < len(data):
        size, = struct.unpack_from('>I', data, at)
        kind = data[at + 4:at + 8]
        block = data[at + 8:at + 8 + size]
        crc, = struct.unpack_from('>I', data, at + 8 + size)
        if zlib.crc32(kind + block) & 0xffffffff != crc:
            raise ValueError('PNG CRC mismatch: ' + str(path))
        if kind == b'IHDR':
            width, height, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', block)
            if not (0 < width * height <= 4194304 and depth == 8 and color in (2, 3, 6)
                    and compression == filtering == interlace == 0):
                raise ValueError('Expected bounded noninterlaced RGB/indexed/RGBA8 atlas')
        elif kind == b'PLTE':
            palette = block
        elif kind == b'tRNS':
            alpha = block
        elif kind == b'IDAT':
            compressed.extend(block)
        elif kind == b'IEND':
            break
        at += size + 12
    raw = zlib.decompress(compressed)
    channels = {2: 3, 3: 1, 6: 4}[color]
    stride = width * channels
    if len(raw) != (stride + 1) * height:
        raise ValueError('Unexpected PNG scanline size')
    result = bytearray(stride * height)
    for y in range(height):
        mode = raw[y * (stride + 1)]
        row = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if mode not in range(5):
            raise ValueError('Unknown PNG filter')
        for x in range(stride) if mode else ():
            left = row[x - channels] if x >= channels else 0
            up = result[(y - 1) * stride + x] if y else 0
            corner = result[(y - 1) * stride + x - channels] if y and x >= channels else 0
            if mode == 1:
                predictor = left
            elif mode == 2:
                predictor = up
            elif mode == 3:
                predictor = (left + up) // 2
            else:
                p = left + up - corner
                a, b, c = abs(p - left), abs(p - up), abs(p - corner)
                predictor = left if a <= b and a <= c else up if b <= c else corner
            row[x] = (row[x] + predictor) & 255
        result[y * stride:(y + 1) * stride] = row
    if color == 3:
        if not palette or len(palette) % 3 or max(result) >= len(palette) // 3:
            raise ValueError('Invalid PNG palette')
        pixels = [tuple(palette[i * 3:i * 3 + 3]) + (alpha[i] if i < len(alpha) else 255,) for i in result]
    elif color == 2:
        pixels = [(r, g, b, 255) for r, g, b in zip(result[::3], result[1::3], result[2::3])]
    else:
        pixels = list(zip(result[::4], result[1::4], result[2::4], result[3::4]))
    return width, height, pixels


def over(a, b):
    if a[3] == 255:
        return a
    if not a[3]:
        return b
    denominator = a[3] * 255 + b[3] * (255 - a[3])
    rgb = tuple((a[c] * a[3] * 255 + b[c] * b[3] * (255 - a[3])
                 + denominator // 2) // denominator for c in range(3))
    return rgb + ((denominator + 127) // 255,)


def regions(changed, width, height):
    pending = {(y + dy) * width + x + dx
               for i in changed for x, y in [(i % width, i // width)]
               for dy in range(-2, 3) for dx in range(-2, 3)
               if 0 <= x + dx < width and 0 <= y + dy < height}
    boxes = []
    while pending:
        todo = [pending.pop()]
        part = []
        while todo:
            i = todo.pop()
            part.append(i)
            x, y = i % width, i // width
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                j = (y + dy) * width + x + dx
                if 0 <= x + dx < width and 0 <= y + dy < height and j in pending:
                    pending.remove(j)
                    todo.append(j)
        xs, ys = [i % width for i in part], [i // width for i in part]
        boxes.append([min(xs), min(ys), max(xs) + 1, max(ys) + 1])
    # Component bounding boxes may overlap even when the components do not.
    # Merge them so the renderer never blends an original patch twice.
    while True:
        pair = next(((i, j) for i, a in enumerate(boxes) for j, b in enumerate(boxes)
                     if i < j and max(a[0], b[0]) < min(a[2], b[2])
                     and max(a[1], b[1]) < min(a[3], b[3])), None)
        if pair is None:
            break
        i, j = pair
        a, b = boxes[i], boxes.pop(j)
        boxes[i] = [min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3])]
    return sorted([[a, b, c - a, d - b] for a, b, c, d in boxes])


def analyze():
    manifest = ROOT / 'assets/demo/manifest.json'
    data = json.loads(manifest.read_text())
    records = {}
    for key in HALLS:
        animation = data['def'][key]
        frames = animation['groups']['0']
        width, height = frames[0]['canvas']
        pages = [rgba_png(ROOT / p['file']) for p in animation['pages']]
        base = [(0, 0, 0, 0)] * (width * height)

        def compose(frame, canvas):
            result = list(canvas)
            pw, _, pixels = pages[frame['page']]
            sx, sy, fw, fh = frame['rect']
            ox, oy = frame['offset']
            for y in range(fh):
                for x in range(fw):
                    index = (oy + y) * width + ox + x
                    result[index] = over(pixels[(sy + y) * pw + sx + x], result[index])
            return result

        base = compose(frames[0], base)
        changed = set()
        for frame in frames[1:]:
            current = compose(frame, base)
            changed.update(i for i, (a, b) in enumerate(zip(base, current)) if a != b)
        patches = regions(changed, width, height)
        records[key] = {'canvas': [width, height], 'frameCount': len(frames),
                        'originalRegions': patches, 'changedPixels': len(changed),
                        'originalRegionArea': sum(r[2] * r[3] for r in patches),
                        'originalSource': animation['source'],
                        'atlasSha256': {p['file']: digest(ROOT / p['file']) for p in animation['pages']}}
    return {'schema': 1, 'sourceManifestSha256': digest(manifest),
            'method': 'Union of original RGBA frame changes, 2-pixel dilation, connected-component bounding boxes, overlapping boxes merged; JSON analysis only',
            'rendererMode': 'original-cloth', 'animations': records,
            'limits': 'Runtime derives a blue/gold cloth matte inside these regions, omitting the original rectangular backdrop. Matte/fringe extraction is a project approximation, not newly drawn 2K cloth. Timing remains the project 100 ms/frame baseline.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / 'references/hd-art/hall-motion-regions.json')
    args = parser.parse_args()
    result = analyze()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n')
    for key, value in result['animations'].items():
        print(key, value['changedPixels'], value['originalRegions'], value['originalRegionArea'])
