"""Original ending inventory ownership: initializer, entry and release prefix.

Entry dispatch compares five live portable inventory bytes with x86 globals.
The initializer and release-prefix checks establish the native preservation
policy; the actual scene lifecycle is covered separately by inventory-app.
Resource load, record clear and fade services are observing boundaries here.
"""
import argparse
import ctypes as C
import hashlib
import json
import struct
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP
import original_ending_entry_oracle as entry
import original_ending_state_oracle as state
from original_matrix_oracle import machine


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    assert hashlib.sha256(exe).hexdigest() == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    initial = state.Native(exe)
    native = entry.Native(exe)
    release = machine(exe)
    lib = entry.library()
    lib.bk_ending_entry_dispatch.argtypes = [C.POINTER(entry.Bindings), C.c_int8,
        C.c_uint32, C.c_float, C.POINTER(entry.Ops), C.c_void_p]
    calls = []
    def fade(u, _address, _size, _context):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, owner, stage = struct.unpack('<3I', u.mem_read(sp, 12))
        assert (owner, stage) == (0xb537e8, 1)
        calls.append(bytes(u.mem_read(0x71bcdc, 5)))
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, ret)
    release.hook_add(UC_HOOK_CODE, fade, begin=0x50e633, end=0x50e633)
    digest = hashlib.sha256()
    errors = C.create_string_buffer(256)
    entries = releases = 0
    for value in range(256):
        before = bytes((value + i * 47) & 255 for i in range(5))
        initial.u.mem_write(0x71bcdc, before)
        initial.run(state.State(), value % 5, value % 2, .75,
            (C.c_int32 * 2)(0, 0), state.ui.t.Fade(.75, 2, 0), False)
        assert bytes(initial.u.mem_read(0x71bcdc, 5)) == before
        for previous in [8, 24, 0, 255]:
            inventory = (C.c_uint8 * 5).from_buffer_copy(before)
            f, control, aux = entry.Frame(), entry.Control(), entry.Aux()
            f.group, aux.variant = value % 5, value % 2
            selected, gauge = C.c_int32(-7), C.c_float(4.5)
            bindings = entry.Bindings(C.pointer(f), C.pointer(control), C.pointer(aux),
                C.cast(C.byref(inventory, 1), C.POINTER(C.c_uint8)),
                C.cast(C.byref(inventory, 2), C.POINTER(C.c_uint8)),
                C.pointer(selected), C.pointer(gauge))
            snap = struct.pack('<iBiffBBiBi', f.phase, control.variant, aux.variant,
                aux.progress, gauge.value, inventory[1], inventory[2], selected.value,
                f.group, f.state_721ee0)
            native.u.mem_write(0x71bcdc, before)
            native.run(snap, previous, 0, .75, False, f.group, aux.variant)
            callbacks = entry.Ops(None, entry.Load(lambda *_: 1),
                                  entry.Clear(lambda *_: 1), entry.Prepare(lambda *_: 1))
            assert lib.bk_ending_entry_dispatch(C.byref(bindings), previous, 0, .75,
                                                C.byref(callbacks), errors), errors.value
            wanted = before[:1] + b'\x01\x01' + before[3:] if previous == 24 else before
            assert bytes(inventory) == bytes(native.u.mem_read(0x71bcdc, 5)) == wanted
            digest.update(bytes(inventory))
            entries += 1
        for previous in range(256):
            release.mem_write(0x71bcdc, before)
            release.mem_write(0x721ad4, bytes([previous]))
            release.reg_write(UC_X86_REG_ESP, 0x2008000)
            release.emu_start(0x4ceb27, 0x4ceb56, count=1000)
            assert release.reg_read(UC_X86_REG_EIP) == 0x4ceb56
            wanted = before[:1] + b'\x00\x00' + before[3:] if previous == 24 else before
            assert bytes(release.mem_read(0x71bcdc, 5)) == wanted and calls[-1] == before
            digest.update(wanted)
            releases += 1
    report = dict(passed=True, scope=__doc__, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  initializations=256, portable_entries=entries,
                  native_release_prefixes=releases, inventory_bytes=5,
                  state_sha256=digest.hexdigest())
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS ending inventory:', json.dumps(report, sort_keys=True), flush=True)


if __name__ == '__main__':
    main()
