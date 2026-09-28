"""Replay ATR collision construction and ground queries over real scenes."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
import sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from model_binding import ROOT, library, decode, Model
from original_visibility_oracle import Vector, Triangle, Native as SightNative
sys.path.insert(0, str(ROOT/'tools'))
from bk3_assets import Archive

class Mesh(C.Structure):
    _fields_ = [('name', C.c_char*260), ('vertex_count', C.c_uint32), ('index_count', C.c_uint32), ('vertices', C.POINTER(Vector)), ('indices', C.POINTER(C.c_uint32)), ('normals', C.POINTER(Vector))]

def bind(lib):
    lib.bk_collision_create.argtypes = [C.POINTER(Model), C.POINTER(C.c_float), C.c_size_t, C.c_char_p, C.c_void_p, C.c_size_t, C.c_void_p]
    lib.bk_collision_create.restype = C.c_void_p
    lib.bk_collision_destroy.argtypes = [C.c_void_p]
    lib.bk_collision_count.argtypes = [C.c_void_p]
    lib.bk_collision_count.restype = C.c_uint32
    lib.bk_collision_mesh.argtypes = [C.c_void_p, C.c_uint32]
    lib.bk_collision_mesh.restype = C.POINTER(Mesh)
    lib.bk_collision_ground.argtypes = [C.POINTER(Mesh), C.POINTER(C.c_float), C.POINTER(C.c_int), C.POINTER(C.c_float), C.c_void_p]
    lib.bk_collision_normal.argtypes = [C.POINTER(C.c_float), C.POINTER(Vector)]

class Native(SightNative):
    scene, atr, model, group = 0x3000000, 0x4000000, 0x3000200, 0x3000500
    def __init__(self, exe):
        super().__init__(exe)
        self.u.mem_map(0x4000000, 0x3000000)
        for addr in [0x46d9b4, 0x46d9df, 0x425904]:
            self.u.hook_add(UC_HOOK_CODE, self.boundary, begin=addr, end=addr)
    def alloc(self, size):
        address = self.cursor
        self.cursor += (size+15)&~15
        assert self.cursor < 0x7000000
        self.allocations[address] = size
        self.u.mem_write(address, bytes(size))
        return address
    def string(self, ptr): return bytes(self.u.mem_read(ptr, 256)).split(b'\0')[0]
    def child_name(self, parent, ordinal, filename):
        # Execute actual grouped loader naming; raw child names have padding.
        base, source = 0x2006000, 0x3000a00
        u = self.u
        u.mem_write(source, parent+b'\0'); u.mem_write(0x5d2604, filename+b'\0')
        u.mem_write(base-0x1ac, struct.pack('<I', source))
        u.mem_write(base-0x1b0, struct.pack('<I', ordinal))
        u.reg_write(UC_X86_REG_EBP, base); u.reg_write(UC_X86_REG_ESP, base-0x1000)
        u.emu_start(0x418b90, 0x418bd7, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x418bd7
        return self.string(base-0x114)
    def boundary(self, u, address, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, arg = struct.unpack('<II', u.mem_read(sp, 8))
        if address == 0x46d9b4: result = self.alloc(arg)
        elif address == 0x46d9df:
            size = struct.unpack('<I', u.mem_read(sp+8, 4))[0]
            result = self.alloc(size)
            u.mem_write(result, bytes(u.mem_read(arg, min(size, self.allocations[arg]))))
        else:
            name, output = struct.unpack('<II', u.mem_read(sp+8, 8))
            key = self.string(name)
            assert key in self.frames, key
            result = self.frames[key]
            u.mem_write(output, struct.pack('<I', result))
            self.lookup_calls += 1
        u.reg_write(UC_X86_REG_ESP, sp+4)
        u.reg_write(UC_X86_REG_EIP, ret)
        u.reg_write(UC_X86_REG_EAX, result)
    def create(self, model, world, filename, atr):
        self.cursor, self.allocations, self.frames, self.lookup_calls = 0x4500000, {}, {}, 0
        u = self.u
        u.mem_write(self.atr, atr)
        u.mem_write(self.scene, bytes(12))
        u.mem_write(self.model+0x160, struct.pack('<I', self.group))
        u.mem_write(self.group+0x14, struct.pack('<I', 0x1234))
        n = struct.unpack_from('<I', atr, 0x418200)[0]
        for i in range(n):
            name = atr[i*0x8304:i*0x8304+256].split(b'\0')[0]
            matches = [j for j in range(model.frame_count) if bytes(model.frames[j].name).split(b' ', 1)[-1] == name]
            assert len(matches) == 1
            fi = matches[0]; f = model.frames[fi]
            frame = self.alloc(0x300)
            self.frames[name] = frame
            u.mem_write(frame+0xf0, struct.pack('<3f', *world[fi*16+12:fi*16+15]))
            assert f.mesh_index != 0xffffffff
            mesh = model.meshes[f.mesh_index]
            children = []
            for si in range(mesh.first_submesh, mesh.first_submesh+mesh.submesh_count):
                s = model.submeshes[si]
                child = self.alloc(0x200)
                u.mem_write(child+4, struct.pack('<I', 0x3ea))
                runtime_name = bytes(mesh.name)+b'@'+filename if mesh.submesh_count == 1 else self.child_name(bytes(mesh.name), si-mesh.first_submesh, filename)
                u.mem_write(child+8, runtime_name+b'\0')
                vertices = self.alloc(s.vertex_count*60)
                indices = self.alloc(s.index_count*2)
                u.mem_write(vertices, C.string_at(s.vertices, s.vertex_count*60))
                u.mem_write(indices, C.string_at(s.indices, s.index_count*2))
                for offset, value in [(0x7c, vertices), (0x84, s.vertex_count), (0x8c, indices), (0x90, s.index_count)]:
                    u.mem_write(child+offset, struct.pack('<I', value))
                children.append(child)
            if len(children) == 1: obj = children[0]
            else:
                obj = self.alloc(0x100); pointers = self.alloc(len(children)*4)
                u.mem_write(pointers, struct.pack('<'+'I'*len(children), *children))
                u.mem_write(obj+4, struct.pack('<I', 0x3f5))
                u.mem_write(obj+0x70, struct.pack('<II', len(children), pointers))
            u.mem_write(frame+0x244, struct.pack('<I', obj))
        u.mem_write(self.stack, struct.pack('<IIII', self.stop, self.scene, self.atr, self.model))
        u.reg_write(UC_X86_REG_ESP, self.stack); u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(0x4b2330, self.stop, count=20000000)
        assert u.reg_read(UC_X86_REG_EIP) == self.stop
        self.fixed_count, self.count, self.meshes = struct.unpack('<III', u.mem_read(self.scene, 12))
        assert self.fixed_count == self.count and self.lookup_calls == n
    def mesh_data(self, index):
        address = self.meshes+index*0x11c
        data = bytes(self.u.mem_read(address, 0x11c))
        nv, ni, pv, pi, pn = struct.unpack_from('<IIIII', data, 0x104)
        vertices = [struct.unpack('<3f', self.u.mem_read(pv+j*60, 12)) for j in range(nv)]
        return dict(name=data[:260].split(b'\0')[0], vertices=vertices, indices=struct.unpack('<'+'I'*ni, self.u.mem_read(pi, ni*4)), normals=struct.unpack('<'+'f'*ni, self.u.mem_read(pn, ni*4)))
    def ground(self, index, point):
        pos = 0x3000800
        self.u.mem_write(pos, bytes(point))
        hit = self.call(0x4b2f8e, struct.pack('<IIII', self.meshes+index*0x11c, pos, pos+4, pos+8), limit=20000000)
        return hit, struct.unpack('<f', self.u.mem_read(pos+4, 4))[0]

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); ap.add_argument('data', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native, lib, rng = Native(exe), library(), random.Random(0x4b2330); bind(lib)
    archive = Archive(args.data/'bk3_03.pp'); entries = {e.name.lower(): e for e in archive.entries}
    records = []; total = dict(meshes=0, vertices=0, normals=0, ground_queries=0, ground_hits=0); worst = 0
    error = C.create_string_buffer(256)
    for file in sorted(args.data.glob('*.atr')):
        filename = file.with_suffix('.x').name
        if filename.lower() not in entries: raise ValueError(filename)
        data = archive.read(entries[filename.lower()]); ok, model, message = decode(lib, data); assert ok == 1, message
        atr = file.read_bytes(); world = (C.c_float*(model.contents.frame_count*16))()
        assert lib.bk_model_world_matrices(model, world, len(world), error), error.value
        collision = lib.bk_collision_create(model, world, len(world), filename.upper().encode(), atr, len(atr), error)
        assert collision, (file.name, error.value)
        try:
            native.create(model.contents, world, filename.upper().encode(), atr)
            count = lib.bk_collision_count(collision); assert count == native.count
            for i in range(count):
                ptr = lib.bk_collision_mesh(collision, i); mesh = ptr.contents; expected = native.mesh_data(i)
                assert mesh.name == expected['name'], (file.name, i, mesh.name, expected['name'])
                assert list(map(tuple, mesh.vertices[:mesh.vertex_count])) == expected['vertices']
                assert tuple(mesh.indices[:mesh.index_count]) == expected['indices']
                actual = [v for row in mesh.normals[:mesh.index_count//3] for v in row]
                for a, b in zip(actual, expected['normals']):
                    delta = abs(a-b)/max(1, abs(b)); worst = max(worst, delta)
                    assert delta < 3e-6, (file.name, i, a, b)
                total['meshes'] += 1; total['vertices'] += mesh.vertex_count; total['normals'] += mesh.index_count//3
                for case in range(32):
                    ti = rng.randrange(mesh.index_count//3)*3
                    triangle = [mesh.vertices[mesh.indices[ti+j]] for j in range(3)]
                    point = Vector(*(sum(t[j] for t in triangle)/3 for j in range(3)))
                    point[1] += rng.choice([-16, -15, 0, 15, 16])
                    wh, wy = native.ground(i, point); hit, height = C.c_int(99), C.c_float(point[1])
                    assert lib.bk_collision_ground(ptr, point, C.byref(hit), C.byref(height), error), (file.name, i, case, error.value)
                    assert hit.value == wh and abs(height.value-wy) <= 3e-6*max(1, abs(wy)), (file.name, i, case, hit.value, wh, height.value, wy)
                    total['ground_queries'] += 1; total['ground_hits'] += hit.value
            records.append(dict(file=file.name, atr_sha256=hashlib.sha256(atr).hexdigest(), model_sha256=hashlib.sha256(data).hexdigest(), meshes=count))
        finally:
            lib.bk_collision_destroy(collision); lib.bk_model_destroy(model)
        print(file.name, count, 'meshes', flush=True)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), **total, max_normal_normalized_error=worst, files=records, native_functions=['0x4b2330', '0x4b1b02', '0x4ae586', '0x522922', '0x4b2f8e', '0x4b43be'], hooks=['allocation/reallocation', 'frame lookup boundary supplied from decoded frame mapping'], x87_control_word='0x037f', scope='Full native ATR selection/static collision copying and normals, then ground queries across all 29 ATR scene assets. Decoded model/object binding supplied; static world translations supplied from CPU composition. Dynamic props and gameplay integration excluded.')
    (ROOT/'local/original-collision-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', total, 'normal max error', worst)
if __name__ == '__main__': main()
