"""Complete4e7917 construction and519476 title frame with actual50e6ba,
43ed45,50db23,4adbb9 and51c47e. Boundaries: texture/audio creation,
vertex-buffer lock/draw, DirectSound gain, input/clock and scene release.
Fixed analyzed Chinese EXE; not Japanese protected-EXE instruction evidence.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_zoom_sprite_oracle import Native as ZoomNative, Zoom, snapshot as zs
from original_pause_oracle import Cursor, Sprite as Flat, KEYS, ss
from original_common_hud_oracle import State as Common, Flow, Timer
from original_item_notice_oracle import Fade
from model_binding import ROOT, library
class Sprite(C.Structure):
    _fields_ = [('zoom', Zoom), ('rect', C.c_float*4), ('half', C.c_int32*2)]
class State(C.Structure):
    _fields_ = [('sprites', Sprite*13), ('row', C.c_int32), ('volume', C.c_int32), ('mode', C.c_uint8), ('loaded', C.c_uint16)]
class Bindings(C.Structure):
    _fields_ = [('common', C.POINTER(Common)), ('flow', C.POINTER(Flow)), ('cursor', C.POINTER(Cursor)), ('latch', C.POINTER(C.c_uint8))]
Sound = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint, C.c_void_p)
Gain = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_int32, C.c_void_p)
Warp = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_float, C.c_float, C.c_void_p)
Pointer = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(C.c_float), C.c_void_p)
Release = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint8, C.c_void_p)
class Ops(C.Structure):
    _fields_ = [('context', C.c_void_p), ('sound', Sound), ('gain', Gain), ('warp', Warp), ('position', Pointer), ('motion', Pointer), ('release', Release)]
class Input(C.Structure):
    _fields_ = [('buttons', C.c_uint32), ('now', C.c_uint32), ('seconds', C.c_float), ('scale', C.c_float), ('master', C.c_int32), ('special', C.c_uint8)]
class Draw(C.Structure):
    _fields_ = [('slot', C.c_uint), ('corners', C.c_float*4), ('alpha', C.c_float)]
class Frame(C.Structure):
    _fields_ = [('count', C.c_uint), ('draws', Draw*9)]
ADDR = [0x734058+i*0x16c for i in range(13)] + [0xb537e8, 0xbeea18]
def sprite_values(p): return (zs(p.zoom), tuple(p.rect), tuple(p.half))
def snapshot(s, c, f, cursor, latch):
    return (tuple(sprite_values(p) for p in s.sprites), s.row, s.volume, s.mode, s.loaded,
            (c.curtain.alpha, c.curtain.speed, c.curtain.stage, bytes(c.wait), c.gate, c.action, c.blocked),
            bytes(f), ss(cursor.sprite), bytes(cursor.idle), cursor.wanted, latch)

class Native(ZoomNative):
    def __init__(self, exe):
        super().__init__(exe)
        self.width = 1280; self.height = 960; self.names = []; self.events = []; self.draw_meta = []
        self.point = [0., 0.]; self.relative = [0., 0.]; self.buttons = 0; self.now = 0
        self.record = False
        self.wi(0x53f10c, 0x300e000)
        self.wi(0x41f0000, 0x41f0100); self.wi(0x41f013c, 0x300e200)
        self.wi(0x728dd0, 0x41f0000)
        for i in range(8): self.wi(0xbeee10+i*0x120, 100+i)
        for a in [0x4af970, 0x4af97a, 0x4ad8ec, 0x466805, 0x466814, 0x43e583,
                  0x4b768d, 0x50d4fa, 0x4b75aa, 0x4b757e, 0x4b76c2, 0x46435e,
                  0x4e77bf, 0x300e000, 0x300e200]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=a, end=a)
        self.u.mem_write(0x5767c8, b'\1')
    def event(self, *v):
        if self.record: self.events.append((*v, snapshot(*self.read())))
    def read(self):
        s = State()
        for i, a in enumerate(ADDR[:13]):
            s.sprites[i] = Sprite(self.read_zoom(a), (C.c_float*4)(*[self.rf(a+o) for o in [0x114, 0x118, 0x10c, 0x110]]),
                                  (C.c_int32*2)(self.ri(a+0x158), self.ri(a+0x15c)))
            if self.ri(a+0x100): s.loaded |= 1 << i
        s.row = self.ri(0xbf9b90); s.volume = self.ri(0x728dd4); s.mode = self.u.mem_read(0x728dd8, 1)[0]
        c = Common.from_buffer_copy(self.u.mem_read(0x300d000, C.sizeof(Common)))
        c.curtain = self.read_zoom(ADDR[14]).fade
        c.wait = Timer.from_buffer_copy(self.u.mem_read(0xbeeb54, C.sizeof(Timer)))
        c.gate, c.action, c.blocked = [self.u.mem_read(a, 1)[0] for a in [0xbeeb7c, 0xbeeb7e, 0xbeeb7f]]
        f = Flow(*[self.u.mem_read(a, 1)[0] for a in [0xbeeb84, 0x721ad4, 0xbfbbb9, 0xbfbb9c]])
        a = ADDR[13]
        cursor = Cursor(Flat(self.read_zoom(a).fade, (C.c_float*4)(*[self.rf(a+o) for o in [0x114, 0x118, 0x10c, 0x110]])),
                        Timer.from_buffer_copy(self.u.mem_read(0xb53924, C.sizeof(Timer))), self.u.mem_read(0xb5394f, 1)[0])
        return s, c, f, cursor, self.u.mem_read(0xbef178, 1)[0]
    def hook(self, u, a, size, data):
        sp = u.reg_read(UC_X86_REG_ESP); args = struct.unpack('<5I', u.mem_read(sp+4, 20)); pop = 4; result = 0
        if a in [0x443bb8, 0x443be5, 0x43e96a]:
            if a == 0x43e96a:
                i = (args[0]-0x4000000)//0x400
                raw = bytes(u.mem_read(self.verts(i), 192))
                corners = (*struct.unpack_from('<2f', raw, 0), *struct.unpack_from('<2f', raw, 64))
                self.draw_meta.append((i, corners, self.rf(ADDR[i]+0x12c)))
            return super().hook(u, a, size, data)
        if a == 0x4af970: result = self.width
        elif a == 0x4af97a: result = self.height
        elif a == 0x4ad8ec: u.mem_write(args[0], b'assets\0')
        elif a == 0x43e583:
            i = ADDR.index(args[0]-0x100); self.blank(i); self.wi(args[0], self.handle(i))
            self.names.append((i, self.text(args[1]).split('\\')[-1]))
        elif a == 0x50d4fa:
            assert args[0] == 0x728cd0 and args[4] == 1 and self.text(args[2]) == 'bg001.wav'
            self.wi(args[0]+0x104, args[3]); u.mem_write(args[0]+0x108, b'\1\1')
        elif a == 0x4b768d:
            self.point = list(struct.unpack('<2f', u.mem_read(sp+4, 8))); self.event('warp', *self.point)
        elif a == 0x4b75aa:
            for p, v in zip(args, self.point): self.wf(p, v)
        elif a == 0x4b757e:
            for p, v in zip(args, self.relative): self.wf(p, v)
        elif a == 0x4b76c2:
            assert args[0] in KEYS and args[1:3] == (1, 0); result = bool(self.buttons & KEYS[args[0]])
        elif a == 0x46435e:
            assert args[1] == 0; self.event('sound', args[0]-100)
        elif a == 0x4e77bf:
            self.event('release', args[0]&255)
            for p in ADDR[:13]: self.wi(p+0x100, 0)
        elif a == 0x300e000: result = self.now
        elif a == 0x300e200:
            assert args[0] == 0x41f0000; self.event('gain', C.c_int32(args[1]).value); pop = 12
        u.reg_write(UC_X86_REG_EAX, result); u.reg_write(UC_X86_REG_EIP, self.ri(sp)); u.reg_write(UC_X86_REG_ESP, sp+pop)
    def install(self, s, c, f, cursor, latch, inp):
        for i, p in enumerate(s.sprites):
            a = ADDR[i]; self.install_zoom(a, i, p.zoom, p.rect, 2 if i else 0)
            self.wi(a+0x158, p.half[0]); self.wi(a+0x15c, p.half[1])
            if not s.loaded & (1 << i): self.wi(a+0x100, 0)
        for i, p in [(13, cursor.sprite), (14, Flat(c.curtain, (C.c_float*4)(0, 0, 1280*inp.scale, 960*inp.scale)))]:
            z = Zoom(p.fade, (C.c_float*2)(1, 1), (C.c_float*2)(0, 0))
            self.install_zoom(ADDR[i], i, z, p.rect, 1); self.u.mem_write(ADDR[i]+0x149, b'\1')
        self.wi(0xbf9b90, s.row); self.wi(0x728dd4, s.volume); self.wi(0xbe9a0c, inp.master)
        for a, value in [(0x728dd8, s.mode), (0xbeeb7c, c.gate), (0xbeeb7e, c.action), (0xbeeb7f, c.blocked),
                         (0xbeeb84, f.current), (0x721ad4, f.previous), (0xbfbbb9, f.target), (0xbfbb9c, f.mode),
                         (0xb5394f, cursor.wanted), (0xbef178, latch), (0xbef778, inp.special)]: self.u.mem_write(a, bytes([value]))
        self.u.mem_write(0xbeeb54, bytes(c.wait)); self.u.mem_write(0xb53924, bytes(cursor.idle))
        self.wf(0x733700, inp.seconds); self.wf(0x721ad0, inp.scale)
        self.buttons = inp.buttons; self.now = inp.now; self.draws = []; self.draw_meta = []; self.events = []
    def run(self, s, c, f, cursor, latch, inp, point, motion):
        self.install(s, c, f, cursor, latch, inp); self.point = list(point); self.relative = list(motion); self.record = True
        self.call(0x519476, b'')
        return snapshot(*self.read()), self.draw_meta, self.events

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); a = ap.parse_args()
    exe = a.exe.read_bytes(); n = Native(exe); lib = library(); e = C.create_string_buffer(256)
    lib.bk_title_menu_initialize.argtypes = [C.POINTER(State), C.c_uint, C.c_uint8, C.c_int32, C.POINTER(Ops), C.c_void_p]
    lib.bk_title_menu_step.argtypes = [C.POINTER(State), C.POINTER(Bindings), C.POINTER(Input), C.POINTER(Ops), C.POINTER(Frame), C.c_void_p]
    lib.bk_title_menu_image.argtypes = [C.c_uint, C.c_uint8]; lib.bk_title_menu_image.restype = C.c_char_p
    lib.bk_menu_cursor_initialize.argtypes = [C.POINTER(Cursor), C.c_uint, C.c_uint]
    rng = random.Random(0x519476); constructors = steps = draws = events = 0
    @Warp
    def nop(_, x, y, err): assert (x, y) == (320, 240); return 1
    initops = Ops(); initops.warp = nop
    def initialized(width, special):
        s = State(); assert lib.bk_title_menu_initialize(C.byref(s), width, special, -900, C.byref(initops), e), e.value
        return s
    for width in [320, 640, 1001, 1280, 1920]:
        for special in [0, 1, 2, 255]:
            s = initialized(width, 0); s.row = 5; c = Common(); f = Flow(1, 0, 0, 0); cursor = Cursor()
            assert lib.bk_menu_cursor_initialize(C.byref(cursor), width, width*3//4)
            inp = Input(0, 0, 0, width/1280, -1234, special)
            n.install(s, c, f, cursor, 0, inp); n.width = width; n.height = width*3//4; n.record = False; n.names = []
            n.call(0x4e7917, b''); want = n.read()[0]
            assert lib.bk_title_menu_initialize(C.byref(s), width, special, inp.master, C.byref(initops), e)
            assert [sprite_values(p) for p in s.sprites] == [sprite_values(p) for p in want.sprites], ('constructor', width, special)
            assert (s.row, s.volume, s.mode, s.loaded) == (want.row, want.volume, want.mode, want.loaded)
            assert n.names == [(i, lib.bk_title_menu_image(i, special).decode()) for i in range(13) if lib.bk_title_menu_image(i, special)]
            constructors += 1
    def check(s, c, f, cursor, latch, inp, point, motion):
        nonlocal steps, draws, events
        want, wd, wt = n.run(s, c, f, cursor, latch, inp, point, motion); trace = []; la = C.c_uint8(latch); current = list(point)
        def snap(): return snapshot(s, c, f, cursor, la.value)
        @Sound
        def sound(_, slot, err): trace.append(('sound', slot, snap())); return 1
        @Gain
        def gain(_, volume, err): trace.append(('gain', volume, snap())); return 1
        @Warp
        def warp(_, x, y, err): current[:] = [x, y]; trace.append(('warp', x, y, snap())); return 1
        @Pointer
        def position(_, out, err): out[0], out[1] = current; return 1
        @Pointer
        def relative(_, out, err): out[0], out[1] = motion; return 1
        @Release
        def release(_, flow, err): trace.append(('release', flow, snap())); return 1
        ops = Ops(None, sound, gain, warp, position, relative, release); b = Bindings(C.pointer(c), C.pointer(f), C.pointer(cursor), C.pointer(la)); frame = Frame()
        assert lib.bk_title_menu_step(C.byref(s), C.byref(b), C.byref(inp), C.byref(ops), C.byref(frame), e), (steps, e.value)
        assert snap() == want, ('state', steps, snap(), want)
        actual = [(d.slot, tuple(d.corners), d.alpha) for d in frame.draws[:frame.count]]
        assert actual == wd, ('draws', steps, actual, wd)
        assert trace == wt, ('events', steps, trace, wt)
        steps += 1; draws += len(actual); events += len(trace)
        return la.value, current
    for case in range(6000):
        width = [320, 640, 1001, 1280, 1920][case % 5]; special = [0, 0, 1, 2, 255][case % 5]
        s = initialized(width, special); s.row = rng.randrange(6); s.mode = rng.choice([0, 1, 2, 255]); s.volume = rng.randrange(-6000, 1)
        for p in s.sprites:
            p.zoom.fade.stage = rng.randrange(6); p.zoom.fade.alpha = rng.random()
            p.zoom.scale[:] = [rng.random(), rng.random()]
        c = Common(); c.curtain = Fade(rng.random(), 2, rng.randrange(6)); c.action = rng.choice([0, 1, 3, 5, 7, 9, 11, 2, 255]); c.blocked = rng.choice([0, 1, 2])
        f = Flow(1, 4, 0, 0); cursor = Cursor(); assert lib.bk_menu_cursor_initialize(C.byref(cursor), width, width*3//4)
        cursor.sprite.fade = Fade(rng.random(), 2, rng.randrange(6)); cursor.idle = Timer(10000, rng.getrandbits(32), rng.randrange(2)); cursor.wanted = rng.randrange(2)
        point = list(s.sprites[1+case%6*2].rect[:2]) if case % 4 else [-100., -100.]
        if case % 9 == 0: point[0] += s.sprites[1+case%6*2].half[0]
        inp = Input(rng.randrange(8), rng.getrandbits(32), rng.choice([0, .00001, 1/60, .1, 1, 10]), width/1280, -900, special)
        check(s, c, f, cursor, rng.choice([0, 1, 2]), inp, point, [0, 0] if case%3 else [2, -1])
    for special in [0, 1]:
        for target in [1, 3, 5, 7, 9, 11]:
            if special and target not in [1, 9]: continue
            s = initialized(1280, special); c = Common(); c.curtain = Fade(0, 2, 0); f = Flow(1, 0, 0, 0); cursor = Cursor()
            assert lib.bk_menu_cursor_initialize(C.byref(cursor), 1280, 960)
            point = [320, 240]; latch = 0
            for tick in range(700):
                if tick == 480: point = list(s.sprites[target].rect[:2])
                inp = Input(int(tick == 481), tick*17, 1/60, 1, -900, special)
                latch, point = check(s, c, f, cursor, latch, inp, point, [0, 0])
                if f.current != 1: break
            assert f.current == 0x50 and f.target == [0x38, 0x28, 0x30, 0x18, 0x58, 0x60][(target-1)//2]
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), constructors=constructors, frames=steps, draws=draws, events=events, max_error=0, scope=__doc__)
    (ROOT/'local/original-title-menu-oracle.json').write_text(json.dumps(report, indent=2)+'\n'); print(json.dumps(report), flush=True)
if __name__ == '__main__': main()
