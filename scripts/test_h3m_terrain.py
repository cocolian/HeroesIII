#!/usr/bin/env python3
"""Independent format boundary tests for the read-only terrain decoder."""
import gzip
import json
import struct
import tempfile
import unittest
from pathlib import Path
from h3m_terrain import Reader, read_terrain
from calibrate_terrain import ASSETS, code_of, geometry, observations
from community_terrain import verified_terrain, scenario_source


def fixture(victory=b'\xff', loss=b'\xff'):
    u32 = lambda n:struct.pack('<I',n)
    result = u32(28)+b'\x01'+u32(36)+b'\0'+u32(4)+b'Test'+u32(0)+b'\x01\0'
    result += bytes(15)*8  # disabled players
    result += victory+loss+b'\0'+bytes(20)+u32(0)+b'\0'
    result += bytes(31+18+9+4)+u32(0)+bytes(156)
    result += bytes([2,49,0,0,0,0,0])*36*36+u32(0)
    return result


class TerrainFormatTests(unittest.TestCase):
    def decode(self,data):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'test.h3m'
            path.write_bytes(data)
            return read_terrain(path)

    def test_uncompressed_and_gzip_identical(self):
        raw = fixture()
        plain,compressed = self.decode(raw),self.decode(gzip.compress(raw))
        self.assertEqual(plain,compressed)
        self.assertEqual(len(plain['tiles']),1296)
        self.assertEqual(plain['tiles'][0],(2,49,0,0,0,0,0))
        self.assertEqual(plain['objectTemplateCount'],0)

    def test_optional_conditions_do_not_shift_tiles(self):
        for victory in [b'\xff']+[bytes([v,1,0])+bytes(n) for v,n in
                        [(0,2),(1,6),(2,5),(3,5),(4,3),(5,3),(6,3),(7,3),(8,0),(9,0),(10,4),(11,0),(12,4)]]:
            for loss in [b'\xff',b'\0'+bytes(3),b'\1'+bytes(3),b'\2'+bytes(2)]:
                self.assertEqual(self.decode(fixture(victory,loss))['tiles'][-1][1],49)

    def test_truncated_map_rejected(self):
        for at in [0,4,10,24,100,400,len(fixture())-1]:
            with self.assertRaises((ValueError,EOFError)):
                self.decode(fixture()[:at])

    def test_unknown_version_and_conditions_rejected(self):
        with self.assertRaises(ValueError):
            self.decode(struct.pack('<I',32)+fixture()[4:])
        with self.assertRaises(ValueError):
            self.decode(fixture(b'\x42\0\0'))

    def test_bad_tile_and_frame_rejected(self):
        raw = bytearray(fixture())
        offset = self.decode(raw)['terrainOffset']
        for field,value in [(0,255),(1,79),(2,5),(3,13),(4,4),(5,17)]:
            malformed = bytearray(raw)
            malformed[offset+field] = value
            with self.assertRaises(ValueError):
                self.decode(malformed)

    def test_oversized_length_rejected(self):
        with self.assertRaises(ValueError):
            Reader(struct.pack('<I',65537)).string()

    def test_real_downloads_have_valid_terrain_blocks(self):
        root = Path(__file__).resolve().parents[1]
        entries = [item for group in json.loads((root/'maps/community/catalog.json').read_text())['maps'] for item in group['mapFiles']]
        self.assertEqual(len(entries),15)
        for entry in entries:
            m = verified_terrain(entry)
            self.assertEqual(len(m['tiles']),m['size']**2*m['levels'],entry['file'])

    def test_conversion_digest_and_path_tampering_rejected(self):
        root = Path(__file__).resolve().parents[1]
        entry = next(item for group in json.loads((root/'maps/community/catalog.json').read_text())['maps'] for item in group['mapFiles'] if item.get('conversion'))
        bad = json.loads(json.dumps(entry));bad['conversion']['sha256']='0'*64
        with self.assertRaises(ValueError):scenario_source(bad)
        bad['conversion']['file']='../outside.h3m'
        with self.assertRaises(ValueError):scenario_source(bad)

    def test_withheld_map_geometry_improves_without_training_leakage(self):
        root = Path(__file__).resolve().parents[1]
        current = json.loads((root/'assets/demo/terrain.json').read_text())
        baseline = json.loads((root/'references/map-review-0.38/baseline-terrain-0.37.json').read_text())['frames']
        training = {Path(s['file']).parent.name for s in current['calibration']['sources']}
        for slug in ['seven-lakes','the-lone-knight','jennifers-dream-journey','jedi-story']:
            self.assertNotIn(slug,training)
            m = read_terrain(root/'maps/community'/slug/'1.h3m')
            before = after = total = 0
            for kind,digits,f,position in observations(m):
                code = code_of(kind,digits)
                if not code:
                    continue
                total += 1
                before += geometry(kind,f) == geometry(kind,baseline[ASSETS[kind]][code][0])
                after += geometry(kind,f) == geometry(kind,current['frames'][ASSETS[kind]][code][0])
            self.assertGreater(total,1000)
            self.assertGreater(after/total,.90)
            self.assertLess(total-after,(total-before)/2)


if __name__ == '__main__':
    unittest.main()
