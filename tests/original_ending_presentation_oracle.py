"""Complete4df6c0 orchestration against the fixed EXE's original instructions.

Animation, material, BOM, face and audio leaves are explicit observing services
here. This verifies their order/arguments and live alias reads, not the resource
implementations or a natural completed ending. Original follow loop executes;
its two node operations are observed individually. Failures stop at each leaf
and compare the already executed prefix. No original code block is replaced.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_prop_route_oracle import Native as Base
from original_ending_frame_oracle import State as Frame, Input
from original_ending_auxiliary_oracle import State as Auxiliary
from model_binding import ROOT, library

I, U, F, P = C.c_int32, C.c_uint32, C.c_float, C.c_void_p

class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('auxiliary', C.POINTER(Auxiliary)),
                ('ready', C.POINTER(I)), ('toggles', C.POINTER(C.c_uint8))] + [
                    (n, C.POINTER(U)) for n in
                    ['primary', 'background', 'secondary', 'direct', 'reference']
                ] + [('flip', C.POINTER(I))]

Clock = C.CFUNCTYPE(I, P, C.POINTER(U), P)
Advance = C.CFUNCTYPE(I, P, I, F, P)
Find = C.CFUNCTYPE(I, P, U, C.c_char_p, C.POINTER(U), P)
Hide = C.CFUNCTYPE(I, P, U, U, P)
Material = C.CFUNCTYPE(I, P, C.c_char_p, U, F, P)
Disable = C.CFUNCTYPE(I, P, U, I, P)
Simple = C.CFUNCTYPE(I, P, P)
Manual = C.CFUNCTYPE(I, P, U, C.POINTER(Input), U, U, I, P)
Face = C.CFUNCTYPE(I, P, I, F, I, U, P)
Level = C.CFUNCTYPE(I, P, C.POINTER(F), P)

class Ops(C.Structure):
    _fields_ = [('context', P), ('clock', Clock), ('advance', Advance),
                ('find', Find), ('hide', Hide), ('material', Material),
                ('disable_bom', Disable), ('publish', Simple), ('manual', Manual),
                ('follow', Simple), ('face', Face), ('gaze', Simple), ('level', Level)]

STATE_WORDS = {
    'phase': 0x721e00, 'state': 0x721ee0, 'cached': 0x721ed0,
    'variant': 0x721e04, 'ready': 0x719b0c,
    'direct': 0x719b28, 'reference': 0x719b2c, 'flip': 0x714fa8,
    'range': 0x721df4, 'expression': 0x721df0,
}
LEAVES = [0x4026fe, 0x425904, 0x423a99, 0x4a7d10, 0x4aaaad,
          0x423be2, 0x49aa50, 0x49ad16, 0x49afde, 0x422c49,
          0x4230bd, 0x411de1, 0x4a0823, 0x410fd8, 0x4110ef,
          0x4ad5a4, 0x411985, 0x300e000]

def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]

class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        self.word(0x53f358, 0x300e000)
        for address in LEAVES:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)
        # fld [explicit float result]; ret, preserving the original caller's
        # fstp and x87 stack discipline for the external PCM-envelope service.
        self.u.mem_write(0x300e100, b'\xd9\x05\x00\xe2\x00\x03\xc3')

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def read(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def set_state(self, data):
        for name, address in STATE_WORDS.items():
            self.word(address, data[name])
        self.u.mem_write(0x721b3c, bytes([data['group']]))
        self.u.mem_write(0x7220f8, bytes(data['toggles']))
        for actor, address in enumerate([0x721b28, 0x721b2c, 0x721b34]):
            clip = 0x3000000 + actor * 0x1000
            model = clip + 0x400
            self.word(address, 0 if actor == 1 and not data['secondary'] else clip)
            self.word(clip + 0x160, model)
            self.word(model + 0x14, 100 + actor)
            self.word(model + 0x180, 900)
        self.word(0x710da0, data['count'])
        for i in range(data['count']):
            self.word(0x715050 + i * 4, 400 + i)
            self.word(0x7150b0 + i * 4, 500 + i)
        self.word(0x645604, 700)
        self.word(0x722334, 800)

    def mutate(self):
        for name, value in self.changes.items():
            if name == 'group':
                self.u.mem_write(0x721b3c, bytes([value]))
            elif name == 'toggles':
                self.u.mem_write(0x7220f8, bytes(value))
            elif name == 'secondary':
                self.word(0x721b2c, 0x3001000 if value else 0)
            else:
                self.word(STATE_WORDS[name], value)

    def hook(self, u, address, size, context):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = self.read(sp)
        args = struct.unpack('<16I', u.mem_read(sp + 4, 64))
        result = 0
        if address == 0x300e000:
            event = ('clock', self.now)
            result = self.now
        elif address == 0x4026fe:
            event = ('advance', (args[0] - 0x3000000) // 0x1000, args[1])
        elif address == 0x425904:
            assert bytes(u.mem_read(args[1], 4)) == b'OYU\0'
            event = ('find', args[0], 'OYU')
            self.word(args[2], self.oyu)
        elif address == 0x423a99:
            event = ('hide', *args[:2])
        elif address == 0x4a7d10:
            name = bytes(u.mem_read(args[0], 260)).split(b'\0')[0].decode('ascii')
            event = ('material', name, args[1], args[2])
        elif address == 0x4aaaad:
            event = ('disable', args[1], args[0])
        elif address == 0x423be2:
            event = ('publish',)
        elif address in (0x49aa50, 0x49ad16, 0x49afde):
            kind = (0x49aa50, 0x49ad16, 0x49afde).index(address)
            assert args[13:15] == (bits(.09), bits(7.5))
            if kind < 2:
                assert args[11:13] == (kind, 0x714fac)
                node, reference = 0, 0
            else:
                node, reference = args[11:13]
            event = ('manual', kind, args[:11], node, reference, args[15])
        elif address == 0x422c49:
            assert args[2:5] == (0, 0, 0)
            event = ('position', args[0], args[1])
        elif address == 0x4230bd:
            assert args[2:8] == (0, 0, bits(1), 0, bits(1), 0)
            event = ('orientation', args[0], args[1])
        elif address == 0x411de1:
            assert args[:2] == (900, 0)
            event = ('face', 0, args[2], 0, 0)
        elif address == 0x4a0823:
            assert args[:4] == (0x70d370, bits(.001), bits(.2), 700)
            event = ('gaze',)
        elif address == 0x410fd8:
            assert args[0] == 900
            event = ('face', 1, 0, args[1], 0)
        elif address == 0x4110ef:
            assert args[0] == 900
            event = ('face', 2, 0, 0, args[1])
        elif address == 0x4ad5a4:
            assert args[0] == 800
            event = ('level', bits(self.level))
            self.word(0x300e200, bits(self.level))
        elif address == 0x411985:
            assert args[0] == 900
            event = ('face', 3, args[1], 0, args[2])
        else:
            raise AssertionError(hex(address))
        self.trace.append(event)
        index = len(self.trace) - 1
        if index == self.mutation_index:
            self.mutate()
        if index == self.failure_index:
            self.failed = True
            u.reg_write(UC_X86_REG_EIP, self.stop)
            return
        if address == 0x4ad5a4:
            u.reg_write(UC_X86_REG_EIP, 0x300e100)
        else:
            u.reg_write(UC_X86_REG_EAX, result)
            u.reg_write(UC_X86_REG_ESP, sp + 4)
            u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, data, inp, seconds, mutation_index, changes, failure_index):
        self.set_state(data)
        self.now, self.level, self.oyu = data['now'], data['level'], data['oyu']
        self.mutation_index, self.changes = mutation_index, changes
        self.failure_index, self.failed = failure_index, False
        self.trace = []
        self.word(0x733700, bits(seconds))
        self.call(0x4df6c0, bytes(inp))
        return self.trace, self.failed

def portable(lib, data, inp, seconds, mutation_index, changes, failure_index):
    frame, aux = Frame(), Auxiliary()
    frame.phase, frame.state_721ee0, frame.camera_cached = data['phase'], data['state'], data['cached']
    frame.group = data['group']
    aux.variant, aux.expression_a, aux.expression_b = data['variant'], data['range'], data['expression']
    ready, flip = I(data['ready']), I(data['flip'])
    toggles = (C.c_uint8 * 8)(*data['toggles'])
    primary, background, secondary = U(100), U(102), U(data['secondary'])
    direct, reference = U(data['direct']), U(data['reference'])
    b = Bindings(C.pointer(frame), C.pointer(aux), C.pointer(ready), toggles,
                 C.pointer(primary), C.pointer(background), C.pointer(secondary),
                 C.pointer(direct), C.pointer(reference), C.pointer(flip))
    def mutate():
        for name, value in changes.items():
            if name in ('group', 'phase', 'state', 'cached'):
                setattr(frame, {'state': 'state_721ee0', 'cached': 'camera_cached'}.get(name, name), value)
            elif name in ('variant', 'range', 'expression'):
                setattr(aux, {'range': 'expression_a', 'expression': 'expression_b'}.get(name, name), value)
            elif name == 'toggles':
                toggles[:] = value
            else:
                {'ready': ready, 'flip': flip, 'secondary': secondary,
                 'direct': direct, 'reference': reference}[name].value = value
    trace = []
    def emit(*event):
        trace.append(event)
        index = len(trace) - 1
        if index == mutation_index:
            mutate()
        return int(index != failure_index)
    @Clock
    def clock(_, out, e):
        out[0] = data['now']
        return emit('clock', out[0])
    @Advance
    def advance(_, actor, seconds, e):
        return emit('advance', actor, bits(seconds))
    @Find
    def find(_, root, name, out, e):
        out[0] = data['oyu']
        return emit('find', root, name.decode())
    @Hide
    def hide(_, node, hidden, e):
        return emit('hide', node, hidden)
    @Material
    def material(_, name, hidden, alpha, e):
        return emit('material', name.decode(), hidden, bits(alpha))
    @Disable
    def disable(_, binding, disabled, e):
        return emit('disable', binding, disabled)
    @Simple
    def publish(_, e):
        return emit('publish')
    @Manual
    def manual(_, kind, inp, node, reference, flip, e):
        return emit('manual', kind, tuple(inp.contents.words), node, reference, flip & 0xffffffff)
    @Simple
    def follow(_, e):
        for i in range(data['count']):
            if not emit('position', 400+i, 500+i) or not emit('orientation', 400+i, 500+i):
                return 0
        return 1
    @Face
    def face(_, kind, value, expression, timestamp, e):
        return emit('face', kind, bits(value), expression & 0xffffffff, timestamp)
    @Simple
    def gaze(_, e):
        return emit('gaze')
    @Level
    def level(_, out, e):
        out[0] = data['level']
        return emit('level', bits(out[0]))
    ops = Ops(None, clock, advance, find, hide, material, disable, publish,
              manual, follow, face, gaze, level)
    err = C.create_string_buffer(256)
    ok = lib.bk_ending_presentation_step(C.byref(b), C.byref(inp), seconds, C.byref(ops), err)
    return trace, not bool(ok)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--frames', type=int, default=12000)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    native, lib = Native(exe), library()
    lib.bk_ending_presentation_step.argtypes = [C.POINTER(Bindings), C.POINTER(Input), F, C.POINTER(Ops), P]
    rng = random.Random(0x4df6c0)
    total = failures = mutations = 0
    rates, manual_kinds = set(), set()
    for case in range(args.frames):
        data = dict(group=case % 5, variant=case % 2,
                    phase=rng.choice([0, 1, 1, 1, 2, 9]),
                    state=rng.choice([0, 3, 3, 3, 4, 5]),
                    cached=rng.choice([0, 9, 10, 11, 12, 26, 27, 99]),
                    ready=rng.choice([0, 1, 1, 6]),
                    toggles=[rng.randrange(256) for _ in range(8)],
                    secondary=int(case % 4 != 0),
                    direct=rng.choice([0, 201]), reference=rng.choice([0, 202]),
                    flip=rng.choice([-1, 0, 1, 2]),
                    range=rng.choice([0, 1, 3, 9, 16777217, 2147483647]),
                    expression=rng.choice([-1, 0, 3, 9, 11]),
                    count=case % 4, oyu=rng.choice([0, 203]),
                    now=rng.getrandbits(32), level=F(rng.uniform(0, 9)).value)
        inp = Input((U * 11)(*[rng.getrandbits(32) for _ in range(11)]))
        seconds = F(rng.choice([0, 1/60, 1/53, .1, .5, 1, 2**-24])).value
        mutation_index = -1 if case % 3 else rng.randrange(24)
        changes = dict(phase=rng.choice([0, 1, 2]), state=rng.choice([0, 3, 4]),
                       ready=rng.choice([1, 6]), cached=rng.choice([9, 11, 12, 26]),
                       group=rng.randrange(5), variant=rng.randrange(2),
                       toggles=[rng.randrange(256) for _ in range(8)],
                       range=rng.randrange(10), expression=rng.randrange(12),
                       secondary=rng.randrange(2), flip=rng.choice([0, 1]))
        failure_index = -1 if case % 4 else rng.randrange(30)
        expected = native.run(data, inp, seconds, mutation_index, changes, failure_index)
        actual = portable(lib, data, inp, seconds, mutation_index, changes, failure_index)
        assert actual == expected, (case, data, mutation_index, failure_index, actual, expected)
        total += len(actual[0])
        failures += actual[1]
        mutations += 0 <= mutation_index < len(actual[0])
        for row in actual[0]:
            if row[0] == 'manual':
                manual_kinds.add(row[1])
            if row[0] == 'advance' and row[1] == 0:
                rates.add(row[2])
    assert manual_kinds == {0, 1, 2}, manual_kinds
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  frames=args.frames, service_calls=total, failure_prefixes=failures,
                  mutable_callbacks=mutations, manual_kinds=sorted(manual_kinds),
                  distinct_primary_steps=len(rates), max_error=0, scope=__doc__)
    (ROOT/'local/original-ending-presentation-oracle.json').write_text(json.dumps(result, indent=2)+'\n')
    print('PASS', json.dumps(result), flush=True)

if __name__ == '__main__':
    main()
