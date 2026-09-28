"""Original4bac5b and4bb612 with actual native trig/D3DX/frame/aim math.
Input, FOV and XAN request/advance/pre-publication are explicit boundaries;
track/focus values supplied by fixtures are not a real animated menu scene.
"""
import argparse, ctypes as C, hashlib, json, math, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_aim_oracle import Pose, I
from model_binding import ROOT, library
Vec = C.c_float*3
class State(C.Structure):
    _fields_ = [('pose', Pose), ('yaw', C.c_float), ('pitch', C.c_float),
                ('radius', C.c_float), ('height', C.c_float), ('matrix', C.c_float*16), ('focus', Vec), ('fov', C.c_float)]
class Native(Base):
    camera, clip, track, focus, frame, context = 0x71af38, 0x3001000, 0x3002000, 0x3003000, 0x3004000, 0x3005000
    def __init__(self, exe):
        super().__init__(exe)
        for a in [0x42cf0e, 0x4b757e, 0x4b76c2, 0x401b0a, 0x4026fe, 0x423be2]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=a, end=a)
    def wi(self, a, v): self.u.mem_write(a, struct.pack('<I', v))
    def vf(self, a, v): self.u.mem_write(a, struct.pack('<'+'f'*len(v), *v))
    def read(self, a, n): return struct.unpack('<'+'f'*n, self.u.mem_read(a, n*4))
    def hook(self, u, a, size, _):
        sp = u.reg_read(UC_X86_REG_ESP); ret, first, second, third = struct.unpack('<4I', u.mem_read(sp, 16)); result = 0
        if a == 0x42cf0e: self.fov = struct.unpack('<f', u.mem_read(sp+4, 4))[0]
        elif a == 0x4b757e:
            self.vf(first, self.motion[:1]); self.vf(second, self.motion[1:])
        elif a == 0x4b76c2:
            assert first in [0, 1] and (second, third) == (2, 0)
            result = bool(self.buttons & (1 << first))
        elif a == 0x401b0a:
            assert first == self.clip and second == 0; self.events.append('request0')
        elif a == 0x4026fe:
            assert first == self.clip and self.read(sp+8, 1)[0] == self.dt; self.events.append('advance')
        else:
            self.events.append('publish')
            self.vf(self.track+0xf0, self.track_value)
            if self.focus_value is not None: self.vf(self.focus+0xf0, self.focus_value)
        u.reg_write(UC_X86_REG_EAX, int(result)); u.reg_write(UC_X86_REG_EIP, ret); u.reg_write(UC_X86_REG_ESP, sp+4)
    def run(self, state, kind, motion, buttons, dt, track, focus, flow):
        self.motion = list(motion); self.buttons = buttons; self.dt = dt; self.track_value = track; self.focus_value = focus
        self.events = []; self.fov = state.fov
        u = self.u; u.mem_write(self.camera, bytes(0x600)); u.mem_write(self.clip, bytes(0x5000))
        self.wi(self.camera, self.clip); self.wi(self.camera+8, self.track); self.wi(self.camera+0x10, self.focus if focus is not None else 0)
        self.wi(0x645600, self.context); self.wi(0x645604, self.frame)
        for p in [self.frame, self.context]:
            for off in [0x80, 0xc0, 0x100]: self.vf(p+off, I)
        self.vf(self.frame+0x80, state.pose.world); self.vf(self.frame+0xc0, state.pose.world)
        self.vf(self.camera+0x420, state.pose.position)
        self.vf(self.camera+0x42c, [state.yaw, state.pitch, state.radius, state.height])
        self.vf(self.camera+0x4a4, state.matrix); self.vf(self.camera+0x5c8, state.focus)
        self.vf(0x733700, [dt]); u.mem_write(0xbeeb84, bytes([flow]))
        self.call(0x4bb612 if kind else 0x4bac5b, struct.pack('<I', self.camera))
        out = State.from_buffer_copy(state)
        out.pose.world[:] = self.read(self.frame+0xc0, 16); out.pose.position[:] = self.read(self.camera+0x420, 3)
        out.yaw, out.pitch, out.radius, out.height = self.read(self.camera+0x42c, 4)
        out.matrix[:] = self.read(self.camera+0x4a4, 16); out.focus[:] = self.read(self.camera+0x5c8, 3); out.fov = self.fov
        assert self.events == (['request0', 'advance', 'publish'] if kind else [])
        return out
def values(s): return [*s.pose.world, *s.pose.position, s.yaw, s.pitch, s.radius, s.height, *s.matrix, *s.focus, s.fov]
def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); a = ap.parse_args()
    exe = a.exe.read_bytes(); native = Native(exe); lib = library(); rng = random.Random(0x4bac5b); e = C.create_string_buffer(256)
    lib.bk_menu_camera_orbit.argtypes = [C.POINTER(State), C.POINTER(C.c_float), C.c_uint, C.c_float, C.c_void_p]
    lib.bk_menu_camera_track.argtypes = [C.POINTER(State), C.POINTER(C.c_float), C.POINTER(C.c_float), C.c_uint8, C.c_float, C.c_void_p]
    worst = 0.; samples = [0, 0]; rejects = 0
    for case in range(12000):
        s = State(); s.pose.world[:] = I; s.matrix[:] = I
        s.pose.world[12:15] = [rng.uniform(-300, 300) for _ in range(3)]
        s.pose.position[:] = [rng.uniform(-300, 300) for _ in range(3)]
        s.yaw = rng.choice([-720, -360, 0, 360, 720, rng.uniform(-500, 500)])
        s.pitch = rng.uniform(-100, 100); s.radius = rng.uniform(-50, 150); s.height = rng.uniform(-10, 50)
        s.focus[:] = [1, 2, 3]; s.fov = 1
        kind = case % 2; dt = C.c_float(rng.choice([0, .00001, 1/60, .1, .5, 1, 2])).value
        motion = (C.c_float*2)(rng.uniform(-200, 200), rng.uniform(-200, 200)); buttons = (case//2)%4
        track = Vec(*(rng.uniform(-200, 200) for _ in range(3)))
        focus = Vec(*(rng.uniform(-100, 100) for _ in range(3))) if case%3 else None
        flow = rng.choice([0x10, 0x38, 8, 0, 255])
        wanted = native.run(s, kind, motion, buttons, dt, track, focus, flow)
        ok = lib.bk_menu_camera_track(C.byref(s), track, focus, flow, dt, e) if kind else lib.bk_menu_camera_orbit(C.byref(s), motion, buttons, dt, e)
        assert ok, (case, e.value)
        for i, (got, want) in enumerate(zip(values(s), values(wanted))):
            delta = abs(got-want)/max(1, abs(want)); worst = max(worst, delta)
            assert math.isfinite(delta) and delta < 3e-6, (case, kind, i, got, want, delta)
        samples[kind] += 1
        if case%100 == 0:
            old = bytes(s)
            assert not lib.bk_menu_camera_orbit(C.byref(s), motion, buttons, float('nan'), e)
            assert bytes(s) == old; rejects += 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), orbit_steps=samples[0], track_steps=samples[1], atomic_rejections=rejects, max_relative_error=worst, scope=__doc__)
    (ROOT/'local/original-menu-camera-oracle.json').write_text(json.dumps(report, indent=2)+'\n'); print(json.dumps(report), flush=True)
if __name__ == '__main__': main()
