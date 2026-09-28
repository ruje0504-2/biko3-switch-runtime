"""Replay original NPC stages continuously from AI through root placement.

Only Windows clock, audio, and static-constructor service boundaries are
replaced. The documented original uninitialized AI choice is seeded to zero.
Later 0x4fd796 action/media dispatch is outside this instruction range.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_collision_oracle import Native as CollisionNative, bind as bind_collision, Archive
from original_npc_point_oracle import Native as PointNative, Point
from original_npc_motion_oracle import Native as MotionNative, Actions, State as MotionState, Timer
from original_npc_ai_oracle import State as Ai, Shared, Effects as AiEffects
from original_npc_head_oracle import State as Head, Matrix
from original_npc_route_oracle import Result as RouteEffects
from original_route_motion_oracle import Motion as PathState
from original_placement_oracle import Placement
from original_visibility_oracle import Vector, Segment
from model_binding import ROOT, library, decode

class State(C.Structure):
    _fields_ = [('path', PathState), ('ai', Ai), ('head', Head), ('alpha', C.c_float), ('visible', C.c_uint8), ('surface_name', C.c_char*260)]
class Input(C.Structure):
    _fields_ = [('group', C.c_int32), ('area', C.c_int32), ('player_action', C.c_int32), ('suppressed_actions', C.c_int32*6), ('active_clip', C.c_int32), ('background_clip', C.c_int32), ('short_range_action', C.c_int32), ('player_position', Vector), ('player_direction', Vector), ('player_head', Vector), ('head_world', Matrix), ('head_local', Matrix), ('torso_local', Matrix), ('vertical_offset', C.c_float), ('interaction_df', C.c_int8), ('interaction_e0', C.c_int8), ('excluded_surface', C.c_char_p)]
class Effects(C.Structure):
    _fields_ = [('ai', AiEffects), ('route', RouteEffects), ('placement', Placement), ('vertical_position', C.c_float)]

class Native(CollisionNative):
    # Use original actor global: sight cone reads this same actor's position
    # after route updates, not a separately supplied copy of its initial pose.
    actor, scene = 0x728de8, 0x726640
    background, clip, root_group, root = 0x3002000, 0x3003000, 0x3004000, 0x3005000
    head, torso, player_head, clock = 0x3007000, 0x3008000, 0x3009000, 0x300e000
    def __init__(self, exe):
        super().__init__(exe)
        self.u.mem_write(0x53f10c, struct.pack('<I', self.clock))
        self.u.hook_add(UC_HOOK_CODE, self.clock_hook, begin=self.clock, end=self.clock)
        for address in [0x46435e, 0x4f6226, 0x4fce2b, 0x534a34, 0x4fd2d5, 0x4fc994, 0x4fc9e0]:
            self.u.hook_add(UC_HOOK_CODE, self.observe, begin=address, end=address)
    def clock_hook(self, u, address, size, user):
        MotionNative.hook(self, u, address, size, user)
    def observe(self, u, address, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        if address == 0x4fce2b:
            # Match the explicit correction in npc_ai, without replacing AI.
            u.mem_write(sp-12, bytes(4)); return
        if address == 0x534a34: self.random_calls += 1; return
        if address == 0x4fd2d5: self.choice_reads += 1; return
        if address == 0x4fc994: self.allowed = u.reg_read(UC_X86_REG_EAX); return
        if address == 0x4fc9e0: self.snap = u.reg_read(UC_X86_REG_EAX); return
        ret = struct.unpack('<I', u.mem_read(sp, 4))[0]
        if address == 0x46435e:
            handle, flags = struct.unpack('<II', u.mem_read(sp+4, 8))
            assert not flags and handle in [0xabc001, 0xabc002, 0xdeadbeef]
            self.sounds.append(handle)
        else:
            args = struct.unpack('<III', u.mem_read(sp+4, 12))
            assert args[:2] == (self.group_index, self.area_index)
            self.route_cursor = args[2]; self.sounds.append('route')
            self.sound_position=struct.unpack('<3f',u.mem_read(self.actor+0x29c,12))
        u.reg_write(UC_X86_REG_ESP, sp+4); u.reg_write(UC_X86_REG_EIP, ret); u.reg_write(UC_X86_REG_EAX, 0)
    def step(self, state, shared, inp, seed, seconds, now):
        actions = Actions(0, 1, 4, (C.c_int32*4)(10,7,12,9))
        data = PointNative.write_point(self, state.ai.point, actions, state.path.run_remaining, inp.background_clip, state.path.cursor)
        struct.pack_into('<IIII', data, 0, self.clip, 0, self.head, inp.group)
        struct.pack_into('<f', data, 0x298, inp.vertical_offset)
        struct.pack_into('<3f', data, 0x29c, *state.path.position)
        struct.pack_into('<f', data, 0x2ac, state.path.yaw_degrees)
        struct.pack_into('<f', data, 0x32c, state.alpha)
        data[0x318], data[0x319] = state.visible, state.ai.stimulus&255
        data[0x5c0:0x5c0+len(state.surface_name)] = state.surface_name
        u = self.u; u.mem_write(self.actor, bytes(data))
        u.mem_write(self.clip+0x140, struct.pack('<i', inp.active_clip))
        u.mem_write(self.clip+0x160, struct.pack('<I', self.root_group))
        u.mem_write(self.root_group+0x14, struct.pack('<I', self.root))
        identity = Matrix(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
        u.mem_write(self.root, bytes(0x200)); u.mem_write(self.root+0x100, bytes(identity))
        u.mem_write(self.head+0x80, bytes(inp.head_local)); u.mem_write(self.head+0xc0, bytes(inp.head_world))
        u.mem_write(self.torso+0x80, bytes(inp.torso_local)); u.mem_write(0xbf3c70, struct.pack('<I', self.torso))
        u.mem_write(self.player_head+0xf0, bytes(inp.player_head)); u.mem_write(0x71b518, struct.pack('<I', self.player_head))
        u.mem_write(0xbf3c80, struct.pack('<II', state.path.segment_start, state.path.segment_end))
        u.mem_write(0xbf3c74, struct.pack('<I', state.path.last_crossed)); u.mem_write(0xbf3c78, bytes([state.path.crossed]))
        u.mem_write(0x7219a8, struct.pack('<ii', inp.group, inp.area))
        u.mem_write(0x71b7ac, bytes(inp.player_position)); u.mem_write(0x71b7c4, bytes(inp.player_direction))
        u.mem_write(0x71bcdf, bytes([inp.interaction_df&255, inp.interaction_e0&255]))
        u.mem_write(0x71b520, struct.pack('<i', inp.player_action)); u.mem_write(0x71b544, struct.pack('<i', inp.short_range_action))
        for address, value in zip([0x71b550,0x71b554,0x71b558,0x71b55c,0x71b564,0x71b568], inp.suppressed_actions):
            u.mem_write(address, struct.pack('<i', value))
        for address, value in zip([0x71ba8c,0x71bcda,0x71bcd8], [shared.prompt,shared.response,shared.outcome]):
            u.mem_write(address, bytes([value]))
        for address, handle in [(0x7299c0,0xabc001),(0x729ae0,0xabc002)]:
            u.mem_write(address, struct.pack('<I', handle))
        u.mem_write(0x58edd8, struct.pack('<I', seed)); u.mem_write(0x733700, struct.pack('<f', seconds))
        u.mem_write(0x725930, inp.excluded_surface+b'\0')
        self.now, self.clock_calls, self.random_calls, self.choice_reads = now, 0, 0, 0
        self.sounds, self.allowed, self.snap, self.route_cursor = [], -1, 0, 0
        self.sound_position=(0,0,0)
        self.group_index, self.area_index = inp.group, inp.area
        u.mem_write(self.stack, struct.pack('<II', self.stop, self.actor))
        u.reg_write(UC_X86_REG_ESP, self.stack); u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(0x4fc8f9, 0x4fcdb6, count=40000000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x4fcdb6
        b = bytes(u.mem_read(self.actor, 0x900)); result = State.from_buffer_copy(state)
        result.path = PathState(Vector(*struct.unpack_from('<3f', b, 0x29c)), struct.unpack_from('<f', b, 0x2ac)[0], struct.unpack_from('<I', b, 0x830)[0], struct.unpack('<i', u.mem_read(0xbf3c6c, 4))[0], *struct.unpack('<II', u.mem_read(0xbf3c80, 8)), struct.unpack('<I', u.mem_read(0xbf3c74, 4))[0], u.mem_read(0xbf3c78, 1)[0])
        result.ai = Ai(Point(MotionState(struct.unpack_from('<i', b, 0x10)[0], struct.unpack_from('<i', b, 0x850)[0], b[0x328], b[0x578], b[0x84c], Timer.from_buffer_copy(b[0x840:0x84c])), Timer.from_buffer_copy(b[0x854:0x860]), b[0x330], b[0x331], b[0x870]), b[0x319])
        result.head = Head(Segment.from_buffer_copy(b[0x814:0x830]), struct.unpack_from('<f', b, 0x808)[0], struct.unpack_from('<f', b, 0x810)[0])
        result.alpha, result.visible = struct.unpack_from('<f', b, 0x32c)[0], b[0x318]
        result.surface_name = b[0x5c0:0x6c4].split(b'\0')[0]
        shared = Shared(*(u.mem_read(a, 1)[0] for a in [0x71ba8c,0x71bcda,0x71bcd8]))
        seed = struct.unpack('<I', u.mem_read(0x58edd8, 4))[0]
        placement = Placement(result.path.position, result.path.yaw_degrees, Matrix.from_buffer_copy(b[0x2d8:0x318]))
        assert b[0x2d8:0x318] == bytes(u.mem_read(self.root+0xc0, 64))
        return result, shared, seed, placement, struct.unpack_from('<f', b, 0x2a8)[0], struct.unpack('<f', u.mem_read(self.stack-16, 4))[0]

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); ap.add_argument('data', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native, lib, rng = Native(exe), library(), random.Random(0x4fc8f9); bind_collision(lib)
    lib.bk_route_decode.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p]; lib.bk_route_decode.restype = C.c_void_p
    lib.bk_route_destroy.argtypes = [C.c_void_p]
    lib.bk_npc_spatial_step.argtypes = [C.POINTER(State),C.POINTER(Shared),C.POINTER(C.c_uint32),C.c_void_p,C.c_void_p,C.POINTER(Input),C.c_float,C.c_uint32,C.POINTER(Effects),C.c_void_p]
    lib.bk_npc_spatial_step.restype = C.c_int
    archive = Archive(args.data/'bk3_03.pp'); entries = {e.name.lower():e for e in archive.entries}; error = C.create_string_buffer(256)
    routes = sorted(args.data.glob('*.ckp')); records = []; cases = failures = startups = media = clocks = corrected = 0; worst = 0
    def equal(actual, wanted, label):
        nonlocal worst
        for a, b in zip(actual, wanted):
            delta = abs(a-b)/max(1,abs(b)); worst = max(worst,delta)
            assert math.isfinite(delta) and delta < 3e-6, (label,a,b,delta)
    for si, file in enumerate(sorted(args.data.glob('*.atr'))):
        filename = file.with_suffix('.x').name
        ok, model, message = decode(lib, archive.read(entries[filename])); assert ok == 1, message
        world = (C.c_float*(model.contents.frame_count*16))(); atr = file.read_bytes()
        assert lib.bk_model_world_matrices(model, world, len(world), error)
        collision = lib.bk_collision_create(model, world, len(world), filename.upper().encode(), atr, len(atr), error); assert collision, error.value
        try:
            native.create(model.contents, world, filename.upper().encode(), atr)
            for ri in range(2):
                route_file = routes[(si*2+ri)%len(routes)]; raw = route_file.read_bytes()
                points = []
                for off in range(0,len(raw),20):
                    p = struct.unpack_from('<4fB',raw,off)
                    if not any(p): break
                    points.append(p)
                route = lib.bk_route_decode(raw,len(raw),error); assert route,error.value
                native.u.mem_write(0xbe9a18,raw+bytes(20480-len(raw)))
                try:
                    for case in range(24):
                        cursor = 0 if case%6 == 0 else rng.randrange(len(points))
                        origin = Vector(*points[max(cursor-1,0)][:3]); origin[1] = rng.choice([0,5,-3])
                        if case%4 == 0:
                            mesh = lib.bk_collision_mesh(collision,case%lib.bk_collision_count(collision)).contents
                            ti = rng.randrange(mesh.index_count//3)*3
                            origin = Vector(*(sum(mesh.vertices[mesh.indices[ti+j]][k] for j in range(3))/3 for k in range(3)))
                        def timer(): return Timer(rng.choice([0,100,2000]),rng.choice([0,1000,0xfffffff0]),rng.choice([0,0,1]))
                        state = State(PathState(origin,rng.uniform(-360,360),cursor,rng.choice([0,0,3]),0,1,0,0),Ai(Point(MotionState(rng.choice([0,1,1,4,7,10]),rng.choice([0,1,1,2,3,4]),1 if case%13 == 0 else 0,rng.choice([0,1,1,2]),rng.choice([0,0,1,4,5]),timer()),timer(),0,rng.randrange(2),rng.randrange(2)),rng.choice([0,0,1,2])),Head(),rng.choice([.9,.99,1]),rng.choice([0,1]),b'old_surface')
                        shared = Shared(0,0,0); seed = C.c_uint32(rng.getrandbits(32))
                        group = (si+case)%5; player = Vector(origin[0]+rng.choice([5,30,100]),origin[1],origin[2]+rng.choice([0,20]))
                        matrix = Matrix(1,0,0,0,0,1,0,0,0,0,1,0,origin[0],origin[1]+18,origin[2],1)
                        local, torso = Matrix(*matrix), Matrix(*matrix); local[1], torso[1] = rng.uniform(-1,1), rng.uniform(-1,1)
                        inp = Input(group,rng.choice([0,5,7,8]),rng.choice([0,4,18,27]),(C.c_int32*6)(18,19,20,21,23,27),rng.choice([0,1,4,7,10]),rng.choice([0,1,2]),1,player,Vector(0,0,1),Vector(player[0],player[1]+18,player[2]),matrix,local,torso,rng.choice([0,18,20]),0,rng.choice([0,0,1]),b'')
                        # A few recurrent steps expose ordering and held-cache
                        # behavior without asserting a full gameplay frame.
                        for step in range(3):
                            before, before_shared, before_seed = State.from_buffer_copy(state), Shared.from_buffer_copy(shared), seed.value
                            output = Effects(); saved_output = bytes(output)
                            seconds = C.c_float(rng.choice([0,1/60,.125,.5])).value; now = (0xfffff000+case*1000+step*17)&0xffffffff
                            valid = lib.bk_npc_spatial_step(C.byref(state),C.byref(shared),C.byref(seed),route,collision,C.byref(inp),seconds,now,C.byref(output),error)
                            if not valid:
                                assert b'past decoded route sentinel' in error.value, (file.name,route_file.name,case,error.value)
                                assert bytes(state)==bytes(before) and bytes(shared)==bytes(before_shared) and seed.value==before_seed and bytes(output)==saved_output
                                failures += 1; break
                            expected, interaction, random_state, placement, vertical, distance = native.step(before,before_shared,inp,before_seed,seconds,now)
                            assert bytes(state.ai)==bytes(expected.ai), (file.name,route_file.name,case,step,'AI',bytes(state.ai).hex(),bytes(expected.ai).hex())
                            assert bytes(shared)==bytes(interaction) and seed.value==random_state
                            assert (state.alpha,state.visible,state.surface_name)==(expected.alpha,expected.visible,expected.surface_name), (file.name,case,'scene',state.visible,expected.visible)
                            for field in ['cursor','run_remaining','segment_start','segment_end','last_crossed','crossed']:
                                assert getattr(state.path,field)==getattr(expected.path,field), (file.name,case,field)
                            equal([*state.path.position,state.path.yaw_degrees],[*expected.path.position,expected.path.yaw_degrees],'path')
                            equal([*state.head.sight.start,*state.head.sight.end,state.head.sight.distance,state.head.bearing,state.head.facing],[*expected.head.sight.start,*expected.head.sight.end,expected.head.sight.distance,expected.head.bearing,expected.head.facing],'head')
                            equal(output.placement.world,placement.world,'root'); equal([output.vertical_position],[vertical],'vertical')
                            assert output.route.movement.allowed==native.allowed and output.route.movement.distance==distance
                            assert output.route.point.snap_to_point==native.snap
                            sounds = ([0xabc000+output.ai.sound] if output.ai.sound else []) + ([0xdeadbeef] if output.route.point.play_wait_sound else []) + (['route'] if output.route.point.play_route_sound else [])
                            assert sounds==native.sounds and output.route.sound_cursor==native.route_cursor
                            equal(output.route.sound_position,native.sound_position,'route sound pre-ground position')
                            assert output.ai.used_zero_choice==bool(native.choice_reads and not native.random_calls)
                            cases += 1; media += len(sounds); clocks += native.clock_calls; corrected += output.ai.used_zero_choice; startups += before.path.cursor==0
                finally: lib.bk_route_destroy(route)
            records.append(file.name); print(file.name,'spatial sequence PASS',flush=True)
        finally: lib.bk_collision_destroy(collision); lib.bk_model_destroy(model)
    report = dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=cases,bounds_rejections=failures,cursor_zero_starts=startups,audio_commands=media,clock_queries=clocks,zero_choice_cases=corrected,max_normalized_error=worst,scenes=records,routes=[r.name for r in routes],native_functions=['0x4fc8f9..0x4fcdb6 with complete AI, route, head, ground and root callees'],hooks=['constructor allocation/frame lookup','GetTickCount','audio boundaries','read-only branch observations','explicit zero seed for native uninitialized AI local'],scope='Combined spatial actor stages on29scene assets and53routes with recurrent supplied-cache fixtures. Root application checked in original frame. Stops before0x4fd796 action events; no animation/material/audio backend/dynamic props/gameplay loop implied.',x87_control_word='0x037f')
    (ROOT/'local/original-npc-spatial-oracle.json').write_text(json.dumps(report,indent=2)+'\n'); print('PASS',cases,'spatial steps;',failures,'bounds rejections; max error',worst)

if __name__ == '__main__': main()
