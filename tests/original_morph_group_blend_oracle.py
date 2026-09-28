"""Original433a7d/433cbe group masking, ordered blend and held plain clocks.
Seven actual BOM auxiliaries; native track blending and raw vertex buffers.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from original_fixed_actor_oracle import Native,bind
from model_binding import ROOT,decode
from playback_binding import library
from bk3_assets import Archive

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
 exe=args.exe.read_bytes();lib=library();bind(lib);e=C.create_string_buffer(256);frames=vertices=blends=0;worst=0.;records=[]
 lib.bk_morph_group_blend.argtypes=[C.c_void_p,C.c_float,C.c_float,C.c_float,C.POINTER(C.c_uint32),C.c_size_t,C.c_void_p]
 for pack,name in [(8,f'h{i:02}_01.x') for i in range(1,6)]+[(11,'h03_30.x'),(11,'h03_31.x')]:
  arc=Archive(args.data/f'bk3_{pack:02}.pp');raw=arc.read(next(x for x in arc.entries if x.name==name));ok,m,msg=decode(lib,raw);assert ok,msg
  g=morph=None
  try:
   g=lib.bk_morph_group_create(m,e);morph=lib.bk_model_morph_create(m,e);assert g and morph,e.value
   vm=Native(exe);vm.bind(raw,m.contents);vm.morph(lib,m.contents,morph)
   for step in range(100):
    mask=None if step%4==0 else (C.c_uint32*vm.count)(*[0 if step%4==1 or (step+i)%3==0 else (0xffffffff if i%2 else 1) for i in range(vm.count)])
    vm.set_mask(mask);n=vm.count if mask is not None else 0
    times=[0,1,5,10,20,70.75,100.125];a=times[step%len(times)];b=times[(step*3+1)%len(times)];w=C.c_float([0,.25,.5,1,-.5,1.5][step%6]).value
    if step%3==0:
     a=lib.bk_morph_group_time(g) if step%9==0 else a
     vm.call(0x433a7d,struct.pack('<If',vm.group_morph,a));assert lib.bk_morph_group_sample(g,a,mask,n,e),e.value
    else:
     vm.call(0x433cbe,struct.pack('<Ifff',vm.group_morph,a,b,w));assert lib.bk_morph_group_blend(g,a,b,w,mask,n,e),e.value;blends+=1
    assert lib.bk_morph_group_time(g)==vm.floats(vm.group_morph+0x74,1)[0]
    for submesh,ptr in vm.mesh_ids.items():
     count,data=vm.meshes[ptr];mesh=lib.bk_morph_group_mesh(g,submesh);got=C.string_at(lib.bk_morph_mesh_vertices(mesh),count*60);want=bytes(vm.uc.mem_read(data,count*60))
     if got!=want:
      for i in range(count):
       for j,(x,y) in enumerate(zip(struct.unpack_from('<9f',got,i*60),struct.unpack_from('<9f',want,i*60))):
        d=abs(x-y)/max(1,abs(y));worst=max(worst,d);assert math.isfinite(d) and d<3e-5,(name,step,i,j,x,y)
       assert got[i*60+36:(i+1)*60]==want[i*60+36:(i+1)*60]
     vertices+=count
    frames+=1
   records.append(dict(pack=pack,name=name,tracks=vm.count));print('PASS MORP group blend',records[-1],flush=True)
  finally:
   lib.bk_morph_group_destroy(g);lib.bk_model_morph_destroy(morph);lib.bk_model_destroy(m)
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,blends=blends,vertices=vertices,max_normalized_error=worst,records=records,scope=__doc__)
 (ROOT/'local/original-morph-group-blend-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',result,flush=True)
if __name__=='__main__':main()
