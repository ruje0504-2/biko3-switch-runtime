"""Compare C material state against isolated original x86 functions.

Only memcpy and two D3D device calls are stubbed; no game or OS API is run.
The shader, depth, lighting and fog are outside this oracle's scope.
"""
import argparse
import ctypes as C
import json
import math
from pathlib import Path
import random
import struct
import sys
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, Material, library, decode

sys.path.insert(0, str(ROOT/'tools'))
from bk3_assets import Archive

class State(C.Structure):
    _fields_ = [('diffuse', C.c_float*4), ('encoded_alpha', C.c_float),
                ('blend', C.c_int), ('visible', C.c_int), ('alpha_hint', C.c_int),
                ('specular_enabled', C.c_int)]

def bind(lib):
    lib.bk_material_state.argtypes = [C.POINTER(Material), C.c_int, C.POINTER(State), C.c_void_p]
    lib.bk_material_state.restype = C.c_int

class Oracle:
    def __init__(self, exe):
        self.uc = machine(exe)
        self.mat, self.src, self.mesh = 0x3000000, 0x3000200, 0x3000300
        self.device, self.table = 0x3001000, 0x3001100
        self.set_state, self.set_material = 0x3002000, 0x3002010
        self.write(self.device, self.table)
        self.write(self.table+0x50, self.set_state)
        self.write(self.table+0x40, self.set_material)
        self.write(0x6455a0, self.device)
        for addr in (0x40b4a0, self.set_state, self.set_material):
            self.uc.hook_add(UC_HOOK_CODE, self.hook, begin=addr, end=addr)
        self.uc.hook_add(UC_HOOK_CODE, lambda uc, a, n, _: uc.emu_stop(),
                         begin=0x444c1f, end=0x444c1f)

    def read(self, addr): return struct.unpack('<I', self.uc.mem_read(addr, 4))[0]
    def write(self, addr, value): self.uc.mem_write(addr, struct.pack('<I', value))

    def hook(self, uc, address, size, _):
        sp = uc.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<4I', uc.mem_read(sp, 16))
        cleanup = 0
        if address == 0x40b4a0:
            _, dest, src, length = args
            assert length == 68
            uc.mem_write(dest, bytes(uc.mem_read(src, length)))
        elif address == self.set_state:
            _, device, state, value = args
            assert device == self.device
            self.states[state] = value
            cleanup = 12
        else:
            _, device, material, _ = args
            assert device == self.device
            self.submitted = bytes(uc.mem_read(material, 68))
            cleanup = 8
        uc.reg_write(UC_X86_REG_EAX, 0)
        uc.reg_write(UC_X86_REG_ESP, sp+4+cleanup)
        uc.reg_write(UC_X86_REG_EIP, args[0])

    def call(self, address, *args, visibility=False):
        stack, stop = 0x2008000, 0x300f000
        self.uc.mem_write(stack, struct.pack('<'+'I'*(len(args)+1), stop, *args))
        self.uc.reg_write(UC_X86_REG_ESP, stack)
        self.uc.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.uc.emu_start(address, stop, timeout=1_000_000, count=10000)
        pc = self.uc.reg_read(UC_X86_REG_EIP)
        assert pc in ([stop, 0x444c1f] if visibility else [stop]), hex(pc)
        return pc

    def apply(self, m, texture_alpha):
        self.states, self.submitted = {}, None
        self.uc.mem_write(self.mat, bytes(0x200))
        self.uc.mem_write(self.mesh, bytes(0x200))
        for addr in (0x645608, 0x645634, 0x645680, 0x645688): self.write(addr, 0)
        self.write(0x64560c, 100)  # Force factor state emission, no cache shortcut.
        self.write(0x645660, texture_alpha)
        self.uc.mem_write(self.src, C.string_at(C.addressof(m)+Material.diffuse.offset, 68))
        self.call(0x43041a, self.mat, self.src)
        encoded = bytes(self.uc.mem_read(self.mat+0x7c, 4))
        self.write(self.mesh+0xc8, self.mat)
        visible = self.call(0x444bc1, self.mesh, visibility=True) == 0x444c1f
        self.call(0x43056d, self.mat)
        assert self.submitted is not None
        return (encoded, self.submitted[:16], self.states[19], self.states[20],
                visible, self.read(0x645608), self.states[29])

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe', type=Path); p.add_argument('data', type=Path)
    args = p.parse_args(); oracle = Oracle(args.exe.read_bytes()); lib = library(); bind(lib)
    checked = 0; unique_alpha = set(); modes = [0, 0, 0]; models = 0

    def compare(m):
        nonlocal checked
        unique_alpha.add(bytes(m.diffuse)[12:16])
        for texture_alpha in (0, 1):
            native = oracle.apply(m, texture_alpha)
            state = State(); error = C.create_string_buffer(256)
            assert lib.bk_material_state(C.byref(m), texture_alpha, C.byref(state), error), error.value
            source, dest = [(5, 6), (5, 2), (1, 4)][state.blend]
            actual = (struct.pack('<f', state.encoded_alpha), bytes(state.diffuse), source, dest,
                      bool(state.visible), state.alpha_hint, state.specular_enabled)
            assert native == actual, (m.diffuse[3], texture_alpha, native, actual)
            checked += 1; modes[state.blend] += 1

    for pack in sorted(args.data.glob('*.pp')):
        archive = Archive(pack)
        for entry in archive.entries:
            if not entry.name.lower().endswith('.x'): continue
            data = archive.read(entry)
            if data[:4] != b'OBJM': continue
            result, out, error = decode(lib, data); assert result == 1, error
            try:
                m = out.contents; models += 1
                for i in range(m.material_count): compare(m.materials[i])
            finally: lib.bk_model_destroy(out)
    original_checks = checked
    # Exact thresholds and neighboring float32 values, plus finite randomized values.
    samples = [-10, -1, -0.5, -0.00001, -0.0, 0, 0.2, 0.99999, 1, 1.00001, 1.5, 2, 10]
    boundaries = samples[:]
    for x in boundaries:
        bits = struct.unpack('<I', struct.pack('<f', x))[0]
        for delta in (-1, 1):
            if 0 <= bits+delta <= 0xffffffff:
                value = struct.unpack('<f', struct.pack('<I', bits+delta))[0]
                if math.isfinite(value):
                    samples.append(value)
    rng = random.Random(271)
    samples += [rng.uniform(-4, 4) for _ in range(1000)]
    for a in samples:
        m = Material(); m.diffuse[:] = [0.3, 0.5, 0.9, a]; m.power = 0.01
        compare(m)
    summary = {'passed': True, 'models': models, 'original_material_checks': original_checks,
               'boundary_random_checks': checked-original_checks, 'checks': checked,
               'blend_counts': modes, 'unique_alpha_bits': len(unique_alpha),
               'native_functions': ['0x43041a', '0x43056d', '0x444bc1 early visibility'],
               'compared': ['effective diffuse bytes', 'normalized encoded alpha bytes',
                            'source/destination factors', 'mesh visibility', 'alpha cache hint', 'specular flag']}
    (ROOT/'local/original-material-oracle.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary, indent=2))

if __name__ == '__main__': main()
