"""Run complete NPC scene query with original cone, occlusion and ground."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from original_collision_oracle import Native as CollisionNative, bind, Archive
from original_visibility_oracle import Vector, Segment, Sight
from model_binding import ROOT, library, decode

class State(C.Structure):
    _fields_ = [('position', Vector), ('behavior', C.c_int32), ('hidden', C.c_uint8), ('visible', C.c_uint8), ('surface_name', C.c_char*260)]
class Input(C.Structure):
    _fields_ = [('cone', Sight), ('sight', Segment), ('suppressed_actions', C.c_int32*6), ('excluded_surface', C.c_char_p)]
class Native(CollisionNative):
    actor = 0x3001000
    def step(self, state, inp, seconds):
        data = bytearray(0x900)
        data[0x318], data[0x328] = state.visible, state.hidden
        struct.pack_into('<3f', data, 0x29c, *state.position)
        struct.pack_into('<i', data, 0x850, state.behavior)
        data[0x5c0:0x5c0+len(state.surface_name)] = state.surface_name
        data[0x814:0x830] = bytes(inp.sight)
        struct.pack_into('<f', data, 0x810, inp.cone.facing)
        u = self.u; u.mem_write(self.actor, bytes(data))
        u.mem_write(0x729084, bytes(inp.cone.actor_position)); u.mem_write(0x71b7ac, bytes(inp.cone.player_position))
        u.mem_write(0x728df4, struct.pack('<i', inp.cone.actor_kind)); u.mem_write(0x71b520, struct.pack('<i', inp.cone.player_action)); u.mem_write(0x71b544, struct.pack('<i', inp.cone.short_range_action))
        for address, action in zip([0x71b550, 0x71b554, 0x71b558, 0x71b55c, 0x71b564, 0x71b568], inp.suppressed_actions):
            u.mem_write(address, struct.pack('<i', action))
        u.mem_write(0x733700, struct.pack('<f', seconds)); u.mem_write(0x725930, inp.excluded_surface+b'\0')
        self.call(0x4b3e6f, struct.pack('<IIIII', self.scene, self.actor+0x29c, self.actor+0x2a0, self.actor+0x2a4, self.actor), limit=40000000)
        result = bytes(u.mem_read(self.actor, len(data)))
        for offset, size in [(0x2a0, 4), (0x318, 1), (0x5c0, 260), (0x850, 4)]:
            data[offset:offset+size] = result[offset:offset+size]
        assert bytes(data) == result
        return State(Vector(*struct.unpack_from('<3f', result, 0x29c)), struct.unpack_from('<i', result, 0x850)[0], result[0x328], result[0x318], result[0x5c0:0x6c4].split(b'\0')[0])

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); ap.add_argument('data', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native, lib, rng = Native(exe), library(), random.Random(0x4b3e6f); bind(lib)
    lib.bk_npc_scene_step.argtypes = [C.POINTER(State), C.c_void_p, C.POINTER(Input), C.c_float, C.c_void_p]
    archive = Archive(args.data/'bk3_03.pp'); entries = {e.name.lower():e for e in archive.entries}; error = C.create_string_buffer(256)
    records = []; cases = visible = detected = surfaces = 0; worst = 0
    for file in sorted(args.data.glob('*.atr')):
        filename = file.with_suffix('.x').name
        ok, model, message = decode(lib, archive.read(entries[filename])); assert ok == 1, message
        atr = file.read_bytes(); world = (C.c_float*(model.contents.frame_count*16))()
        assert lib.bk_model_world_matrices(model, world, len(world), error)
        collision = lib.bk_collision_create(model, world, len(world), filename.upper().encode(), atr, len(atr), error); assert collision, error.value
        try:
            native.create(model.contents, world, filename.upper().encode(), atr)
            for case in range(48):
                mesh = lib.bk_collision_mesh(collision, case%lib.bk_collision_count(collision)).contents
                ti = rng.randrange(mesh.index_count//3)*3
                tri = [mesh.vertices[mesh.indices[ti+j]] for j in range(3)]
                position = Vector(*(sum(t[j] for t in tri)/3 for j in range(3)))
                position[1] += rng.choice([-16, -15, 0, 15, 16])
                state = State(position, rng.choice([0, 1, 2, 3, 4]), 1 if case%11 == 0 else 0, rng.choice([0, 1, 255]), b'previous_surface')
                player = Vector(position[0]+rng.choice([-20, 0, 20]), position[1]+rng.choice([-21, 0, 20]), position[2]+rng.choice([-100, -40, 0, 40, 100]))
                ray = Segment(Vector(position[0], position[1]+18, position[2]), Vector(player[0], player[1]+18, player[2]), C.c_float(math.dist(position, player)).value)
                cone = Sight(position, player, ray.distance, rng.choice([0, 60, 90, 180, 270, 360]), rng.randrange(5), rng.choice([0, 1, 18, 27]), 1)
                inp = Input(cone, ray, (C.c_int32*6)(18, 19, 20, 21, 23, 27), mesh.name if case%3 == 0 else b'')
                seconds = C.c_float(rng.choice([0, 1/60, .25, 1, 2])).value
                wanted = native.step(state, inp, seconds)
                assert lib.bk_npc_scene_step(C.byref(state), collision, C.byref(inp), seconds, error), (file.name, case, error.value)
                assert (state.behavior, state.hidden, state.visible, state.surface_name) == (wanted.behavior, wanted.hidden, wanted.visible, wanted.surface_name), (file.name, case, bytes(state).hex(), bytes(wanted).hex())
                for a, b in zip(state.position, wanted.position):
                    delta = abs(a-b)/max(1, abs(b)); worst = max(worst, delta)
                    assert delta < 3e-6, (file.name, case, list(state.position), list(wanted.position), delta)
                cases += 1; visible += state.visible == 1; detected += state.behavior == 4; surfaces += bool(state.surface_name)
            records.append(file.name)
        finally: lib.bk_collision_destroy(collision); lib.bk_model_destroy(model)
        print(file.name, '48 complete scene queries', flush=True)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=cases, visible_cases=visible, behavior4_cases=detected, named_surface_cases=surfaces, max_normalized_error=worst, scene_files=records, native_functions=['0x4b3e6f and complete geometry callees', '0x4b2330 static construction'], hooks=['allocation/reallocation and frame lookup for construction only; query has no hooks'], scope='Complete NPC visibility/occlusion, suppression, ground mesh order and height smoothing on 29 real scenes; supplied cached head line/facing. Dynamic props/player sliding/full actor loop not implied.', x87_control_word='0x037f')
    (ROOT/'local/original-npc-scene-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', cases, 'complete scene queries; max error', worst)
if __name__ == '__main__': main()
