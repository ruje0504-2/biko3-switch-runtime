"""Original light frame attachment/submission for the ten selection actors.
Captured D3D state validates light parameters, not Windows rasterization.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from model_binding import ROOT,library,decode,Model
from environment_binding import bind,Light,Lighting,Environment
from original_lighting_oracle import Oracle
from original_lighting_pass_oracle import Input,Command,Pass,Native
from bk3_assets import Archive

def packed(l):
 return struct.pack('<I25f',l.type,*l.diffuse,*l.specular,*l.ambient,*l.position,*l.direction,l.range,l.falloff,*l.attenuation,l.theta,l.phi)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();lib=bind(library());n=Oracle(exe);plan_native=Native(exe);error=C.create_string_buffer(256);rng=random.Random(4260)
 for name,types,result in [
 ('bk_model_light_frame',[C.POINTER(Light),C.POINTER(C.c_float),C.c_void_p],C.c_int),
 ('bk_scene_lighting_create',[C.POINTER(Model),C.POINTER(C.c_float),C.c_size_t,C.c_void_p],C.c_void_p),
 ('bk_scene_lighting_destroy',[C.c_void_p],None),
 ('bk_scene_lighting_input',[C.c_void_p,C.POINTER(Input)],C.c_int),
 ('bk_scene_lighting_environment',[C.c_void_p],C.POINTER(Environment)),
 ('bk_scene_lighting_command',[C.c_void_p,C.POINTER(Command)],C.c_int),
 ('bk_scene_lighting_values',[C.c_void_p,C.POINTER(C.c_float),C.c_size_t,C.POINTER(Lighting),C.c_void_p],C.c_int),
 ('bk_lighting_pass',[C.POINTER(Input),C.POINTER(Pass)],C.c_int)]:
  f=getattr(lib,name);f.argtypes=types;f.restype=result
 arc=Archive(a.data/'bk3_01.pp');submissions=snapshots=0;records=[];worst=0
 for g in range(1,6):
  for variant in [60,61]:
   name=f'h{g:02}_{variant}.x';raw=arc.read(next(e for e in arc.entries if e.name==name));ok,m,msg=decode(lib,raw);assert ok,msg
   world=(C.c_float*(m.contents.frame_count*16))();assert lib.bk_model_world_matrices(m,world,len(world),error)
   scene=lib.bk_scene_lighting_create(m,world,len(world),error);assert scene,(name,error.value)
   try:
    env=lib.bk_scene_lighting_environment(scene).contents;off=next(c.offset for c in m.contents.chunks[:m.contents.chunk_count] if bytes(c.tag)==b'LIGH');inp=Input();assert lib.bk_scene_lighting_input(scene,C.byref(inp));inp.mode=1;inp.objects[0]=10;inp.objects[4]=20
    expected=plan_native.run(inp);plan=Pass();assert lib.bk_lighting_pass(C.byref(inp),C.byref(plan));assert [(c.kind,c.target,c.value) for c in plan.commands[:plan.count]]==expected
    for case in range(128):
     for i in range(env.light_count):
      light=env.lights[i];f=light.frame_index;w=world[f*16:(f+1)*16]
      if case:
       w[2],w[6],w[10]=[rng.uniform(-4,4) for _ in range(3)]
       w[12:15]=[rng.uniform(-100,100) for _ in range(3)]
       w=list((C.c_float*16)(*w));world[f*16:(f+1)*16]=w
      actual=Light.from_buffer_copy(light);assert lib.bk_model_light_frame(C.byref(actual),(C.c_float*16)(*w),error),error.value
      native,states=n.light_state(raw[off+i*172+68:off+(i+1)*172],w)
      if light.type!=0xffffffff:
       assert native==packed(actual),(name,case,i,struct.unpack('<I25f',native),struct.unpack('<I25f',packed(actual)))
       submissions+=1
     enabled={}
     for c in plan.commands[:plan.count]:
      if c.kind<=1:
       assert lib.bk_scene_lighting_command(scene,C.byref(c))
       if c.kind==1:enabled[c.target]=c.value
      elif c.kind==2:
       out=Lighting();assert lib.bk_scene_lighting_values(scene,world,len(world),C.byref(out),error),error.value
       selected=[l for i,l in enumerate(env.lights[:env.light_count]) if enabled.get(i) and l.type!=0xffffffff]
       assert out.point_count+out.spot_count==len(selected)
       pi=si=0
       for l in selected:
        actual=Light.from_buffer_copy(l);assert lib.bk_model_light_frame(C.byref(actual),(C.c_float*16)(*world[l.frame_index*16:(l.frame_index+1)*16]),error)
        if l.type==1:p=out.points[pi];pi+=1
        else:
         spot=out.spots[si];si+=1;p=spot.point
         assert list(spot.direction)==list(actual.direction)
         assert [spot.falloff,spot.theta,spot.phi]==[actual.falloff,actual.theta,actual.phi]
        assert list(p.position)==list(actual.position)
        for field in ['diffuse','ambient','specular']:assert list(getattr(p,field))==list(getattr(actual,field))[:3]
       snapshots+=1
    records.append(dict(name=name,sha256=hashlib.sha256(raw).hexdigest(),lights=env.light_count,spots=env.lighting.spot_count))
    before=bytes(out);assert not lib.bk_scene_lighting_values(scene,world,len(world)-1,C.byref(out),error);assert bytes(out)==before
   finally:lib.bk_scene_lighting_destroy(scene);lib.bk_model_destroy(m)
 # Radiance above1 is valid even in the native integer ambient selection
 # path (which packs unmasked integers, not clamped color channels).
 for case in range(2048):
  inp=Input();inp.light_count=case%17;inp.mode=[0,1,2,10][case%4];inp.objects[0]=10;inp.objects[4]=20
  for l in inp.lights:
   l.diffuse[:]=[rng.uniform(0,8) for _ in range(3)];l.group=rng.choice([1,2]);l.ambient_rank=rng.randrange(3)
  plan=Pass();assert lib.bk_lighting_pass(C.byref(inp),C.byref(plan))
  assert [(c.kind,c.target,c.value) for c in plan.commands[:plan.count]]==plan_native.run(inp)
 # Defined portable failure for original degenerate normalization inputs.
 light=Light(type=2);before=bytes(light);assert not lib.bk_model_light_frame(C.byref(light),(C.c_float*16)(),error);assert bytes(light)==before
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),submissions=submissions,draw_snapshots=snapshots,hdr_passes=2048,max_error=worst,records=records,scope=__doc__)
 (ROOT/'local/original-spotlighting-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
