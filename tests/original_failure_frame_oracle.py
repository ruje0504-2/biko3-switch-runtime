"""Full51b244 dispatcher with camera/audio/presentation boundary captures.
Compare ordered callbacks plus live actor/prop/visibility bytes. Five camera
implementations and actor tails have separate native-instruction oracles.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from original_prop_route_oracle import Native as Base
from model_binding import ROOT, library
CAMERAS = [0x4bc919, 0x4bcab3, 0x4bcca9, 0x4bd04f, 0x4bd3ed]
STAGES = [0x4bfea9, 0x4fc36d, 0x4f737b, 0x512102]
BYTES = [0xbf9b98, 0x71b838, 0x729110]
INTS = [0x71b520, 0x728df8, 0x71b524, 0x728dfc, 0x728e50]
class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        for address in CAMERAS + STAGES + [0x46435e]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)
    def snapshot(self):
        return tuple(self.u.mem_read(a, 1)[0] for a in BYTES) + tuple(struct.unpack('<i', self.u.mem_read(a, 4))[0] for a in INTS + [self.prop])
    def hook(self, u, a, size, _):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, arg0, arg1 = struct.unpack('<3I', u.mem_read(sp, 12))
        if a in CAMERAS:
            assert arg0 == 0x71af38
            event = ('camera', CAMERAS.index(a))
            u.reg_write(UC_X86_REG_EAX, self.arrived)
        elif a == 0x46435e:
            assert arg1 == 0
            event = ('restart', {0x3100000: 0, 0x3101000: 1}[arg0])
        else:
            if a != STAGES[3]:
                assert arg0 == [0x71b510, 0x728de8, 0x725718][STAGES.index(a)]
            event = ('present', STAGES.index(a))
        self.events.append((event, self.snapshot()))
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, ret)
    def run(self, outcome, state, arrived, index):
        self.events, self.arrived, self.prop = [], arrived, 0x729d90 + index * 0x998
        for a, x in zip(BYTES, state): self.u.mem_write(a, bytes([x]))
        for a, x in zip(INTS + [self.prop], state[3:]): self.u.mem_write(a, struct.pack('<i', x))
        self.u.mem_write(0x71bcd8, bytes([outcome]))
        self.u.mem_write(0x728de4, struct.pack('<I', index))
        self.u.mem_write(0x72976c, struct.pack('<I', 0x3100000))
        self.u.mem_write(0x72a1c8 + index * 0x998, struct.pack('<I', 0x3101000))
        self.call(0x51b244, b'')
        return self.snapshot(), self.events
P8, P32 = C.POINTER(C.c_uint8), C.POINTER(C.c_int32)
class Bindings(C.Structure):
    _fields_ = [(x, P8) for x in ['visible', 'player_hidden', 'npc_hidden']] + [(x, P32) for x in ['player_action', 'npc_action', 'player_idle', 'npc_idle', 'npc_rear', 'prop_action']]
Camera = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_int, C.POINTER(C.c_int), C.c_void_p)
Event = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_int, C.c_void_p)
class Ops(C.Structure): _fields_ = [('context', C.c_void_p), ('camera', Camera), ('restart', Event), ('present', Event)]
def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); digest = hashlib.sha256(exe).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    n, lib, rng, error = Native(exe), library(), random.Random(0x51b244), C.create_string_buffer(256)
    lib.bk_failure_frame_step.argtypes = [C.c_uint8, C.POINTER(Bindings), C.POINTER(Ops), C.c_void_p]
    state = [C.c_uint8() for _ in range(3)] + [C.c_int32() for _ in range(6)]
    b = Bindings(*(C.pointer(x) for x in state)); seen = []; fail_at = -1
    def record(kind, value):
        seen.append(((kind, value), tuple(x.value for x in state)))
        return len(seen) - 1 != fail_at
    def camera(_, kind, out, err): out[0] = arrived; return record('camera', kind)
    ops = Ops(None, Camera(camera), Event(lambda _, value, err: record('restart', value)), Event(lambda _, value, err: record('present', value)))
    callbacks = restarts = 0
    for case in range(12000):
        initial = [rng.choice([0, 0, 1, 2, 255]) for _ in range(3)] + [rng.randrange(-2**31, 2**31) for _ in range(6)]
        outcome = rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 128, 255]); arrived = rng.randrange(2)
        expected, trace = n.run(outcome, initial, arrived, case % 16)
        for x, value in zip(state, initial): x.value = value
        seen.clear()
        assert lib.bk_failure_frame_step(outcome, C.byref(b), C.byref(ops), error), error.value
        assert tuple(x.value for x in state) == expected, (case, expected)
        assert seen == trace, (case, seen, trace)
        callbacks += len(trace); restarts += sum(e[0][0] == 'restart' for e in trace)
    # A service failure must stop at that exact prefix with no later writes.
    initial = [0, 0, 0, 10, 20, 30, 40, 50, 60]; arrived = 1
    expected, trace = n.run(5, initial, arrived, 0)
    for fail_at in range(len(trace)):
        for x, value in zip(state, initial): x.value = value
        seen.clear()
        assert not lib.bk_failure_frame_step(5, C.byref(b), C.byref(ops), error)
        assert seen == trace[:fail_at + 1]
        assert tuple(x.value for x in state) == trace[fail_at][1]
    fail_at = -1; b.prop_action = P32(); seen.clear(); before = tuple(x.value for x in state)
    assert not lib.bk_failure_frame_step(5, C.byref(b), C.byref(ops), error)
    assert not seen and tuple(x.value for x in state) == before
    report = dict(passed=True, exe_sha256=digest, frames=12000, callbacks=callbacks, restarts=restarts, failure_prefixes=len(trace), scope=__doc__)
    (ROOT / 'local/original-failure-frame-oracle.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)
if __name__ == '__main__': main()
