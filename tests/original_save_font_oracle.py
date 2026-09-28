"""Japanese checkpoint alphabet using original Type_G.FTT, original474a58 and
475514 instructions. Surface allocation/blits are boundary hooks; original
metrics and every used glyph bitmap execute unmodified. No new font is drawn.
"""
import argparse
import ctypes as C
import hashlib
import json
import struct
from pathlib import Path
from original_font_oracle import Native, Glyph
from model_binding import ROOT, library


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('font', type=Path)
    args = ap.parse_args()
    exe, raw = args.exe.read_bytes(), args.font.read_bytes()
    native, lib, error = Native(exe, raw), library(), C.create_string_buffer(256)
    lib.bk_font_decode.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p]
    lib.bk_font_decode.restype = C.c_void_p
    lib.bk_font_destroy.argtypes = [C.c_void_p]
    lib.bk_font_glyph.argtypes = [C.c_void_p, C.c_uint16, C.POINTER(Glyph)]
    font = lib.bk_font_decode(raw, len(raw), error)
    assert font, error.value
    codes = [int.from_bytes(ch.encode('cp932'), 'big') for ch in '０１２３４５６７８９／－：エリア']
    pixels = 0
    try:
        for code in codes:
            glyph = Glyph()
            assert lib.bk_font_glyph(font, code, C.byref(glyph))
            assert glyph.advance == native.glyph(code)
            offset, pitch, width, height, _ = struct.unpack_from('<I4H', raw, 4 + (code - 0x8000) * 12)
            assert (glyph.row_bytes, glyph.width, glyph.height, glyph.advance_width) == (pitch, pitch * 8, height, width)
            assert native.blits == [(0x1234, 100 + glyph.offset_x, 200 + glyph.offset_y, native.surface, 0, 0, glyph.width, glyph.height, 0)]
            bits = C.string_at(glyph.bits, pitch * height)
            assert bits == raw[0x60004+offset:0x60004+offset+pitch*height]
            expected = b''.join(b'\xff\xff' if b & (0x80 >> bit) else b'\0\0' for b in bits for bit in range(8))
            assert native.bitmap(0x4100000+offset, pitch*8, height) == expected
            pixels += pitch * 8 * height
    finally:
        lib.bk_font_destroy(font)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), font_sha256=hashlib.sha256(raw).hexdigest(), glyphs=len(codes), pixels=pixels, scope=__doc__)
    (ROOT/'local/original-save-font-jp-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS save-font', report)


if __name__ == '__main__':
    main()
