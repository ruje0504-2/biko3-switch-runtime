"""Run original NPC movement permission and timer, replacing only GetTickCount."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, library

class Timer(C.Structure):
    _fields_ = [('duration', C.c_uint32), ('deadline', C.c_uint32), ('armed', C.c_uint8)]
class Actions(C.Structure):
    _fields_ = [('idle', C.c_int32), ('walk', C.c_int32), ('run', C.c_int32), ('stationary', C.c_int32 * 4)]
class State(C.Structure):
    _fields_ = [('action', C.c_int32), ('behavior', C.c_int32), ('hidden', C.c_uint8), ('mode', C.c_int8), ('route_flag', C.c_int8), ('wait', Timer)]
class Motion(C.Structure):
    _fields_ = [('distance', C.c_float), ('allowed', C.c_int)]

class Native:
    actor, background, result, clock, stop, stack = 0x3000000, 0x3002000, 0x3003000, 0x300e000, 0x300f000, 0x2008000
    def __init__(self, exe):
        self.u = machine(exe)
        self.u.mem_write(0x53f10c, struct.pack('<I', self.clock))
        self.u.hook_add(UC_HOOK_CODE, self.hook, begin=self.clock, end=self.clock)
    def hook(self, u, address, size, user):
        self.clock_calls += 1
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', u.mem_read(sp, 4))[0]
        u.reg_write(UC_X86_REG_EAX, self.now)
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, ret)
    def call(self, address, *args):
        self.u.mem_write(self.stack, struct.pack('<' + 'I' * (len(args) + 1), self.stop, *args))
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(address, self.stop, count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP) == self.stop
        return self.u.reg_read(UC_X86_REG_EAX)
    def actions(self, group):
        self.u.mem_write(self.actor, bytes(0x900))
        self.u.mem_write(self.stack + 8, struct.pack('<II', self.actor, group))
        self.u.reg_write(UC_X86_REG_EBP, self.stack)
        self.u.reg_write(UC_X86_REG_ESP, self.stack - 0x100)
        self.u.emu_start(0x4fb645, 0x4fb73e, count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP) == 0x4fb73e
        return [struct.unpack('<i', self.u.mem_read(self.actor + 0x14 + i * 4, 4))[0] for i in [0, 1, 3, 21, 23, 22, 24]]
    def timer(self, timer, now):
        self.now, self.clock_calls = now, 0
        self.u.mem_write(self.actor, bytes(timer))
        result = self.call(0x4adbb9, self.actor)
        return result, Timer.from_buffer_copy(self.u.mem_read(self.actor, C.sizeof(Timer)))
    def motion(self, state, actions, background, seconds, now):
        self.now, self.clock_calls = now, 0
        data = bytearray(0x900)
        struct.pack_into('<i', data, 0x10, state.action)
        slots = [actions.idle, actions.walk, actions.run, *actions.stationary]
        for i, value in zip([0, 1, 3, 21, 23, 22, 24], slots):
            struct.pack_into('<i', data, 0x14 + i * 4, value)
        data[0x328], data[0x578], data[0x84c] = state.hidden, state.mode & 255, state.route_flag & 255
        struct.pack_into('<i', data, 0x850, state.behavior)
        data[0x840:0x840+C.sizeof(Timer)] = bytes(state.wait)
        self.u.mem_write(self.actor, bytes(data))
        self.u.mem_write(self.background + 0x140, struct.pack('<i', background))
        self.u.mem_write(0x725718, struct.pack('<I', self.background))
        self.u.mem_write(0x733700, struct.pack('<f', seconds))
        allowed = self.call(0x4fd5f1, self.actor, self.result) & 255
        final = bytes(self.u.mem_read(self.actor, len(data)))
        return (allowed, bytes(self.u.mem_read(self.result, 4)), struct.unpack_from('<i', final, 0x10)[0], struct.unpack_from('<i', final, 0x850)[0], final[0x840:0x840+C.sizeof(Timer)])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    native, lib, rng = Native(exe), library(), random.Random(0x4fd5f1)
    lib.bk_timer_poll.argtypes = [C.POINTER(Timer), C.c_uint32]
    lib.bk_timer_poll.restype = C.c_int
    lib.bk_npc_motion_actions.argtypes = [C.POINTER(Actions), C.c_uint]
    lib.bk_npc_motion_actions.restype = C.c_int
    lib.bk_npc_motion_select.argtypes = [C.POINTER(State), C.POINTER(Actions), C.c_int32, C.c_float, C.c_uint32, C.POINTER(Motion)]
    lib.bk_npc_motion_select.restype = C.c_int
    for group in range(5):
        a = Actions()
        assert lib.bk_npc_motion_actions(C.byref(a), group)
        assert [a.idle, a.walk, a.run, *a.stationary] == native.actions(group)
    timer_checks = 0
    clocks = [0, 1, 99, 100, 0x7fffffff, 0x80000000, 0xfffffff0, 0xffffffff]
    for duration in clocks:
        for deadline in clocks:
            for now in clocks:
                for armed in [0, 1, 2, 255]:
                    t = Timer(duration, deadline, armed)
                    wanted, wt = native.timer(t, now)
                    assert lib.bk_timer_poll(C.byref(t), now) == wanted
                    assert bytes(t) == bytes(wt) and native.clock_calls == 1
                    timer_checks += 1
    checks, waited = 0, 0
    for i in range(16000):
        if i < 12000:
            a = Actions(0, 1, 4, (C.c_int32 * 4)(10, 7, 12, 9))
            action = rng.choice([0, 1, 4, 10, 7, 12, 9, 99, -1])
        else:
            a = Actions(*[rng.randrange(-2, 3) for _ in range(3)], (C.c_int32 * 4)(*[rng.randrange(-2, 3) for _ in range(4)]))
            action = rng.randrange(-2, 3)
        s = State(action, rng.randrange(-2, 5), rng.choice([0, 0, 1, 2, 255]), rng.choice([-128, -1, 0, 1, 1, 2, 127]), rng.choice([-128, -1, 0, 1, 2, 3, 4, 5, 6, 127]), Timer(rng.choice(clocks), rng.choice(clocks), rng.choice([0, 1, 2, 255])))
        seconds = C.c_float(rng.choice([0, 1/60, .25, .125, 1.0, rng.uniform(0, 1000)])).value
        background, now = rng.choice([-1, 0, 1, 2, 2, 3]), rng.choice(clocks)
        original_bytes = bytes(s)
        expected = native.motion(s, a, background, seconds, now)
        result = Motion(-123, -123)
        assert lib.bk_npc_motion_select(C.byref(s), C.byref(a), background, seconds, now, C.byref(result))
        actual = (result.allowed, struct.pack('<f', result.distance), s.action, s.behavior, bytes(s.wait))
        assert actual == expected, (i, original_bytes.hex(), actual, expected)
        assert s.hidden == original_bytes[State.hidden.offset] and (s.mode & 255) == original_bytes[State.mode.offset] and (s.route_flag & 255) == original_bytes[State.route_flag.offset]
        checks += 1
        waited += native.clock_calls
    report = dict(x87_control_word="0x037f",passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), action_profiles=5, timer_cases=timer_checks, motion_cases=checks, native_timer_queries=waited, native_functions=['0x4adbb9', '0x4fd5f1', '0x4fb645..0x4fb73e'], hooks=['GetTickCount IAT 0x53f10c'], scope='Native movement permission/distance, action selection, wait timer and original group slot initializer. No route movement, collision, animation or full AI implied.')
    (ROOT/'local/original-npc-motion-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', timer_checks, 'timers,', checks, 'movement selections, 5 action profiles')

if __name__ == '__main__':
    main()
