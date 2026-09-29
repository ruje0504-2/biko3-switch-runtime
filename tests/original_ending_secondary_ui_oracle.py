"""Original47C334/47CB41/47CD36, including native projection and menu search.

The original target, matrix, distance, circle, sine/cosine and sprite setters
execute unmodified. Device handles are absent; this checks CPU geometry, not
rendered pixels. Cached worlds and logical content dimensions are explicit
inputs. Impossible menu placement is compared up to the portable bounded
failure, without claiming the original infinite search has a normal return.
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
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP
from original_prop_route_oracle import Native
from original_ending_frame_oracle import State as Frame
from original_ending_ui_oracle import UiSprite, Ui, bind as bind_ui
from original_ending_ui_pick_oracle import Bindings as Geometry, ident
from model_binding import ROOT, library

F, I, U, B = C.c_float, C.c_int32, C.c_uint32, C.c_uint8
M, V, XY = F * 16, F * 3, I * 2


class Pick(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('kind', C.POINTER(I)),
                ('column', C.POINTER(I)), ('targets', C.POINTER(XY)),
                ('alternate', C.POINTER(I)), ('ring', C.POINTER(UiSprite)),
                ('geometry', C.POINTER(Geometry)), ('alternate_world', C.POINTER(F))]


class Menu(C.Structure):
    _fields_ = [('width', U), ('height', U), ('scale', F), ('menu_width', F)]


def bind(lib):
    lib.bk_ending_secondary_pick.argtypes = [C.POINTER(Pick), C.POINTER(F), I,
                                             C.POINTER(I), C.c_void_p]
    lib.bk_ending_secondary_menu_zone.argtypes = [C.POINTER(Menu), C.POINTER(I),
                                                  C.POINTER(I), C.c_void_p]
    lib.bk_ending_secondary_menu.argtypes = [C.POINTER(Menu), I, I, I,
                                             C.POINTER(XY), C.POINTER(I),
                                             C.POINTER(I), C.POINTER(XY), C.c_void_p]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--report', type=Path,
                        default=ROOT / 'local/original-ending-secondary-ui-oracle.json')
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    n, lib = Native(exe), library()
    u = n.u
    bind(lib)
    bind_ui(lib)
    rng = random.Random(0x47c334)
    error = C.create_string_buffer(256)

    def wi(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def wf(address, value):
        u.mem_write(address, struct.pack('<f', value))

    def ri(address):
        return struct.unpack('<i', u.mem_read(address, 4))[0]

    def rf(address):
        return struct.unpack('<f', u.mem_read(address, 4))[0]

    def setup_menu(g):
        wi(0x709890, g.width)
        wi(0x709888, g.height)
        wf(0x721ad0, g.scale)
        wf(0x7389e8, g.menu_width)

    zones = {i: 0 for i in range(-1, 9)}
    for case in range(7200):
        width, height = rng.choice([(320, 240), (640, 480), (800, 600),
                                    (960, 720), (1001, 750), (1280, 960),
                                    (1920, 1440), (16384, 12288)])
        g = Menu(width, height, rng.choice([0, width / 1280, .1, .7, 1, 2]),
                 rng.choice([0, .5, 1, 63.9, 96, 150.25, 256]))
        radius = F(400 * g.scale).value + int(g.menu_width / 2)
        point = XY(rng.choice([-1, 0, 1, width, width + 1, width // 2,
                                int(radius), int(width - radius), rng.randrange(width)]),
                   rng.choice([-1, 0, 1, height, height + 1, height // 2,
                                int(radius), int(height - radius), rng.randrange(height)]))
        setup_menu(g)
        n.call(0x47cb41, bytes(point))
        expected = I(u.reg_read(UC_X86_REG_EAX)).value
        result = I(99)
        assert lib.bk_ending_secondary_menu_zone(C.byref(g), point,
                                                 C.byref(result), error), error.value
        assert result.value == expected, (case, tuple(point), bytes(g), result.value, expected)
        zones[result.value] += 1
    assert all(zones.values()), zones

    menu_cases = corrections = outside = untouched = 0
    menu_zones = set()
    actual_widths = []
    targets, alternate = (XY * 39)(), XY()
    for width, height in [(320, 240), (640, 480), (960, 720), (1001, 750),
                           (1280, 960), (1920, 1440)]:
        ui, flags, gauge = Ui(), (B * 6)(), F()
        assert lib.bk_ending_ui_initialize(C.byref(ui), width, flags,
                                           C.byref(gauge), error), error.value
        menu_width = ui.sprites[51].rect[2]
        actual_widths.append([width, height, ui.sprites[50].rect[2], menu_width])
        g = Menu(width, height, width / 1280, menu_width)
        setup_menu(g)
        coordinates = [(x, y) for y in [-1, 0, height // 2, height, height + 1]
                       for x in [-1, 0, width // 2, width, width + 1]]
        coordinates += [(rng.randrange(width + 1), rng.randrange(height + 1))
                        for _ in range(35)]
        for event in [1, 2, 0, 7]:
            for index, point in enumerate(coordinates):
                selected = (index * 7) % 39
                automatic = [-1, 0, 1, 2, 3, 4][index % 6]
                for i in range(39):
                    targets[i][:] = [rng.randrange(width), rng.randrange(height)]
                targets[selected][:] = alternate[:] = point
                choices = (I * 3)(9, 10, 11)
                points = (XY * 3)(XY(-99, 91), XY(31, -41), XY(75, 12))
                before = bytes(points)
                wi(0x7220e0, event)
                wi(0x6afd40, automatic)
                wi(0x721ed0, selected)
                u.mem_write(0x721f90, bytes(targets))
                u.mem_write(0x6afd38, bytes(alternate))
                u.mem_write(0x7220e4, bytes(choices))
                u.mem_write(0x7220c8, bytes(points))
                attempts = [0]

                def observe(_u, _a, _size, _ctx):
                    attempts[0] += 1

                handle = u.hook_add(UC_HOOK_CODE, observe, begin=0x47d0f9, end=0x47d0f9)
                try:
                    n.call(0x47cd36, b'')
                finally:
                    u.hook_del(handle)
                assert lib.bk_ending_secondary_menu(C.byref(g), event, automatic,
                    selected, targets, alternate, choices, points, error), (menu_cases, error.value)
                wanted_choices = bytes(u.mem_read(0x7220e4, 12))
                wanted_points = bytes(u.mem_read(0x7220c8, 24))
                assert bytes(choices) == wanted_choices, ('choices', menu_cases, event, automatic)
                assert bytes(points) == wanted_points, ('menu', menu_cases, point,
                    event, list(map(tuple, points)), struct.unpack('<6i', wanted_points))
                zone = I()
                assert lib.bk_ending_secondary_menu_zone(C.byref(g), XY(*point),
                                                         C.byref(zone), error)
                if event in (1, 2):
                    menu_zones.add((event, zone.value))
                if attempts[0]:
                    assert attempts[0] >= 3
                    corrections += attempts[0] - 3
                    half = float(menu_width) / 2
                    for p in points:
                        assert p[0] - half > 0 and p[1] - half > 0
                        assert F(p[0] + half).value < width and F(p[1] + half).value < height
                else:
                    assert bytes(points) == before
                    outside += zone.value == -1 and event in (1, 2)
                    untouched += event not in (1, 2)
                menu_cases += 1
    assert menu_zones == {(event, zone) for event in (1, 2) for zone in range(-1, 9)}

    impossible = 0
    for g in [Menu(64, 64, 1, 32), Menu(64, 64, 0, 64)]:
        setup_menu(g)
        targets[0][:] = [32, 32]
        choices = (I * 3)(9, 10, 11)
        points = (XY * 3)(XY(-99, 91), XY(31, -41), XY(75, 12))
        wi(0x7220e0, 1)
        wi(0x6afd40, 0)
        wi(0x721ed0, 0)
        u.mem_write(0x721f90, bytes(targets))
        u.mem_write(0x7220e4, bytes(choices))
        u.mem_write(0x7220c8, bytes(points))
        attempts = [0]

        def bounded(_u, _a, _size, _ctx):
            attempts[0] += 1
            if attempts[0] == 4098:
                _u.emu_stop()

        handle = u.hook_add(UC_HOOK_CODE, bounded, begin=0x47d0ec, end=0x47d0ec)
        try:
            try:
                n.call(0x47cd36, b'')
            except AssertionError:
                assert attempts[0] == 4098 and u.reg_read(UC_X86_REG_EIP) == 0x47d0ec
            else:
                raise AssertionError('impossible original menu unexpectedly returned')
        finally:
            u.hook_del(handle)
        assert not lib.bk_ending_secondary_menu(C.byref(g), 1, 0, 0, targets,
                                                alternate, choices, points, error)
        assert b'4096' in error.value, error.value
        assert bytes(points) == bytes(u.mem_read(0x7220c8, 24))
        assert bytes(choices) == bytes(u.mem_read(0x7220e4, 12))
        impossible += 1

    world, present = (M * 39)(), (B * 39)()
    nodes = [0x3001000 + i * 0x200 for i in range(39)]
    results = [0, 0, 0]
    zero_w = zero_distance = missing = ties = 0
    for case in range(4800):
        view, projection, screen = ident(), ident(), ident()
        view[12:15] = [rng.uniform(-30, 30) for _ in range(3)]
        projection[0], projection[5] = rng.uniform(.5, 2), rng.uniform(.5, 2)
        projection[11] = rng.choice([0, .001, -.001])
        screen[0], screen[5], screen[12], screen[13] = 20, -20, 320, 240
        camera = V(5, 9, 100)
        pointer = (F * 2)(rng.uniform(-100, 800), rng.uniform(-100, 600))
        preferred = rng.choice([0, 4, 6, 38, -1, 39, 100])
        other = ident()
        other[12:15] = [rng.uniform(-150, 150) for _ in range(3)]
        has_other = case % 3 != 0
        for i in range(39):
            world[i] = ident()
            world[i][12:15] = [rng.uniform(-150, 150) for _ in range(3)]
            present[i] = rng.randrange(7) != 0
            targets[i][:] = [rng.randrange(-1000, 1000), rng.randrange(-1000, 1000)]
        if case % 7 == 0:
            projection[11] = 0
            for i in range(39):
                world[i][15] = 0
            other[15] = 0
            zero_w += 1
        if case % 11 == 0:
            world[0][12:15] = camera[:]
            zero_distance += bool(present[0])
        if case % 5 == 0:
            view, projection, screen = ident(), ident(), ident()
            camera[:], pointer[:] = [0, 0, 100], [rng.choice([0, 1, 10, 50]), 0]
            for i in range(39):
                world[i] = ident()
                world[i][12] = rng.choice([0, 0, 5, 10])
                world[i][14] = rng.choice([0, 50, 90])
                present[i] = 1
            other = ident()
            other[12], other[14] = rng.choice([0, 1, 50]), rng.choice([0, 50, 100])
            ties += 1
        if case % 19 == 0:
            present[:] = [0] * 39
            missing += 1
        frame, kind, column = Frame(), I(91), I(92)
        frame.camera_cached, frame.camera_event = 99, 98
        alternate = XY(-741, 811)
        ring = UiSprite()
        ring.rect[:] = [91, 73, rng.choice([1, 32, 96, 300]), 100]
        ring.transform.scale[:], ring.transform.pivot[:] = [1.25, .7], [.3, .9]
        ring.rgb = 99
        for i, address in enumerate(nodes):
            wi(0x721ef4 + i * 4, address if present[i] else 0)
            u.mem_write(address + 0xc0, bytes(world[i]))
        wi(0x719b40, 0x3008000 if has_other else 0)
        u.mem_write(0x30080c0, bytes(other))
        wi(0x721ed0, frame.camera_cached)
        wi(0x7220e0, frame.camera_event)
        wi(0x721e10, kind.value)
        wi(0x7220f0, column.value)
        u.mem_write(0x71b40c, bytes(camera))
        u.mem_write(0x642fa8, bytes(view))
        u.mem_write(0x642830, bytes(projection))
        u.mem_write(0x642af0, bytes(screen))
        u.mem_write(0x721f90, bytes(targets))
        u.mem_write(0x6afd38, bytes(alternate))
        u.mem_write(0x738770, bytes(0x16c))
        for address, value in zip([0x738884, 0x738888, 0x73887c, 0x738880,
                                    0x738894, 0x738898, 0x73888c, 0x738890],
                                   [*ring.rect, *ring.transform.scale, *ring.transform.pivot]):
            wf(address, value)
        n.call(0x47c334, bytes(pointer) + struct.pack('<i', preferred))
        expected_result = u.reg_read(UC_X86_REG_EAX)
        expected_fields = tuple(ri(a) for a in [0x721ed0, 0x7220e0, 0x721e10, 0x7220f0])
        expected_ring = UiSprite.from_buffer_copy(ring)
        expected_ring.rect[0:2] = [rf(0x738884), rf(0x738888)]
        expected_ring.transform.scale[:] = [rf(0x738894), rf(0x738898)]
        expected_ring.transform.pivot[:] = [rf(0x73888c), rf(0x738890)]
        geometry = Geometry(world, present, 39, camera, view, projection, screen, 0)
        bindings = Pick(C.pointer(frame), C.pointer(kind), C.pointer(column), targets,
                        alternate, C.pointer(ring), C.pointer(geometry), other if has_other else None)
        result = I(-1)
        assert lib.bk_ending_secondary_pick(C.byref(bindings), pointer, preferred,
                                             C.byref(result), error), (case, error.value)
        got = (frame.camera_cached, frame.camera_event, kind.value, column.value)
        assert got == expected_fields and result.value == expected_result, (
            'pick', case, got, expected_fields, result.value, expected_result)
        assert bytes(targets) == bytes(u.mem_read(0x721f90, 39 * 8)), ('targets', case)
        assert bytes(alternate) == bytes(u.mem_read(0x6afd38, 8)), ('alternate', case)
        assert bytes(ring) == bytes(expected_ring), ('ring', case,
            tuple(ring.rect), tuple(expected_ring.rect), tuple(ring.transform.scale),
            tuple(expected_ring.transform.scale))
        results[result.value] += 1
    assert all(results), results

    invalid = 0
    for g in [Menu(0, 720, 1, 96), Menu(960, 0, 1, 96), Menu(960, 720, math.nan, 96),
              Menu(960, 720, 1, math.inf), Menu(960, 720, -1, 96)]:
        result = I(123)
        assert not lib.bk_ending_secondary_menu_zone(C.byref(g), XY(1, 2),
                                                     C.byref(result), error)
        assert result.value == 123
        invalid += 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  zone_calls=sum(zones.values()), zones=zones, menu_calls=menu_cases,
                  menu_zones=sorted(menu_zones), native_angle_increments=corrections,
                  outside_menus=outside, unchanged_unknown_events=untouched,
                  actual_sprite_widths=actual_widths, impossible_bounded_failures=impossible,
                  pick_calls=sum(results), pick_results=results, zero_w_cases=zero_w,
                  coincident_target_cases=zero_distance, all_targets_absent_cases=missing,
                  overlapping_target_cases=ties, invalid_inputs=invalid,
                  max_error=0, substitutions=[], scope=__doc__)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
