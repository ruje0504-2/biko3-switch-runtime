"""Complete unhooked4dac12 and4da594 from the fixed original EXE.

Cached39 node worlds, held camera local, device matrices, action tables and
pointer are explicit inputs. Original projection, sqrt, circle, eligibility,
camera sectors and action-row control all execute. Slot50 has no device
handle: retain its actual CPU scale/pivot writes, not a GPU rendering claim.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX
from original_prop_route_oracle import Native
from original_ending_frame_oracle import State as Frame
from original_ending_control_oracle import State as Control
from original_ending_auxiliary_oracle import State as Auxiliary
from original_ending_ui_oracle import UiSprite
from original_ending_ui_pick_oracle import Bindings as Geometry, ident
from model_binding import ROOT, library

F, I, B = C.c_float, C.c_int32, C.c_uint8
M, V, XY = F * 16, F * 3, I * 2


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('control', C.POINTER(Control)),
                ('auxiliary', C.POINTER(Auxiliary)), ('active', C.POINTER(I)),
                ('actions', C.POINTER(I)), ('targets', C.POINTER(XY)),
                ('kind', C.POINTER(I)), ('column', C.POINTER(I)),
                ('ring', C.POINTER(UiSprite)), ('camera', C.POINTER(F)),
                ('geometry', C.POINTER(Geometry))]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    exe = parser.parse_args().exe.read_bytes()
    native, lib = Native(exe), library()
    u = native.u
    lib.bk_ending_target_eligible.argtypes = [B, I, B, F, I]
    lib.bk_ending_target_step.argtypes = [C.POINTER(Bindings), C.POINTER(F),
                                         C.POINTER(I), C.c_void_p]
    error = C.create_string_buffer(256)
    rng = random.Random(0x4dac12)

    def wi(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def wf(address, value):
        u.mem_write(address, struct.pack('<f', value))

    def ri(address):
        return struct.unpack('<i', u.mem_read(address, 4))[0]

    def rf(address):
        return struct.unpack('<f', u.mem_read(address, 4))[0]

    eligible = 0
    for group in [0, 1, 2, 3, 4, 5, 127, 255]:
        u.mem_write(0x721b3c, bytes([group]))
        for action_variant in [0, 1, -1]:
            wi(0x721e04, action_variant)
            for variant in [0, 5, 1, 255]:
                u.mem_write(0x721b3d, bytes([variant]))
                for progress in [.389, .39, .391, float('nan')]:
                    value = F(progress).value
                    wf(0x721e20, value)
                    for target in range(-1, 39):
                        native.call(0x4da594, struct.pack('<i', target))
                        expected = u.reg_read(UC_X86_REG_EAX)
                        assert lib.bk_ending_target_eligible(
                            group, action_variant, variant, value, target) == expected
                        eligible += 1

    world, present = (M * 39)(), (B * 39)()
    nodes = [0x3001000 + i * 0x200 for i in range(39)]
    wi(0x645604, 0x300a000)
    wi(0x721b28, 0x300b000)
    selected = actions_selected = zero_depth = ties = absent = 0
    matched_rows = set()
    choices = set()
    for case in range(2016):
        frame = Frame()
        control = Control()
        aux = Auxiliary()
        frame.group = case % 5
        frame.phase = rng.choice([1, 3, 8])
        frame.camera_cached = 100
        control.variant = rng.choice([0, 5])
        aux.variant = rng.choice([0, 1, -1])
        aux.progress = F(rng.choice([0, .2, .389, .39, .4, 1])).value
        active = I(rng.choice([0, 3, 4, 10]))
        camera = ident()
        yaw = rng.choice([0, 90, 120, 135, 140, 180, 200, 220, 240, 265, 320, 359])
        camera[8], camera[10] = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
        position = V(5, 9, 100)
        view, projection, screen = ident(), ident(), ident()
        view[12:15] = [rng.uniform(-30, 30) for _ in range(3)]
        projection[0] = rng.uniform(.5, 2)
        projection[5] = rng.uniform(.5, 2)
        projection[11] = rng.choice([0, .001, -.001])
        screen[0], screen[5], screen[12], screen[13] = 20, -20, 320, 240
        pointer = (F * 2)(rng.uniform(-100, 800), rng.uniform(-100, 600))
        table = (I * 80)(*[rng.randrange(-1, 39) for _ in range(80)])
        targets = (XY * 39)(*[XY(rng.randrange(-1000, 1000),
                                  rng.randrange(-1000, 1000)) for _ in range(39)])
        kind, column = I(91), I(92)
        ring = UiSprite()
        ring.rect[:] = [91, 73, rng.choice([1, 32, 96, 300]), 100]
        ring.transform.scale[:] = [1.25, .7]
        ring.transform.pivot[:] = [.3, .9]
        ring.rgb = 99
        for i in range(39):
            world[i] = ident()
            world[i][12:15] = [rng.uniform(-150, 150) for _ in range(3)]
            present[i] = rng.randrange(7) != 0
        if case % 13 == 0:
            for i in range(39):
                world[i][15] = 0
            projection[11] = 0
        if case % 17 == 0:
            world[1][12:15] = position[:]
            zero_depth += bool(present[1])
        if case % 5 == 0:
            view, projection, screen = ident(), ident(), ident()
            position[:] = [0, 0, 100]
            pointer[:] = [0, 0]
            camera = ident()
            for i in range(39):
                world[i] = ident()
                world[i][14] = rng.choice([0, 50, 90])
                present[i] = 1
            ties += 1
        if case % 101 == 0:
            for i in range(39):
                present[i] = 0
            absent += 1
        if case >= 2000:
            row = case - 2000
            frame.group, frame.phase = 0, 1
            control.variant, aux.variant, aux.progress = 0, 0, .4
            camera, view, projection, screen = ident(), ident(), ident(), ident()
            position[:], pointer[:] = [0, 0, 100], [0, 0]
            ring.rect[2] = 100
            node = 6 if row == 15 else 1
            for i in range(39):
                world[i], present[i] = ident(), i == node
            for i in range(16):
                table[i * 5] = node if i == row else -1
        for i, address in enumerate(nodes):
            wi(0x721ef4 + i * 4, address if present[i] else 0)
            u.mem_write(address + 0xc0, bytes(world[i]))
        u.mem_write(0x721b3c, bytes([frame.group, control.variant]))
        wi(0x721e00, frame.phase)
        wi(0x721e04, aux.variant)
        wf(0x721e20, aux.progress)
        wi(0x300b140, active.value)
        wi(0x721ed0, frame.camera_cached)
        wi(0x721e10, kind.value)
        wi(0x7220f0, column.value)
        u.mem_write(0x300a080, bytes(camera))
        u.mem_write(0x71b40c, bytes(position))
        u.mem_write(0x642fa8, bytes(view))
        u.mem_write(0x642830, bytes(projection))
        u.mem_write(0x642af0, bytes(screen))
        u.mem_write(0x721f90, bytes(targets))
        u.mem_write(0x709db8, bytes(table))
        u.mem_write(0x738770, bytes(0x16c))
        wf(0x73887c, ring.rect[2])
        for address, value in zip([0x738894, 0x738898, 0x73888c, 0x738890],
                                  [*ring.transform.scale, *ring.transform.pivot]):
            wf(address, value)
        native.call(0x4dac12, bytes(pointer))
        expected_result = u.reg_read(UC_X86_REG_EAX)
        expected_fields = (ri(0x721ed0), ri(0x721e10), ri(0x7220f0))
        expected_targets = bytes(u.mem_read(0x721f90, 39 * 8))
        expected_ring = UiSprite.from_buffer_copy(ring)
        expected_ring.transform.scale[:] = [rf(0x738894), rf(0x738898)]
        expected_ring.transform.pivot[:] = [rf(0x73888c), rf(0x738890)]
        geometry = Geometry(world, present, 39, position, view, projection, screen, 0)
        bindings = Bindings(C.pointer(frame), C.pointer(control), C.pointer(aux),
                            C.pointer(active), table, targets, C.pointer(kind),
                            C.pointer(column), C.pointer(ring), camera,
                            C.pointer(geometry))
        result = I(-1)
        assert lib.bk_ending_target_step(C.byref(bindings), pointer,
                                         C.byref(result), error), (case, error.value)
        got = (frame.camera_cached, kind.value, column.value)
        assert got == expected_fields and result.value == expected_result, (
            case, got, expected_fields, result.value, expected_result)
        assert bytes(targets) == expected_targets, ('projection', case)
        assert bytes(ring) == bytes(expected_ring), (
            'ring', case, list(ring.transform.scale), list(expected_ring.transform.scale))
        selected += frame.camera_cached != -1
        actions_selected += result.value != 0
        choices.add(frame.camera_cached)
        if case >= 2000:
            assert result.value == 1 and column.value == (case - 2000) % 5
            matched_rows.add(case - 2000)
    assert len(matched_rows) == 16
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  eligibility_calls=eligible, target_calls=2016,
                  selected=selected, actions_selected=actions_selected,
                  matched_rows=sorted(matched_rows), selected_nodes=sorted(choices),
                  coincident_target_cases=zero_depth, tie_cases=ties,
                  all_nodes_absent_cases=absent, max_error=0, hooks=[], scope=__doc__)
    (ROOT / 'local/original-ending-target-oracle.json').write_text(
        json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
