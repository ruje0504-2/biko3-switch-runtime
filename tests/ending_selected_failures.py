"""Selected-stage CPU ownership and failure contracts on real Japanese assets.

Missing/corrupt inputs use independent stores and temporary read-only overlays;
the retail files are never modified. Check uncommitted camera/presets/RNG,
background retry, and terminal ownership after an invalid forest append.
These are portable API/lifetime checks, not original executable comparisons,
GPU checks, leak detection, or a playable selected-stage validation.
"""
from __future__ import annotations
import argparse
import ctypes as C
import faulthandler
import hashlib
import json
from pathlib import Path
import tempfile

from bk3_assets import Archive
from clip_binding import State as ClipState
from ending_selected_binding import bind, Load, Config, CameraState, Presets
from model_binding import ROOT
from playback_binding import library

PACKS = ['bk3_10', 'bk3_03', 'bk3_04', 'fambom']
IDENTITY = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]


def main():
    faulthandler.enable()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('--output', type=Path,
                        default=ROOT / 'local/ending-selected-failures.json')
    args = parser.parse_args()
    lib = library()
    bind(lib)
    pointer = C.c_void_p
    lib.bk_resources_mount_directory.argtypes = [pointer, C.c_char_p,
        C.c_char_p, C.c_size_t, pointer]
    lib.bk_resources_mount_directory.restype = C.c_int
    lib.bk_actor_forest_append.argtypes = [pointer, pointer,
        C.POINTER(C.c_uint32), pointer]
    lib.bk_actor_forest_append.restype = C.c_int
    error = C.create_string_buffer(256)
    records, inputs = [], {}
    digest = hashlib.sha256()

    def read(pack, name):
        archive = Archive(args.data / (pack + '.pp'))
        entry = next(entry for entry in archive.entries
                     if entry.name.lower() == name.lower())
        data = archive.read(entry)
        inputs[pack + '/' + entry.name] = hashlib.sha256(data).hexdigest()
        return data

    def store(omit=()):
        resource = lib.bk_resources_create(error)
        assert resource, error.value
        try:
            for pack in PACKS:
                if pack not in omit:
                    assert lib.bk_resources_mount(resource, pack.encode(),
                        str(args.data / (pack + '.pp')).encode(), error), error.value
            return resource
        except BaseException:
            lib.bk_resources_destroy(resource)
            raise

    def state():
        camera, presets = CameraState(), Presets()
        camera.pose.world[:] = IDENTITY
        camera.matrix[:] = IDENTITY
        camera.pose.position[:] = [7, 8, 9]
        camera.focus[:] = [5, -4, 3]
        camera.yaw, camera.pitch, camera.radius = 41, 9, 30
        camera.height, camera.fov = 3, .75
        C.memset(C.byref(presets), 0xa5, C.sizeof(presets))
        return camera, presets, C.c_uint32(0x4d1025)

    def create(resource, load):
        camera, presets, rng = state()
        before = bytes(camera), bytes(presets), bytes(rng)
        clocks = (C.c_uint32 * 4)(100, 1000, 1015, 1031)
        owner = lib.bk_ending_selected_assets_create(resource, C.byref(load),
            clocks, C.byref(rng), C.byref(camera), C.byref(presets), error)
        if not owner:
            assert (bytes(camera), bytes(presets), bytes(rng)) == before
        return owner

    def record(label, message=None):
        item = dict(case=label, passed=True)
        if message is not None:
            assert message
            item['error'] = message.decode(errors='replace')
        records.append(item)
        digest.update(label.encode())

    def clip(pose):
        result = ClipState()
        assert lib.bk_actor_pose_state(pose, C.byref(result))
        return bytes(result)

    # Missing inputs exercise partial construction at distinct resource stages.
    for pack in ['bk3_10', 'fambom', 'bk3_04', 'bk3_03']:
        resource = store([pack])
        owner = None
        try:
            group = 1 if pack == 'bk3_03' else 0
            load = Load(group, 0, 0, -1, f'\\h{group+1:02}_03.xan'.encode(), None)
            owner = create(resource, load)
            assert not owner, ('missing pack unexpectedly loaded', pack)
            record('missing-' + pack, error.value)
        finally:
            lib.bk_ending_selected_assets_destroy(owner)
            lib.bk_resources_destroy(resource)

    # Independent overlays make corrupt entries stop at their own layer.
    # Truncate actual resources instead of altering the original archives.
    primary_xan = read('bk3_10', 'h01_03.xan')
    primary_model = primary_xan[:256].split(b'\0')[0].decode()
    for pack, name in [('bk3_10', 'h01_03.xan'), ('bk3_10', primary_model),
                       ('fambom', 'h01_03.fam'), ('bk3_04', 'cam00_00.xan'),
                       ('bk3_03', 'm02_90.xan')]:
        raw = read(pack, name)
        parent = ROOT / 'build/validation'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='selected-corrupt-', dir=parent) as folder:
            Path(folder, name).write_bytes(raw[:16])
            resource = store()
            owner = None
            try:
                assert lib.bk_resources_mount_directory(resource, pack.encode(),
                    str(folder).encode(), 64 * 1024 * 1024, error), error.value
                group = 1 if pack == 'bk3_03' else 0
                owner = create(resource, Load(group, 0, 0, -1,
                    f'\\h{group+1:02}_03.xan'.encode(), None))
                assert not owner, ('corrupt resource unexpectedly loaded', pack, name)
                message = error.value.replace(str(folder).encode(), b'<overlay>')
                record('corrupt-' + pack + '/' + name, message)
            finally:
                lib.bk_ending_selected_assets_destroy(owner)
                lib.bk_resources_destroy(resource)

    resource = store()
    try:
        for label, load in [
            ('group', Load(5, 0, 0, -1, b'\\h01_03.xan', None)),
            ('variant', Load(0, 2, 0, -1, b'\\h01_03.xan', None)),
            ('selection', Load(0, 0, 3, -1, b'\\h01_03.xan', None)),
            ('null-path', Load(0, 0, 0, -1, None, None)),
            ('empty-path', Load(0, 0, 0, -1, b'', None)),
            ('separator', Load(0, 0, 0, -1, b'\\', None)),
            ('nested-path', Load(0, 0, 0, -1, b'folder/h01_03.xan', None)),
            ('parent-path', Load(0, 0, 0, -1, b'..\\h01_03.xan', None)),
            ('long-path', Load(0, 0, 0, -1, b'a' * 260, None)),
        ]:
            owner = create(resource, load)
            try:
                assert not owner, label
                record('invalid-' + label, error.value)
            finally:
                lib.bk_ending_selected_assets_destroy(owner)
        foreign = lib.bk_ending_background_assets_create(resource, b'm02_90.xan', error)
        assert foreign, error.value
        owner = None
        try:
            pose = lib.bk_ending_background_assets_data(foreign).contents.pose
            before = clip(pose)
            owner = create(resource, Load(0, 0, 0, -1, b'\\h01_03.xan', foreign))
            assert not owner and clip(pose) == before
            record('foreign-background', error.value)
        finally:
            lib.bk_ending_selected_assets_destroy(owner)
            lib.bk_ending_background_assets_destroy(foreign)
    finally:
        lib.bk_resources_destroy(resource)

    # An absent outer background can be retried without reloading the primary.
    resource = store(['bk3_03'])
    owner = extra = None
    try:
        owner = create(resource, Load(0, 0, 0, -1, b'\\h01_03.xan', None))
        assert owner, error.value
        held = [C.string_at(lib.bk_ending_selected_assets_target(owner, i), 12)
                for i in range(3)]
        pose = lib.bk_ending_selected_assets_pose(owner, 0)
        before = clip(pose)
        assert not lib.bk_ending_selected_assets_load_background(owner, resource, error)
        assert lib.bk_ending_selected_assets_pose(owner, 0) == pose
        assert clip(pose) == before
        record('missing-outer-background-retry', error.value)
        assert lib.bk_resources_mount(resource, b'bk3_03',
            str(args.data / 'bk3_03.pp').encode(), error), error.value
        assert lib.bk_ending_selected_assets_load_background(owner, resource, error), error.value
        assert held == [C.string_at(lib.bk_ending_selected_assets_target(owner, i), 12)
                        for i in range(3)]
        assert clip(pose) == before
        background = lib.bk_ending_selected_assets_background(owner)
        assert lib.bk_ending_selected_assets_load_background(owner, resource, error)
        assert lib.bk_ending_selected_assets_background(owner) == background
        record('outer-background-retry-succeeded')
        lib.bk_ending_selected_assets_destroy(owner)
        owner = None

        # Invalid external topology is a deliberate fault injection. The
        # forest borrows the extra pose until the failing owner is destroyed.
        owner = create(resource, Load(0, 0, 0, -1, b'\\h01_03.xan', None))
        assert owner, error.value
        extra = lib.bk_ending_background_assets_create(resource, b'm01_90.xan', error)
        assert extra, error.value
        extra_pose = lib.bk_ending_background_assets_data(extra).contents.pose
        index = C.c_uint32()
        assert lib.bk_actor_forest_append(lib.bk_ending_selected_assets_forest(owner),
            extra_pose, C.byref(index), error), error.value
        assert index.value == 3
        assert not lib.bk_ending_selected_assets_load_background(owner, resource, error)
        record('terminal-outer-background-registry', error.value)
        assert not lib.bk_ending_selected_assets_forest(owner)
        assert not lib.bk_ending_selected_assets_pose(owner, 0)
        assert not lib.bk_ending_selected_assets_load_background(owner, resource, error)
        assert not lib.bk_ending_selected_assets_advance(owner, 0, 0, error)
        assert not lib.bk_ending_selected_assets_advance_plain(owner, 0, 0, error)
        record('terminal-owner-rejects-reuse')
    finally:
        lib.bk_ending_selected_assets_destroy(owner)
        lib.bk_ending_background_assets_destroy(extra)
        lib.bk_resources_destroy(resource)
    result = dict(passed=True, cases=records, input_sha256=inputs,
                  state_sha256=digest.hexdigest(), scope=__doc__,
                  original_comparison=False, leak_detection=False,
                  full_loader=False, gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS selected failure contracts', len(records), digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
