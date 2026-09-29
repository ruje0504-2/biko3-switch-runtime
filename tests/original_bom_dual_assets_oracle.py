"""Actual three-actor4a52bc, twice around4a65fc's3+2 binding composition.

Uses decoded h03_10/h03_30/h03_31 and real VIX. Native name lookup, node
alignment, global traversal, nearest-vertex mapping,40168c/4021a1 ANIM/MORP
and hiding run unmodified. Native heap/file/mesh-lock/matrix-stack lifetime
are boundaries. Initial placed matrices are explicit shared fixtures; this
does not exercise4d2320 face/camera/UI, rendering, or a natural ending.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import struct
from unicorn import UcError, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX

from original_bom_assets_oracle import Native as Base, Actor, Binding, Mesh, bind, NONE, F16, I
from original_bom_oracle import Native as Parser, Config
from original_bom_append_oracle import append_native
from original_fixed_actor_oracle import Visibility
from model_binding import Model, ROOT, decode
from playback_binding import library
from clip_binding import State
from bk3_assets import Archive


class DualBinding(C.Structure):
    _fields_ = [('nodes', Binding), ('child_actor', C.c_uint32), ('secondary_aux_actor', C.c_uint32)]


def bindings(lib):
    bind(lib)
    fp = C.POINTER(C.c_float)
    lib.bk_model_submesh_name.argtypes = [C.POINTER(Model), C.c_uint32, C.c_char_p, C.c_void_p, C.c_void_p]
    for name, args, result in [
        ('create', [C.c_void_p, C.c_char_p, C.POINTER(Config), C.POINTER(Actor), C.c_void_p, C.c_void_p], C.c_void_p),
        ('append', [C.c_void_p, C.c_void_p, C.c_char_p, C.POINTER(Config), C.c_void_p, C.c_void_p], C.c_int),
        ('destroy', [C.c_void_p], None), ('count', [C.c_void_p], C.c_uint32),
        ('mode', [C.c_void_p], C.c_uint32),
        ('binding', [C.c_void_p, C.c_uint32], C.POINTER(DualBinding)),
        ('mesh', [C.c_void_p, C.c_uint32], C.POINTER(Mesh)),
        ('mapping', [C.c_void_p, C.c_uint32, C.POINTER(C.c_int32), C.POINTER(C.POINTER(C.c_uint32)), C.POINTER(C.c_size_t)], C.c_int),
        ('morph', [C.c_void_p, C.c_uint], C.c_void_p),
        ('materials', [C.c_void_p, C.c_uint], C.c_void_p),
        ('advance_frame', [C.c_void_p, C.c_uint, C.c_void_p], C.c_int),
        ('advance', [C.c_void_p, C.c_uint, C.c_float, C.c_void_p], C.c_int),
    ]:
        fn = getattr(lib, 'bk_bom_dual_assets_' + name)
        fn.argtypes, fn.restype = args, result
    lib.bk_actor_pose_request.argtypes = [C.c_void_p, C.c_uint, C.c_void_p]


class Native(Base):
    def __init__(self, exe, files):
        super().__init__(exe, files)
        # Same engine allocator/free boundary as the existing actor-forest
        # oracle. Packaged4a52bc uses it for VIX storage before real mapping.
        for address in [0x42d217, 0x42d270]:
            self.uc.hook_add(UC_HOOK_CODE, self.engine_heap, begin=address, end=address)

    def engine_heap(self, u, address, size, _):
        sp = u.reg_read(UC_X86_REG_ESP)
        result = 0
        if address == 0x42d217:
            count = self.read(sp + 4)
            assert count <= 0x2000000, ('native allocation bound', count)
            result = self.alloc(bytes(count))
        u.reg_write(UC_X86_REG_EAX, result)
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, self.read(sp))

    def setup_three(self, lib, models, poses, names, rawmodels, xans):
        assert len(models) == 3
        self.setup_actors(lib, models, poses, names, rawmodels, xans, linked=True)

    def setup_actors(self, lib, models, poses, names, rawmodels, xans, linked=False,
                     primary_animations=False):
        assert 1 <= len(models) <= 6 and all(len(items) == len(models)
            for items in [poses, names, rawmodels, xans])
        actor_count = len(models)
        self.frame_bases, self.roots, self.clips, self.models = [], [], [], []
        self.aux_effects = {}
        for actor, model in enumerate(models):
            base = self.alloc(bytes(model.frame_count * 0x400))
            root = next(i for i in range(model.frame_count) if model.frames[i].parent_index == NONE)
            self.frame_bases.append(base)
            self.roots.append(root)
            children = {i: [] for i in range(model.frame_count)}
            for i in range(model.frame_count):
                frame = model.frames[i]
                ptr = base + i * 0x400
                self.uc.mem_write(ptr + 8, frame.name + b'\0')
                for getter, offset in [('local', 0x80), ('frame', 0xc0), ('parent_world', 0x100)]:
                    self.uc.mem_write(ptr + offset, C.string_at(getattr(lib, 'bk_actor_pose_' + getter)(poses[actor], i), 64))
                if frame.parent_index != NONE:
                    children[frame.parent_index].append(i)
            links = self.alloc(bytes(model.frame_count * 16))
            for parent, items in children.items():
                ptr = base + parent * 0x400
                self.word(ptr + 0x238, len(items))
                self.word(ptr + 0x230, links + items[0] * 16 if items else 0)
                self.word(ptr + 0x234, links + items[-1] * 16 if items else 0)
                for j, i in enumerate(items):
                    self.word(links + i * 16, base + i * 0x400)
                    self.word(links + i * 16 + 4, links + items[j - 1] * 16 if j else 0)
                    self.word(links + i * 16 + 8, links + items[j + 1] * 16 if j + 1 < len(items) else 0)
                    self.word(base + i * 0x400 + 0x22c, ptr)
            clip = self.alloc(xans[actor][512:])
            native_model = self.alloc(bytes(0x200))
            self.clips.append(clip)
            self.models.append(native_model)
            self.word(clip + 0x160, native_model)
            self.word(clip + 0x18c, 0)
            self.word(native_model + 0x14, base + root * 0x400)
            if actor == 0 and not primary_animations:
                continue  # Binder never samples the primary actor.
            tracks = []
            chunk = next((c for c in model.chunks[:model.chunk_count] if c.tag == b'ANIM'), None)
            raw = rawmodels[actor][chunk.offset:chunk.offset + chunk.size] if chunk else b''
            if chunk and len(raw) != chunk.size:
                # Text-X pose decoding appends its canonical ANIM input after
                # the original text. It is an explicit decoded resource, not
                # a binary chunk address inside the unconverted archive bytes.
                assert rawmodels[actor].startswith(b'xof 0302txt 0032')
                source = C.cast(model.source, C.c_void_p).value
                raw = C.string_at(source + chunk.offset, chunk.size)
            count = struct.unpack_from('<I', raw, 68)[0] if chunk else 0
            offset = 72 if chunk else 0
            for _ in range(count):
                ident, _, _, _, _, keys_count = struct.unpack_from('<6I', raw, offset)
                offset += 24
                keys = bytearray(raw[offset:offset + keys_count * 220])
                offset += keys_count * 220
                for j in range(keys_count):
                    struct.pack_into('<ii', keys, j * 220 + 212, j - 1, j + 1 if j + 1 < keys_count else -1)
                ptr = self.alloc(bytes(0x800))
                key_ptr = self.alloc(bytes(keys))
                frame = next(i for i in range(model.frame_count) if model.frames[i].id == ident)
                for off, value in [(0x74, base + frame * 0x400), (0x7c, keys_count), (0x80, keys_count),
                                   (0x84, key_ptr), (0x88, keys_count - 1), (0x8c, 1)]:
                    self.word(ptr + off, value)
                self.uc.mem_write(ptr + 0xec, b'\xff' * 401 * 4)
                tracks.append(ptr)
            assert offset == len(raw)
            if tracks:
                group = self.alloc(bytes(0x100))
                array = self.alloc(struct.pack('<' + 'I' * len(tracks), *tracks))
                self.word(group + 0x78, len(tracks))
                self.word(group + 0x7c, array)
                self.word(native_model + 0x148, group)
            if any(c.tag == b'MORP' for c in model.chunks[:model.chunk_count]):
                morph = lib.bk_model_morph_create(C.pointer(model), C.create_string_buffer(256))
                assert morph
                self.model, self.mesh_ids = native_model, {}
                try:
                    self.morph(lib, model, morph)
                finally:
                    lib.bk_model_morph_destroy(morph)
                self.aux_effects[actor] = (dict(self.mesh_ids), self.group_morph)
        self.uc.mem_write(self.context, bytes(0x400))
        for off in [0x80, 0xc0, 0x100]:
            self.vector(self.context + off, I)
        links = self.alloc(bytes(actor_count * 16))
        self.word(self.context + 0x238, actor_count if linked else 0)
        self.word(self.context + 0x230, links if linked else 0)
        self.word(self.context + 0x234, links + (actor_count - 1) * 16 if linked else 0)
        for actor in range(actor_count if linked else 0):
            ptr = self.frame_bases[actor] + self.roots[actor] * 0x400
            self.word(links + actor * 16, ptr)
            self.word(links + actor * 16 + 4, links + (actor - 1) * 16 if actor else 0)
            self.word(links + actor * 16 + 8, links + (actor + 1) * 16 if actor + 1 < actor_count else 0)
            self.word(ptr + 0x22c, self.context)
        self.word(0x645600, self.context)
        self.word(0x63f0fc, 1)
        self.word(0x63f104, 1)
        self.word(0x63f108, self.matrix_stack)
        self.uc.mem_write(self.matrix_stack, struct.pack('<5I', self.vt, 1024, self.matrices, 0, 1))
        self.vector(self.matrices, I)
        self.word(0x6455f4, 0)
        self.registry = {}
        table = self.alloc(bytes(sum(m.submesh_count for m in models) * 0x88))
        entry = 0
        for actor, model in enumerate(models):
            for index in range(model.submesh_count):
                mesh = model.submeshes[index]
                frame = next(i for i in range(model.frame_count) if model.frames[i].mesh_index == mesh.mesh_index)
                existing = self.aux_effects.get(actor, ({}, 0))[0]
                ptr = existing.get(index)
                if ptr is None:
                    ptr = self.alloc(bytes(0x120))
                    vertices = self.alloc(C.string_at(mesh.vertices, mesh.vertex_count * 60))
                    self.meshes[ptr] = (mesh.vertex_count, vertices)
                self.word(ptr + 0x100, self.frame_bases[actor] + frame * 0x400)
                self.registry[actor, index] = ptr
                name = C.create_string_buffer(256)
                assert lib.bk_model_submesh_name(C.pointer(model), index, names[actor], name, C.create_string_buffer(256))
                self.uc.mem_write(table + entry * 0x88, name.value + b'\0')
                self.word(table + entry * 0x88 + 0x80, ptr)
                self.word(table + entry * 0x88 + 0x84, 0x3ea)
                entry += 1
        self.word(0x645614, table)
        self.word(0x645618, entry)
        self.config = self.alloc(bytes(0x420c))
        self.output = self.alloc(bytes(0x144))
        self.data_path = self.alloc(b'\\Data\0')

    def initialize_combined(self, parser):
        self.uc.mem_write(self.config, bytes(parser.u.mem_read(parser.config, 0x420c)))
        self.events = []
        try:
            self.call(0x4a52bc, struct.pack('<6I', self.config, self.output, *self.clips[:3], self.data_path))
        except UcError:
            pc = self.uc.reg_read(UC_X86_REG_EIP)
            sp = self.uc.reg_read(UC_X86_REG_ESP)
            words = struct.unpack('<12I', self.uc.mem_read(sp, 48))
            print('Native binder failure', hex(pc), 'stack', [hex(word) for word in words], flush=True)
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--cases', type=int, default=8)
    parser.add_argument('--steps', type=int, default=60)
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-bom-dual-assets-oracle.json')
    args = parser.parse_args()
    assert args.cases > 0 and args.steps >= 0
    exe, lib = args.exe.read_bytes(), library()
    bindings(lib)
    err = C.create_string_buffer(256)
    arc, bom = Archive(args.data / 'bk3_11.pp'), Archive(args.data / 'fambom.pp')
    files = {entry.name.lower().encode(): arc.read(entry) for entry in arc.entries if entry.name.lower().endswith('.vix')}
    rawconfigs = [bom.read(next(e for e in bom.entries if e.name == n)) for n in ['jouhansin.bom', 'kahansin.bom']]
    configs = [Config(), Config()]
    for config, raw in zip(configs, rawconfigs):
        assert lib.bk_bom_decode(raw, len(raw), C.byref(config), err)
    assert [c.count for c in configs] == [3, 2]
    names = [b'h03_10.x', b'h03_30.x', b'h03_31.x']
    rawmodels = [arc.read(next(e for e in arc.entries if e.name.encode() == name)) for name in names]
    xans = [arc.read(next(e for e in arc.entries if e.name.encode() == name + b'an')) for name in names]
    matrices = vertices = maps = clocks = frames = failures = 0
    worst = 0.0
    digest = hashlib.sha256()
    store = lib.bk_resources_create(err)
    assert store and lib.bk_resources_mount(store, b'bk3_11', str(args.data / 'bk3_11.pp').encode(), err)

    def equal(got, want, label):
        nonlocal worst
        assert len(got) == len(want)
        for i, (g, w) in enumerate(zip(got, want)):
            if g == w:
                continue
            delta = abs(g - w) / max(1, abs(w))
            worst = max(worst, delta)
            assert math.isfinite(delta) and delta < 3e-5, (label, i, g, w, delta)

    try:
        for case in range(args.cases):
            models, clips, poses, roots = [], [], [], []
            owner = forest = None
            try:
                for actor in range(3):
                    ok, model, message = decode(lib, rawmodels[actor])
                    assert ok, message
                    models.append(model)
                    clip = lib.bk_clip_set_decode(xans[actor], len(xans[actor]), err)
                    assert clip, err.value
                    clips.append(clip)
                    root = next(i for i in range(model.contents.frame_count) if model.contents.frames[i].parent_index == NONE)
                    roots.append(root)
                    pose = lib.bk_actor_pose_create_loaded(model, clip, root, (C.c_float * 3)(0, 0, 0), 0, err)
                    assert pose, err.value
                    poses.append(pose)
                    local = F16(*model.contents.frames[root].local)
                    local[12] += actor * .25 * case
                    local[13] -= actor * .125 * case
                    assert lib.bk_actor_pose_root_local(pose, local, err), err.value
                forest = lib.bk_actor_forest_create((C.c_void_p * 3)(*poses), 3, err)
                assert forest, err.value
                for actor in range(3):
                    assert lib.bk_actor_forest_attach(forest, 0, lib.bk_actor_forest_node(forest, actor, roots[actor]), err)
                vm = Native(exe, files)
                vm.setup_three(lib, [m.contents for m in models], poses, names, rawmodels, xans)
                decoder = Parser(exe)
                decoder.decode(rawconfigs[0])
                # Actual packaged4d2320 supplies both the preserved VIX
                # archive path and the sixth Data-directory argument.
                decoder.u.mem_write(decoder.config + 4, b'\\bk3_11.pp\0')
                vm.initialize_combined(decoder)
                inputs = (Actor * 3)(*[Actor(poses[i], names[i], i, roots[i]) for i in range(3)])
                owner = lib.bk_bom_dual_assets_create(store, b'bk3_11', C.byref(configs[0]), inputs, forest, err)
                assert owner, err.value

                def check(where, mapping=False):
                    nonlocal matrices, vertices, maps, clocks
                    for actor, model in enumerate(models):
                        for frame in range(model.contents.frame_count):
                            for getter, off in [('local', 0x80), ('frame', 0xc0), ('parent_world', 0x100)]:
                                pointer = getattr(lib, 'bk_actor_pose_' + getter)(poses[actor], frame)
                                equal(pointer[:16], vm.floats(vm.frame_bases[actor] + frame * 0x400 + off, 16), (case, where, actor, frame, getter))
                                digest.update(C.string_at(pointer, 64))
                                matrices += 1
                            hidden = C.c_uint32()
                            assert lib.bk_actor_pose_hidden(poses[actor], frame, C.byref(hidden))
                            assert hidden.value == vm.read(vm.frame_bases[actor] + frame * 0x400 + 0x70)
                        if actor:
                            state = State()
                            assert lib.bk_actor_pose_state(poses[actor], C.byref(state))
                            vm.clip = vm.clips[actor]
                            equal([getattr(state, k) for k, _ in State._fields_], vm.state(), (where, actor, 'clip'))
                            digest.update(bytes(state))
                            clocks += 1
                            group = lib.bk_bom_dual_assets_morph(owner, actor)
                            mesh_ids, native_group = vm.aux_effects[actor]
                            assert lib.bk_morph_group_time(group) == vm.floats(native_group + 0x74, 1)[0]
                            for index, mesh_pointer in mesh_ids.items():
                                count, pointer = vm.meshes[mesh_pointer]
                                mesh = lib.bk_morph_group_mesh(group, index)
                                got = C.string_at(lib.bk_morph_mesh_vertices(mesh), count * 60)
                                want = bytes(vm.uc.mem_read(pointer, count * 60))
                                if got != want:
                                    for v in range(count):
                                        equal(struct.unpack_from('<9f', got, v * 60), struct.unpack_from('<9f', want, v * 60), (where, actor, index, v))
                                        assert got[v * 60 + 36:(v + 1) * 60] == want[v * 60 + 36:(v + 1) * 60]
                                digest.update(got)
                                vertices += count
                    if mapping:
                        count = lib.bk_bom_dual_assets_count(owner)
                        assert count == decoder.read(decoder.config)
                        for i in range(count):
                            binding = lib.bk_bom_dual_assets_binding(owner, i).contents
                            for field, offset, actor in [('parent', 0x70594c, 0), ('reference', 0x70598c, 0),
                                    ('child', 0x70592c, binding.child_actor), ('primary_aux', 0x70596c, 0),
                                    ('secondary_aux', 0x7059ac, binding.secondary_aux_actor)]:
                                frame = getattr(binding.nodes, field)
                                assert vm.read(offset + i * 4) == (0 if frame == NONE else vm.frame_bases[actor] + frame * 0x400), (where, i, field)
                            for field, offset in [('source', 0x7080e4), ('target', 0x708104)]:
                                index = getattr(binding.nodes, field)
                                mesh = lib.bk_bom_dual_assets_mesh(owner, index).contents if index != NONE else None
                                assert vm.read(offset + i * 4) == (vm.registry[mesh.actor, mesh.submesh] if mesh else 0)
                            group, source, selected = C.c_int32(), C.POINTER(C.c_uint32)(), C.c_size_t()
                            assert lib.bk_bom_dual_assets_mapping(owner, i, C.byref(group), C.byref(source), C.byref(selected))
                            assert group.value == vm.read(0x708184 + i * 4)
                            assert selected.value == binding.nodes.selection_count == vm.read(0x708164 + i * 4)
                            native = bytes(vm.uc.mem_read(vm.read(0x708124 + i * 4), selected.value * 4)) if selected.value else b''
                            actual = C.string_at(source, selected.value * 4) if selected.value else b''
                            assert actual == native, (case, where, i, 'mapping')
                            digest.update(actual)
                            maps += selected.value

                check('first-bind', True)
                materials = [lib.bk_bom_dual_assets_materials(owner, i) for i in [1, 2]]
                morphs = [lib.bk_bom_dual_assets_morph(owner, i) for i in [1, 2]]
                assert materials[0] != materials[1] and morphs[0] != morphs[1]
                append_native(decoder, rawconfigs[1])
                assert decoder.string(decoder.config + 4) == b'\\bk3_11.pp'
                vm.initialize_combined(decoder)
                assert lib.bk_bom_dual_assets_append(owner, store, b'bk3_11', C.byref(configs[1]), forest, err), err.value
                assert materials == [lib.bk_bom_dual_assets_materials(owner, i) for i in [1, 2]]
                assert morphs == [lib.bk_bom_dual_assets_morph(owner, i) for i in [1, 2]]
                check('second-bind', True)
                assert [lib.bk_bom_dual_assets_binding(owner, i).contents.child_actor for i in range(5)] == [1, 1, 1, 2, 2]
                for failure in ['invalid-count', 'overflow', 'missing-parent', 'unterminated-child']:
                    bad = Config.from_buffer_copy(bytes(configs[0]))
                    if failure == 'invalid-count':
                        bad.count = 5
                    elif failure == 'overflow':
                        bad.count = 4
                    elif failure == 'missing-parent':
                        bad.bindings[0].parent = b'explicit-missing-parent-fixture'
                    else:
                        bad.bindings[0].child = b'x' * 260
                    assert not lib.bk_bom_dual_assets_append(owner, store, b'bk3_11', C.byref(bad), forest, err), failure
                    check((failure, 'held'), True)
                    failures += 1
                for step in range(args.steps):
                    for actor in [1, 2]:
                        hidden = int((step + actor * 3) % 13 < 3)
                        edit = Visibility(roots[actor], hidden)
                        assert lib.bk_actor_pose_visibility(poses[actor], C.byref(edit), 1, err)
                        vm.call(0x423a99, struct.pack('<II', vm.frame_bases[actor] + roots[actor] * 0x400, hidden))
                        if step % 11 == 0:
                            active = [i for i in range(128) if lib.bk_clip_definition(clips[actor], i).contents.active]
                            slot = active[(step // 11 + actor) % len(active)]
                            assert lib.bk_actor_pose_request(poses[actor], slot, err), err.value
                            vm.call(0x401b0a, struct.pack('<II', vm.clips[actor], slot))
                        if step % 2:
                            dt = C.c_float([0, .016, .05, .1][(step + actor) % 4]).value
                            assert lib.bk_bom_dual_assets_advance(owner, actor, dt, err), err.value
                            vm.call(0x4026fe, struct.pack('<If', vm.clips[actor], dt))
                        else:
                            assert lib.bk_bom_dual_assets_advance_frame(owner, actor, err), err.value
                            vm.call(0x4021a1, struct.pack('<I', vm.clips[actor]))
                    check(step)
                    if step % 7 == 0:
                        vm.call(0x423be2, b'')
                        assert lib.bk_actor_forest_refresh(forest, err), err.value
                        check(('refresh', step))
                    frames += 1
                print('PASS dual BOM case', case, flush=True)
            finally:
                lib.bk_bom_dual_assets_destroy(owner)
                lib.bk_actor_forest_destroy(forest)
                for pose in poses:
                    lib.bk_actor_pose_destroy(pose)
                for clip in clips:
                    lib.bk_clip_set_destroy(clip)
                for model in models:
                    lib.bk_model_destroy(model)
    finally:
        lib.bk_resources_destroy(store)
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=args.cases,
                  frames=frames, matrices=matrices, vertices=vertices, mapped_vertices=maps,
                  clip_states=clocks, rejected_appends=failures, max_normalized_error=worst,
                  state_sha256=digest.hexdigest(), scope=__doc__)
    result['input_sha256'] = {name.decode(): hashlib.sha256(raw).hexdigest()
                              for name, raw in zip(names, rawmodels)}
    result['input_sha256'].update({(name + b'an').decode(): hashlib.sha256(raw).hexdigest()
                                   for name, raw in zip(names, xans)})
    result['input_sha256'].update({name.decode(): hashlib.sha256(raw).hexdigest()
                                   for name, raw in files.items()})
    result['input_sha256'].update({name: hashlib.sha256(raw).hexdigest() for name, raw in
                                   zip(['jouhansin.bom', 'kahansin.bom'], rawconfigs)})
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS dual BOM', args.cases, frames, matrices, vertices, maps, worst, digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
