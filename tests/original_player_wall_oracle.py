"""Execute complete original wall response/restore, including native math callees."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, library

Vec = C.c_float * 3
Triangle = Vec * 3
class Wall(C.Structure):
    _fields_ = [('position', Vec), ('camera_distance', C.c_float), ('normal', Vec),
                ('rays_blocked', C.c_int32 * 7), ('near_wall', C.c_int32),
                ('singular_camera', C.c_uint32), ('missing_projection', C.c_uint32)]
class Input(C.Structure):
    _fields_ = [('previous', Vec), ('motion', Vec), ('camera', Vec),
                ('rays', Vec * 7), ('height', C.c_float)]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe')
    args = parser.parse_args()
    exe = open(args.exe, 'rb').read()
    u, lib = machine(exe), library()
    lib.bk_player_wall_triangle.argtypes = [C.POINTER(Wall), C.POINTER(C.c_int), C.POINTER(Input), Triangle, C.c_void_p]
    lib.bk_player_wall_restore.argtypes = [Vec, Vec, C.c_float, Triangle, C.c_void_p]
    stack, stop, dest = 0x2008000, 0x300f000, 0x3000100
    branches = {a: 0 for a in [0x4b5178, 0x4b5821, 0x4b58de, 0x4b5b13, 0x4b5dae, 0x4b61b4]}
    def observe(uc, address, size, data):
        branches[address] += 1
    for address in branches:
        u.hook_add(UC_HOOK_CODE, observe, begin=address, end=address)
    def native(address, state, inp, triangle):
        # Original stack locals have no input contract; zeroing makes undefined
        # cases reproducible, but those are counted separately, not claimed equal.
        u.mem_write(stack - 8192, bytes(8192))
        u.mem_write(dest, bytes(state.position) + struct.pack('<f', 17))
        u.mem_write(0x71b7d8, bytes(inp.previous))
        u.mem_write(0x71b7c4, struct.pack('<4f', inp.motion[0], 0, inp.motion[1], inp.motion[2]))
        u.mem_write(0x71b45c, bytes(inp.camera))
        for j, base in enumerate([0x71b474, 0x71b494, 0x71b4b4]):
            u.mem_write(base, struct.pack('<7f', *(r[j] for r in inp.rays)))
        u.mem_write(0x71b374, struct.pack('<f', state.camera_distance))
        u.mem_write(0x71b4dc, bytes(state.rays_blocked))
        u.mem_write(0x71b82a, bytes([state.near_wall]))
        u.mem_write(0x71b82c, bytes(state.normal))
        payload = struct.pack('<II', stop, dest)
        for vertex in triangle:
            payload += bytes(vertex) + bytes(4)
        payload += struct.pack('<f', inp.height)
        u.mem_write(stack, payload)
        u.reg_write(UC_X86_REG_ESP, stack)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(address, stop, count=300000)
        assert u.reg_read(UC_X86_REG_EIP) == stop, hex(u.reg_read(UC_X86_REG_EIP))
        result = Wall.from_buffer_copy(bytes(state))
        result.position[:] = struct.unpack('<3f', u.mem_read(dest, 12))
        result.camera_distance = struct.unpack('<f', u.mem_read(0x71b374, 4))[0]
        result.normal[:] = struct.unpack('<3f', u.mem_read(0x71b82c, 12))
        result.rays_blocked[:] = struct.unpack('<7i', u.mem_read(0x71b4dc, 28))
        result.near_wall = u.mem_read(0x71b82a, 1)[0]
        return result, u.reg_read(UC_X86_REG_EAX) & 255
    rng = random.Random(0x4b4f31)
    error, near = C.create_string_buffer(256), C.c_int()
    worst = 0.0
    worst_case = None
    checked = changes = restores = singular = uninitialized = nonfinite = 0
    def compare(actual, wanted, context):
        nonlocal worst, worst_case
        delta = abs(actual - wanted) / max(1, abs(wanted))
        if delta > worst:
            worst, worst_case = delta, [context, actual, wanted]
        assert delta < 3e-5, (context, actual, wanted, delta)
    for case in range(16000):
        def vec(scale=30):
            return Vec(*(rng.uniform(-scale, scale) for _ in range(3)))
        triangle = Triangle(*(vec() for _ in range(3)))
        inp = Input(previous=vec(), motion=vec(), camera=vec(300),
                    rays=(Vec*7)(*(vec(100) for _ in range(7))), height=rng.uniform(0, 40))
        state = Wall(position=vec(), camera_distance=rng.uniform(0, 100), normal=vec(),
                     rays_blocked=(C.c_int32*7)(*(rng.randrange(2) for _ in range(7))), near_wall=rng.randrange(2))
        if case % 2 == 0:
            inp.previous[:] = (state.position[0] + rng.uniform(-5, 5), state.position[1], state.position[2] + rng.uniform(-5, 5))
        if case % 5 == 0:
            inp.motion[:] = (0, 0, 0)
        if case % 11 == 0:
            triangle[1][0] = triangle[0][0]
        if case % 11 == 1:
            triangle[1][2] = triangle[0][2]
        if case % 11 == 2:
            triangle[1][0], triangle[1][2] = triangle[0][0], triangle[0][2]
        if case % 11 == 3:
            state.position[:] = triangle[rng.randrange(3)][:]
        if case % 11 == 4:
            triangle = Triangle(Vec(-20, 0, 0), Vec(20, 30, 0), Vec(20, 0, 20))
            state.position[:] = (0, 0, 2)
            inp.previous[:] = (0, 0, 3)
            inp.camera[:] = (0, 0, -10)
            inp.height = 20
        expected, native_near = native(0x4b4f31, state, inp, triangle)
        actual = Wall.from_buffer_copy(bytes(state))
        ok = lib.bk_player_wall_triangle(C.byref(actual), C.byref(near), C.byref(inp), triangle, error)
        assert ok, (case, error.value, list(state.position), list(expected.position), [list(v) for v in triangle])
        singular += actual.singular_camera
        uninitialized += bool(actual.missing_projection)
        values = [*expected.position, expected.camera_distance, *expected.normal]
        if actual.missing_projection:
            pass # Original position depends on stack garbage, explicit C policy.
        elif not all(math.isfinite(v) for v in values):
            nonfinite += 1
            assert actual.singular_camera
        else:
            checked += 1
            assert near.value == native_near, (case, 'near', near.value, native_near)
            assert actual.near_wall == expected.near_wall
            assert tuple(actual.rays_blocked) == tuple(expected.rays_blocked)
            for a, w in zip(actual.position, expected.position):
                compare(a, w, (case, 'position'))
            for a, w in zip(actual.normal, expected.normal):
                compare(a, w, (case, 'normal'))
            # Skip distance in native singular cases: port explicitly retains a
            # finite previous best, regardless of native NaN comparison results.
            if not actual.singular_camera:
                compare(actual.camera_distance, expected.camera_distance, (case, 'camera'))
            changes += bytes(actual.position) != bytes(state.position)
        expected, _ = native(0x4b5f21, state, inp, triangle)
        position = Vec(*state.position)
        assert lib.bk_player_wall_restore(position, inp.previous, inp.height, triangle, error), error.value
        assert bytes(position) == bytes(expected.position), (case, 'restore', tuple(position), tuple(expected.position))
        restores += bytes(position) != bytes(state.position)
    assert checked > 12000 and changes > 1000 and restores > 1000
    assert all(branches.values()), branches
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=16000,
                  finite_primary_cases=checked, corrected_cases=changes, restores=restores,
                  native_calls=32000, max_relative_error=worst, largest_error=worst_case,
                  original_singular_camera_intersections=singular,
                  original_uninitialized_projection_cases=uninitialized,
                  original_nonfinite_cases=nonfinite,
                  native_branches={hex(k): v for k, v in branches.items()},
                  replaced_functions=[], hooks='Read-only instruction counters; no math replacement',
                  scope='Complete triangle functions 4b4f31/4b5f21. Undefined projection and singular camera behavior explicitly bounded. Mesh passes and player frame are separate.')
    (ROOT/'local/original-player-wall-oracle.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))

if __name__ == '__main__':
    main()
