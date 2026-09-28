"""Real 58 backgrounds: native registry/enumeration/parent classification,
initial ambient and four light-pass plans. CPU light snapshots use the actual
per-frame cached light nodes. Graphics services are captured, not rendered.
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from original_lighting_pass_oracle import Native,Input,Pass,Command
from original_background_config_oracle import Config
from model_binding import ROOT,Model,library,decode
from environment_binding import Environment,Lighting
from bk3_assets import Archive

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();lib=library();n=Native(exe);err=C.create_string_buffer(256)
 for name,types,result in [
 ('bk_background_config',[C.c_uint32,C.c_uint32],C.POINTER(Config)),
 ('bk_scene_lighting_create',[C.POINTER(Model),C.POINTER(C.c_float),C.c_size_t,C.c_void_p],C.c_void_p),
 ('bk_scene_lighting_destroy',[C.c_void_p],None),
 ('bk_scene_lighting_input',[C.c_void_p,C.POINTER(Input)],C.c_int),
 ('bk_scene_lighting_environment',[C.c_void_p],C.POINTER(Environment)),
 ('bk_scene_lighting_command',[C.c_void_p,C.POINTER(Command)],C.c_int),
 ('bk_scene_lighting_values',[C.c_void_p,C.POINTER(C.c_float),C.c_size_t,C.POINTER(Lighting),C.c_void_p],C.c_int),
 ('bk_lighting_pass',[C.POINTER(Input),C.POINTER(Pass)],C.c_int)]:
  f=getattr(lib,name);f.argtypes=types;f.restype=result
 profiles=snapshots=lights=0;records=[]
 for pack in ['bk3_03','bk3_17']:
  arc=Archive(args.data/(pack+'.pp'));names=sorted({lib.bk_background_config(g,a).contents.clip.decode().replace('.xan','.x') for g in range(5) for a in range(9)})
  for name in names:
   raw=arc.read(next(e for e in arc.entries if e.name==name));ok,m,msg=decode(lib,raw);assert ok,msg;world=(C.c_float*(m.contents.frame_count*16))();assert lib.bk_model_world_matrices(m,world,len(world),err)
   scene=lib.bk_scene_lighting_create(m,world,len(world),err);assert scene,(name,err.value)
   try:
    inp=Input();assert lib.bk_scene_lighting_input(scene,C.byref(inp));env=lib.bk_scene_lighting_environment(scene).contents
    # Mirror loader-produced registry order, execute original full4a435a.
    registry=0x3009000;n.word(0x645614,registry);n.word(0x645618,env.light_count);n.u.mem_write(registry,bytes(0x1000));n.u.mem_write(n.key,b'BK3_L\0');n.commands=[]
    for i in range(env.light_count):
     light=env.lights[i];p=n.light+i*0x100;frame=0x3004000+i*0x400;parent=0x3008000+i*0x100
     n.u.mem_write(p,bytes(0x100));n.word(p+4,0x3ef);n.word(p+0x6c,1);n.word(p+0xe0,frame);n.word(frame+0x22c,parent)
     n.u.mem_write(p+8,light.name+b'\0');n.u.mem_write(parent+8,m.contents.frames[m.contents.frames[light.frame_index].parent_index].name+b'\0');n.u.mem_write(p+0x7c,bytes(light.diffuse))
     n.word(registry+i*0x88+0x80,p);n.word(registry+i*0x88+0x84,0x3ef)
    n.call(0x4a435a,struct.pack('<I',n.key));n.call(0x4a4438,b'')
    groups=struct.unpack('<'+'i'*env.light_count,n.u.mem_read(0x7056f8,env.light_count*4));ranks=struct.unpack('<'+'i'*env.light_count,n.u.mem_read(0x70573c,env.light_count*4))
    assert [l.group for l in inp.lights[:inp.light_count]]==list(groups)
    assert [l.ambient_rank for l in inp.lights[:inp.light_count]]==list(ranks)
    snapshot=Lighting();assert lib.bk_scene_lighting_values(scene,world,len(world),C.byref(snapshot),err)
    assert [round(v*255) for v in snapshot.ambient]==[(n.commands[-1][2]>>shift)&255 for shift in [16,8,0]]
    inp.objects[:]=list(range(1,53));inp.scene_root=99
    for mode in [0,1,2,10]:
     inp.mode=mode;expected=n.run(inp);plan=Pass();assert lib.bk_lighting_pass(C.byref(inp),C.byref(plan));actual=[(c.kind,c.target,c.value) for c in plan.commands[:plan.count]];assert actual==expected
     enabled={};ambient=0
     for c in plan.commands[:plan.count]:
      if c.kind<=1:
       assert lib.bk_scene_lighting_command(scene,C.byref(c))
       if c.kind==0:ambient=c.value
       else:enabled[c.target]=c.value
      elif c.kind==2:
       # Snapshot preserves real light order and reads latest cache, not
       # LIGH exported positions or a transform of the already-world origin.
       for i in range(env.light_count):
        f=env.lights[i].frame_index;world[f*16+12]+=C.c_float(.125).value;world[f*16+13]-=C.c_float(.0625).value
       assert lib.bk_scene_lighting_values(scene,world,len(world),C.byref(snapshot),err),err.value
       selected=[i for i in range(env.light_count) if env.lights[i].type==1 and enabled.get(i,0)]
       assert snapshot.point_count==len(selected)
       assert [round(v*255) for v in snapshot.ambient]==[(ambient>>shift)&255 for shift in [16,8,0]]
       for j,i in enumerate(selected):
        p,l=snapshot.points[j],env.lights[i]
        assert list(p.position)==list(world[l.frame_index*16+12:l.frame_index*16+15])
        for field in ['diffuse','ambient','specular']:assert list(getattr(p,field))==list(getattr(l,field))[:3]
        assert [p.range,p.attenuation0,p.attenuation1,p.attenuation2]==[l.range,*l.attenuation]
       snapshots+=1
    before=bytes(snapshot);assert not lib.bk_scene_lighting_values(scene,world,len(world)-1,C.byref(snapshot),err);assert bytes(snapshot)==before
    profiles+=1;lights+=env.light_count;records.append(dict(pack=pack,file=name,sha256=hashlib.sha256(raw).hexdigest(),groups=list(groups),ranks=list(ranks)))
   finally:lib.bk_scene_lighting_destroy(scene);lib.bk_model_destroy(m)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=profiles,lights=lights,draw_snapshots=snapshots,max_error=0,records=records,scope=__doc__)
 (ROOT/'local/original-lighting-assets-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
