import ctypes
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from bk3_assets import Archive, decode_tbl, NEGATE


def make_pp(items):
    return (struct.pack('<II', len(items), sum(len(v) for _, v in items)) +
            b''.join(k.encode().ljust(32, b'\0').translate(NEGATE) for k, _ in items) +
            b''.join(struct.pack('<I', len(v)) for _, v in items) +
            b''.join(v.translate(NEGATE) for _, v in items))


def make_tbl():
    # One record in bucket 'a'; the final 256 bytes are writer scratch.
    header = b''.join(struct.pack('<II', 0 if i <= ord('a') else 80,
                                 1 if i == ord('a') else 0) for i in range(256))
    record = struct.pack('<4I', 80, 0, 3, 0) + b'a.bmp'.ljust(64, b'\0')
    raw = header + record + bytes(256)
    packed = bytearray()
    for i in range(0, len(raw), 8):
        chunk = raw[i:i+8]
        packed.append((1 << len(chunk)) - 1)
        packed.extend(chunk)
    for i in range(len(packed) // 4 * 4):
        packed[i] ^= b'\x2f\xca\xd8\x35'[i % 4]
    return struct.pack('<I', len(raw) ^ 0xa67f54cb) + packed, header + record


class Assets(unittest.TestCase):
    def test_inline_and_binary_payload(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'test.pp'
            p.write_bytes(make_pp([('a.bmp', bytes(range(256))), ('b.txt', b'xyz')]))
            a = Archive(p)
            self.assertEqual(a.read(a.entries[0]), bytes(range(256)))
            self.assertEqual(a.read(a.entries[1]), b'xyz')

    def test_bad_inline_inputs(self):
        valid = make_pp([('ok.bmp', b'0123')])
        cases = [valid[:n] for n in range(len(valid))]
        cases += [valid + b'x', make_pp([('../bad', b'')]),
                  make_pp([('a.bmp', b''), ('A.bmp', b'')]), b'\xff'*8]
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'bad.pp'
            for data in cases:
                p.write_bytes(data)
                with self.assertRaises((ValueError, UnicodeDecodeError)):
                    Archive(p)

    def test_tbl_layout_and_scratch(self):
        encoded, raw = make_tbl()
        self.assertEqual(decode_tbl(encoded), raw)
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'test.pp'
            p.write_bytes(b'abc')
            p.with_suffix('.tbl').write_bytes(encoded)
            a = Archive(p)
            self.assertFalse(a.negated)
            self.assertEqual(a.read(a.entries[0]), b'abc')
            # A TBL-indexed PP is raw payload and can be smaller than an
            # inline PP header. Check the shipped native reader too.
            native = subprocess.run([str(Path(os.environ.get('BK3_BUILD_DIR', ROOT/'build'))/'asset-probe'), str(p)],
                                    capture_output=True, check=True)
            self.assertEqual(native.stdout, b'0\t3\ta.bmp\n')

    def test_tbl_truncation(self):
        data, _ = make_tbl()
        for length in [0, 1, 4, 5, 128, 1024, 2048]:
            with self.assertRaises(ValueError):
                decode_tbl(data[:length])


class NativeImage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lib = ctypes.CDLL(str(Path(os.environ.get('BK3_BUILD_DIR', ROOT/'build')) / ('libassets-test.dylib' if sys.platform == 'darwin' else 'libassets-test.so')))
        class Image(ctypes.Structure):
            _fields_ = [('width', ctypes.c_uint32), ('height', ctypes.c_uint32),
                        ('rgba', ctypes.POINTER(ctypes.c_ubyte))]
        cls.Image = Image
        cls.lib.bk_image_decode.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(Image), ctypes.c_void_p]
        cls.lib.bk_image_free.argtypes = [ctypes.POINTER(Image)]

    def decode(self, data):
        out, error = self.Image(), ctypes.create_string_buffer(256)
        ok = self.lib.bk_image_decode(data, len(data), ctypes.byref(out), error)
        pixels = ctypes.string_at(out.rgba, out.width*out.height*4) if ok else None
        self.lib.bk_image_free(ctypes.byref(out))
        return ok, pixels

    def test_tga_orientation_and_rle(self):
        red, green, blue, white = b'\x00\x00\xff', b'\x00\xff\x00', b'\xff\x00\x00', b'\xff\xff\xff'
        expected = b'\xff\x00\x00\xff\x00\xff\x00\xff\x00\x00\xff\xff\xff\xff\xff\xff'
        for descriptor, order in [(32, [red, green, blue, white]), (0, [blue, white, red, green]),
                                  (48, [green, red, white, blue]), (16, [white, blue, green, red])]:
            header = bytearray(18);header[2]=2;header[12]=2;header[14]=2;header[16]=24;header[17]=descriptor
            self.assertEqual(self.decode(bytes(header)+b''.join(order)), (1, expected))
        header[2]=10;header[17]=32
        self.assertEqual(self.decode(bytes(header)+b'\x83'+red), (1, b'\xff\0\0\xff'*4))
        self.assertEqual(self.decode(bytes(header)+b'\x84'+red)[0], 0)

    def test_truncated_image(self):
        header = bytearray(18);header[2]=2;header[12]=2;header[14]=2;header[16]=32
        valid = bytes(header)+bytes(16)
        for n in range(len(valid)):
            self.assertEqual(self.decode(valid[:n])[0], 0)


if __name__ == '__main__':
    unittest.main()
