"""Fixed CKP flag slots versus native area loader and prop consumers.

Runs unmodified 4ff78e/5011bc for all45 actor entries after dirtying the
destination. All selected actor CKPs are full20480-byte tables. Then runs
512595/4f56a3 with a retained index past each active route's sentinel, both
with authored flags and explicit tail-flag fixtures. Windows I/O and recursive
material writes are existing boundary hooks; route/prop decisions are x86.
Short-input zero extension is decoder policy, not inferred native behavior.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct

from unicorn import UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_EAX
from model_binding import library
from original_route_oracle import Native as RouteNative
from original_prop_motion_oracle import Native as PropNative, State, Shared, Input, Effects, bind


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('report', type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    loader, prop, lib = RouteNative(exe, args.data), PropNative(exe), library()
    bind(lib)
    lib.bk_route_decode.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p]
    lib.bk_route_decode.restype = C.c_void_p
    lib.bk_route_destroy.argtypes = [C.c_void_p]
    lib.bk_route_count.argtypes = [C.c_void_p]
    lib.bk_route_flag_slot.argtypes = [C.c_void_p, C.c_uint32, C.POINTER(C.c_uint8)]
    lib.bk_route_point.argtypes = [C.c_void_p, C.c_uint32]
    lib.bk_route_point.restype = C.c_void_p
    error = C.create_string_buffer(256)
    profiles, flag_checks, decisions, reads = [], 0, 0, []

    def read_hook(u, access, address, size, value, user):
        reads.append((address, size))

    prop.u.hook_add(UC_HOOK_MEM_READ, read_hook, begin=loader.destination,
                   end=loader.destination + 20479)

    def compare(state, wanted):
        for name in ['action', 'hidden', 'background_wait', 'route_flag']:
            assert getattr(state, name) == wanted[name], (name, wanted)
        assert (state.wait.duration, state.wait.deadline, state.wait.armed) == wanted['wait']

    for group in range(5):
        for area in range(9):
            loader.uc.mem_write(loader.destination, b'\xa5' * 20480)
            name, count = loader.select(group, area, 1)
            raw = (args.data / name).read_bytes()
            assert len(raw) == 20480
            assert bytes(loader.uc.mem_read(loader.destination, 20480)) == raw
            route = lib.bk_route_decode(raw, len(raw), error)
            assert route, error.value
            try:
                assert lib.bk_route_count(route) == count
                for index in range(1024):
                    flag = C.c_uint8(99)
                    assert lib.bk_route_flag_slot(route, index, C.byref(flag))
                    assert flag.value == raw[index * 20 + 16], (group, area, index)
                    flag_checks += 1
            finally:
                lib.bk_route_destroy(route)
            # Both queried slots lie beyond the first zero sentinel. The
            # explicit variants distinguish real table reads from clamping or
            # unconditionally returning zero for inactive slots.
            last_crossed = count + 3
            assert last_crossed <= 1024
            indices = [last_crossed - 1, last_crossed - 2]
            for variant in [None, (3, 0), (0, 3), (255, 128)]:
                payload = bytearray(raw)
                if variant:
                    for index, flag in zip(indices, variant):
                        payload[index * 20 + 16] = flag
                route = lib.bk_route_decode(bytes(payload), len(payload), error)
                assert route and lib.bk_route_count(route) == count, error.value
                try:
                    inp = Input()
                    inp.seconds, inp.player_action = 1/60, 1
                    inp.player_actions[10] = 99
                    inp.npc_last_crossed = last_crossed
                    for i, index in enumerate(indices):
                        assert not lib.bk_route_point(route, index)
                        flag = C.c_uint8()
                        assert lib.bk_route_flag_slot(route, index, C.byref(flag))
                        inp.npc_previous_flags[i] = C.c_int8(flag.value).value
                    for operation in ['motion', 'point']:
                        state = State()
                        state.actions[:] = [10, 11, 12, 13]
                        state.action, state.background_wait = 10, 1
                        shared = Shared(2, 1, 17)
                        prop.prepare(state, shared, inp)
                        prop.u.mem_write(loader.destination, bytes(payload))
                        before = len(reads)
                        if operation == 'motion':
                            prop.call(0x512595, struct.pack('<II', prop.actor, 0x3002000))
                            wanted, allowed = prop.read(), prop.u.reg_read(UC_X86_REG_EAX) & 255
                            effects = Effects()
                            assert lib.bk_prop_motion_select(C.byref(state), C.byref(shared),
                                                             C.byref(inp), C.byref(effects))
                            assert effects.allowed == allowed
                            assert (shared.distance, shared.alpha) == (
                                struct.unpack('<f', prop.u.mem_read(0x3002000, 4))[0],
                                struct.unpack('<f', prop.u.mem_read(0xbf4b58, 4))[0])
                            assert (effects.material, effects.alpha) == prop.command
                        else:
                            prop.call(0x4f56a3, struct.pack('<I4fB3x', prop.actor, 1, 2, 3, 4, 3))
                            wanted = prop.read()
                            assert lib.bk_prop_point_apply(C.byref(state), 3, C.byref(inp))
                        compare(state, wanted)
                        observed = reads[before:]
                        expected = [(loader.destination + index * 20 + 16, 1) for index in indices]
                        if inp.npc_previous_flags[0] == 3:
                            expected = expected[:1]
                        assert observed == expected, (group, area, operation, observed, expected)
                        decisions += 1
                finally:
                    lib.bk_route_destroy(route)
            profiles.append(dict(group=group, area=area, file=name, count=count,
                                 sha256=hashlib.sha256(raw).hexdigest(), retained_index=last_crossed))
    report = dict(passed=True, scope=__doc__, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  profiles=profiles, fixed_flag_checks=flag_checks,
                  native_prop_decisions=decisions, native_tail_reads=len(reads))
    args.report.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS checkpoint route flags', len(profiles), 'entries', flag_checks,
          'flags', decisions, 'native prop decisions', len(reads), 'tail reads')


if __name__ == '__main__':
    main()
