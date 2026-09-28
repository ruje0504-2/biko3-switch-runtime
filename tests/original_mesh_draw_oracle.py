"""Capture original D3D7 list/strip/fan dispatch and lighting initialization.

Native DrawPrimitiveVB receives sequential vertex count, not index payload.
This compares submission and CPU expansion, not the original GPU rasterizer.
"""
import argparse
import ctypes as C
import hashlib
import json
import struct
import sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_lighting_oracle import Oracle as LightsOracle
from model_binding import ROOT, Model, library, decode
from test_model import fixture
sys.path.insert(0, str(ROOT / 'tools'))
from bk3_assets import Archive

class Triangles(C.Structure):
    _fields_ = [('indices', C.POINTER(C.c_uint16)), ('count', C.c_uint32)]

class Oracle(LightsOracle):
    mesh, vb, indices = 0x3007000, 0x3007400, 0x3007800
    primitive, indexed = 0x3003030, 0x3003040
    def __init__(self, exe):
        super().__init__(exe)
        self.write(self.table + 0x7c, self.primitive)
        self.write(self.table + 0x80, self.indexed)
        for address in (self.primitive, self.indexed):
            self.uc.hook_add(UC_HOOK_CODE, self.draw, begin=address, end=address)
    def draw(self, uc, address, size, user):
        count = 8 if address == self.indexed else 6
        sp = uc.reg_read(UC_X86_REG_ESP)
        self.result = (address, struct.unpack('<' + 'I' * count, uc.mem_read(sp + 4, count * 4)))
        uc.emu_stop()
    def dispatch(self, topology, vertices, indices):
        for offset, value in [(0x78, self.vb), (0x84, vertices), (0x8c, self.indices),
                              (0x90, indices), (0xd8, topology)]:
            self.write(self.mesh + offset, value)
        bp = 0x2008000
        self.write(bp + 8, self.mesh)
        self.write(0x5444e0, 7)
        self.uc.reg_write(UC_X86_REG_EBP, bp)
        self.uc.reg_write(UC_X86_REG_ESP, bp - 0x1000)
        self.result = None
        self.uc.emu_start(0x445071, 0x4451b3, count=1000)
        assert self.result is not None
        fn, args = self.result
        want = (self.device, 4, self.vb, 0, vertices, self.indices, indices, 7) if topology == 0 else (
            self.device, 5 if topology == 1 else 6, self.vb, 0, vertices, 7)
        assert args == want, (topology, args, want)
        assert fn == (self.indexed if topology == 0 else self.primitive)
    def initialization(self):
        self.states = {}
        self.uc.reg_write(UC_X86_REG_EBP, 0x2008000)
        self.uc.reg_write(UC_X86_REG_ESP, 0x2007000)
        self.uc.emu_start(0x42967f, 0x4296da, count=1000)
        assert self.uc.reg_read(UC_X86_REG_EIP) == 0x4296da
        assert self.states == {141: 1, 29: 1, 143: 1}, self.states

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    sha = hashlib.sha256(exe).hexdigest()
    assert sha == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    oracle = Oracle(exe)
    oracle.initialization()
    lib = library()
    lib.bk_model_triangles.argtypes = [C.POINTER(Model), C.c_uint32, C.POINTER(Triangles), C.c_void_p]
    lib.bk_model_triangles.restype = C.c_int
    lib.bk_model_triangles_free.argtypes = [C.POINTER(Triangles)]
    error = C.create_string_buffer(256)
    out = Triangles()
    checks = expanded = 0
    def compare(model, index):
        nonlocal checks, expanded
        m = model.contents
        sub = m.submeshes[index]
        topology = struct.unpack('<I', C.string_at(C.addressof(m.source.contents) + sub.source_header_offset + 56, 4))[0]
        oracle.dispatch(topology, sub.vertex_count, sub.index_count)
        assert lib.bk_model_triangles(model, index, C.byref(out), error), error.value
        try:
            got = list(out.indices[:out.count])
            if topology == 0:
                assert got == list(sub.indices[:sub.index_count])
            elif topology == 1:
                want = []
                for k in range(2, sub.vertex_count):
                    want.extend((k - 1, k - 2, k) if k % 2 else (k - 2, k - 1, k))
                assert got == want
            else:
                assert got == [j for k in range(2, sub.vertex_count) for j in (0, k - 1, k)]
            checks += 1
            expanded += out.count // 3
        finally:
            lib.bk_model_triangles_free(C.byref(out))
    ok, model, message = decode(lib, fixture())
    assert ok == 1, message
    try:
        for topology in range(3):
            model.contents.source[model.contents.submeshes[0].source_header_offset + 56] = topology
            for count in ([3] if topology == 0 else [3, 4, 5, 6, 17, 65535]):
                model.contents.submeshes[0].vertex_count = count
                compare(model, 0)
    finally:
        lib.bk_model_destroy(model)
    archive = Archive(args.data / 'bk3_01.pp')
    records = []
    for asset in archive.entries:
        if not asset.name.endswith(('_80.x', '_60.x', '_61.x', '_70.x')):
            continue
        ok, model, message = decode(lib, archive.read(asset))
        assert ok == 1, message
        try:
            for i in range(model.contents.submesh_count):
                compare(model, i)
            records.append(asset.name)
        finally:
            lib.bk_model_destroy(model)
    result = dict(passed=True, exe_sha256=sha, original_models=records,
                  dispatch_checks=checks, expanded_triangles=expanded,
                  local_viewer=True, normalize_normals=True, initial_specular_enabled=True,
                  native_regions=['0x445071..0x4451b3', '0x42967f..0x4296da'],
                  scope='captured D3D7 submission + CPU triangle expansion, not native rasterization')
    (ROOT / 'local/original-mesh-draw-oracle.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
if __name__ == '__main__':
    main()
