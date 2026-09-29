"""Original4949DC selected menu with actual projection, heading and point search.

Runs42D4B6,4AEBA9,47CB41,47D0E6,495125 and4951B5 unchanged. Compare all39
projected slots, choices and three output centers. Hooks only observe native
coverage or stop the original impossible search at the portable guard.
This proves CPU layout for supplied old worlds and logical viewport inputs,
not a rendered scene, complete selected controller or Switch acceptance.
"""
from __future__ import annotations
import argparse
import ctypes as C
import faulthandler
import hashlib
import json
from pathlib import Path
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP
from original_prop_route_oracle import Native
from original_ending_secondary_ui_oracle import Menu, Geometry, ident, M, V, XY, F, I, U, B
from original_ending_ui_oracle import Ui, bind as bind_ui
from model_binding import ROOT, library

EXE_SHA256 = 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'


def main():
    faulthandler.enable()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-ending-selected-ui.json')
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    assert hashlib.sha256(exe).hexdigest() == EXE_SHA256
    native, lib = Native(exe), library()
    u, error = native.u, C.create_string_buffer(256)
    bind_ui(lib)
    lib.bk_ending_selected_menu.argtypes = [C.POINTER(Menu), C.POINTER(Geometry), I, I, I,
                                          C.POINTER(XY), C.POINTER(I), C.POINTER(I),
                                          C.POINTER(XY), C.c_void_p]
    lib.bk_ending_selected_menu.restype = I
    rng, digest = random.Random(0x4949dc), hashlib.sha256()
    counters = dict(cases=0, projected=0, corrected=0, outside=0, untouched=0,
                    bearing_fit=0, bearing_fallback=0, equal_bearing_points=0,
                    center_order=0, impossible=0, rejected=0)
    zones, selections, viewport_cases = set(), set(), []
    attempts, observed_zones, fit_results, projections, bounded = [], [], [], [], [False]
    slots = []

    def wi(address, value): u.mem_write(address, struct.pack('<I', value & 0xffffffff))
    def wf(address, value): u.mem_write(address, struct.pack('<f', value))
    def observe(_u, address, _size, _context):
        if address == 0x42d4b6: projections.append(1)
        elif address in [0x494abc, 0x494d44]:
            observed_zones.append(I(u.reg_read(UC_X86_REG_EAX)).value)
        elif address == 0x4952c7: fit_results.append(u.reg_read(UC_X86_REG_EAX))
        elif address == 0x47d0e6:
            from unicorn.x86_const import UC_X86_REG_ESP
            sp = u.reg_read(UC_X86_REG_ESP)
            slots.append(struct.unpack('<I', u.mem_read(sp + 8, 4))[0])
        elif address == 0x47d0ec:
            attempts.append(1)
            if bounded[0] and len(attempts) == 4098: u.emu_stop()

    for address in [0x42d4b6, 0x494abc, 0x494d44, 0x4952c7, 0x47d0e6, 0x47d0ec]:
        u.hook_add(UC_HOOK_CODE, observe, begin=address, end=address)
    worlds, present = (M * 39)(), (B * 39)(*[1] * 39)
    camera, view, projection, screen = V(0, 0, 100), ident(), ident(), ident()
    geometry = Geometry(worlds, present, 39, camera, view, projection, screen, 0)
    targets, alternate = (XY * 39)(), XY()

    def setup(g, event, selection, selected, choices, points):
        wi(0x709890, g.width); wi(0x709888, g.height)
        wf(0x721ad0, g.scale); wf(0x7389e8, g.menu_width)
        wi(0x7220e0, event); wi(0x7220f4, selection); wi(0x721ed0, selected)
        wi(0x721f04, 0x3001000)
        u.mem_write(0x30010c0, bytes(worlds[4]))
        u.mem_write(0x642fa8, bytes(view))
        u.mem_write(0x642830, bytes(projection))
        u.mem_write(0x642af0, bytes(screen))
        u.mem_write(0x721f90, bytes(targets))
        u.mem_write(0x6afd38, bytes(alternate))
        u.mem_write(0x7220e4, bytes(choices))
        u.mem_write(0x7220c8, bytes(points))
        attempts.clear(); observed_zones.clear(); fit_results.clear(); projections.clear(); slots.clear()

    for width, height in [(320, 240), (640, 480), (960, 720), (1001, 750),
                          (1280, 960), (1920, 1440)]:
        ui, flags, gauge = Ui(), (B * 6)(), F()
        assert lib.bk_ending_ui_initialize(C.byref(ui), width, flags, C.byref(gauge), error), error.value
        g = Menu(width, height, width / 1280, ui.sprites[51].rect[2])
        first = counters['cases']
        coordinates = [(x, y) for y in [-1, 0, height // 2, height, height + 1]
                       for x in [-1, 0, width // 2, width, width + 1]]
        coordinates += [(rng.randrange(width + 1), rng.randrange(height + 1)) for _ in range(35)]
        for event in [1, 2, 0, 7]:
            for selection in [-1, 0, 1, 2, 3]:
                for index, center in enumerate(coordinates):
                    selected = index * 7 % 39
                    for i in range(39):
                        worlds[i] = ident()
                        targets[i][:] = [rng.randrange(width), rng.randrange(height)]
                    targets[selected][:] = alternate[:] = center
                    worlds[4][12:15] = [rng.randrange(-width, width * 2),
                                       rng.randrange(-height, height * 2), 1]
                    view[:] = projection[:] = screen[:] = ident()
                    if index % 3 == 0:
                        view[12:15] = [5.25, -3.75, 1.5]
                        projection[0], projection[5], projection[11] = .75, 1.25, .015625
                        screen[0], screen[5], screen[12], screen[13] = 2, -2, width * .5, height * .5
                    if index % 11 == 0:
                        view[:] = projection[:] = screen[:] = ident()
                        worlds[4][12:14] = center
                        if event == 2: counters['equal_bearing_points'] += 1
                    choices = (I * 3)(91, 92, 93)
                    points = (XY * 3)(XY(-99, 91), XY(31, -41), XY(75, 12))
                    before = bytes(points)
                    setup(g, event, selection, selected, choices, points)
                    native.call(0x4949dc, b'')
                    assert lib.bk_ending_selected_menu(C.byref(g), C.byref(geometry), event,
                        selection, selected, targets, alternate, choices, points, error), (
                            counters['cases'], event, selection, center, error.value)
                    for name, actual, address in [('targets', targets, 0x721f90),
                                                  ('choices', choices, 0x7220e4),
                                                  ('points', points, 0x7220c8)]:
                        wanted = bytes(u.mem_read(address, C.sizeof(actual)))
                        assert bytes(actual) == wanted, (counters['cases'], name, event, selection,
                            center, bytes(actual).hex(), wanted.hex())
                        digest.update(wanted)
                    assert len(projections) == 1
                    counters['projected'] += 1
                    if observed_zones:
                        assert len(observed_zones) == 1
                        zone = observed_zones[0]
                        zones.add((event, zone))
                        counters['outside'] += zone == -1
                        if zone == 0 and event == 2:
                            assert slots == [1, 0, 2], slots
                            counters['center_order'] += 1
                    if event not in [1, 2]:
                        assert bytes(points) == before and not slots
                        counters['untouched'] += 1
                    if attempts:
                        assert len(attempts) >= 3
                        counters['corrected'] += len(attempts) - 3
                        half = float(g.menu_width) / 2
                        for p in points:
                            assert p[0] - half > 0 and p[1] - half > 0
                            assert F(p[0] + half).value < width and F(p[1] + half).value < height
                    for result in fit_results:
                        assert result in [0, 1]
                        counters['bearing_fit' if result else 'bearing_fallback'] += 1
                    counters['cases'] += 1
                    selections.add(selection)
        viewport_cases.append([width, height, g.menu_width, counters['cases'] - first])

    assert zones == {(event, zone) for event in [1, 2] for zone in range(-1, 9)}, zones
    assert counters['bearing_fit'] and counters['bearing_fallback'] and counters['center_order']
    for event in [1, 2]:
        for g in [Menu(64, 64, 1, 32), Menu(64, 64, 0, 64)]:
            view[:] = projection[:] = screen[:] = ident()
            worlds[4] = ident(); worlds[4][12:14] = [32, 32]
            targets[0][:] = alternate[:] = [32, 32]
            choices = (I * 3)(9, 10, 11)
            points = (XY * 3)(XY(-99, 91), XY(31, -41), XY(75, 12))
            setup(g, event, 0, 0, choices, points)
            bounded[0] = True
            try:
                native.call(0x4949dc, b'')
            except AssertionError:
                assert len(attempts) == 4098 and u.reg_read(UC_X86_REG_EIP) == 0x47d0ec
            else: raise AssertionError('original impossible search unexpectedly returned')
            finally: bounded[0] = False
            assert not lib.bk_ending_selected_menu(C.byref(g), C.byref(geometry), event,
                0, 0, targets, alternate, choices, points, error)
            assert b'4096' in error.value, error.value
            assert bytes(targets) == bytes(u.mem_read(0x721f90, C.sizeof(targets)))
            assert bytes(choices) == bytes(u.mem_read(0x7220e4, 12))
            assert bytes(points) == bytes(u.mem_read(0x7220c8, 24))
            counters['impossible'] += 1
    g = Menu(960, 720, .75, 72)
    present[4] = 0
    before = bytes(targets), bytes(choices), bytes(points)
    assert not lib.bk_ending_selected_menu(C.byref(g), C.byref(geometry), 0,
        0, 0, targets, alternate, choices, points, error)
    assert (bytes(targets), bytes(choices), bytes(points)) == before
    counters['rejected'] += 1
    present[4] = 1
    for selected in [-1, 39, 2147483647]:
        assert not lib.bk_ending_selected_menu(C.byref(g), C.byref(geometry), 1,
            0, selected, targets, alternate, choices, points, error)
        assert b'outside range' in error.value, error.value
        counters['rejected'] += 1
    for bad in [Menu(0, 720, .75, 72), Menu(960, 720, float('nan'), 72), Menu(960, 720, .75, -1)]:
        before = bytes(targets), bytes(choices), bytes(points)
        assert not lib.bk_ending_selected_menu(C.byref(bad), C.byref(geometry), 1,
            0, 0, targets, alternate, choices, points, error)
        assert (bytes(targets), bytes(choices), bytes(points)) == before
        counters['rejected'] += 1
    report = dict(passed=True, exe_sha256=EXE_SHA256, counters=counters,
                  zones=sorted(zones), selections=sorted(selections), viewports=viewport_cases,
                  state_sha256=digest.hexdigest(), max_error=0, scope=__doc__,
                  full_loader=False, gpu_or_device_validation=False)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS selected UI', counters['cases'], 'cases', counters['corrected'],
          'angle corrections', counters['bearing_fit'], 'bearing fits',
          counters['bearing_fallback'], 'fallbacks', counters['impossible'],
          'bounded searches', counters['rejected'], 'rejections', digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
