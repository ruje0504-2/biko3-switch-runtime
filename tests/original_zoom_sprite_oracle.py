"""Original50e633/50e6ba enter2/exit0/idle0 and43ed45 geometry.
Only texture creation and vertex-buffer lock/unlock/draw are substituted.
No hooks in transition, scale, pivot, alpha or vertex calculations.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_player_hud_oracle import Native as HudNative
from original_item_notice_oracle import Fade
from model_binding import ROOT, library

class Zoom(C.Structure):
    _fields_ = [('fade', Fade), ('scale', C.c_float*2), ('pivot', C.c_float*2)]

def snapshot(s):
    return (s.fade.alpha, s.fade.speed, s.fade.stage, *s.scale, *s.pivot)

class Native(HudNative):
    base = 0x734058
    def __init__(self, exe):
        Base.__init__(self, exe)
        self.u.mem_map(0x4000000, 0x200000)
        self.draws = []
        for a in [0x443bb8, 0x443be5, 0x43e96a]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=a, end=a)

    def hook(self, u, a, size, _):
        sp = u.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<4I', u.mem_read(sp+4, 16))
        if a == 0x443bb8:
            i = (args[0]-0x4000200)//0x400
            self.wi(args[2], self.verts(i))
        elif a == 0x43e96a:
            i = (args[0]-0x4000000)//0x400
            self.draws.append(bytes(u.mem_read(self.verts(i), 192)))
        u.reg_write(UC_X86_REG_EAX, 0)
        u.reg_write(UC_X86_REG_EIP, self.ri(sp))
        u.reg_write(UC_X86_REG_ESP, sp+4)

    def read_zoom(self, a):
        return Zoom(Fade(self.rf(a+0x12c), self.rf(a+0x138), self.u.mem_read(a+0x134, 1)[0]),
                    (C.c_float*2)(self.rf(a+0x124), self.rf(a+0x128)),
                    (C.c_float*2)(self.rf(a+0x11c), self.rf(a+0x120)))

    def install_zoom(self, a, i, s, rect, enter=2):
        u = self.u; h = self.handle(i)
        self.blank(i); u.mem_write(a, bytes(0x16c)); self.wi(a+0x100, h)
        for off, value in [(0x12c, s.fade.alpha), (0x138, s.fade.speed),
                           (0x124, s.scale[0]), (0x128, s.scale[1]),
                           (0x11c, s.pivot[0]), (0x120, s.pivot[1]),
                           (0x114, rect[0]), (0x118, rect[1]),
                           (0x10c, rect[2]), (0x110, rect[3])]:
            self.wf(a+off, value)
        u.mem_write(a+0x134, bytes([s.fade.stage]))
        u.mem_write(a+0x148, bytes([enter, 0, 0]))
        for off, value in [(0x88, s.fade.alpha), (0x8c, s.scale[0]),
                           (0x90, s.scale[1]), (0xdc, s.pivot[0]),
                           (0xe0, s.pivot[1]), (0x7c, rect[0]),
                           (0x80, rect[1]), (0xf4, rect[2]), (0xf8, rect[3])]:
            self.wf(h+off, value)
        self.wi(h+0x74, 1)
        for j in range(6):
            self.wi(self.verts(i)+32*j+16, (int(s.fade.alpha*255)<<24)|0xffffff)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path); a = ap.parse_args()
    exe = a.exe.read_bytes(); n = Native(exe); lib = library()
    lib.bk_zoom_sprite_initialize.argtypes = [C.POINTER(Zoom)]
    lib.bk_zoom_sprite_advance.argtypes = [C.POINTER(Zoom), C.c_float]
    lib.bk_fade_sprite_request.argtypes = [C.POINTER(Fade), C.c_uint8]
    lib.bk_zoom_sprite_rect.argtypes = [C.POINTER(Zoom), C.POINTER(C.c_float), C.POINTER(C.c_float)]
    rng = random.Random(0x50fc57); count = vertices = 0
    def check(s, rect, dt, request):
        nonlocal count, vertices
        n.install_zoom(n.base, 0, s, rect); n.draws = []; n.wf(0x733700, dt)
        if request is not None:
            n.call(0x50e633, struct.pack('<II', n.base, request))
            assert lib.bk_fade_sprite_request(C.byref(s.fade), request)
        n.call(0x50e6ba, struct.pack('<II', n.base, 0))
        assert lib.bk_zoom_sprite_advance(C.byref(s), dt)
        assert snapshot(s) == snapshot(n.read_zoom(n.base)), (count, snapshot(s), snapshot(n.read_zoom(n.base)))
        out = (C.c_float*4)(); assert lib.bk_zoom_sprite_rect(C.byref(s), rect, out)
        assert len(n.draws) == 1
        for j, (x, y) in enumerate([(0, 1), (2, 1), (2, 3), (0, 3), (0, 1), (2, 3)]):
            want = struct.unpack_from('<2f', n.draws[0], 32*j)
            assert (out[x], out[y]) == want, (count, j, tuple(out), want)
            assert struct.unpack_from('<I', n.draws[0], 32*j+16)[0] == (int(s.fade.alpha*255)<<24)|0xffffff
            vertices += 1
        count += 1
    for case in range(6000):
        s = Zoom(Fade(rng.random(), rng.choice([0, .01, .5, 2, 100]), case % 6),
                 (C.c_float*2)(rng.uniform(0, 2), rng.uniform(0, 2)),
                 (C.c_float*2)(rng.uniform(-1, 2), rng.uniform(-1, 2)))
        rect = (C.c_float*4)(rng.uniform(-200, 1300), rng.uniform(-200, 1000), rng.uniform(0, 1300), rng.uniform(0, 1000))
        check(s, rect, C.c_float(rng.choice([0, 1/60, .00001, .5, 1, 10])).value, rng.choice([None, 0, 1, 2, 255]))
    for sequence in range(24):
        s = Zoom(); lib.bk_zoom_sprite_initialize(C.byref(s))
        for tick in range(240):
            request = {sequence % 3: 1, 61: 0, 70: 1, 85: 0, 90: 1, 140: 0, 141: 1}.get(tick)
            check(s, (C.c_float*4)(640, 480, 256, 56), C.c_float([0, 1/60, .1, .5][sequence % 4]).value, request)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), steps=count, vertices=vertices, max_error=0, scope=__doc__)
    (ROOT/'local/original-zoom-sprite-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report), flush=True)
if __name__ == '__main__': main()
