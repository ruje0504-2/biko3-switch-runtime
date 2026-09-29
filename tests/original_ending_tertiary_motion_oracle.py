"""Original479bc9/479cc2/495125 numerical services and479739 voice names.

Pointer and drag control, squared-length/sqrt/fabs, radial menu search and
sine/cosine execute without hooks. Voice formatting/copy and the actual load
boundary are observed explicitly; this does not claim PCM output or a complete
scene. The retained direction words and clock edits are compared independently.
"""
from __future__ import annotations

import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP,
                              UC_X86_REG_FPSW, UC_X86_REG_FPTAG)
from original_prop_route_oracle import Native
from original_ending_secondary_ui_oracle import Menu
from model_binding import library

I, U, F, P = C.c_int32, C.c_uint32, C.c_float, C.c_void_p
XY = I * 2


class State(C.Structure):
    _fields_ = [('direction', I * 2)]


class Clip(C.Structure):
    _fields_ = [('duration', I)] + [(name, F) for name in
                ['start', 'end', 'source', 'rate', 'elapsed']]


def bind(lib):
    lib.bk_ending_tertiary_motion_initial.restype = State
    lib.bk_ending_tertiary_motion_pointer.argtypes = [C.POINTER(Clip)] + [C.POINTER(I)]*3 + [P]
    lib.bk_ending_tertiary_motion_drag.argtypes = [C.POINTER(State), U, U, F,
                                                  C.POINTER(I), C.POINTER(Clip), P]
    lib.bk_ending_radial_menu_place.argtypes = [C.POINTER(Menu), I, I, C.POINTER(I),
                                                C.POINTER(XY), P]
    lib.bk_ending_sound_tertiary_voice.argtypes = [U, I, I, P, P]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('--samples', type=int, default=16000)
    ap.add_argument('--menus', type=int, default=3600)
    ap.add_argument('--voices', type=int, default=3000)
    ap.add_argument('--output', type=Path)
    args = ap.parse_args()
    assert min(args.samples, args.menus, args.voices) > 0
    raw = args.exe.read_bytes()
    n, lib = Native(raw), library()
    bind(lib)
    u, error = n.u, C.create_string_buffer(256)
    rng, digest = random.Random(0x479cc2), hashlib.sha256()
    clip_address = n.actor + 0x190 + 3*0x9c
    mapping = [('duration', 0x50), ('start', 0x54), ('end', 0x58), ('source', 0x60),
               ('rate', 0x5c), ('elapsed', 0x44)]

    def wi(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def wf(address, value):
        u.mem_write(address, struct.pack('<f', value))

    def put_clip(c):
        for name, offset in mapping:
            (wi if name == 'duration' else wf)(clip_address+offset, getattr(c, name))

    def get_clip():
        return Clip(*[struct.unpack('<i' if name == 'duration' else '<f',
                                     u.mem_read(clip_address+offset, 4))[0]
                      for name, offset in mapping])

    def call(address, data):
        #479cc2 returns a float that its real caller discards. Clear the x87
        #tag/status before isolated calls, so test iterations do not accumulate
        #unconsumed return values in the emulated stack.
        u.reg_write(UC_X86_REG_FPSW, 0)
        u.reg_write(UC_X86_REG_FPTAG, 0xffff)
        n.call(address, data)

    initial = bytes(u.mem_read(0x54ccdc, 8))
    assert bytes(lib.bk_ending_tertiary_motion_initial()) == initial
    constants = {hex(a): struct.unpack('<f', u.mem_read(a, 4))[0]
                 for a in [0x53f4a0, 0x53f3b4]}
    assert list(constants.values()) == [40., 60.], constants
    wi(n.actor+0x140, 3)

    pointer_cases = drag_cases = overshoots = zero_anchors = 0
    for case in range(args.samples):
        c = Clip(rng.choice([0, 1, 10, 40, 120, 400]),
                 rng.uniform(0, 500), rng.uniform(0, 500), rng.uniform(0, 500),
                 rng.uniform(-20, 20), rng.uniform(0, 100))
        target = XY(rng.randrange(-2000, 2001), rng.randrange(-2000, 2001))
        menu = XY(rng.randrange(-2000, 2001), rng.randrange(-2000, 2001))
        pointer = XY(rng.randrange(-2000, 2001), rng.randrange(-2000, 2001))
        if case % 7 == 0:
            target[:] = menu[:]
            zero_anchors += 1
        if case % 11 == 0:
            pointer[:] = menu[:]
        if case % 13 == 0:
            pointer[:] = target[:]
        if case % 17 == 0:
            for item in [target, menu, pointer]:
                item[:] = [rng.choice([-2147483648, -16777217, 16777217, 2147483647]) for _ in range(2)]
        put_clip(c)
        u.mem_write(0x7220c8, bytes(menu))
        inputs = (U * 11)()
        inputs[9:11] = [v & 0xffffffff for v in pointer]
        call(0x479bc9, struct.pack('<I', n.actor) + bytes(inputs) + bytes(target))
        expected = get_clip()
        assert lib.bk_ending_tertiary_motion_pointer(C.byref(c), target, menu, pointer, error), error.value
        assert bytes(c) == bytes(expected), ('pointer', case, list(target), list(menu),
                                                list(pointer), c.source, expected.source)
        digest.update(bytes(c))
        pointer_cases += 1

        state = State((I * 2)(rng.choice([-1, 0, 1, 17]), rng.choice([-1, 0, 1, 17])))
        motion = XY(rng.choice([-2147483648, -17000, -17, -1, 0, 1, 17, 17000, 2147483647]),
                    rng.randrange(-5000, 5001))
        group, mode = case % 5, case // 5 % 2
        seconds = F(rng.choice([0, 1e-7, 1/120, 1/60, 1/30, .1, .25, 1, 5, 30])).value
        put_clip(c)
        u.mem_write(0x54ccdc, bytes(state))
        u.mem_write(0x721b3c, bytes([group]))
        wf(0x733700, seconds)
        call(0x479cc2, struct.pack('<IiiI', n.actor, *motion, mode))
        expected = get_clip()
        expected_state = bytes(u.mem_read(0x54ccdc, 8))
        assert lib.bk_ending_tertiary_motion_drag(C.byref(state), group, mode, seconds,
                                                  motion, C.byref(c), error), error.value
        assert bytes(c) == bytes(expected), ('drag', case, group, mode, seconds,
                                             list(motion), c.source, expected.source)
        assert bytes(state) == expected_state, ('direction', case)
        overshoots += c.source > c.end
        digest.update(bytes(c)+bytes(state))
        drag_cases += 1
    # Two actors share the auxiliary direction lane. Keep the direction words
    #over a sequence while changing the selected actor clock and input group.
    state = lib.bk_ending_tertiary_motion_initial()
    clocks = [Clip(100, 10, 210, 10, 2, 17), Clip(80, 25, 185, 180, 2, 33)]
    for case in range(1600):
        c, mode = clocks[case % 2], case % 2
        seconds, group = F(1/30).value, case // 320
        motion = XY((case % 101)-50, 21)
        put_clip(c)
        u.mem_write(0x54ccdc, bytes(state))
        u.mem_write(0x721b3c, bytes([group]))
        wf(0x733700, seconds)
        call(0x479cc2, struct.pack('<IiiI', n.actor, *motion, mode))
        expected, expected_state = get_clip(), bytes(u.mem_read(0x54ccdc, 8))
        assert lib.bk_ending_tertiary_motion_drag(C.byref(state), group, mode, seconds,
                                                  motion, C.byref(c), error), error.value
        assert bytes(c) == bytes(expected) and bytes(state) == expected_state, ('sequence', case)
        digest.update(bytes(c)+bytes(state))
        drag_cases += 1

    for case in range(args.menus):
        width, height = [(640, 480), (960, 720), (1001, 750), (1280, 960),
                          (1920, 1440), (320, 240)][case % 6]
        scale = F(width/1280).value
        g = Menu(width, height, scale, F(96*scale).value)
        center = XY(rng.randrange(1, width), rng.randrange(1, height))
        angle = rng.choice([-2147483648, -721, -180, 0, 45, 90, 135, 180, 225,
                             270, 315, 360, 721, 2147483647])
        step = rng.choice([-2147483648, -90, 0, 1, 90, 120, 180, 2147483647])
        points = (XY * 3)(XY(-9, -8), XY(-7, -6), XY(-5, -4))
        wi(0x709890, width)
        wi(0x709888, height)
        wf(0x721ad0, scale)
        wf(0x7389e8, g.menu_width)
        u.mem_write(0x7220c8, bytes(points))
        call(0x495125, struct.pack('<4i', angle, step, *center))
        expected = bytes(u.mem_read(0x7220c8, 24))
        assert lib.bk_ending_radial_menu_place(C.byref(g), angle, step, center, points, error), error.value
        assert bytes(points) == expected, ('menu', case, angle, step,
                                            list(map(tuple, points)), struct.unpack('<6i', expected))
        digest.update(bytes(points))

    loads, formats = [], set()
    wi(0x53f2f0, 0x300d010)
    wi(0x53f214, 0x300d020)
    def cstring(address):
        return bytes(u.mem_read(address, 260)).split(b'\0')[0]
    def voice_service(_u, address, _size, _context):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, a, b, c, d = struct.unpack('<5I', u.mem_read(sp, 20))
        cleanup = 0
        if address == 0x300d010:
            fmt = cstring(b)
            formats.add(fmt.decode())
            text = fmt % (I(c).value, I(d).value)
            u.mem_write(a, text+b'\0')
            result = len(text)
        elif address == 0x300d020:
            u.mem_write(a, cstring(b)+b'\0')
            result, cleanup = a, 8
        else:
            assert address == 0x4e0956
            loads.append((cstring(a), b, bytes(u.mem_read(0x722224+b*0x120, 32))))
            result = 0
        u.reg_write(UC_X86_REG_EAX, result)
        u.reg_write(UC_X86_REG_ESP, sp+4+cleanup)
        u.reg_write(UC_X86_REG_EIP, ret)
    hooks = [u.hook_add(UC_HOOK_CODE, voice_service, begin=a, end=a)
              for a in [0x300d010, 0x300d020, 0x4e0956]]
    try:
        for case in range(args.voices):
            group, slot = case % 5, case // 5 % 2
            cue = rng.choice([-2147483648, -1, 0, 1, 9, 10, 99, 100, 2147483647])
            select = rng.choice([-2147483648, -1, 0, 0, 1, 2, 2147483647])
            bank = rng.randrange(-20, 20)
            u.mem_write(0x721b3c, bytes([group]))
            u.mem_write(0x722224+slot*0x120, b'\xa5'*32)
            before = len(loads)
            call(0x479739, struct.pack('<4i', cue, slot, bank, select))
            assert len(loads) == before + 1
            out = C.create_string_buffer(b'\xa5'*31, 32)
            out.raw = b'\xa5'*32
            assert lib.bk_ending_sound_tertiary_voice(group, cue, select, out, error), error.value
            name, chosen_slot, copied = loads[-1]
            assert chosen_slot == slot and out.value == name and out.raw == copied
            digest.update(copied)
    finally:
        for hook in hooks:
            u.hook_del(hook)

    rejected = 0
    for bad in [float('nan'), float('inf'), -1]:
        state, c = State((I*2)(1, 1)), Clip(40, 0, 100, 50, 2.5, 17)
        previous = bytes(state)+bytes(c)
        assert not lib.bk_ending_tertiary_motion_drag(C.byref(state), 0, 0, bad,
                                                     XY(1, 2), C.byref(c), error)
        assert bytes(state)+bytes(c) == previous
        rejected += 1
    report = {'passed': True, 'exe_sha256': hashlib.sha256(raw).hexdigest(),
              'pointer_cases': pointer_cases, 'drag_cases': drag_cases,
              'source_overshoots': overshoots, 'zero_anchors': zero_anchors,
              'menu_cases': args.menus, 'voice_cases': args.voices,
              'voice_formats': sorted(formats), 'constants': constants,
              'rejections': rejected, 'max_error': 0,
              'state_sha256': digest.hexdigest(), 'scope': __doc__,
              'scene_or_gpu_validation': False}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS tertiary motion', pointer_cases, drag_cases, args.menus, args.voices,
          overshoots, zero_anchors, rejected, digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
