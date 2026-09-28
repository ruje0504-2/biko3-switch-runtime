"""Execute complete 0x4fce2b, supplying only clock/audio boundaries.

The native suppressed near50 branch reads an uninitialized local. That local
is explicitly seeded to zero for comparison with the port's documented fix;
separate fixtures demonstrate the original stack-residue dependency.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from original_npc_point_oracle import Native as PointNative, Point
from original_npc_motion_oracle import Actions, State as Motion, Timer
from original_npc_contact_oracle import Input as Contact
from model_binding import ROOT, library

class State(C.Structure):
    _fields_ = [('point', Point), ('stimulus', C.c_int8)]
class Shared(C.Structure):
    _fields_ = [('prompt', C.c_uint8), ('response', C.c_uint8), ('outcome', C.c_uint8)]
class Input(C.Structure):
    _fields_ = [('contact', Contact), ('player_action', C.c_int32), ('suppressed_actions', C.c_int32*6), ('active_clip', C.c_int32)]
class Effects(C.Structure):
    _fields_ = [('sound', C.c_uint8), ('used_zero_choice', C.c_uint8)]

class Native(PointNative):
    def __init__(self, exe):
        super().__init__(exe)
        for address in [0x534a34, 0x4fd2d5]:
            self.u.hook_add(UC_HOOK_CODE, self.observe, begin=address, end=address)
    def observe(self, u, address, size, user):
        if address == 0x534a34: self.random_calls += 1
        else: self.choice_reads += 1
    def media(self, u, address, size, user):
        assert address == 0x46435e
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, handle, flags = struct.unpack('<III', u.mem_read(sp, 12))
        assert flags == 0 and handle in [0xabc001, 0xabc002]
        self.sounds.append(handle - 0xabc000)
        u.reg_write(UC_X86_REG_ESP, sp+4)
        u.reg_write(UC_X86_REG_EIP, ret)
        u.reg_write(UC_X86_REG_EAX, 0)
    def run(self, state, shared, inp, seed, now, residue=0):
        actions = Actions(0, 1, 4, (C.c_int32*4)(10, 7, 12, 9))
        data = self.write_point(state.point, actions, 0, inp.active_clip, inp.contact.cursor)
        struct.pack_into('<I', data, 0, self.background)
        data[0x319] = state.stimulus & 255
        struct.pack_into('<3f', data, 0x29c, *inp.contact.actor_position)
        struct.pack_into('<f', data, 0x32c, inp.contact.alpha)
        u = self.u
        u.mem_write(self.actor, bytes(data))
        u.mem_write(0x7219a8, struct.pack('<ii', inp.contact.group, inp.contact.area))
        u.mem_write(0x71b7ac, struct.pack('<3f', *inp.contact.player_position))
        u.mem_write(0x71b7c4, struct.pack('<3f', *inp.contact.player_direction))
        u.mem_write(0x71bcdf, bytes([inp.contact.interaction_df & 255, inp.contact.interaction_e0 & 255]))
        u.mem_write(0x71b520, struct.pack('<i', inp.player_action))
        for address, action in zip([0x71b550, 0x71b554, 0x71b558, 0x71b55c, 0x71b564, 0x71b568], inp.suppressed_actions):
            u.mem_write(address, struct.pack('<i', action))
        for address, value in zip([0x71ba8c, 0x71bcda, 0x71bcd8], [shared.prompt, shared.response, shared.outcome]):
            u.mem_write(address, bytes([value]))
        for address, handle in [(0x7299c0, 0xabc001), (0x729ae0, 0xabc002)]:
            u.mem_write(address, struct.pack('<I', handle))
        u.mem_write(0x58edd8, struct.pack('<I', seed))
        u.mem_write(self.stack-12, struct.pack('<i', residue))
        self.now, self.clock_calls, self.random_calls, self.choice_reads, self.sounds = now, 0, 0, 0, []
        self.call(0x4fce2b, self.actor)
        result = bytes(u.mem_read(self.actor, len(data)))
        # Verify that the original changed only fields represented by this API.
        for start, size in [(0x10, 4), (0x319, 1), (0x84c, 1), (0x850, 4), (0x854, 12), (0x870, 1)]:
            data[start:start+size] = result[start:start+size]
        assert bytes(data) == result
        out = State.from_buffer_copy(state)
        out.point.motion.action = struct.unpack_from('<i', result, 0x10)[0]
        out.point.motion.behavior = struct.unpack_from('<i', result, 0x850)[0]
        out.point.motion.route_flag = result[0x84c]
        out.point.action_wait = Timer.from_buffer_copy(result[0x854:0x860])
        out.point.gate_state = result[0x870]
        out.stimulus = result[0x319]
        interaction = Shared(*(u.mem_read(a, 1)[0] for a in [0x71ba8c, 0x71bcda, 0x71bcd8]))
        random_state = struct.unpack('<I', u.mem_read(0x58edd8, 4))[0]
        assert len(self.sounds) <= 1 and self.random_calls <= 1
        return out, interaction, random_state, Effects(self.sounds[0] if self.sounds else 0, self.choice_reads and not self.random_calls)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    native, lib, rng = Native(exe), library(), random.Random(0x4fce2b)
    lib.bk_npc_ai_step.argtypes = [C.POINTER(State), C.POINTER(Shared), C.POINTER(C.c_uint32), C.POINTER(Input), C.c_uint32, C.POINTER(Effects), C.c_void_p]
    lib.bk_npc_ai_step.restype = C.c_int
    lib.bk_random_next.argtypes = [C.POINTER(C.c_uint32)]
    lib.bk_random_next.restype = C.c_uint16
    random_checks = 0
    for _ in range(4096):
        seed = rng.getrandbits(32)
        native.u.mem_write(0x58edd8, struct.pack('<I', seed))
        native.random_calls = 0
        wanted = native.call(0x534a34)
        actual = C.c_uint32(seed)
        assert lib.bk_random_next(C.byref(actual)) == wanted
        assert actual.value == struct.unpack('<I', native.u.mem_read(0x58edd8, 4))[0]
        random_checks += 1
    totals = dict(cases=0, timer_queries=0, random_queries=0, sound_commands=0, zero_choice_cases=0)
    error = C.create_string_buffer(256)
    clocks = [0, 1, 100, 2000, 5000, 0x7fffffff, 0x80000000, 0xfffffff0, 0xffffffff]
    for case in range(24000):
        def timer(): return Timer(rng.choice(clocks), rng.choice(clocks), rng.choice([0, 0, 1, 2, 255]))
        s = State(Point(Motion(rng.choice([0, 1, 4, 7, 9, 10, 12, -1, 99]), rng.choice([0, 1, 1, 2, 3, 4, -1]), rng.choice([0, 0, 0, 1, 2, 255]), rng.choice([0, 1, 1, 1, 2, -1, 127]), rng.choice([0, 1, 3, 4, 5, 7, 9, -1]), timer()), timer(), rng.randrange(256), rng.randrange(256), rng.choice([0, 0, 1, 2, 255])), rng.choice([-128, -1, 0, 0, 1, 2, 127]))
        shared = Shared(*(rng.randrange(256) for _ in range(3)))
        group = rng.randrange(5)
        area, cursor = rng.choice([-1, 0, 4, 5, 6, 7, 8, 8, 9]), rng.choice([0, 47, 67, 83, 195, 231])
        if case % 6 == 0:
            area, cursor, s.point.motion.behavior = [4, 5, 5, 6, 5][group], [67, 83, 195, 47, 231][group], 0
        player = [rng.uniform(-100, 100) for _ in range(3)]
        actor = [player[0]+rng.choice([0, 8, 15, 20, 50, 50.00001, 70, 70.00001, 100]), player[1]+rng.choice([-20.00001, -20, 0, 20, 20.00001]), player[2]+rng.choice([0, 0, 5, 15, 40])]
        direction = rng.choice([[0, 0, 0], [0, 1, 0], [0, 0, 1], [1, 0, 0], [rng.uniform(-1, 1), 0, rng.uniform(-1, 1)]])
        inp = Input(Contact(group, area, cursor, (C.c_float*3)(*actor), rng.choice([0, .9, .90000004, .99, 1, 1]), (C.c_float*3)(*player), (C.c_float*3)(*direction), rng.choice([-1, 0, 1, 2]), rng.choice([-1, 0, 0, 1, 2])), rng.choice([0, 4, 10, 18, 19, 20, 21, 23, 27, -1]), (C.c_int32*6)(18, 19, 20, 21, 23, 27), rng.choice([0, 1, 4, 7, 9, 10, 12, -1]))
        seed, now = rng.getrandbits(32), rng.choice(clocks)
        expected, interaction, random_state, effect = native.run(s, shared, inp, seed, now)
        before = bytes(s).hex()
        output, actual_seed = Effects(255, 255), C.c_uint32(seed)
        assert lib.bk_npc_ai_step(C.byref(s), C.byref(shared), C.byref(actual_seed), C.byref(inp), now, C.byref(output), error), error.value
        assert (bytes(s), bytes(shared), actual_seed.value, bytes(output)) == (bytes(expected), bytes(interaction), random_state, bytes(effect)), (case, before, bytes(s).hex(), bytes(expected).hex(), bytes(shared).hex(), bytes(interaction).hex(), actual_seed.value, random_state, bytes(output).hex(), bytes(effect).hex())
        totals['cases'] += 1
        totals['timer_queries'] += native.clock_calls
        totals['random_queries'] += native.random_calls
        totals['sound_commands'] += len(native.sounds)
        totals['zero_choice_cases'] += output.used_zero_choice
    # Reproducible evidence for the one deliberate correction.
    s = State(Point(Motion(1, 1, 0, 1, 0, Timer()), Timer(2000, 0, 0), 0, 0, 0), 0)
    inp = Input(Contact(0, 0, 1, (C.c_float*3)(30, 0, 0), 1, (C.c_float*3)(0, 0, 0), (C.c_float*3)(0, 0, 1), 0, 0), 18, (C.c_int32*6)(18, 19, 20, 21, 23, 27), 1)
    residue_results = []
    for residue in [0, 1, 2, 3, 99]:
        result, _, _, effect = native.run(s, Shared(), inp, 123, 100, residue)
        assert effect.used_zero_choice and not native.random_calls
        residue_results.append(dict(stack_residue=residue, action=result.point.motion.action, duration=result.point.action_wait.duration, sound=effect.sound))
    assert residue_results[0]['action'] == 7 and residue_results[1]['action'] == 10 and residue_results[4]['action'] == 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), **totals, random_generator_cases=random_checks, x87_control_word='0x037f', native_functions=['0x4fce2b', '0x500b50', '0x500e74', '0x500ff5', '0x4ae641', '0x4ae783', '0x4adbb9', '0x534a34'], hooks=['GetTickCount', '0x46435e audio boundary', 'read-only branch/RNG counters'], intentional_correction=dict(local='ebp-8 in suppressed near50 reaction', port_choice=0, native_fixture_seed=0, consumes_extra_rng=False, original_residue_results=residue_results), scope='Complete NPC AI decisions, early returns, interaction codes, sound commands and action timer tail. Defined branches exact; uninitialized native choice explicitly controlled to zero. No route movement, collision, audio playback or game loop implied.')
    (ROOT/'local/original-npc-ai-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', totals, 'RNG cases', random_checks, 'native residue variants', residue_results)

if __name__ == '__main__': main()
