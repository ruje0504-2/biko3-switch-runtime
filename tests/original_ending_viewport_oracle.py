"""Original ending viewport initialization, float scale and fitted origins."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path

from unicorn.x86_const import (
    UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW,
)
from model_binding import ROOT, library
from original_matrix_oracle import machine


class Viewport(C.Structure):
    _fields_ = [(name, C.c_uint32) for name in ('x', 'y', 'width', 'height')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--output', type=Path,
                        default=ROOT / 'local/original-ending-viewport.json')
    args = parser.parse_args()
    raw = args.exe.read_bytes()
    native = machine(raw)
    duck_initial = struct.unpack('<i', native.mem_read(0x719c5c, 4))[0]
    assert duck_initial == 0, ('original process719c5c initial value', duck_initial)
    lib = library()
    fn = lib.bk_ending_special_viewport
    fn.argtypes = [C.POINTER(Viewport), C.POINTER(Viewport), C.c_void_p]
    fn.restype = C.c_int
    error = C.create_string_buffer(256)
    rng = random.Random(0x4e755e)
    widths = [64, 320, 640, 960, 1280, 1920, 2560, 3840]
    widths += [rng.randrange(16, 8193) for _ in range(4088)]
    cases = rejects = 0
    records = []
    for i, width in enumerate(widths):
        scale = C.c_float(width / 1280.0).value
        native.mem_write(0x721ad0, struct.pack('<f', scale))
        native.reg_write(UC_X86_REG_ESP, 0x2008000)
        native.reg_write(UC_X86_REG_EBP, 0x200a000)
        native.reg_write(UC_X86_REG_FPCW, 0x037f)
        native.emu_start(0x4e755e, 0x4e75aa, count=10000)
        assert native.reg_read(UC_X86_REG_EIP) == 0x4e75aa
        assert native.reg_read(UC_X86_REG_ESP) == 0x2008000
        x, y, w, h, z0, z1 = struct.unpack('<4I2f', native.mem_read(0x6886a8, 24))
        assert (z0, z1) == (0.0, 1.0)
        origin = (160, 0) if i == 3 else (rng.randrange(501), rng.randrange(251))
        content = Viewport(*origin, width, width * 3 // 4)
        actual = Viewport(11, 22, 33, 44)
        valid = w > 0 and h > 0 and x + w <= width and y + h <= content.height
        assert bool(fn(C.byref(actual), C.byref(content), error)) == valid, (i, error.value)
        if valid:
            wanted = (origin[0] + x, origin[1] + y, w, h)
            assert tuple(getattr(actual, name) for name, _ in actual._fields_) == wanted
            # The output may alias the input; this must not shift the origin twice.
            assert fn(C.byref(content), C.byref(content), error)
            assert bytes(content) == bytes(actual)
        else:
            assert bytes(actual) == bytes(Viewport(11, 22, 33, 44))
            rejects += 1
        if i < 8:
            records.append(dict(content_width=width, scale=scale,
                                relative=[x, y, w, h], depth=[z0, z1]))
        cases += 1
    for content in (Viewport(0, 0, 0, 720), Viewport(0, 0, 1280, 720),
                    Viewport(0xffffffff, 0, 960, 720),
                    Viewport(0, 0xffffffff, 960, 720),
                    Viewport(0, 0, 0xffffffff, 0xffffffff)):
        actual = Viewport(11, 22, 33, 44)
        assert not fn(C.byref(actual), C.byref(content), error)
        assert bytes(actual) == bytes(Viewport(11, 22, 33, 44))
        rejects += 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(raw).hexdigest(),
                  native_samples=cases, atomic_rejections=rejects, records=records,
                  duck_process_initial=duck_initial,
                  native_functions=['0x4e755e..0x4e75aa', '0x4a9f10', '0x534398'],
                  scope='Exact viewport initialization and stored float-scale/truncation. '
                        'Original called instructions run without hooks; adding the fitted '
                        'content origin is the explicit platform adaptation. No draw or '
                        'camera-controller equivalence is implied.')
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS ending viewport', cases, 'native samples;', rejects, 'atomic rejections')


if __name__ == '__main__':
    main()
