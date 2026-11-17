"""Known-pixel fixtures and corruption checks for original binary resource readers."""
import pathlib
import struct
import tempfile
import unittest
import zlib
from h3_resources import Lod, Snd, Def, pcx, FormatError


def definition(encoding, width, height, body, compact_header=False):
    header = struct.pack('<4I', 0x42, width, height, 1) + bytes(768)
    table = struct.pack('<4I', 2, 1, 0, 0) + b'test\0'.ljust(13, b'\0') + struct.pack('<I', 817)
    frame = struct.pack('<4I', len(body), encoding, width, height)
    if not compact_header:
        frame += struct.pack('<4I', width, height, 0, 0)
    return header + table + frame + body


class Resources(unittest.TestCase):
    def pixels(self, data):
        resource = Def(data)
        return resource.pixels(resource.groups[2][0])


    def test_snd_mac_and_windows_names(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / 'known.snd'
            header = struct.pack('<I', 2)
            header += b'LONGNAMEHERE\0wav\0'.ljust(40, b'\0') + struct.pack('<II', 100, 3)
            header += b'Other.WAV\0'.ljust(40, b'\0') + struct.pack('<II', 103, 2)
            path.write_bytes(header + b'abcde')
            reader = Snd(path)
            self.assertEqual(reader.read('longnamehere.wav'), b'abc')
            self.assertEqual(reader.read('OTHER.wav'), b'de')

    def test_snd_reject_ranges_names_and_duplicates(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / 'corrupt.snd'
            good = struct.pack('<I', 1) + b'One\0wav\0'.ljust(40, b'\0') + struct.pack('<II', 52, 1) + b'x'
            for bad in [b'', struct.pack('<I', 20000), good[:-1], good[:44] + struct.pack('<II', 4, 1) + b'x', struct.pack('<I', 1) + b'../out.wav'.ljust(40, b'\0') + good[44:]]:
                path.write_bytes(bad)
                with self.assertRaises(FormatError):
                    Snd(path)
            name = b'One.wav'.ljust(40, b'\0')
            path.write_bytes(struct.pack('<I', 2) + (name + struct.pack('<II', 100, 1)) * 2 + b'x')
            with self.assertRaises(FormatError):
                Snd(path)

    def test_raw_pixels(self):
        self.assertEqual(self.pixels(definition(0, 3, 2, b'\x08\x09\x0a\x0b\x0c\x0d')), bytes(range(8, 14)))

    def test_row_offsets_repetition_and_literal(self):
        # Rows stored in reverse order; one repeated run and a literal run.
        body = struct.pack('<2I', 15, 8) + b'\xff\x02\x0b\x0c\x0d\x01\x00' + b'\x08\x01\xff\x01\x09\x0a'
        self.assertEqual(self.pixels(definition(1, 4, 2, body)), b'\x08\x08\x09\x0a\x0b\x0c\x0d\x01')

    def test_compact_continuous_rows(self):
        body = struct.pack('<H', 2) + b'\x02\xe0\x08' + b'\x41\xe1\x09\x0a'
        self.assertEqual(self.pixels(definition(2, 4, 2, body)), b'\0\0\0\x08\x02\x02\x09\x0a')

    def test_block_offsets_and_last_literal_marker(self):
        body = struct.pack('<4H', 9, 8, 43, 42) + b'\x3f\xff' + bytes(range(32)) + b'\x7f\x9f'
        expected = bytes(range(32)) + b'\x01' * 32 + b'\x04' * 32 + b'\x03' * 32
        self.assertEqual(self.pixels(definition(3, 64, 2, body)), expected)

    def test_compact_original_header(self):
        body = struct.pack('<2I', 8, 10) + b'\x08\x02\x09\x02'
        self.assertEqual(self.pixels(definition(1, 3, 2, body, True)), b'\x08' * 3 + b'\x09' * 3)

    def test_reject_cross_row_run(self):
        with self.assertRaises(FormatError):
            self.pixels(definition(1, 2, 1, struct.pack('<I', 4) + b'\x08\x02'))

    def test_reject_truncated_literal(self):
        with self.assertRaises(FormatError):
            self.pixels(definition(2, 3, 1, b'\x02\0\xe2\x09'))

    def test_reject_offset_table_reference(self):
        with self.assertRaises(FormatError):
            self.pixels(definition(3, 32, 1, b'\0\0\x1f'))

    def test_reject_crop_outside_canvas(self):
        data = bytearray(definition(0, 3, 2, bytes(6)))
        struct.pack_into('<I', data, 817 + 24, 1)
        with self.assertRaises(FormatError):
            Def(data)

    def test_pcx_bgr_pixels(self):
        w, h, encoded = pcx(struct.pack('<3I', 6, 2, 1) + b'\x03\x02\x01\x06\x05\x04')
        self.assertEqual((w, h), (2, 1))
        start = encoded.index(b'IDAT')
        size = struct.unpack('>I', encoded[start - 4:start])[0]
        self.assertEqual(zlib.decompress(encoded[start + 4:start + 4 + size]), b'\0\x01\x02\x03\x04\x05\x06')

    def test_reject_pcx_truncation_and_excess(self):
        data = struct.pack('<3I', 1, 1, 1) + b'\x08' + bytes(768)
        self.assertEqual(pcx(data)[:2], (1, 1))
        for bad in (data[:-1], data + b'\0', struct.pack('<3I', 0, 0, 1)):
            with self.assertRaises(FormatError):
                pcx(bad)

    def test_lod_zlib_and_casefold(self):
        pixels = b'resource payload'
        compressed = zlib.compress(pixels)
        data = b'LOD\0' + struct.pack('<2I', 200, 1) + bytes(80)
        data += b'Example.PCX\0'.ljust(16, b'\0') + struct.pack('<4I', 124, len(pixels), 0, len(compressed)) + compressed
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / 'test.lod'
            path.write_bytes(data)
            self.assertEqual(Lod(path).read('EXAMPLE.pcx'), pixels)
            # Claimed uncompressed size must bound decompression.
            bad = bytearray(data)
            struct.pack_into('<I', bad, 112, len(pixels) - 1)
            path.write_bytes(bad)
            with self.assertRaises(FormatError):
                Lod(path).read('example.pcx')
            # Archive names cannot escape the extraction root.
            path.write_bytes(data[:92] + b'../escape\0'.ljust(16, b'\0') + data[108:])
            with self.assertRaises(FormatError):
                Lod(path)


if __name__ == '__main__':
    unittest.main()
