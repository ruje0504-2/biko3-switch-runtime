"""Skinning threshold/order fixtures, repeated indices, affine scale/shear."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from original_skin_oracle import Native,bind
from model_binding import ROOT,Model,Frame,Submesh,Vertex,Chunk,library

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);error=C.create_string_buffer(256);rng=random.Random(0x41075e);cases=rejects=0;worst=0
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
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),fixtures=20,samples=cases,vertex_comparisons=cases*8,atomic_rejections=rejects,max_normalized_error=worst,native_functions=['0x41b454..0x41b7a9','0x410a0a','0x41075e','0x522b0d','0x522bd9'],scope='Ordered weight accumulation near .001/.999, negative/over1 synthetic weights, repeated indices, unweighted vertex retention, nonuniform affine scale/shear, output reuse and atomic matrix rejection. All native deform instructions execute.')
    (ROOT/'local/original-skin-weights-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',cases,'skin weight/order samples; max error',worst)
if __name__=='__main__':main()
