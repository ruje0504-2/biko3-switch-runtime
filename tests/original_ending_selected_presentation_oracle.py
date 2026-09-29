"""Original494015 branches, rates and ordered mutable service boundaries.

The original instructions run unchanged; actual model/face/audio operations
are explicit observing fixtures. Check every callback snapshot, live alias
change, captured timestamp and failure prefix. The actual selected asset
effects are independently tested. This alone is not4968CB, a complete scene,
rendered output, Japanese-EXE equivalence, or Switch validation.
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
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from model_binding import ROOT, library
from original_matrix_oracle import machine
from original_ending_frame_oracle import State as Frame, FIELDS as FRAME_FIELDS, BYTES
from original_ending_auxiliary_oracle import State as Auxiliary, ADDR
from original_face_controller_oracle import State as Face, FIELDS as FACE_FIELDS
from original_ending_tertiary_control_oracle import field_view

I, U, F, B, P = C.c_int32, C.c_uint32, C.c_float, C.c_uint8, C.c_void_p
PRIMARY, BACKGROUND, FACE = 0x3002000, 0x3005000, 0x3007000
CONTEXT, VISIBLE, SECOND = 0x70d370, 0x709ef8, 0x300e020
PLAIN = {0x402e18: 0, 0x4e18ad: 1, 0x4a9019: 2}
NAMES = ['S_sobi2a', 'S_sobi2b', 'S_sobi1', '', 'S_sobi3a',
         'S_sobi3b', 'S_sobi5a', 'S_sobi5b', 'S_sobi4']


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('auxiliary', C.POINTER(Auxiliary)),
                ('face', C.POINTER(Face))] + [(name, C.POINTER(I)) for name in
                ['mode', 'plain_scheduled', 'face_mode', 'expression_override',
                 'eye_lower', 'expression_latch']] + [('scale', C.POINTER(F)),
                ('toggles', C.POINTER(B))] + [(name, C.POINTER(U)) for name in
                ['primary_root', 'background_root', 'hidden_nodes', 'secondary_node']]


Clock = C.CFUNCTYPE(I, P, C.POINTER(U), P)
Advance = C.CFUNCTYPE(I, P, I, F, P)
Find = C.CFUNCTYPE(I, P, U, C.c_char_p, C.POINTER(U), P)
Hide = C.CFUNCTYPE(I, P, U, U, P)
Simple = C.CFUNCTYPE(I, P, P)
Active = C.CFUNCTYPE(I, P, C.POINTER(I), P)
Duration = C.CFUNCTYPE(I, P, U, C.POINTER(I), P)
Controlled = C.CFUNCTYPE(I, P, I, P)
Material = C.CFUNCTYPE(I, P, C.c_char_p, U, F, P)
Expression = C.CFUNCTYPE(I, P, I, P)
Range = C.CFUNCTYPE(I, P, F, F, P)
Blink = C.CFUNCTYPE(I, P, U, P)
Level = C.CFUNCTYPE(I, P, C.POINTER(F), P)
Mouth = C.CFUNCTYPE(I, P, F, U, P)


class Ops(C.Structure):
    _fields_ = [('context', P), ('clock', Clock), ('advance', Advance),
                ('find', Find), ('hide', Hide), ('auxiliary_tick', Simple),
                ('active', Active), ('duration', Duration), ('controlled', Controlled),
                ('material', Material), ('publish', Simple), ('expression', Expression),
                ('eye_range', Range), ('gaze', Range), ('blink', Blink),
                ('level', Level), ('mouth', Mouth)]


SCALARS = [('mode', 0x6dde94), ('plain_scheduled', 0xb53c38),
           ('face_mode', 0x721dfc), ('expression_override', 0x6dde98),
           ('eye_lower', 0x721df8), ('expression_latch', 0x6ea354),
           ('scale', 0x55469c), ('toggles', 0x7220f8)]
OWNERS = ['frame', 'auxiliary', 'face', 'primary_root', 'background_root',
          'hidden_nodes', 'secondary_node', 'active', 'durations', 'found',
          'hidden', 'advances', 'controls', 'materials', 'published', 'now',
          'level', 'auxiliary_ticks'] + [name for name, _ in SCALARS]


def views(f):
    for name, address in FRAME_FIELDS + BYTES:
        yield field_view(f.frame, name, address)
    for name, address in [('camera_values', 0x721e14), ('camera_table', 0x709fcc)]:
        yield field_view(f.frame, name, address)
    for (name, _), address in zip(Auxiliary._fields_, ADDR):
        yield field_view(f.auxiliary, name, address)
    for name, _, offset in FACE_FIELDS:
        yield field_view(f.face, name, FACE + offset)
    for name, address in SCALARS:
        value = getattr(f, name)
        yield address, C.addressof(value), C.sizeof(value)
    yield PRIMARY + 0x140, C.addressof(f.active), 4
    for i in range(32):
        yield PRIMARY + 0x1e0 + i * 0x9c, C.addressof(f.durations) + 4*i, 4


class Fixture:
    def clone(self):
        f = Fixture()
        for name in OWNERS:
            value = getattr(self, name)
            setattr(f, name, type(value).from_buffer_copy(value))
        f.seconds, f.next_clip = self.seconds, self.next_clip
        f.mutate_at, f.mutations = self.mutate_at, self.mutations
        return f

    def snapshot(self):
        return tuple(bytes(getattr(self, name)) for name in OWNERS)

    def effect(self, event, number):
        kind = event[0]
        if kind == 'advance':
            self.advances[event[1]] = F(self.advances[event[1]] + event[2]).value
            if event[1] == 0 and self.next_clip >= 0:
                self.active.value = self.next_clip
        elif kind == 'controlled':
            self.controls[event[1]] += 1
            if self.next_clip >= 0:
                self.active.value = self.next_clip
        elif kind == 'auxiliary_tick':
            self.auxiliary_ticks.value += 1
            if self.next_clip >= 0:
                self.active.value = self.next_clip
        elif kind == 'hide':
            self.hidden[event[1]] = event[2]
        elif kind == 'material':
            self.materials[NAMES.index(event[1])] = 0 if event[2] else event[3]
        elif kind == 'publish':
            self.published.value += 1
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


def pointer(node):
    return 0x300a000 + node * 16 if node else 0


def token(address):
    if not address:
        return 0
    assert 0x300a000 < address < 0x300b000 and address % 16 == 0, hex(address)
    return (address - 0x300a000) // 16


class Native:
    def __init__(self, exe):
        self.u = machine(exe)
        self.stack, self.stop = 0x2008000, 0x300f000
        self.word(0x53f358, 0x300d100)
        for actor, address, model in [(PRIMARY, 0x721b28, 0x3008000),
                                       (BACKGROUND, 0x721b34, 0x30080c0)]:
            self.word(address, actor)
            self.word(actor + 0x160, model)
        self.word(0x3008180, FACE)
        self.word(0x645604, pointer(25))
        self.word(0x722334, 0x300c000)
        self.u.mem_write(0x300d200, b'\xd9\x05' + struct.pack('<I', 0x300e080) + b'\xc3')
        for address in [0x300d100, 0x4026fe, 0x425904, 0x423a99, 0x4968cb,
                        *PLAIN, 0x4a7d10, 0x423be2, 0x410fd8, 0x482867,
                        0x4a0823, 0x482961, 0x4ad5a4, 0x411985]:
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
        links = [('primary_root', 0x3008014), ('background_root', 0x30080d4),
                 ('secondary_node', SECOND + 4)]
        for name, address in links:
            if read:
                getattr(self.f, name).value = token(self.integer(address))
            else:
                self.word(address, pointer(getattr(self.f, name).value))
        for i in range(3):
            if read:
                self.f.hidden_nodes[i] = token(self.integer(VISIBLE + i*4))
            else:
                self.word(VISIBLE + i*4, pointer(self.f.hidden_nodes[i]))

    def hook(self, u, address, _size, _context):
        sp = u.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<4I', u.mem_read(sp + 4, 16))
        ret, result = self.integer(sp), 0
        self.sync(True)
        f = self.f
        if address == 0x300d100:
            event, result = ('clock',), f.now.value
        elif address == 0x4026fe:
            assert args[0] in [PRIMARY, BACKGROUND]
            event = ('advance', 0 if args[0] == PRIMARY else 2,
                     struct.unpack('<f', u.mem_read(sp + 8, 4))[0])
        elif address == 0x425904:
            name = bytes(u.mem_read(args[1], 16)).split(b'\0')[0].decode()
            assert name == 'OYU'
            event, result = ('find', token(args[0]), name), pointer(f.found.value)
        elif address == 0x423a99:
            event = ('hide', token(args[0]), args[1])
        elif address == 0x4968cb:
            event = ('auxiliary_tick',)
        elif address in PLAIN:
            assert args[0] == PRIMARY and args[1] == 0
            event = ('controlled', PLAIN[address])
        elif address == 0x4a7d10:
            ordinal, remainder = divmod(args[0] - 0x5554bc, 260)
            assert not remainder and 0 <= ordinal < 20, args
            name = bytes(u.mem_read(args[0], 260)).split(b'\0')[0].decode()
            event = ('material', name, args[1], struct.unpack('<f', u.mem_read(sp + 12, 4))[0])
        elif address == 0x423be2:
            event = ('publish',)
        elif address == 0x410fd8:
            assert args[0] == FACE
            event = ('expression', I(args[1]).value)
        elif address == 0x482867:
            assert args[0] == FACE
            event = ('range', *struct.unpack('<2f', u.mem_read(sp + 8, 8)))
        elif address == 0x4a0823:
            assert args[0] == CONTEXT and args[3] == pointer(25)
            event = ('gaze', *struct.unpack('<2f', u.mem_read(sp + 8, 8)))
        elif address == 0x482961:
            assert args[0] == FACE
            event = ('blink', args[1])
        elif address == 0x4ad5a4:
            assert args[0] == 0x300c000
            event, result = ('level',), f.level.value
        elif address == 0x411985:
            assert args[0] == FACE
            event = ('mouth', struct.unpack('<f', u.mem_read(sp + 8, 4))[0], args[2])
        else:
            raise AssertionError(hex(address))
        self.trace.append((event, f.snapshot()))
        if self.fail_at == len(self.trace):
            self.interrupted = True
            u.emu_stop()
            return
        f.effect(event, len(self.trace))
        self.sync(False)
        if address == 0x425904:
            self.word(args[2], result)
        if address == 0x4ad5a4:
            u.mem_write(0x300e080, struct.pack('<f', result))
            u.reg_write(UC_X86_REG_EIP, 0x300d200)
        else:
            u.reg_write(UC_X86_REG_EAX, result)
            u.reg_write(UC_X86_REG_ESP, sp + 4)
            u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, original, fail_at=0):
        self.f, self.trace = original.clone(), []
        self.fail_at, self.interrupted = fail_at, False
        self.sync(False)
        self.u.mem_write(0x733700, struct.pack('<f', self.f.seconds))
        self.u.mem_write(self.stack, struct.pack('<I', self.stop) + bytes(44)
                        + struct.pack('<3I', CONTEXT, VISIBLE, SECOND))
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(0x494015, self.stop, count=100000)
        assert self.interrupted or self.u.reg_read(UC_X86_REG_EIP) == self.stop
        self.sync(True)
        return self.f, self.trace


def portable(lib, original, fail_at=0, missing=None):
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
    def advance(_, actor, seconds, _e):
        return emit(('advance', actor, seconds))
    @decorate(Find)
    def find(_, root, name, out, _e):
        out[0] = f.found.value
        return emit(('find', root, name.decode()))
    @decorate(Hide)
    def hide(_, node, hidden, _e):
        return emit(('hide', node, hidden))
    @decorate(Simple)
    def auxiliary_tick(_, _e):
        return emit(('auxiliary_tick',))
    @decorate(Active)
    def active(_, out, _e):
        out[0] = f.active.value
        return 1
    @decorate(Duration)
    def duration(_, clip, out, _e):
        out[0] = f.durations[clip]
        return 1
    @decorate(Controlled)
    def controlled(_, mode, _e):
        return emit(('controlled', mode))
    @decorate(Material)
    def material(_, name, hidden, alpha, _e):
        return emit(('material', name.decode(), hidden, alpha))
    @decorate(Simple)
    def publish(_, _e):
        return emit(('publish',))
    @decorate(Expression)
    def expression(_, choice, _e):
        return emit(('expression', choice))
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
    ops = Ops(None, clock, advance, find, hide, auxiliary_tick, active, duration,
              controlled, material, publish, expression, eye_range, gaze, blink, level, mouth)
    if missing is not None:
        typ = dict(Ops._fields_)[missing]
        setattr(ops, missing, typ())
    bindings = Bindings()
    for name, typ in Bindings._fields_:
        setattr(bindings, name, C.cast(C.byref(getattr(f, name)), typ))
    error = C.create_string_buffer(256)
    ok = lib.bk_ending_selected_presentation_step(C.byref(bindings), f.seconds,
                                                C.byref(ops), error)
    assert not errors, errors
    return bool(ok), f, trace, error.value.decode()


def fixture(rng, case):
    f = Fixture()
    f.frame, f.auxiliary, f.face = Frame(), Auxiliary(), Face()
    f.frame.group, f.frame.phase = case % 5, rng.choice([5, 6])
    f.frame.auxiliary_mode = rng.choice([0, 0, 0, 0, 1, -1])
    f.auxiliary.variant, f.auxiliary.selection = rng.randrange(2), rng.randrange(3)
    f.auxiliary.gate = rng.choice([1, 2, 3, 3, 3, 4])
    f.auxiliary.expression_a = rng.choice([-0x80000000, 0, 5, 9, 0x7fffffff])
    f.auxiliary.expression_b = rng.choice([3, 4, 5])
    f.face.blink_phase, f.face.rapid_count = rng.randrange(3), rng.randrange(3)
    f.mode, f.plain_scheduled = I(rng.choice(list(range(8)) + [-1])), I(rng.choice([0, 1, -1]))
    f.face_mode, f.expression_latch = I(rng.randrange(3)), I(rng.randrange(2))
    f.expression_override, f.eye_lower = I(rng.choice([0, 5, 9, 0x7fffffff])), I(rng.randrange(3))
    f.scale = F(rng.choice([.01, .02, 0, .035, .3]))
    f.toggles = (B*8)(*[rng.choice([0, 1, 255]) for _ in range(8)])
    f.primary_root, f.background_root = U(20), U(14)
    f.hidden_nodes = (U*3)(*[i+1 if rng.randrange(2) else 0 for i in range(3)])
    f.secondary_node, f.found = U(rng.choice([0, 15])), U(rng.choice([0, 4]))
    f.active = I(rng.randrange(24))
    f.durations = (I*32)(*[rng.choice([0, 1, 7, 100, 913, 80000003, 0x7fffffff]) for _ in range(32)])
    f.next_clip = rng.choice([-1, 1, 9, 11, 13, 15, 16, 17, 18, 19, 20, 21])
    f.hidden, f.advances, f.controls = (U*64)(), (F*3)(), (U*3)()
    f.materials, f.published, f.auxiliary_ticks = (F*len(NAMES))(), U(), U()
    f.now, f.level = U(rng.getrandbits(32)), F(rng.uniform(0, 10))
    f.seconds = F(rng.choice([0, .000001, 1/120, 1/60, .5, 1, 2])).value
    f.mutate_at, f.mutations = 0, {}
    if case % 3 == 0:
        f.mutate_at = case % 23 + 1
        f.mutations = {'frame.group': (f.frame.group+3) % 5,
                       'frame.phase': 6, 'mode.value': 0,
                       'face.blink_phase': case % 2, 'face.rapid_count': 1,
                       'auxiliary.expression_b': 5, 'auxiliary.expression_a': 9,
                       'active.value': 13, 'toggles.0': 1, 'toggles.5': 255,
                       'scale.value': .031, 'hidden_nodes.0': 0,
                       'secondary_node.value': 15, 'now.value': 0xfffffffa}
    return f


def compare(want, got, expected, actual, label):
    assert [event for event, _ in actual] == [event for event, _ in expected], (
        label, 'calls', [event for event, _ in actual], [event for event, _ in expected])
    for i, ((event, a), (_, b)) in enumerate(zip(actual, expected)):
        assert a == b, (label, 'snapshot', i, event,
            [(OWNERS[j], x, y) for j, (x, y) in enumerate(zip(a, b)) if x != y][:3])
    assert got.snapshot() == want.snapshot(), (label, 'final',
        [(OWNERS[j], x, y) for j, (x, y) in enumerate(zip(got.snapshot(), want.snapshot())) if x != y][:3])


def main():
    faulthandler.enable()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--cases', type=int, default=12000)
    parser.add_argument('--output', type=Path, default=ROOT/'local/original-ending-selected-presentation.json')
    args = parser.parse_args()
    if args.cases < 1:
        parser.error('cases must be positive')
    exe = args.exe.read_bytes()
    native, lib = Native(exe), library()
    lib.bk_ending_selected_presentation_step.argtypes = [C.POINTER(Bindings), F, C.POINTER(Ops), P]
    lib.bk_ending_selected_presentation_step.restype = I
    lib.bk_ending_presentation_material.argtypes = [U, U]
    lib.bk_ending_presentation_material.restype = C.c_char_p
    for group in range(5):
        for slot in range(4):
            expected = bytes(native.u.mem_read(0x5554bc+(group*4+slot)*260, 260)).split(b'\0')[0]
            assert lib.bk_ending_presentation_material(group, slot) == expected
    rng, digest = random.Random(0x494015), hashlib.sha256()
    calls = failures = mutations = constant_mouth = envelope_mouth = 0
    operations, controlled, actors = set(), set(), set()
    service_cases = {}
    for case in range(args.cases):
        original = fixture(rng, case)
        want, expected = native.run(original)
        ok, got, trace, error = portable(lib, original)
        assert ok, (case, error)
        compare(want, got, expected, trace, case)
        calls += len(trace)
        mutations += 0 < original.mutate_at <= len(trace)
        envelope = any(event[0] == 'level' for event, _ in trace)
        envelope_mouth += envelope
        constant_mouth += not envelope
        for event, _ in trace:
            operations.add(event[0])
            service = 'eye_range' if event[0] == 'range' else event[0]
            service_cases.setdefault(service, original)
            if event[0] == 'controlled': controlled.add(event[1])
            if event[0] == 'advance': actors.add(event[1])
        digest.update(b''.join(got.snapshot()))
        if case % 11 == 0:
            fail_at = case % len(trace) + 1
            want, expected = native.run(original, fail_at)
            ok, got, prefix, _ = portable(lib, original, fail_at)
            assert not ok
            compare(want, got, expected, prefix, (case, 'failure'))
            failures += 1
    rejections = 0
    for seconds in [float('nan'), float('inf'), -1]:
        original = fixture(rng, 0)
        original.seconds = seconds
        ok, got, trace, error = portable(lib, original)
        assert not ok and not trace and got.snapshot() == original.snapshot(), error
        rejections += 1
    missing_services = 0
    for service, original in service_cases.items():
        ok, _, _, error = portable(lib, original, missing=service)
        assert not ok and error == 'selected presentation: missing ' + service + ' service', error
        missing_services += 1
    if 'auxiliary_tick' in service_cases:
        for service in ['active', 'duration']:
            original = service_cases['auxiliary_tick']
            ok, _, _, error = portable(lib, original, missing=service)
            assert not ok and error == 'selected presentation: missing ' + service + ' service', error
            missing_services += 1
    if args.cases >= 12000:
        assert actors == {0, 2} and controlled == {0, 1, 2}
        assert operations == {'clock', 'advance', 'find', 'hide', 'auxiliary_tick',
                              'controlled', 'material', 'publish', 'expression',
                              'range', 'gaze', 'blink', 'level', 'mouth'}
        assert constant_mouth and envelope_mouth and missing_services == 16
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  frames=args.cases, calls=calls, failure_prefixes=failures,
                  live_mutations=mutations, bounded_rejections=rejections,
                  missing_services=missing_services, constant_mouth=constant_mouth,
                  envelope_mouth=envelope_mouth, actors=sorted(actors),
                  controlled=sorted(controlled), operations=sorted(operations),
                  state_sha256=digest.hexdigest(), max_error=0, scope=__doc__,
                  full_loader=False, gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print('PASS selected presentation', args.cases, 'frames', calls, 'calls',
          failures, 'failure prefixes', mutations, 'live mutations', rejections,
          'rejections', missing_services, 'missing services', digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
