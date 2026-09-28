"""Native MORP disk loader vs portable owned tracks, key vertices and matrices."""
import argparse
import ctypes as C
import hashlib
import json
import struct
import sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, Vertex, Model, library, decode
sys.path.insert(0, str(ROOT/'tools'))
from bk3_assets import Archive

class Key(C.Structure):
    _fields_ = [('time', C.c_float), ('interpolation', C.c_uint32), ('vertex_count', C.c_uint32), ('vertices', C.POINTER(Vertex)), ('matrix', C.c_float*16)]
class Track(C.Structure):
    _fields_ = [('submesh', C.c_uint32), ('key_count', C.c_uint32)]
def bind(lib):
    for name, args, result in [
        ('bk_model_morph_create', [C.POINTER(Model), C.c_void_p], C.c_void_p),
        ('bk_model_morph_destroy', [C.c_void_p], None),
        ('bk_model_morph_count', [C.c_void_p], C.c_uint32),
        ('bk_model_morph_track', [C.c_void_p, C.c_uint32], C.POINTER(Track)),
        ('bk_model_morph_key', [C.c_void_p, C.c_uint32, C.c_uint32], C.POINTER(Key))]:
        fn = getattr(lib, name); fn.argtypes = args; fn.restype = result

class Native:
    stack, data, group = 0x2008000, 0x4000000, 0x7200000
    def __init__(self, exe):
        self.u = machine(exe); self.u.mem_map(self.data, 0x2000000); self.u.mem_map(0x7000000, 0x400000)
        for ptr in [0x4338b0, 0x430e00, 0x415ed8, 0x431316, 0x4339c5, 0x42a0cd]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=ptr, end=ptr)
    def word(self, ptr, value): self.u.mem_write(ptr, struct.pack('<I', value))
    def read(self, ptr): return struct.unpack('<I', self.u.mem_read(ptr, 4))[0]
    def hook(self, u, address, size, user):
        sp = u.reg_read(UC_X86_REG_ESP); ret = self.read(sp); args = [self.read(sp+4+i*4) for i in range(3)]; result = 0
        if address == 0x4338b0:
            self.word(args[0], self.group)
        elif address == 0x430e00:
            obj = 0x7000000 + len(self.tracks)*0x100
            self.u.mem_write(obj, bytes(0x100)); self.word(args[0], obj)
            self.tracks.append(dict(obj=obj, keys=[]))
        elif address == 0x415ed8:
            result = self.ids[args[0]]
        elif address == 0x431316:
            track = self.tracks[(args[0]-0x7000000)//0x100]
            time, interpolation, count, vertices = struct.unpack('<fIII', u.mem_read(args[2], 16))
            assert args[1] == self.read(args[2])
            track['keys'].append((time, interpolation, count, bytes(u.mem_read(vertices, count*60)), bytes(u.mem_read(args[2]+0x1c, 64))))
            track['mesh'] = (self.read(args[0]+0x70)-0x7100000)//0x100
            assert self.read(args[0]+0x88) == 1 # actual native loop setter
        elif address == 0x4339c5:
            assert args[0] == self.group and args[1] == self.tracks[-1]['obj']
            self.attachments += 1
        u.reg_write(UC_X86_REG_EAX, result); u.reg_write(UC_X86_REG_ESP, sp+4); u.reg_write(UC_X86_REG_EIP, ret)
    def load(self, raw, model, chunk):
        payload = raw[chunk.offset:chunk.offset+chunk.size]; assert len(payload) < 0x2000000
        self.u.mem_write(self.data, payload); self.tracks = []; self.attachments = 0
        self.ids = {model.submeshes[i].id: 0x7100000+i*0x100 for i in range(model.submesh_count)}
        self.word(self.stack-0x164, self.data); self.word(self.stack-0x194, 0x3000000)
        self.u.reg_write(UC_X86_REG_EBP, self.stack); self.u.reg_write(UC_X86_REG_ESP, self.stack-0x2000)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(0x41ac62, 0x41ae64, count=10000000)
        assert self.u.reg_read(UC_X86_REG_EIP) == 0x41ae64
        assert self.read(self.stack-0x164) == self.data+len(payload)
        assert self.read(0x3000000) == self.group and self.attachments == len(self.tracks)

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); ap.add_argument('data', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native, lib = Native(exe), library(); bind(lib)
    error = C.create_string_buffer(256); records = []; tracks = keys = vertices = 0
    for path in sorted(args.data.glob('*.pp')):
        archive = Archive(path)
        for entry in archive.entries:
            if not entry.name.lower().endswith('.x'): continue
            raw = archive.read(entry)
            if not raw.startswith(b'OBJM'): continue
            pos = 12; found = False
            while pos < len(raw):
                if raw[pos:pos+4] == b'MORP': found = True; break
                pos += 8 + struct.unpack_from('<I', raw, pos+4)[0]
            if not found: continue
            ok, model, message = decode(lib, raw); assert ok == 1, message
            morph = None
            try:
                chunk = next(model.contents.chunks[i] for i in range(model.contents.chunk_count) if model.contents.chunks[i].tag == b'MORP')
                native.load(raw, model.contents, chunk)
                morph = lib.bk_model_morph_create(model, error); assert morph, (path.name, entry.name, error.value)
                count = lib.bk_model_morph_count(morph); assert count == len(native.tracks); tracks += count
                for t in range(count):
                    track = lib.bk_model_morph_track(morph, t).contents; expected = native.tracks[t]
                    assert track.submesh == expected['mesh'] and track.key_count == len(expected['keys'])
                    for k, wanted in enumerate(expected['keys']):
                        key = lib.bk_model_morph_key(morph, t, k).contents
                        assert (key.time, key.interpolation, key.vertex_count) == wanted[:3]
                        assert C.string_at(key.vertices, key.vertex_count*60) == wanted[3]
                        assert bytes(key.matrix) == wanted[4]
                        keys += 1; vertices += key.vertex_count
                # Decoder owns all returned key data independently of model.
                first = lib.bk_model_morph_key(morph, 0, 0).contents
                saved = C.string_at(first.vertices, first.vertex_count*60)
                lib.bk_model_destroy(model); model = None
                assert C.string_at(lib.bk_model_morph_key(morph, 0, 0).contents.vertices, len(saved)) == saved
                records.append(dict(pack=path.name, name=entry.name, tracks=count, sha256=hashlib.sha256(raw).hexdigest()))
                print(path.name, entry.name, count, 'tracks PASS', flush=True)
            finally:
                lib.bk_model_morph_destroy(morph)
                if model: lib.bk_model_destroy(model)
    assert len(records) == 84
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), models=len(records), tracks=tracks, keys=keys, key_vertices=vertices, records=records, native_functions=['0x41ac62..0x41ae64', '0x43165d', 'native64-byte matrix copy'], hooks=['group/track allocation and attachment', 'exported mesh ID lookup', '0x431316 key-storage boundary', 'reference release'], scope='All actual MORP disk loader outputs, complete vertex bytes and key matrices vs C-owned decoding. Native chunk walk/layout not replaced. Does not execute native key storage normalization, interpolation, subset selection, retargeting, skinning or rendering.')
    (ROOT/'local/original-morph-decode-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', len(records), 'models;', tracks, 'tracks;', keys, 'keys;', vertices, 'key vertices')

if __name__ == '__main__': main()
