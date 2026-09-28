"""Execute native MORP interpolation and blending with CPU vertex lock boundary."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
import sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from original_morph_decode_oracle import bind as bind_decode
from model_binding import ROOT, Model, Submesh, Vertex, Chunk, library, decode
sys.path.insert(0, str(ROOT/'tools'))
from bk3_assets import Archive

class Sample(C.Structure):
    _fields_ = [('blend', C.c_int), ('from_', C.c_float), ('to', C.c_float), ('weight', C.c_float)]
def bind(lib):
    bind_decode(lib)
    for name, args, result in [
        ('bk_morph_mesh_create', [C.POINTER(Submesh), C.c_void_p], C.c_void_p),
        ('bk_morph_mesh_destroy', [C.c_void_p], None),
        ('bk_morph_mesh_vertices', [C.c_void_p], C.POINTER(Vertex)),
        ('bk_morph_binding_create', [C.c_void_p, C.c_uint32, C.c_void_p, C.c_void_p], C.c_void_p),
        ('bk_morph_binding_destroy', [C.c_void_p], None),
        ('bk_morph_binding_loop', [C.c_void_p, C.c_int, C.c_void_p], C.c_int),
        ('bk_morph_binding_selection', [C.c_void_p, C.c_int, C.POINTER(C.c_uint16), C.c_size_t, C.c_void_p], C.c_int),
        ('bk_morph_binding_apply', [C.c_void_p, C.POINTER(Sample), C.c_void_p], C.c_int),
        ('bk_morph_binding_time', [C.c_void_p], C.c_float)]:
        fn = getattr(lib, name); fn.argtypes = args; fn.restype = result

class Native:
    stack, stop, track, mesh, nodes, vertices, destination = 0x2008000, 0x300f000, 0x3000000, 0x3000800, 0x4000000, 0x4100000, 0x6000000
    def __init__(self, exe):
        self.u = machine(exe); self.u.mem_map(0x4000000, 0x3000000)
        for ptr in [0x445a8e, 0x445ac1]: self.u.hook_add(UC_HOOK_CODE, self.hook, begin=ptr, end=ptr)
    def word(self, p, n): self.u.mem_write(p, struct.pack('<I', n))
    def hook(self, u, address, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, mesh = struct.unpack('<II', u.mem_read(sp, 8)); assert mesh == self.mesh
        if address == 0x445a8e:
            count, vertices = struct.unpack('<II', u.mem_read(sp+8, 8))
            self.word(count, self.count); self.word(vertices, self.destination)
        u.reg_write(UC_X86_REG_ESP, sp+4); u.reg_write(UC_X86_REG_EIP, ret)
    def seed(self, lib, morph, track, target):
        info = lib.bk_model_morph_track(morph, track).contents
        self.count = target.vertex_count; self.u.mem_write(self.track, bytes(0x400))
        # Blended, unselected FROM interpolation uses mesh+0x84 directly;
        # other branches use the count returned by the lock boundary.
        self.word(self.mesh+0x84, self.count)
        self.word(self.track+0x70, self.mesh); self.word(self.track+0x7c, info.key_count)
        self.word(self.track+0x80, self.nodes); self.word(self.track+0x84, self.nodes+(info.key_count-1)*0x60)
        self.word(self.track+0x88, 1); cursor = self.vertices
        for k in range(info.key_count):
            key = lib.bk_model_morph_key(morph, track, k).contents; ptr = self.nodes+k*0x60
            self.u.mem_write(ptr, bytes(0x60)); self.u.mem_write(ptr, struct.pack('<fIII', key.time, key.interpolation, key.vertex_count, cursor))
            self.word(ptr+0x18, self.nodes+(k+1)*0x60 if k+1<info.key_count else 0)
            self.u.mem_write(ptr+0x1c, bytes(key.matrix)); size = key.vertex_count*60
            self.u.mem_write(cursor, C.string_at(key.vertices, size)); cursor += size
        assert cursor < self.destination
        self.u.mem_write(self.track+0x78, struct.pack('<f', key.time))
        self.u.mem_write(self.destination, C.string_at(target.vertices, target.vertex_count*60))
    def selection(self, enabled, values):
        self.word(self.track+0x90, enabled); self.word(self.track+0x94, len(values)); self.word(self.track+0x98, 0x3001000)
        if values: self.u.mem_write(0x3001000, struct.pack('<'+'H'*len(values), *values))
    def apply(self, sample):
        words = [self.stop, self.track, struct.unpack('<I', bytes(C.c_float(sample.from_)))[0]]
        if sample.blend: words += [struct.unpack('<I', bytes(C.c_float(v)))[0] for v in [sample.to, sample.weight]]
        self.u.mem_write(self.stack, struct.pack('<'+'I'*len(words), *words))
        self.u.reg_write(UC_X86_REG_ESP, self.stack); self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(0x432642 if sample.blend else 0x4316be, self.stop, count=20000000)
        assert self.u.reg_read(UC_X86_REG_EIP) == self.stop
        return bytes(self.u.mem_read(self.destination, self.count*60))

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); ap.add_argument('data', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native, lib = Native(exe), library(); bind(lib); rng = random.Random(0x432642)
    error = C.create_string_buffer(256); cases = compared = rejects = track_count = 0; worst = 0; records = []
    def run(model, name, steps):
        nonlocal cases, compared, rejects, worst, track_count
        morph = lib.bk_model_morph_create(model, error); assert morph, (name, error.value)
        try:
            for track in range(lib.bk_model_morph_count(morph)):
                info = lib.bk_model_morph_track(morph, track).contents; target = model.contents.submeshes[info.submesh]
                key_times = [lib.bk_model_morph_key(morph, track, k).contents.time for k in range(info.key_count)]
                mesh = lib.bk_morph_mesh_create(C.byref(target), error); assert mesh, error.value
                binding = lib.bk_morph_binding_create(morph, track, mesh, error); assert binding, error.value
                try:
                    native.seed(lib, morph, track, target)
                    for case in range(steps):
                        loop = case % 3 != 0; native.word(native.track+0x88, loop)
                        assert lib.bk_morph_binding_loop(binding, loop, error)
                        selection = [0, target.vertex_count-1, 0, target.vertex_count//2] if case%3 else []
                        enabled = case % 5 != 0; native.selection(enabled, selection)
                        values = (C.c_uint16*len(selection))(*selection)
                        assert lib.bk_morph_binding_selection(binding, enabled, values, len(selection), error)
                        left = rng.randrange(len(key_times)-1); right = rng.randrange(len(key_times)-1)
                        from_time = [key_times[left], (key_times[left]+key_times[left+1])*.49, key_times[-1]+.25, key_times[-1], 0][case%5]
                        to_time = [key_times[right], key_times[right]*.9+key_times[right+1]*.1, key_times[-1]*2+.25][case%3]
                        sample = Sample(case%4 != 0, from_time, to_time, [0, .49999997, .5, 1, -.2, 1.2][case%6])
                        wanted = native.apply(sample)
                        assert lib.bk_morph_binding_apply(binding, C.byref(sample), error), (name, track, case, error.value)
                        actual = C.string_at(lib.bk_morph_mesh_vertices(mesh), target.vertex_count*60)
                        for v in range(target.vertex_count):
                            a, b = struct.unpack_from('<9f', actual, v*60), struct.unpack_from('<9f', wanted, v*60)
                            for field, (x, y) in enumerate(zip(a, b)):
                                delta = abs(x-y)/max(1, abs(y)); worst = max(worst, delta)
                                assert math.isfinite(delta) and delta < 3e-6, (name, track, case, v, field, x, y, bytes(sample).hex())
                            assert actual[v*60+36:(v+1)*60] == wanted[v*60+36:(v+1)*60]
                        assert lib.bk_morph_binding_time(binding) == struct.unpack('<f', native.u.mem_read(native.track+0x74, 4))[0]
                        if case%7 == 0:
                            before = actual; previous = lib.bk_morph_binding_time(binding)
                            invalid = Sample(1, sample.from_, float('nan'), .5)
                            assert not lib.bk_morph_binding_apply(binding, C.byref(invalid), error)
                            assert before == C.string_at(lib.bk_morph_mesh_vertices(mesh), len(before)) and previous == lib.bk_morph_binding_time(binding)
                            rejects += 1
                        cases += 1; compared += target.vertex_count
                    track_count += 1
                finally: lib.bk_morph_binding_destroy(binding); lib.bk_morph_mesh_destroy(mesh)
        finally: lib.bk_model_morph_destroy(morph)
    # Synthetic modes1..3, changing normals/UV/beta/residue and duplicate indices.
    for mode in range(4):
        raw = bytearray(96); struct.pack_into('<I', raw, 68, 1); struct.pack_into('<6I', raw, 72, 7, 0, 0, 0, 0, 3)
        for k in range(3):
            raw += struct.pack('<fII', k*10, mode, 3)
            for v in range(3): raw += struct.pack('<15f', *[rng.uniform(-4, 4) for _ in range(15)])
            raw += struct.pack('<16f', *[1 if i%5 == 0 else 0 for i in range(16)])
        source = (C.c_ubyte*len(raw)).from_buffer_copy(raw); chunk = Chunk(b'MORP', 0, len(raw))
        vertices = (Vertex*3)(); submesh = Submesh(); submesh.id = 7; submesh.vertex_count = 3; submesh.vertices = vertices
        model = Model(); model.source = source; model.source_size = len(raw); model.chunks = C.pointer(chunk); model.chunk_count = 1; model.submeshes = C.pointer(submesh); model.submesh_count = 1
        run(C.pointer(model), f'synthetic-mode{mode}', 100); print('synthetic', mode, 'PASS', flush=True)
    for pack, names in [('bk3_01', ['h01_20.x','h01_21.x','h02_20.x','h03_20.x','h04_20.x','h05_20.x'])]:
        archive = Archive(args.data/(pack+'.pp'))
        for name in names:
            raw = archive.read(next(e for e in archive.entries if e.name == name)); ok, model, message = decode(lib, raw); assert ok == 1, message
            try: run(model, name, 48)
            finally: lib.bk_model_destroy(model)
            records.append(dict(pack=pack, name=name, sha256=hashlib.sha256(raw).hexdigest())); print(name, 'PASS', flush=True)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), tracks=track_count, samples=cases, vertex_comparisons=compared, atomic_rejections=rejects, max_normalized_error=worst, records=records, native_functions=['0x4316be','0x432642','0x431ec6','0x409520','0x42d92f','0x42d9bb','0x42da1d'], hooks=['0x445a8e/0x445ac1 mesh vertex lock/unlock only'], scope='Original plain/blended MORP vertex math, four interpolation modes, exact/between keys, integer loop, ordered/repeated/empty subsets and plain-time retention. Six real face morph models and synthetic modes; no facial asset retarget mapping, skinning, GPU draw or complete actor frame.')
    (ROOT/'local/original-morph-pose-oracle.json').write_text(json.dumps(report, indent=2)+'\n'); print('PASS', cases, 'samples;', compared, 'vertices; max error', worst)

if __name__ == '__main__': main()
