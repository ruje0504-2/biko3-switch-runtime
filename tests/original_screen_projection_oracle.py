"""Original screen frame projection, native lens and rectangle matrix setup."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from original_camera_oracle import Lens
from model_binding import ROOT, library

Matrix = C.c_float*16
class Viewport(C.Structure):
    _fields_ = [(k, C.c_uint32) for k in ['x', 'y', 'width', 'height']]
class Screen(C.Structure):
    _fields_ = [('position', C.c_int32*2), ('depth', C.c_float)]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe')
    args = p.parse_args()
    exe = open(args.exe, 'rb').read()
    u, lib = machine(exe), library()
    lib.bk_camera_project_frame.argtypes = [C.POINTER(Screen), Matrix, Matrix, C.POINTER(Lens), C.POINTER(Viewport)]
    stack, stop, out = 0x2008000, 0x300f000, 0x3005000
    u.mem_write(0x6455a0, struct.pack('<I', 0x3002000))
    u.mem_write(0x3002000, struct.pack('<I', 0x3003000))
    u.mem_write(0x300303c, struct.pack('<I', 0x3004000))
    def rectangle(uc, address, size, data):
        sp = uc.reg_read(UC_X86_REG_ESP)
        ret, device, dest = struct.unpack('<III', uc.mem_read(sp, 12))
        assert device == 0x3002000
        uc.mem_write(dest, struct.pack('<4I', vp.x, vp.y, vp.x+vp.width, vp.y+vp.height))
        uc.reg_write(UC_X86_REG_EIP, ret)
        uc.reg_write(UC_X86_REG_ESP, sp+12)
        uc.reg_write(UC_X86_REG_EAX, 0)
    u.hook_add(UC_HOOK_CODE, rectangle, begin=0x3004000, end=0x3004000)
    def call(address, args, float_result=False):
        if float_result:
            u.mem_write(stop, b'\xd9\x1d'+struct.pack('<I', out+8)) # fstp depth
        u.mem_write(stack, struct.pack('<I', stop)+args)
        u.reg_write(UC_X86_REG_ESP, stack)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        end = stop+6 if float_result else stop
        u.emu_start(address, end, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == end
    # Actual graphics initialization427c4b uses this identity setter first.
    call(0x409580, struct.pack('<I', 0x642af0))
    rng = random.Random(0x42d56c)
    worst = 0.0
    behind = rejects = 0
    for case in range(12000):
        frame, view = Matrix(*(rng.uniform(-2, 2) for _ in range(16))), Matrix(*(rng.uniform(-2, 2) for _ in range(16)))
        for m in [frame, view]:
            m[3], m[7], m[11], m[15] = 0, 0, 0, rng.choice([.5, 1, 2])
            m[12], m[13], m[14] = (rng.uniform(-300, 300) for _ in range(3))
        lens = Lens(rng.uniform(.2, 2.5), rng.uniform(.4, 1.5), rng.uniform(.1, 10), rng.uniform(1000, 126384))
        vp = Viewport(rng.randrange(50), rng.randrange(50), rng.choice([640, 1024, 1280, 1920]), rng.choice([480, 720, 768, 1080]))
        call(0x523a10, struct.pack('<I', 0x642830)+bytes(lens))
        call(0x42cfdc, b'')
        u.mem_write(0x642fa8, bytes(view))
        u.mem_write(0x3000000, bytes(frame))
        call(0x42d56c, struct.pack('<II', 0x3000000, out), True)
        expected = Screen.from_buffer_copy(bytes(u.mem_read(out, 12)))
        actual = Screen()
        assert lib.bk_camera_project_frame(C.byref(actual), frame, view, C.byref(lens), C.byref(vp))
        assert tuple(actual.position) == tuple(expected.position), (case, list(actual.position), list(expected.position))
        delta = abs(actual.depth-expected.depth)/max(1, abs(expected.depth))
        worst = max(worst, delta)
        assert delta < 2e-6, (case, actual.depth, expected.depth)
        behind += actual.depth > 1
        if case%10 == 0:
            saved = bytes(actual)
            frame[:] = [0]*16
            assert not lib.bk_camera_project_frame(C.byref(actual), frame, view, C.byref(lens), C.byref(vp))
            assert bytes(actual) == saved
            rejects += 1
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=12000,
                  native_functions=['42d56c', '523a10', '42cfdc', '522d9a', '534398'],
                  max_relative_error=worst, depth_over_one_cases=behind, zero_w_rejections=rejects,
                  hooks=['Device rectangle query only'],
                  scope='Cached frame/view matrix inputs, original lens and viewport construction, all three matrix compositions, truncate XY/depth. No actor/cache publication or gameplay camera selection.')
    (ROOT/'local/original-screen-projection-oracle.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))

if __name__ == '__main__':
    main()
