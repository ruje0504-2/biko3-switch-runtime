"""Original4a5870/4a65fc file composition, with bounded atomic rejection.

Native decoding runs on valid inputs only. Malformed-input and capacity
rejections are explicit portable policies, not claims about original UB.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct

from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP
from original_bom_oracle import Binding, Config, Native, NAMES, OFFSETS
from model_binding import ROOT
from playback_binding import library
from bk3_assets import Archive


class BindingSet(C.Structure):
    _fields_ = [('mode', C.c_uint32), ('count', C.c_uint32), ('bindings', Binding * 8)]


def append_native(vm, raw):
    vm.data = raw
    vm.u.mem_write(vm.stack, struct.pack('<4I', vm.stop, vm.config, 0x300d000, 0x300d000))
    vm.u.reg_write(UC_X86_REG_ESP, vm.stack)
    vm.u.emu_start(0x4a65fc, vm.stop, count=1000000)
    assert vm.u.reg_read(UC_X86_REG_EIP) == vm.stop


def check(exe, data, lib):
    lib.bk_bom_decode_append.argtypes = [C.c_void_p, C.c_size_t, C.POINTER(BindingSet), C.c_void_p]
    lib.bk_bom_binding_set_append.argtypes = [C.POINTER(BindingSet), C.POINTER(Config), C.c_void_p]
    err = C.create_string_buffer(256)
    archive = Archive(data / 'fambom.pp')
    files = {entry.name: archive.read(entry) for entry in archive.entries if entry.name.endswith('.bom')}
    vm = Native(exe)
    sequences = [('jouhansin.bom', 'kahansin.bom')]
    sequences += [(a, b) for a in files for b in files]
    comparisons = 0
    for names in sequences:
        result = BindingSet()
        for step, name in enumerate(names):
            raw = files[name]
            assert lib.bk_bom_decode_append(raw, len(raw), C.byref(result), err), (names, err.value)
            if step == 0:
                vm.decode(raw)
                vm.u.mem_write(vm.config + 4, b'\\bk3_11.pp\0')
            else:
                append_native(vm, raw)
                assert vm.string(vm.config + 4) == b'\\bk3_11.pp'
            assert result.count == vm.read(vm.config)
            assert result.mode == vm.read(vm.config + 0x4208)
            for i in range(result.count):
                for field, offset in zip(NAMES, OFFSETS):
                    assert getattr(result.bindings[i], field) == vm.string(vm.config + offset + i * 260), (names, step, i, field)
                    comparisons += 1
    # Mode replacement on a zero-row file and exact eight-row capacity.
    def fixture(count, mode):
        rows = [b'ignored-primary', b'ignored-secondary'] + [b'field'] * 32
        if count < 4:
            rows[2 + count * 8] = b''
        return struct.pack('<I', mode) + b''.join(struct.pack('<I', len(s) + 1) + s + b'\0' for s in rows)
    result = BindingSet()
    for i, count in enumerate([4, 4, 0]):
        raw = fixture(count, 0x12345678 + i)
        assert lib.bk_bom_decode_append(raw, len(raw), C.byref(result), err), err.value
        if i == 0:
            vm.decode(raw)
        else:
            append_native(vm, raw)
        assert result.count == vm.read(vm.config) and result.mode == vm.read(vm.config + 0x4208)
    before = bytes(result)
    raw = fixture(1, 97)
    assert not lib.bk_bom_decode_append(raw, len(raw), C.byref(result), err)
    assert bytes(result) == before
    rejected = 1
    raw = files['kahansin.bom']
    rng = random.Random(0x4a65fc)
    malformed = [raw[:i] for i in range(len(raw))]
    for _ in range(4096):
        candidate = bytearray(raw)
        for _ in range(rng.randrange(1, 6)):
            candidate[rng.randrange(len(candidate))] = rng.randrange(256)
        malformed.append(bytes(candidate))
    for candidate in malformed:
        result = BindingSet()
        first = files['jouhansin.bom']
        assert lib.bk_bom_decode_append(first, len(first), C.byref(result), err)
        before = bytes(result)
        if not lib.bk_bom_decode_append(candidate, len(candidate), C.byref(result), err):
            assert bytes(result) == before
            rejected += 1
        else:
            assert 3 <= result.count <= 7
            previous = BindingSet.from_buffer_copy(before)
            assert all(bytes(result.bindings[i]) == bytes(previous.bindings[i]) for i in range(3))
    return dict(passed=True, sequences=len(sequences), native_fields=comparisons,
                malformed_cases=len(malformed), rejected=rejected, exact_capacity=8,
                input_sha256={name: hashlib.sha256(raw).hexdigest() for name, raw in files.items()})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-bom-append-oracle.json')
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    result = check(exe, args.data, library())
    result.update(exe_sha256=hashlib.sha256(exe).hexdigest(), scope=__doc__)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS BOM append', result['sequences'], result['native_fields'], result['rejected'], flush=True)


if __name__ == '__main__':
    main()
