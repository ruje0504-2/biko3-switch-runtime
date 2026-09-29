"""Native skinning weight/order and optional homogeneous-coordinate fixtures."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from original_skin_oracle import Native,bind
from model_binding import ROOT,Model,Frame,Submesh,Vertex,Chunk,library

def f32(value):
    return C.c_float(value).value

def adjacent(value, bits_delta):
    bits = struct.unpack('<I', struct.pack('<f', value))[0]
    return struct.unpack('<f', struct.pack('<I', bits + bits_delta))[0]

def homogeneous_values():
    lower = f32(1.0 - f32(1e-5))
    upper = f32(1.0 + f32(1e-5))
    return [adjacent(1.0, -1), adjacent(1.0, 1), lower,
            adjacent(lower, 1), adjacent(upper, -1), upper,
            .5, 2.0, -2.0, 0.0]

def coordinate_probe(native, lib, native_only):
    """Execute the original divide branch at float neighbors of its bounds."""
    constants = [struct.unpack('<f', native.u.mem_read(p, 4))[0]
                 for p in (0x53fad4, 0x53fad8, 0x53fadc)]
    assert constants == [1.0, -f32(1e-5), f32(1e-5)], constants
    divide_hits = [0]
    def divided(*unused):
        divide_hits[0] += 1
    hook = native.u.hook_add(UC_HOOK_CODE, divided,
                             begin=0x522bba, end=0x522bba)
    fn = lib.bk_matrix_transform_coord if not native_only else None
    if fn:
        fn.argtypes = [C.POINTER(C.c_float)] * 3
        fn.restype = C.c_int
    rng = random.Random(0x522b0d)
    cases = kept = rejected = 0
    worst = 0.0
    values = homogeneous_values()
    try:
        for case in range(2048):
            point = (C.c_float * 3)(*[rng.uniform(-20, 20) for _ in range(3)])
            matrix = (C.c_float * 16)(*[rng.uniform(-4, 4) for _ in range(16)])
            for j in (3, 7, 11):
                matrix[j] = 0 if case < 1024 else rng.uniform(-.25, .25)
            matrix[15] = values[case % len(values)]
            w = f32(sum(float(point[j]) * matrix[j * 4 + 3]
                        for j in range(3)) + matrix[15])
            expected_divide = w - 1.0 < constants[1] or w - 1.0 > constants[2]
            native.u.mem_write(native.data, bytes(point))
            native.u.mem_write(native.data + 64, bytes(matrix))
            before_hits = divide_hits[0]
            native.call(0x522b0d, native.out + 256, native.data, native.data + 64)
            wanted = struct.unpack('<3f', native.u.mem_read(native.out + 256, 12))
            assert divide_hits[0] - before_hits == int(expected_divide), (case, w)
            valid = all(math.isfinite(value) for value in wanted)
            if fn:
                actual = (C.c_float * 3)(123, 456, 789)
                accepted = fn(actual, point, matrix)
                assert bool(accepted) == valid, (case, w, accepted, wanted)
                if valid:
                    for a, b in zip(actual, wanted):
                        delta = abs(a - b) / max(1, abs(b))
                        worst = max(worst, delta)
                        assert delta < 3e-6, (case, w, a, b)
                else:
                    assert list(actual) == [123, 456, 789]
            kept += not expected_divide
            rejected += not valid
            cases += 1
    finally:
        native.u.hook_del(hook)
    return dict(samples=cases, unchanged=kept, divided=divide_hits[0],
                native_nonfinite=rejected, max_normalized_error=worst,
                constants=constants)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe',type=Path)
    ap.add_argument('--homogeneous',action='store_true',help='Compare native XYZ with non-unit W and finite projective columns')
    ap.add_argument('--native-only',action='store_true',help='Measure original tolerance/divide behavior without invoking portable math')
    ap.add_argument('--output',type=Path)
    args=ap.parse_args()
    if args.native_only and not args.homogeneous:
        ap.error('--native-only requires --homogeneous')
    exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);error=C.create_string_buffer(256);rng=random.Random(0x41075e);cases=rejects=0;worst=0
    coordinate = coordinate_probe(n, lib, args.native_only) if args.homogeneous else None
    native_nonfinite = unchanged = changed = 0
    for fixture in range(20):
        raw=bytearray(152);struct.pack_into('<I',raw,68,1);struct.pack_into('<3I',raw,140,10,100,6)
        frames=(Frame*6)();initial=(Vertex*8)();sub=Submesh();sub.id=100;sub.vertex_count=8;sub.vertices=initial
        for j in range(6):
            frames[j].id=10+j
            indices=[0,1,2,3,4,5,6,0];positions=[rng.uniform(-20,20) for _ in range(24)];normals=[rng.uniform(-5,5) for _ in range(24)]
            weights=[C.c_float(rng.choice([-.2,0,.0009,.001,.0010001,.4995,.5,.6,.9989,.999,.9991,1,1.2])).value for _ in range(8)]
            raw+=struct.pack('<II',10+j,8)+struct.pack('<24f',*positions)+struct.pack('<24f',*normals)+struct.pack('<8I',*indices)+struct.pack('<8f',*weights)
        for v in initial:
            for j in range(3):v.position[j]=rng.uniform(-1,1);v.normal[j]=rng.uniform(-1,1)
            v.beta=.33;v.uv[0][0]=.25;v.uv[1][0]=float('nan')
        source=(C.c_ubyte*len(raw)).from_buffer_copy(raw);chunk=Chunk(b'ENVL',0,len(raw));model=Model();model.source=source;model.source_size=len(raw);model.chunks=C.pointer(chunk);model.chunk_count=1;model.frames=frames;model.frame_count=6;model.submeshes=C.pointer(sub);model.submesh_count=1
        skin=lib.bk_model_skin_create(C.byref(model),error);assert skin,error.value;mesh=lib.bk_skin_mesh_create(skin,0,C.byref(sub),error);assert mesh,error.value
        n.load(bytes(raw),model,chunk);world=(C.c_float*96)()
        try:
            for pose in range(40):
                for f in range(6):
                    for j in range(16):world[f*16+j]=0 if j in [3,7,11] else 1 if j==15 else rng.uniform(-4,4)
                n.pose(world);wanted=n.apply(0)
                if args.homogeneous:
                    affine_wanted=wanted
                    constants=homogeneous_values()
                    variant=pose % (len(constants) + 2)
                    for f in range(6):
                        for j in [3,7,11]:
                            world[f*16+j]=0 if variant<len(constants) else (f+1)*(j-6)*.125
                        world[f*16+15]=constants[variant] if variant<len(constants) else 1.0
                    n.pose(world);wanted=n.apply(0)
                    near_one=variant<len(constants) and abs(constants[variant]-1)<=f32(1e-5)
                    if near_one:
                        assert wanted==affine_wanted,('native tolerance changed XYZ',fixture,pose)
                        unchanged+=1
                    else:
                        assert wanted!=affine_wanted,('native divide had no effect',fixture,pose)
                        changed+=1
                    finite=all(math.isfinite(value) for v in range(8)
                               for value in struct.unpack_from('<7f',wanted,v*60))
                    if not finite:
                        native_nonfinite+=1
                        if not args.native_only:
                            saved=C.string_at(lib.bk_skin_mesh_vertices(mesh),480)
                            assert not lib.bk_skin_mesh_apply(mesh,world,96,None,0,error)
                            assert C.string_at(lib.bk_skin_mesh_vertices(mesh),480)==saved
                            rejects+=1
                        cases+=1
                        continue
                    if args.native_only:
                        cases+=1
                        continue
                assert lib.bk_skin_mesh_apply(mesh,world,96,lib.bk_skin_mesh_vertices(mesh),8,error),error.value
                actual=C.string_at(lib.bk_skin_mesh_vertices(mesh),480)
                for v in range(8):
                    for field,(a,b) in enumerate(zip(struct.unpack_from('<7f',actual,v*60),struct.unpack_from('<7f',wanted,v*60))):
                        delta=abs(a-b)/max(1,abs(b));worst=max(worst,delta);assert math.isfinite(delta) and delta<3e-6,(fixture,pose,v,field,a,b)
                    assert actual[v*60+28:(v+1)*60]==wanted[v*60+28:(v+1)*60]
                assert actual[420:480]==C.string_at(C.byref(initial[7]),60)
                if pose%8==0:
                    old=world[0];world[0]=float('nan');assert not lib.bk_skin_mesh_apply(mesh,world,96,None,0,error)
                    assert C.string_at(lib.bk_skin_mesh_vertices(mesh),480)==actual;world[0]=old;rejects+=1
                cases+=1
        finally:lib.bk_skin_mesh_destroy(mesh);lib.bk_model_skin_destroy(skin)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),fixtures=20,samples=cases,vertex_comparisons=(cases-native_nonfinite)*8,atomic_rejections=rejects,max_normalized_error=worst,homogeneous=args.homogeneous,native_only=args.native_only,coordinate=coordinate,native_nonfinite=native_nonfinite,unchanged=unchanged,changed=changed,native_functions=['0x41b454..0x41b7a9','0x410a0a','0x41075e','0x522b0d','0x522bd9'],scope='Ordered weight accumulation near .001/.999, negative/over1 synthetic weights, repeated indices, unweighted vertex retention, nonuniform affine scale/shear. Homogeneous mode compares the actual original tolerance/divide branches, constant W and projective columns. Native nonfinite output is an explicit atomic rejection policy, not finite equivalence. Native-only mode does not test portable math; portable mode checks output reuse and atomic NaN rejection. All native deformation instructions execute.')
    output=args.output or ROOT/('local/original-skin-homogeneous-native.json' if args.native_only else 'local/original-skin-homogeneous-oracle.json' if args.homogeneous else 'local/original-skin-weights-oracle.json')
    output.write_text(json.dumps(report,indent=2)+'\n');print('PASS',cases,'skin weight/order samples; homogeneous',args.homogeneous,'native_only',args.native_only,'max error',worst)
if __name__=='__main__':main()
