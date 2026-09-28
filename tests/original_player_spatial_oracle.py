"""Combined original player movement, scene physics and primary root application."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_FPCW
from original_player_scene_oracle import Native as SceneNative, State as Scene, Input as SceneInput
from original_player_wall_oracle import Wall, Input as WallInput, Vec
from original_player_movement_oracle import State as Movement, Input as MovementInput, FIELDS, ALIASES
from original_collision_oracle import bind
from model_binding import ROOT, library, decode
from bk3_assets import Archive

class State(C.Structure):
    _fields_ = [('movement', Movement), ('scene', Scene), ('vertical_position', C.c_float)]
class Input(C.Structure):
    _fields_ = [('movement', MovementInput), ('scene', SceneInput),
                ('base_head_height', C.c_float), ('cached_head_height', C.c_float)]
class Placement(C.Structure):
    _fields_ = [('position', Vec), ('yaw_degrees', C.c_float), ('world', C.c_float*16)]

class Native(SceneNative):
    scene = 0x726640
    clip, root_group, root, head = 0x3007000, 0x3006000, 0x3008000, 0x3009000
    def __init__(self, exe):
        super().__init__(exe)
        for address in [0x4b757e, 0x4b76c2, 0x4c20ee]:
            self.u.hook_add(UC_HOOK_CODE, self.input_hook, begin=address, end=address)
    def input_hook(self, u, address, size, data):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', u.mem_read(sp, 4))[0]
        result = 0
        if address == 0x4b757e:
            a, b = struct.unpack('<2I', u.mem_read(sp+4, 8))
            u.mem_write(a, struct.pack('<f', self.spatial_input.movement.look[0]))
            u.mem_write(b, struct.pack('<f', self.spatial_input.movement.look[1]))
        elif address == 0x4b76c2:
            result = struct.unpack('<3I', u.mem_read(sp+4, 12)) in self.keys
        else:
            result = self.spatial_input.movement.controls_allowed
        u.reg_write(UC_X86_REG_EAX, int(result))
        u.reg_write(UC_X86_REG_ESP, sp+4)
        u.reg_write(UC_X86_REG_EIP, ret)
    def step_spatial(self, state, inp, rng):
        self.spatial_input = inp
        self.keys = {rng.choice(aliases) for bit, aliases in enumerate(ALIASES)
                     if inp.movement.buttons & (1 << bit)}
        self.prepare(state.scene, inp.scene)
        u, p = self.u, self.player
        u.mem_write(p, struct.pack('<III', self.clip, 0, self.head))
        u.mem_write(p+0x298, struct.pack('<f', inp.base_head_height))
        u.mem_write(p+0x14, bytes(inp.movement.actions))
        for name, offset, fmt in FIELDS:
            value = getattr(state.movement, name)
            values = list(value) if isinstance(value, C.Array) else [value]
            u.mem_write(p+offset, struct.pack('<'+fmt, *values))
        u.mem_write(0x7099ec, struct.pack('<i', state.movement.move_latch))
        u.mem_write(0x733700, struct.pack('<f', inp.movement.seconds))
        u.mem_write(self.clip+0x140, struct.pack('<i', inp.movement.active_clip))
        u.mem_write(self.clip+0x160, struct.pack('<I', self.root_group))
        u.mem_write(self.root_group+0x14, struct.pack('<I', self.root))
        identity = struct.pack('<16f', 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
        u.mem_write(self.root, bytes(0x300))
        u.mem_write(self.root+0x100, identity)
        u.mem_write(self.head+0xf4, struct.pack('<f', inp.cached_head_height))
        u.mem_write(self.stack, struct.pack('<III', self.stop, 0x3000000, p))
        u.reg_write(UC_X86_REG_ESP, self.stack)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(0x4c0126, 0x4c0ce6, count=30000000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x4c0ce6, hex(u.reg_read(UC_X86_REG_EIP))
        result = State(scene=self.read(state.scene))
        for name, offset, fmt in FIELDS:
            values = struct.unpack('<'+fmt, u.mem_read(p+offset, struct.calcsize(fmt)))
            field = getattr(result.movement, name)
            if isinstance(field, C.Array):
                field[:] = values
            else:
                setattr(result.movement, name, values[0])
        result.movement.move_latch = struct.unpack('<i', u.mem_read(0x7099ec, 4))[0]
        result.vertical_position = struct.unpack('<f', u.mem_read(p+0x2a8, 4))[0]
        root = struct.unpack('<16f', u.mem_read(self.root+0xc0, 64))
        return result, root

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('data', type=Path)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    native, lib, rng = Native(exe), library(), random.Random(0x4c0ce6)
    bind(lib)
    lib.bk_player_spatial_step.argtypes = [C.POINTER(State), C.c_void_p, C.POINTER(Input), C.POINTER(Placement), C.c_void_p]
    lib.bk_player_actions_initialize.argtypes = [C.POINTER(C.c_int32)]
    # Actual partial initializer, including its original slot writer. Preserve
    # untouched7/15, with nonzero sentinels, instead of assuming an action ID.
    bindings = C.c_int32*21
    for case in range(128):
        actual = bindings(*(rng.randrange(-100, 100) for _ in range(21)))
        u, base = native.u, 0x2007000
        u.mem_write(native.player+0x14, bytes(actual))
        u.mem_write(base+8, struct.pack('<I', native.player))
        u.reg_write(UC_X86_REG_EBP, base)
        u.reg_write(UC_X86_REG_ESP, base-0x1000)
        u.emu_start(0x4bf290, 0x4bf3c0, count=10000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x4bf3c0
        assert lib.bk_player_actions_initialize(actual)
        assert bytes(actual) == bytes(u.mem_read(native.player+0x14, 84))
    actions = bindings()
    assert lib.bk_player_actions_initialize(actions)
    archive = Archive(args.data/'bk3_03.pp')
    entries = {e.name.lower(): e for e in archive.entries}
    error, placement = C.create_string_buffer(256), Placement()
    worst, samples, compared, undefined, bypasses, aliases = 0.0, 0, 0, 0, 0, 0
    records = []
    def compare(a, w, context):
        nonlocal worst
        delta = abs(a-w)/max(1, abs(w))
        worst = max(worst, delta)
        assert delta < 4e-5, (context, a, w, delta)
    for file in sorted(args.data.glob('*.atr')):
        filename = file.with_suffix('.x').name
        data, atr = archive.read(entries[filename.lower()]), file.read_bytes()
        ok, model, message = decode(lib, data)
        assert ok, message
        world = (C.c_float*(model.contents.frame_count*16))()
        assert lib.bk_model_world_matrices(model, world, len(world), error)
        collision = lib.bk_collision_create(model, world, len(world), filename.upper().encode(), atr, len(atr), error)
        assert collision, error.value
        try:
            native.create(model.contents, world, filename.upper().encode(), atr)
            meshes = [lib.bk_collision_mesh(collision, i).contents for i in range(lib.bk_collision_count(collision))]
            points = [Vec(*m.vertices[m.indices[i]]) for m in meshes for i in range(0, m.index_count, 3)]
            for case in range(32):
                point = Vec(*rng.choice(points))
                point[0] += rng.uniform(-5, 5)
                point[2] += rng.uniform(-5, 5)
                camera = Vec(point[0]+7, point[1]+18, point[2]+23)
                motion = Movement(position=point, velocity=Vec(1, 2, 3),
                                  yaw=rng.uniform(-30, 390), pitch=rng.uniform(-90, 90),
                                  action=rng.choice(list(actions)), move_latch=case%2,
                                  interaction_mode=rng.choice([0, 1, 2, 5]))
                state = State(movement=motion,
                              scene=Scene(wall=Wall(position=point, camera_distance=200, near_wall=1),
                                          wall_heading=77, wall_name=b'keep wall', surface_name=b'keep ground', any_wall=1))
                inputs = Input(movement=MovementInput(seconds=rng.choice([0, .016, .033, .5]),
                                                       look=(C.c_float*2)(3, -2), buttons=rng.randrange(64),
                                                       controls_allowed=case%4 != 0, active_clip=case%3, actions=actions),
                               scene=SceneInput(wall=WallInput(camera=camera, rays=(Vec*7)(*(camera for _ in range(7)))),
                                                head_distance=99, projected_depth=.9, screen_scale=(C.c_float*2)(.625, .625),
                                                screen_position=(C.c_int32*2)(320, 240), npc_hidden=case%6 == 0, excluded_surface=b''),
                               base_head_height=18, cached_head_height=point[1]+12)
                if case%7 == 0:
                    inputs.movement.actions[11] = inputs.movement.actions[1]
                    aliases += 1
                expected, root = native.step_spatial(state, inputs, rng)
                actual = State.from_buffer_copy(bytes(state))
                assert lib.bk_player_spatial_step(C.byref(actual), collision, C.byref(inputs), C.byref(placement), error), (file.name, case, error.value)
                samples += 1
                compare(actual.vertical_position, expected.vertical_position, (file.name, case, 'vertical'))
                assert actual.scene.npc_in_view == expected.scene.npc_in_view
                if actual.scene.wall.missing_projection:
                    undefined += 1
                else:
                    compared += 1
                    for name, offset, fmt in FIELDS:
                        av, wv = getattr(actual.movement, name), getattr(expected.movement, name)
                        if isinstance(av, C.Array):
                            for a, w in zip(av, wv):
                                compare(a, w, (file.name, case, name))
                        else:
                            compare(av, wv, (file.name, case, name))
                    assert actual.movement.move_latch == expected.movement.move_latch
                    assert actual.scene.wall_name == expected.scene.wall_name
                    assert actual.scene.surface_name == expected.scene.surface_name
                    assert actual.scene.any_wall == expected.scene.any_wall
                    assert actual.scene.wall.near_wall == expected.scene.wall.near_wall
                    assert tuple(actual.scene.wall.rays_blocked) == tuple(expected.scene.wall.rays_blocked)
                    for a, w in zip(placement.world, root):
                        compare(a, w, (file.name, case, 'root'))
                    compare(actual.scene.wall_heading, expected.scene.wall_heading, (file.name, case, 'wall heading'))
                    for a, w in zip(actual.scene.wall.normal, expected.scene.wall.normal):
                        compare(a, w, (file.name, case, 'normal'))
                    if not actual.scene.wall.singular_camera:
                        compare(actual.scene.wall.camera_distance, expected.scene.wall.camera_distance, (file.name, case, 'camera'))
                bypasses += actual.movement.action in [inputs.movement.actions[j] for j in [11, 12, 13, 14, 16, 17]]
            records.append(file.name)
            print(file.name, 'combined32 PASS', flush=True)
        finally:
            lib.bk_collision_destroy(collision)
            lib.bk_model_destroy(model)
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=samples,
                  finite_defined_cases=compared, original_undefined_projection_cases=undefined,
                  special_action_bypasses=bypasses, action_alias_cases=aliases,
                  original_action_initializer_cases=128, max_relative_error=worst, scenes=records,
                  native_function='4c0126..4c0ce6 including actual root setter, scene/wall/ground callees',
                  hooks=['Constructor allocation/frame binding', '4b757e mouse,4b76c2 keys', '4c20ee prior interaction decision', '42d56c cached head screen projection'],
                  scope='Combined ordinary player movement/physics/root stage. Interaction selection, scripted movement, cameras, UI triggers and full player presentation remain separate.')
    (ROOT/'local/original-player-spatial-oracle.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))

if __name__ == '__main__':
    main()
