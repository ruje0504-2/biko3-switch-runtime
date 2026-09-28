"""Original 4b39d9, both complete wall passes and ground, on all real ATR scenes."""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import sys
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
from model_binding import ROOT, library, decode
from original_collision_oracle import Native as CollisionNative, bind
from original_player_wall_oracle import Wall, Input as WallInput, Vec
sys.path.insert(0, str(ROOT/'tools'))
from bk3_assets import Archive

class State(C.Structure):
    _fields_ = [('wall', Wall), ('wall_heading', C.c_float),
                ('wall_name', C.c_char*260), ('surface_name', C.c_char*260),
                ('npc_in_view', C.c_int32), ('any_wall', C.c_int32)]
class Input(C.Structure):
    _fields_ = [('wall', WallInput), ('head_distance', C.c_float),
                ('projected_depth', C.c_float), ('screen_scale', C.c_float*2),
                ('screen_position', C.c_int32*2), ('npc_hidden', C.c_int32),
                ('excluded_surface', C.c_char_p)]

class Native(CollisionNative):
    player = 0x71b510
    def __init__(self, exe):
        super().__init__(exe)
        self.projection_calls = 0
        # Execute an actual FLD/RET stub so x87 stack semantics remain native.
        self.u.mem_write(0x300e000, b'\xd9\x05'+struct.pack('<I', 0x300e010)+b'\xc3')
        self.u.hook_add(UC_HOOK_CODE, self.project, begin=0x42d56c, end=0x42d56c)
    def project(self, u, address, size, data):
        sp = u.reg_read(UC_X86_REG_ESP)
        output = struct.unpack('<I', u.mem_read(sp+8, 4))[0]
        u.mem_write(output, bytes(self.input.screen_position))
        u.mem_write(0x300e010, struct.pack('<f', self.input.projected_depth))
        u.reg_write(UC_X86_REG_EIP, 0x300e000)
        self.projection_calls += 1
    def prepare(self, state, inp):
        self.input = inp
        u, p = self.u, self.player
        u.mem_write(p, bytes(0x900))
        u.mem_write(self.stack - 8192, bytes(8192))
        u.mem_write(p+0x29c, bytes(state.wall.position))
        u.mem_write(p+0x2a8, struct.pack('<ff', inp.wall.height, 23))
        u.mem_write(p+0x2c8, bytes(inp.wall.previous))
        u.mem_write(p+0x2b4, struct.pack('<4f', inp.wall.motion[0], 0, inp.wall.motion[1], inp.wall.motion[2]))
        u.mem_write(p+0x318, bytes([state.npc_in_view, 0, state.wall.near_wall]))
        u.mem_write(p+0x31c, bytes(state.wall.normal))
        u.mem_write(p+0x7d4, struct.pack('<f', state.wall_heading))
        u.mem_write(p+0x5c0, state.surface_name+b'\0')
        u.mem_write(p+0x6c4, state.wall_name+b'\0')
        u.mem_write(0x709898, bytes([state.any_wall]))
        u.mem_write(0x71b45c, bytes(inp.wall.camera))
        u.mem_write(0x71b374, struct.pack('<f', state.wall.camera_distance))
        u.mem_write(0x71b4dc, bytes(state.wall.rays_blocked))
        for j, base in enumerate([0x71b474, 0x71b494, 0x71b4b4]):
            u.mem_write(base, struct.pack('<7f', *(r[j] for r in inp.wall.rays)))
        u.mem_write(0x728df0, struct.pack('<I', 0x300b000))
        u.mem_write(0x729110, bytes([inp.npc_hidden]))
        u.mem_write(0x729614, struct.pack('<f', inp.head_distance))
        u.mem_write(0xbeecf4, bytes(inp.screen_scale))
        u.mem_write(0x725930, inp.excluded_surface+b'\0')
        # Unused mesh return still executes its genuine native visibility code.
        u.mem_write(0x7295fc, bytes(inp.wall.previous)+bytes(inp.wall.camera)+struct.pack('<f', inp.head_distance))
    def read(self, state):
        u, p = self.u, self.player
        result = State.from_buffer_copy(bytes(state))
        result.wall.position[:] = struct.unpack('<3f', u.mem_read(p+0x29c, 12))
        result.wall.camera_distance = struct.unpack('<f', u.mem_read(0x71b374, 4))[0]
        result.wall.normal[:] = struct.unpack('<3f', u.mem_read(p+0x31c, 12))
        result.wall.rays_blocked[:] = struct.unpack('<7i', u.mem_read(0x71b4dc, 28))
        result.wall.near_wall = u.mem_read(p+0x31a, 1)[0]
        result.wall_heading = struct.unpack('<f', u.mem_read(p+0x7d4, 4))[0]
        result.wall_name = bytes(u.mem_read(p+0x6c4, 260)).split(b'\0')[0]
        result.surface_name = bytes(u.mem_read(p+0x5c0, 260)).split(b'\0')[0]
        result.npc_in_view = u.mem_read(p+0x318, 1)[0]
        result.any_wall = u.mem_read(0x709898, 1)[0]
        return result
    def step(self, state, inp):
        self.prepare(state, inp)
        self.call(0x4b39d9, struct.pack('<II', self.scene, self.player), limit=30000000)
        return self.read(state)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('data', type=Path)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    native, lib, rng = Native(exe), library(), random.Random(0x4b39d9)
    bind(lib)
    lib.bk_player_scene_step.argtypes = [C.POINTER(State), C.c_void_p, C.POINTER(Input), C.c_void_p]
    archive = Archive(args.data/'bk3_03.pp')
    entries = {e.name.lower(): e for e in archive.entries}
    error = C.create_string_buffer(256)
    worst, example = 0.0, None
    records = []
    def compare(a, w, key):
        nonlocal worst, example
        delta = abs(a-w)/max(1, abs(w))
        if delta > worst:
            worst, example = delta, [key, a, w]
        assert math.isfinite(a) and math.isfinite(w) and delta < 4e-5, (key, a, w, delta)
    for file in sorted(args.data.glob('*.atr')):
        filename = file.with_suffix('.x').name
        data = archive.read(entries[filename.lower()])
        ok, model, message = decode(lib, data)
        assert ok, message
        atr = file.read_bytes()
        world = (C.c_float*(model.contents.frame_count*16))()
        assert lib.bk_model_world_matrices(model, world, len(world), error), error.value
        collision = lib.bk_collision_create(model, world, len(world), filename.upper().encode(), atr, len(atr), error)
        assert collision, error.value
        counts = dict(cases=0, compared=0, missing_projection=0, singular_camera=0, wall_hits=0, floor_hits=0)
        try:
            native.create(model.contents, world, filename.upper().encode(), atr)
            meshes = [lib.bk_collision_mesh(collision, i).contents for i in range(lib.bk_collision_count(collision))]
            samples = []
            for mesh in meshes:
                for i in range(0, mesh.index_count, 3):
                    triangle = [mesh.vertices[mesh.indices[i+j]] for j in range(3)]
                    samples.append(Vec(*(sum(v[j] for v in triangle)/3 for j in range(3))))
            for case in range(48):
                point = Vec(*rng.choice(samples))
                point[0] += rng.uniform(-4, 4)
                point[1] += rng.choice([-14, 0, 1, 14])
                point[2] += rng.uniform(-4, 4)
                previous = Vec(point[0]+rng.uniform(-5, 5), point[1], point[2]+rng.uniform(-5, 5))
                camera = Vec(point[0]+rng.uniform(-50, 50), point[1]+5, point[2]+rng.uniform(-50, 50))
                state = State(wall=Wall(position=point, camera_distance=200,
                                       normal=Vec(.25, .5, .75), near_wall=case%2),
                              wall_heading=77, wall_name=b'old wall', surface_name=b'old floor')
                wi = WallInput(previous=previous, motion=Vec(*(rng.uniform(-1, 1) for _ in range(3))),
                               camera=camera, height=point[1]+18,
                               rays=(Vec*7)(*(Vec(camera[0]+i*.375, camera[1], camera[2]-i*.7) for i in range(7))))
                if case%6 == 0:
                    wi.motion[:] = (0, 0, 0)
                inp = Input(wall=wi, head_distance=rng.choice([99, 100, 101]),
                            projected_depth=rng.choice([-.1, .999, 1, 1.001]),
                            screen_scale=(C.c_float*2)(.625, .625),
                            screen_position=(C.c_int32*2)(rng.choice([-1, 0, 320, 640, 641]), rng.choice([-1, 0, 240, 480, 481])),
                            npc_hidden=int(case%8 == 0), excluded_surface=rng.choice([b'', rng.choice(meshes).name]))
                expected = native.step(state, inp)
                actual = State.from_buffer_copy(bytes(state))
                assert lib.bk_player_scene_step(C.byref(actual), collision, C.byref(inp), error), (file.name, case, error.value)
                counts['cases'] += 1
                counts['missing_projection'] += actual.wall.missing_projection
                counts['singular_camera'] += actual.wall.singular_camera
                counts['wall_hits'] += actual.any_wall
                counts['floor_hits'] += bool(actual.surface_name)
                assert actual.npc_in_view == expected.npc_in_view, (file.name, case, actual.npc_in_view, expected.npc_in_view, inp.head_distance, inp.projected_depth, list(inp.screen_position), inp.npc_hidden, list(inp.screen_scale))
                if not actual.wall.missing_projection:
                    counts['compared'] += 1
                    assert actual.any_wall == expected.any_wall, (file.name, case, 'wall hit')
                    assert actual.wall.near_wall == expected.wall.near_wall
                    assert actual.wall_name == expected.wall_name, (file.name, case, actual.wall_name, expected.wall_name)
                    assert actual.surface_name == expected.surface_name, (file.name, case, actual.surface_name, expected.surface_name)
                    assert tuple(actual.wall.rays_blocked) == tuple(expected.wall.rays_blocked)
                    for a, w in zip(actual.wall.position, expected.wall.position):
                        compare(a, w, (file.name, case, 'position'))
                    for a, w in zip(actual.wall.normal, expected.wall.normal):
                        compare(a, w, (file.name, case, 'normal'))
                    compare(actual.wall_heading, expected.wall_heading, (file.name, case, 'heading'))
                    if not actual.wall.singular_camera:
                        compare(actual.wall.camera_distance, expected.wall.camera_distance, (file.name, case, 'camera distance'))
            records.append(dict(file=file.name, atr_sha256=hashlib.sha256(atr).hexdigest(), model_sha256=hashlib.sha256(data).hexdigest(), **counts))
            print(file.name, counts, flush=True)
        finally:
            lib.bk_collision_destroy(collision)
            lib.bk_model_destroy(model)
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), scenes=records,
                  totals={k: sum(r[k] for r in records) for k in counts},
                  max_relative_error=worst, largest_error=example,
                  native_function='4b39d9, both full wall passes, ground and math callees',
                  projection_calls=native.projection_calls,
                  hooks=['Static constructor allocation and frame lookup', '42d56c cached head projection input only'],
                  scope='29 original static collision scenes, first/second wall passes, names/heading, screen visibility and ground smoothing. Original uninitialized projection cases excluded from fidelity claims; deterministic policy tested separately. No dynamic props or complete player frame.')
    (ROOT/'local/original-player-scene-oracle.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'scenes'}))

if __name__ == '__main__':
    main()
