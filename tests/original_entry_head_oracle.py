"""Check real entry head/torso binding and native head stage across all clips."""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
from pathlib import Path
from original_actor_phase_oracle import bind as bind_actor, Archive
from original_entry_oracle import Request
from original_npc_head_oracle import Native, Input, State
from model_binding import ROOT, library, decode

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path); ap.add_argument('data', type=Path)
    args = ap.parse_args(); exe = args.exe.read_bytes()
    lib, native = library(), Native(exe); bind_actor(lib)
    for name, arguments, result in [
        ('bk_resources_create', [C.c_void_p], C.c_void_p),
        ('bk_resources_destroy', [C.c_void_p], None),
        ('bk_resources_mount', [C.c_void_p, C.c_char_p, C.c_char_p, C.c_void_p], C.c_int),
        ('bk_resources_mount_directory', [C.c_void_p, C.c_char_p, C.c_char_p, C.c_size_t, C.c_void_p], C.c_int),
        ('bk_entry_assets_create', [C.c_void_p, C.POINTER(Request), C.c_void_p], C.c_void_p),
        ('bk_entry_assets_destroy', [C.c_void_p], None),
        ('bk_entry_assets_actor', [C.c_void_p], C.c_void_p),
        ('bk_entry_assets_head_input', [C.c_void_p, C.c_float, C.POINTER(Input), C.c_void_p], C.c_int),
        ('bk_npc_head_update', [C.POINTER(State), C.POINTER(Input), C.c_void_p], C.c_int)]:
        fn = getattr(lib, name); fn.argtypes = arguments; fn.restype = result
    error = C.create_string_buffer(256); store = lib.bk_resources_create(error); assert store
    archive = Archive(args.data/'bk3_01.pp'); entries = {e.name:e for e in archive.entries}
    cases = profiles = rejects = 0; worst = 0; records = []
    def native_name(address):
        ptr = struct.unpack('<I', native.u.mem_read(address, 4))[0]
        return bytes(native.u.mem_read(ptr, 256)).split(b'\0')[0]
    try:
        for pack in ['bk3_01', 'bk3_04']:
            assert lib.bk_resources_mount(store, pack.encode(), str(args.data/(pack+'.pp')).encode(), error), error.value
        assert lib.bk_resources_mount_directory(store, b'routes', str(args.data).encode(), 20480, error), error.value
        assert lib.bk_resources_mount_directory(store, b'faces', str(args.data).encode(), 20480, error), error.value
        for group in range(5):
            filename = f'h0{group+1}_80'; raw = archive.read(entries[filename+'.x'])
            ok, model, message = decode(lib, raw); assert ok == 1, message
            xan = archive.read(entries[filename+'.xan'])
            slots = [i for i in range(128) if struct.unpack_from('<2f', xan, 512+0x190+i*156+0x54) != (0,0)]
            head_name = native_name(0x5848d0+group*4)
            torso_name = native_name(0x5848e0+group*4) if group in [1,2] else None
            head, torso = C.c_uint32(), C.c_uint32()
            assert lib.bk_model_find_frame(model, head_name, C.byref(head), error)
            if torso_name: assert lib.bk_model_find_frame(model, torso_name, C.byref(torso), error)
            try:
                for area in range(9):
                    assets = lib.bk_entry_assets_create(store, C.byref(Request(group, area, 0, 8)), error); assert assets, error.value
                    try:
                        actor = lib.bk_entry_assets_actor(assets)
                        placement = lib.bk_actor_pose_placement(actor).contents
                        origin = (C.c_float*3)(*placement.position)
                        yaw = placement.yaw_degrees
                        for i, slot in enumerate(slots):
                            for phase, seconds in enumerate([0, .1, 1, 3]):
                                cached = tuple(lib.bk_actor_pose_frame(actor, head.value)[:16])
                                assert lib.bk_actor_pose_step(actor, origin, yaw, slot, seconds, error), (filename, slot, error.value)
                                inp = Input()
                                assert lib.bk_entry_assets_head_input(assets, yaw, C.byref(inp), error), error.value
                                assert tuple(inp.actor_head_world) == cached
                                assert list(inp.actor_head_local) == lib.bk_actor_pose_local(actor, head.value)[:16]
                                if torso_name: assert list(inp.torso_local) == lib.bk_actor_pose_local(actor, torso.value)[:16]
                                assert inp.actor_kind == group
                                wanted, actual = native.run(inp), State()
                                assert lib.bk_npc_head_update(C.byref(actual), C.byref(inp), error), (filename, slot, phase, error.value, inp.actor_head_local[1], inp.torso_local[1])
                                for a, b in zip([*actual.sight.start,*actual.sight.end,actual.sight.distance,actual.bearing,actual.facing], [*wanted.sight.start,*wanted.sight.end,wanted.sight.distance,wanted.bearing,wanted.facing]):
                                    delta = abs(a-b)/max(1,abs(b)); worst = max(worst,delta)
                                    assert math.isfinite(delta) and delta < 3e-6, (filename, slot, phase, a, b)
                                cases += 1
                                if phase != 1: lib.bk_actor_pose_publish(actor)
                        held = bytes(inp)
                        assert not lib.bk_entry_assets_head_input(assets, float('nan'), C.byref(inp), error)
                        assert bytes(inp) == held; rejects += 1; profiles += 1
                    finally: lib.bk_entry_assets_destroy(assets)
                records.append(dict(file=filename, model_sha256=hashlib.sha256(raw).hexdigest(), head=head_name.decode(), torso=torso_name.decode() if torso_name else None, clips=len(slots)))
                print(filename, len(slots), 'clips x9 entries PASS', flush=True)
            finally: lib.bk_model_destroy(model)
    finally: lib.bk_resources_destroy(store)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=cases, entry_profiles=profiles, rejections=rejects, max_normalized_error=worst, records=records, native_functions=['0x4fca12..0x4fcc57'], native_binding_tables=['0x5848d0+group*4 head', '0x5848e0+group*4 torso for groups1/2'], scope='All real NPC clips and45entry profiles: named head/torso lookup, live local matrices and held world cache followed by native head update. Explicit animation requests; no action dispatch, rendering or gameplay loop implied.')
    (ROOT/'local/original-entry-head-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', cases, 'real asset head updates; max error', worst)

if __name__ == '__main__': main()
