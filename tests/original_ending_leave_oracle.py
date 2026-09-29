"""Execute all six original leave routines, including the real CRT memset.
Compare every recovered ending field after each operation, preserving unrelated
fields and padding. Memory-write observation checks that every non-stack native
write has a modeled owner. No leaf service, resource or controller is replaced.
This proves retained-state resets, not the unfinished ending stage controllers.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
from pathlib import Path
from unicorn import UC_HOOK_MEM_WRITE
from original_prop_route_oracle import Native
from original_ending_state_oracle import State, REGIONS, snapshot
from model_binding import ROOT, library

ADDRESSES = [0x4e1c8f, 0x47dbcc, 0x49739a, 0x47a033, 0x482f91, 0x48d7f2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    native = Native(exe)
    lib = library()
    lib.bk_ending_state_leave.argtypes = [C.POINTER(State), C.c_int, C.c_void_p]
    error = C.create_string_buffer(256)
    rng = random.Random(0x4e1c8f)
    covered = set()
    for _, address, size, _ in REGIONS:
        covered.update(range(address, address + size))
    writes = set()

    def observe(_uc, _access, address, size, _value, _user):
        if 0x2000000 <= address < 0x2010000:
            return
        actual = set(range(address, address + size))
        assert actual <= covered, ('unmodeled original write', hex(address), size)
        writes.update(actual)

    # Observe each original once; later cases have no hooks at all.
    handle = native.u.hook_add(UC_HOOK_MEM_WRITE, observe)
    for address in ADDRESSES:
        native.call(address, b'')
    native.u.hook_del(handle)
    calls = 0
    per_snapshot = sum(size for _, _, size, _ in REGIONS)
    for case in range(1200):
        state = State.from_buffer_copy(rng.randbytes(C.sizeof(State)))
        # Alternate complete original order with one isolated operation.
        operations = range(6) if case % 2 == 0 else [case // 2 % 6]
        raw = bytes(state)
        for offset, address, size, _ in REGIONS:
            native.u.mem_write(address, raw[offset:offset + size])
        for operation in operations:
            before = bytes(state)
            native.call(ADDRESSES[operation], b'')
            assert lib.bk_ending_state_leave(C.byref(state), operation, error), error.value
            expected = b''.join(bytes(native.u.mem_read(a, size))
                                for _, a, size, _ in REGIONS)
            if snapshot(state) != expected:
                for offset, address, size, name in REGIONS:
                    actual = bytes(state)[offset:offset + size]
                    want = bytes(native.u.mem_read(address, size))
                    assert actual == want, (case, operation, name, actual.hex(), want.hex())
            masked = bytearray(before)
            for offset, _, size, _ in REGIONS:
                masked[offset:offset + size] = bytes(state)[offset:offset + size]
            assert bytes(state) == masked, 'padding/unregistered field changed'
            calls += 1
        before = bytes(state)
        assert not lib.bk_ending_state_leave(C.byref(state), -1, error)
        assert not lib.bk_ending_state_leave(C.byref(state), 6, error)
        assert bytes(state) == before
        if case % 200 == 199:
            print('PASS ending leave cases', case + 1, 'native calls', calls, flush=True)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  cases=1200, native_calls=calls, ordered_sequences=600,
                  invalid_operations=2400, mapped_regions=len(REGIONS),
                  compared_bytes=calls * per_snapshot,
                  native_written_bytes=len(writes), max_error=0,
                  replaced_services=[], scope=__doc__)
    (ROOT / 'local/original-ending-leave-oracle.json').write_text(
        json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
