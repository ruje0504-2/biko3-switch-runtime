"""Entire original479137 orchestration with mutable explicit child boundaries.

Compares ordered animation, visibility, BOM, publication and face calls, every
callback snapshot and final state. Native branches and floating conversions
execute unmodified. Child callbacks observe deterministic fixtures; this is
not real-asset animation/MORP, an implemented4E18AD/4A9019, or Switch evidence.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from model_binding import ROOT, library
from original_matrix_oracle import machine
from original_ending_frame_oracle import State as Frame, FIELDS as FRAME_FIELDS, BYTES
from original_ending_auxiliary_oracle import State as Auxiliary, ADDR
from original_face_controller_oracle import State as Face, FIELDS as FACE_FIELDS
from original_ending_tertiary_control_oracle import field_view

I, U, F, B, P = C.c_int32, C.c_uint32, C.c_float, C.c_uint8, C.c_void_p
ACTORS = [0x3002000, 0x3003000, 0x3004000, 0x3005000]
FACE, CONTEXT, VISIBLE, SECOND = 0x3007000, 0x70d370, 0x709ef8, 0x300e020


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('auxiliary', C.POINTER(Auxiliary)),
                ('face', C.POINTER(Face)), ('dispatch', C.POINTER(I)),
                ('face_mode', C.POINTER(I)), ('expression_override', C.POINTER(I)),
                ('eye_lower', C.POINTER(I)), ('expression_latch', C.POINTER(I)),
                ('toggles', C.POINTER(B)), ('background_root', C.POINTER(U)),
                ('hidden_nodes', C.POINTER(U)), ('secondary_node', C.POINTER(U)),
                ('binding_count', C.POINTER(I)), ('binding_capacity', U)]


Clock = C.CFUNCTYPE(I, P, C.POINTER(U), P)
Advance = C.CFUNCTYPE(I, P, I, F, P)
Active = C.CFUNCTYPE(I, P, I, C.POINTER(I), P)
Hide = C.CFUNCTYPE(I, P, U, U, P)
Disable = C.CFUNCTYPE(I, P, U, U, P)
Controlled = C.CFUNCTYPE(I, P, I, I, P)
Material = C.CFUNCTYPE(I, P, U, U, U, F, P)
Publish = C.CFUNCTYPE(I, P, P)
Follow = C.CFUNCTYPE(I, P, U, I, P)
Expression = C.CFUNCTYPE(I, P, I, P)
Range = C.CFUNCTYPE(I, P, F, F, P)
Blink = C.CFUNCTYPE(I, P, U, P)
Level = C.CFUNCTYPE(I, P, C.POINTER(F), P)
Mouth = C.CFUNCTYPE(I, P, F, U, P)


class Ops(C.Structure):
    _fields_ = [('context', P), ('clock', Clock), ('advance', Advance),
                ('active', Active), ('hide', Hide), ('disable_bom', Disable),
                ('controlled', Controlled), ('material', Material), ('publish', Publish),
                ('follow', Follow), ('expression', Expression), ('eye_range', Range),
                ('gaze', Range), ('blink', Blink), ('level', Level), ('mouth', Mouth)]


SCALARS = [('dispatch', 0x721ee8), ('face_mode', 0x721dfc),
           ('expression_override', 0x6a3c24), ('eye_lower', 0x721df8),
           ('expression_latch', 0x6afd04), ('toggles', 0x7220f8),
           ('binding_count', CONTEXT+0x3a30)]
OWNERS = ['frame', 'auxiliary', 'face', 'active', 'hidden', 'advances', 'controls',
          'disabled', 'materials', 'published', 'followed', 'now', 'level',
          'background_root', 'hidden_nodes', 'secondary_node', 'binding_capacity']+[name for name, _ in SCALARS]


def views(f):
    for name, address in FRAME_FIELDS+BYTES:
        yield field_view(f.frame, name, address)
    for name, address in [('camera_values', 0x721e14), ('camera_table', 0x709fcc)]:
        yield field_view(f.frame, name, address)
    for (name, _), address in zip(Auxiliary._fields_, ADDR):
        yield field_view(f.auxiliary, name, address)
    for name, _, offset in FACE_FIELDS:
        yield field_view(f.face, name, FACE+offset)
    for name, address in SCALARS:
        value = getattr(f, name)
        yield address, C.addressof(value), C.sizeof(value)
    for i, actor in enumerate(ACTORS):
        yield actor+0x140, C.addressof(f.active)+4*i, 4


class Fixture:
    def clone(self):
        result = Fixture()
        for name in OWNERS:
            owner = getattr(self, name)
            setattr(result, name, type(owner).from_buffer_copy(owner))
        result.seconds, result.next_clip = self.seconds, self.next_clip
        result.mutate_at, result.mutations = self.mutate_at, self.mutations
        return result

    def snapshot(self):
        return tuple(bytes(getattr(self, name)) for name in OWNERS)

    def effect(self, event, number):
        kind = event[0]
        if kind in ['advance', 'controlled']:
            actor = event[1]
            if kind == 'advance':
                self.advances[actor] = F(self.advances[actor]+event[2]).value
            else:
                self.controls[actor] += 1
            if self.next_clip[actor] >= 0:
                self.active[actor] = self.next_clip[actor]
        elif kind == 'hide':
            self.hidden[event[1]] = event[2]
        elif kind == 'disable':
            self.disabled[event[1]] = event[2]
        elif kind == 'material':
            self.materials[event[1]*4+event[2]] = 0 if event[3] else event[4]
        elif kind == 'publish':
            self.published.value += 1
        elif kind == 'follow':
            self.followed[event[1]*2+event[2]] += 1
        elif kind == 'expression':
            self.face.expression = event[1]
        elif kind == 'range':
            self.face.eye_min, self.face.eye_max = event[1:3]
        elif kind == 'blink':
            self.face.deadline_ms = event[1]
        elif kind == 'mouth':
            self.face.mouth = event[1]
        if number == self.mutate_at:
            for path, value in self.mutations.items():
                name, field = path.split('.')
                owner = getattr(self, name)
                if field.isdigit():
                    owner[int(field)] = value
                else:
                    setattr(owner, field, value)


def pointer(token):
    return 0x300a000+token*16 if token else 0


def token(address):
    if not address:
        return 0
    assert 0x300a000 <= address < 0x300b000 and address % 16 == 0, hex(address)
    return (address-0x300a000)//16


class Native:
    def __init__(self, exe):
        self.u = machine(exe)
        self.stack, self.stop = 0x2008000, 0x300f000
        self.word(0x53f358, 0x300d100)
        for i, actor in enumerate(ACTORS):
            self.word(0x721b28+i*4, actor)
            self.word(actor+0x160, 0x3008000+i*64)
            self.word(0x3008014+i*64, pointer(20+i))
        self.word(0x3008180, FACE)
        self.word(0x645604, pointer(25))
        self.word(0x722334, 0x300c000)
        for i in range(24):
            self.word(CONTEXT+0x7ce0+i*4, pointer(1+i))
            self.word(CONTEXT+0x7d40+i*4, pointer(33+i))
        self.u.mem_write(0x300d200, b'\xd9\x05'+struct.pack('<I', 0x300e080)+b'\xc3')
        for address in [0x300d100, 0x4026fe, 0x423a99, 0x4aaaad, 0x4e18ad,
                        0x4a9019, 0x4a7d10, 0x423be2, 0x422c49, 0x4230bd,
                        0x410fd8, 0x482867, 0x4a0823, 0x482961, 0x4ad5a4, 0x411985]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def integer(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def sync(self, read):
        for address, data, size in views(self.f):
            if read:
                C.memmove(data, bytes(self.u.mem_read(address, size)), size)
            else:
                self.u.mem_write(address, C.string_at(data, size))
        if read:
            self.f.background_root.value = token(self.integer(0x30080d4))
            self.f.secondary_node.value = token(self.integer(SECOND+4))
            self.f.hidden_nodes[:] = [token(self.integer(VISIBLE+i*4)) for i in range(3)]
        else:
            self.word(0x30080d4, pointer(self.f.background_root.value))
            self.word(SECOND+4, pointer(self.f.secondary_node.value))
            for i, value in enumerate(self.f.hidden_nodes):
                self.word(VISIBLE+i*4, pointer(value))

    def hook(self, u, address, _size, _context):
        sp = u.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<8I', u.mem_read(sp+4, 32))
        ret, result = self.integer(sp), 0
        self.sync(True)
        f = self.f
        if address == 0x300d100:
            event, result = ('clock',), f.now.value
        elif address == 0x4026fe:
            event = ('advance', ACTORS.index(args[0]), struct.unpack('<f', u.mem_read(sp+8, 4))[0])
        elif address == 0x423a99:
            event = ('hide', token(args[0]), args[1])
        elif address == 0x4aaaad:
            event = ('disable', args[1], args[0])
        elif address in [0x4e18ad, 0x4a9019]:
            assert args[1] == 0
            event = ('controlled', ACTORS.index(args[0]), 0 if address == 0x4e18ad else 1)
        elif address == 0x4a7d10:
            ordinal, remainder = divmod(args[0]-0x54b828, 260)
            assert not remainder and 0 <= ordinal < 20, args
            event = ('material', ordinal//4, ordinal%4, args[1], struct.unpack('<f', u.mem_read(sp+12, 4))[0])
        elif address == 0x423be2:
            event = ('publish',)
        elif address in [0x422c49, 0x4230bd]:
            index = token(args[0])-1
            assert token(args[1]) == index+33
            direction = address == 0x4230bd
            assert args[2:8] == (0, 0, 0x3f800000, 0, 0x3f800000, 0) if direction else args[2:5] == (0, 0, 0)
            event = ('follow', index, int(direction))
        elif address == 0x410fd8:
            assert args[0] == FACE
            event = ('expression', I(args[1]).value)
        elif address == 0x482867:
            assert args[0] == FACE
            event = ('range', *struct.unpack('<2f', u.mem_read(sp+8, 8)))
        elif address == 0x4a0823:
            assert args[0] == CONTEXT and args[3] == pointer(25)
            event = ('gaze', *struct.unpack('<2f', u.mem_read(sp+8, 8)))
        elif address == 0x482961:
            assert args[0] == FACE
            event = ('blink', args[1])
        elif address == 0x4ad5a4:
            assert args[0] == 0x300c000
            event, result = ('level',), f.level.value
        elif address == 0x411985:
            assert args[0] == FACE
            event = ('mouth', struct.unpack('<f', u.mem_read(sp+8, 4))[0], args[2])
        else:
            raise AssertionError(hex(address))
        self.trace.append((event, f.snapshot()))
        if self.fail_at == len(self.trace):
            self.interrupted = True
            u.emu_stop()
            return
        f.effect(event, len(self.trace))
        self.sync(False)
        if address == 0x4ad5a4:
            u.mem_write(0x300e080, struct.pack('<f', result))
            u.reg_write(UC_X86_REG_EIP, 0x300d200)
        else:
            u.reg_write(UC_X86_REG_EAX, result)
            u.reg_write(UC_X86_REG_ESP, sp+4)
            u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, original, fail_at=0):
        self.f, self.trace = original.clone(), []
        self.fail_at, self.interrupted = fail_at, False
        self.sync(False)
        self.u.mem_write(0x733700, struct.pack('<f', self.f.seconds))
        self.u.mem_write(self.stack, struct.pack('<I', self.stop)+bytes(44)+struct.pack('<3I', CONTEXT, SECOND, VISIBLE))
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(0x479137, self.stop, count=100000)
        assert self.interrupted or self.u.reg_read(UC_X86_REG_EIP) == self.stop
        self.sync(True)
        return self.f, self.trace


def portable(lib, original, fail_at=0):
    f, trace, callbacks, errors = original.clone(), [], [], []
    def emit(event):
        trace.append((event, f.snapshot()))
        if fail_at == len(trace):
            return 0
        f.effect(event, len(trace))
        return 1
    def decorate(typ):
        def wrapped(fn):
            def checked(*args):
                try:
                    return fn(*args)
                except Exception as exc:
                    errors.append(repr(exc))
                    return 0
            callback = typ(checked)
            callbacks.append(callback)
            return callback
        return wrapped
    @decorate(Clock)
    def clock(_, out, _e):
        out[0] = f.now.value
        return emit(('clock',))
    @decorate(Advance)
    def advance(_, actor, dt, _e):
        return emit(('advance', actor, dt))
    @decorate(Active)
    def active(_, actor, out, _e):
        out[0] = f.active[actor]
        return 1
    @decorate(Hide)
    def hide(_, node, hidden, _e):
        return emit(('hide', node, hidden))
    @decorate(Disable)
    def disable(_, binding, disabled, _e):
        return emit(('disable', binding, disabled))
    @decorate(Controlled)
    def controlled(_, actor, mode, _e):
        return emit(('controlled', actor, mode))
    @decorate(Material)
    def material(_, group, slot, hidden, alpha, _e):
        return emit(('material', group, slot, hidden, alpha))
    @decorate(Publish)
    def publish(_, _e):
        return emit(('publish',))
    @decorate(Follow)
    def follow(_, binding, direction, _e):
        return emit(('follow', binding, direction))
    @decorate(Expression)
    def expression(_, selected, _e):
        return emit(('expression', selected))
    @decorate(Range)
    def eye_range(_, minimum, maximum, _e):
        return emit(('range', minimum, maximum))
    @decorate(Range)
    def gaze(_, minimum, maximum, _e):
        return emit(('gaze', minimum, maximum))
    @decorate(Blink)
    def blink(_, timestamp, _e):
        return emit(('blink', timestamp))
    @decorate(Level)
    def level(_, out, _e):
        out[0] = f.level.value
        return emit(('level',))
    @decorate(Mouth)
    def mouth(_, level, timestamp, _e):
        return emit(('mouth', level, timestamp))
    ops = Ops(None, clock, advance, active, hide, disable, controlled, material,
              publish, follow, expression, eye_range, gaze, blink, level, mouth)
    b = Bindings()
    for name, typ in Bindings._fields_:
        if typ == U:
            setattr(b, name, getattr(f, name).value)
        else:
            setattr(b, name, C.cast(C.byref(getattr(f, name)), typ))
    e = C.create_string_buffer(256)
    ok = lib.bk_ending_tertiary_presentation_step(C.byref(b), f.seconds, C.byref(ops), e)
    assert not errors, errors
    return bool(ok), f, trace, e.value.decode()


def fixture(rng, case):
    f = Fixture()
    f.frame, f.auxiliary, f.face = Frame(), Auxiliary(), Face()
    f.frame.group = case % 5
    f.auxiliary.expression_a = rng.choice([-0x80000000, 0, 5, 9, 0x7fffffff])
    f.auxiliary.expression_b = rng.choice([3, 4, 5])
    f.face.blink_phase = rng.randrange(3)
    f.face.rapid_count = rng.randrange(6)
    f.dispatch = I(rng.choice([0, 1, 2, 3, 3, 4, 5]))
    f.face_mode = I(rng.choice([0, 1, 2]))
    f.expression_override = I(rng.choice([-0x80000000, 0, 5, 9, 0x7fffffff]))
    f.eye_lower, f.expression_latch = I(rng.randrange(3)), I(rng.randrange(2))
    f.toggles = (B*8)(*[rng.choice([0, 1, 255]) for _ in range(8)])
    f.background_root = U(14)
    f.hidden_nodes = (U*3)(*[i+1 if rng.randrange(2) else 0 for i in range(3)])
    f.secondary_node = U(rng.choice([0, 15]))
    f.binding_count = I(rng.choice([-1, 0, 1, 5, 24]))
    f.binding_capacity = U(24)
    f.active = (I*4)(rng.choice([1, 3, 4, 7, 9]), rng.choice([1, 7, 8]), rng.choice([1, 9, 10]), 0)
    f.next_clip = [rng.choice([-1, 1, 3, 7, 9]) for _ in range(4)]
    f.hidden, f.advances, f.controls = (U*32)(), (F*4)(), (U*4)()
    f.disabled, f.materials, f.published, f.followed = (U*5)(), (F*20)(), U(), (U*48)()
    f.now, f.level = U(rng.getrandbits(32)), F(rng.uniform(0, 10))
    f.seconds = F(rng.choice([0, .000001, 1/60, .5, 1, 2])).value
    f.mutate_at, f.mutations = 0, {}
    if case % 3 == 0:
        f.mutate_at = case % 29 + 1
        f.mutations = {'frame.group': (f.frame.group+3) % 5,
                       'dispatch.value': 3, 'face.blink_phase': case % 2,
                       'auxiliary.expression_b': 5, 'auxiliary.expression_a': 9,
                       'active.0': 7, 'toggles.0': 1, 'toggles.5': 255,
                       'hidden_nodes.0': 0, 'binding_count.value': 5,
                       'now.value': 0xfffffffa}
    return f


def compare(want, got, wanted_trace, trace, label):
    expected = [event for event, _ in wanted_trace]
    actual = [event for event, _ in trace]
    assert actual == expected, (label, 'calls', actual, expected)
    for index, ((_, a), (_, b)) in enumerate(zip(trace, wanted_trace)):
        assert a == b, (label, 'snapshot', index, actual[index],
                        [(OWNERS[i], x, y) for i, (x, y) in enumerate(zip(a, b)) if x != y][:3])
    assert got.snapshot() == want.snapshot(), (label, 'final',
        [(OWNERS[i], x, y) for i, (x, y) in enumerate(zip(got.snapshot(), want.snapshot())) if x != y][:3])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe', type=Path)
    p.add_argument('--cases', type=int, default=9000)
    p.add_argument('--output', type=Path, default=ROOT/'local/original-ending-tertiary-presentation.json')
    args = p.parse_args()
    exe = args.exe.read_bytes()
    native, lib = Native(exe), library()
    assert struct.unpack('<f', native.u.mem_read(0x53f43c, 4))[0] == F(.6).value
    assert struct.unpack('<f', native.u.mem_read(0x53f49c, 4))[0] == F(.3).value
    lib.bk_ending_tertiary_presentation_step.argtypes = [C.POINTER(Bindings), F, C.POINTER(Ops), P]
    rng, digest = random.Random(0x479137), hashlib.sha256()
    calls = failures = mutations = 0
    operations, actors, controlled = set(), set(), set()
    for case in range(args.cases):
        original = fixture(rng, case)
        want, expected = native.run(original)
        ok, got, trace, error = portable(lib, original)
        assert ok, (case, error)
        compare(want, got, expected, trace, case)
        calls += len(trace)
        mutations += 0 < original.mutate_at <= len(trace)
        for event, _ in trace:
            operations.add(event[0])
            if event[0] == 'advance':
                actors.add(event[1])
            if event[0] == 'controlled':
                controlled.add(event[2])
        digest.update(b''.join(got.snapshot()))
        if case % 11 == 0:
            fail_at = case % len(trace)+1
            want, expected = native.run(original, fail_at)
            ok, got, prefix, _ = portable(lib, original, fail_at)
            assert not ok
            compare(want, got, expected, prefix, (case, 'failure'))
            failures += 1
    original = fixture(rng, 2)
    original.mutate_at, original.binding_count.value = 0, 25
    ok, got, trace, error = portable(lib, original)
    assert not ok and 'available bindings' in error and list(got.followed) == [1]*48, error
    rejected = 1
    for seconds in [float('nan'), float('inf'), -1]:
        original = fixture(rng, 0)
        original.seconds = seconds
        ok, got, trace, error = portable(lib, original)
        assert not ok and not trace and got.snapshot() == original.snapshot(), error
        rejected += 1
    if args.cases >= 9000:
        assert actors == {0, 1, 2, 3} and controlled == {0, 1}
        assert operations == {'clock', 'advance', 'hide', 'disable', 'controlled', 'material',
                              'publish', 'follow', 'expression', 'range', 'gaze', 'blink', 'level', 'mouth'}
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), frames=args.cases,
                  calls=calls, failure_prefixes=failures, live_mutations=mutations,
                  bounded_rejections=rejected, actors=sorted(actors), controlled=sorted(controlled),
                  operations=sorted(operations), state_sha256=digest.hexdigest(), max_error=0, scope=__doc__)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS tertiary presentation:', args.cases, 'frames;', calls, 'calls;', failures,
          'failure prefixes;', mutations, 'live mutations;', rejected, 'rejections; state',
          digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
