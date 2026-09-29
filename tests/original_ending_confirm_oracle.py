"""Execute original4e2223..4e252a against the portable confirmation controller.

Only key and sound services are observed. Compare every service's prior state,
ordered short-circuit queries, low AL results, overlapping bounds, retained
flag bytes, callback changes and the final state. No game assets or rendering.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from model_binding import ROOT, library
from original_prop_route_oracle import Native as Base
from original_ending_frame_oracle import State as Frame, Input
from original_ending_control_oracle import State, Rect, Key, Sound, Ops


class Bindings(C.Structure):
    _fields_ = [("frame", C.POINTER(Frame)),
                ("action", C.POINTER(C.c_uint8)),
                ("wanted", C.POINTER(C.c_uint8)),
                ("rects", C.POINTER(Rect)),
                ("volume", C.POINTER(C.c_int32))]


WORDS = [("hover", 0x70c8bc), ("hover_armed", 0x709f18),
         ("previous_hover", 0x709f20), ("previous_phase", 0x719b1c)]
FLAGS = [0x7392cb, 0x739437, 0x7395a3, 0x73970f, 0x73987b, 0x7399e7]
RECTS = [0x739550, 0x739828]


class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        for address in (0x4b76c2, 0x4ad2bf):
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack("<I", value & 0xffffffff))

    def write_state(self, state, frame, action, wanted):
        for name, address in WORDS:
            self.word(address, getattr(state, name))
        self.word(0x721e00, frame.phase)
        for address, value in zip(FLAGS, state.pause_flags):
            self.u.mem_write(address, bytes([value]))
        for address, value in [(0xbeeb7d, state.pause_selection),
                               (0xbeeb7e, action), (0xbeeb7f, wanted)]:
            self.u.mem_write(address, bytes([value]))

    def snapshot(self):
        state = State.from_buffer_copy(self.initial_state)
        frame = Frame.from_buffer_copy(self.initial_frame)
        for name, address in WORDS:
            setattr(state, name, struct.unpack("<i", self.u.mem_read(address, 4))[0])
        frame.phase = struct.unpack("<i", self.u.mem_read(0x721e00, 4))[0]
        state.pause_selection = self.u.mem_read(0xbeeb7d, 1)[0]
        state.pause_flags[:] = [self.u.mem_read(a, 1)[0] for a in FLAGS]
        return (bytes(state), bytes(frame), self.u.mem_read(0xbeeb7e, 1)[0],
                self.u.mem_read(0xbeeb7f, 1)[0])

    def hook(self, uc, address, _size, _context):
        sp = uc.reg_read(UC_X86_REG_ESP)
        ret, first, second, third = struct.unpack("<4I", uc.mem_read(sp, 16))
        result = 0
        if address == 0x4b76c2:
            assert second == 1 and third == 0
            result = self.keys[self.key_index]
            self.key_index += 1
            event = ("key", first, second, result)
        else:
            assert second == 0 and first in (100, 102, 103)
            event = ("sound", first - 100, C.c_int32(third).value)
        self.trace.append((event, self.snapshot()))
        if len(self.trace) == self.mutate_at:
            state, frame, action, wanted = self.change
            self.write_state(state, frame, action, wanted)
        uc.reg_write(UC_X86_REG_EAX, result)
        uc.reg_write(UC_X86_REG_ESP, sp + 4)
        uc.reg_write(UC_X86_REG_EIP, ret)

    def run(self, state, frame, action, wanted, rects, inp, volume, keys,
            mutate_at, change):
        self.initial_state, self.initial_frame = bytes(state), bytes(frame)
        self.write_state(state, frame, action, wanted)
        for address, rect in zip(RECTS, rects):
            self.u.mem_write(address - 8, struct.pack(
                "<4f", rect.width, rect.height, rect.x, rect.y))
        for slot in (0, 2, 3):
            self.word(0xbeee10 + slot * 0x120, 100 + slot)
        self.word(0xbe9a10, volume)
        self.trace, self.key_index, self.keys = [], 0, keys
        self.mutate_at, self.change = mutate_at, change
        self.call(0x4e2223, bytes(inp))
        return self.trace, self.snapshot()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("exe", type=Path)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    native = Native(exe)  # machine() enforces the fixed EXE SHA-256.
    lib = library()
    lib.bk_ending_confirm_step.argtypes = [C.POINTER(State), C.POINTER(Bindings),
                                          C.POINTER(Input), C.POINTER(Ops),
                                          C.c_void_p]
    rng = random.Random(0x4e2223)
    error = C.create_string_buffer(256)
    calls = blocked = mutations = failures = 0
    sounds = [0] * 4
    hits = [0, 0]
    for case in range(12000):
        state = State()
        frame = Frame()
        state.hover, state.hover_armed, state.previous_hover = [
            rng.choice([0, 1, -1, 59, 61]) for _ in range(3)]
        state.previous_phase = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, -1])
        state.pause_selection = rng.choice([0, 45, 47, 128, 255])
        state.pause_flags[:] = [rng.randrange(256) for _ in range(6)]
        state.targets[1][2] = 123.5  # Unrelated fields must remain untouched.
        state.mode_721ec4 = -77
        frame.phase = rng.choice([9, 9, 9, state.previous_phase])
        frame.camera_mode, frame.group = 3, case % 5
        action = rng.randrange(256)
        wanted = rng.choice([0, 0, 0, 0, 1, 2, 255])
        rects = (Rect * 2)(Rect(10, 10, 12, 12), Rect(30, 10, 12, 12))
        if case % 5 == 0:
            rects[1] = rects[0]
        if case % 7 == 0:
            for rect in rects:
                rect.width = rng.choice([-1, 0, 12])
                rect.height = rng.choice([-1, 0, 12])
        x = rng.choice([9, 10, 22, 23, 29, 30, 42, 43])
        y = rng.choice([9, 10, 11, 22, 23])
        if case % 17 == 0:
            rects[0] = Rect(16777216, -10, 1, 20)
            x, y = 16777217, 0
        if case % 19 == 0:
            rects[1] = Rect(-2147483648, 0, 32, 1)
            x, y = -2147483648, 0
        inp = Input()
        inp.words[:] = [rng.getrandbits(32) for _ in range(11)]
        inp.words[9], inp.words[10] = x & 0xffffffff, y & 0xffffffff
        volume = rng.choice([0, -700, -3000, -10000])
        keys = [rng.choice([0, 0, 1, 255, 256, 0x80000000, 0xffffffff])
                for _ in range(16)]
        changed = State.from_buffer_copy(state)
        new_frame = Frame.from_buffer_copy(frame)
        changed.pause_selection = rng.choice([45, 47, 255])
        changed.previous_phase = rng.randrange(1, 10)
        changed.hover, changed.hover_armed = rng.choice([0, 59, 61]), 0
        changed.previous_hover = 71
        changed.pause_flags[:] = [rng.randrange(256) for _ in range(6)]
        new_frame.phase = rng.randrange(1, 10)
        change = (changed, new_frame, rng.randrange(256), rng.choice([0, 1]))
        mutate_at = rng.randrange(1, 12) if case % 3 == 0 else -1
        expected, final = native.run(state, frame, action, wanted, rects, inp,
                                      volume, keys, mutate_at, change)

        def portable(fail_at=0):
            s = State.from_buffer_copy(state)
            f = Frame.from_buffer_copy(frame)
            a, w, v = C.c_uint8(action), C.c_uint8(wanted), C.c_int32(volume)
            bindings = Bindings(C.pointer(f), C.pointer(a), C.pointer(w),
                                rects, C.pointer(v))
            trace, ki = [], 0

            def snap():
                return bytes(s), bytes(f), a.value, w.value

            def commit(event):
                trace.append((event, snap()))
                if len(trace) == fail_at:
                    return 0
                if len(trace) == mutate_at:
                    C.memmove(C.addressof(s), bytes(changed), C.sizeof(s))
                    C.memmove(C.addressof(f), bytes(new_frame), C.sizeof(f))
                    a.value, w.value = change[2:]
                return 1

            @Key
            def key(_context, code, mode, out, _err):
                nonlocal ki
                out[0] = keys[ki]
                ki += 1
                return commit(("key", code, mode, out[0]))

            @Sound
            def sound(_context, slot, vol, _err):
                return commit(("sound", slot, vol))

            ops = Ops()
            ops.key, ops.sound = key, sound
            ok = lib.bk_ending_confirm_step(C.byref(s), C.byref(bindings),
                                            C.byref(inp), C.byref(ops), error)
            return ok, trace, snap()

        ok, actual, end = portable()
        assert ok and actual == expected and end == final, (
            case, error.value, actual, expected, end, final)
        calls += len(expected)
        blocked += wanted != 0
        mutations += 0 < mutate_at <= len(expected)
        for i, r in enumerate(rects):
            hits[i] += (not wanted and r.x <= x <= float(r.x) + r.width
                        and r.y <= y <= float(r.y) + r.height)
        for event, _snapshot in expected:
            if event[0] == "sound":
                sounds[event[1]] += 1
        if case % 31 == 0:
            for at in range(1, len(expected) + 1):
                ok, trace, end = portable(at)
                assert not ok and trace == expected[:at] and end == expected[at-1][1], (
                    case, "failure prefix", at)
                failures += 1
    assert all(sounds[i] for i in (0, 2, 3)) and all(hits) and mutations and failures
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  frames=12000, service_calls=calls, sounds=sounds,
                  blocked_frames=blocked, region_hits=hits,
                  callback_mutations=mutations, failed_prefixes=failures,
                  byte_exact=True, scope=__doc__)
    (ROOT / "local/original-ending-confirm-oracle.json").write_text(
        json.dumps(report, indent=2) + "\n")
    print("PASS ending confirmation", json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
