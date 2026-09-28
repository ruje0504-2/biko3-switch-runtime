"""Two-model normal ending light registry and ordered original4a435a/4438/4701.
Use real loaded actor/background caches and an explicit moving-light fixture.
Original425f79/421dd4 supplies per-light D3D parameters; no rasterization or
complete ending frame is claimed. Also exercise reversed source order to catch
ambient-priority/order assumptions; production remains primary then background.
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from original_ending_normal_assets_oracle import bind as normal_bind,State,Presets,I
from original_lighting_pass_oracle import Native,Input,Pass,Command
from original_lighting_oracle import Oracle
from environment_binding import bind as environment_bind,Lighting
from model_binding import ROOT,Model
from playback_binding import library
class World(C.Structure):
 _fields_=[('matrices',C.POINTER(C.c_float)),('floats',C.c_size_t)]
class Source(C.Structure):
 _fields_=[('model',C.POINTER(Model)),('world',World)]
def bind(lib):
 normal_bind(lib);environment_bind(lib)
 for name,args,result in [
 ('bk_actor_pose_world',[C.c_void_p,C.POINTER(C.c_size_t)],C.POINTER(C.c_float)),
 ('bk_scene_light_registry_create',[C.POINTER(Source),C.c_uint,C.c_void_p],C.c_void_p),
 ('bk_scene_light_registry_destroy',[C.c_void_p],None),
 ('bk_scene_light_registry_input',[C.c_void_p,C.POINTER(Input)],C.c_int),
 ('bk_scene_light_registry_command',[C.c_void_p,C.POINTER(Command)],C.c_int),
 ('bk_scene_light_registry_values',[C.c_void_p,C.POINTER(World),C.c_uint,C.POINTER(Lighting),C.c_void_p],C.c_int),
 ('bk_lighting_pass',[C.POINTER(Input),C.POINTER(Pass)],C.c_int)]:
  f=getattr(lib,name);f.argtypes=args;f.restype=result

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();lib=library();bind(lib);err=C.create_string_buffer(256)
 store=lib.bk_resources_create(err);assert store,err.value
 for pack in ['bk3_08','bk3_04','bk3_03','fambom']:
  assert lib.bk_resources_mount(store,pack.encode(),str(a.data/(pack+'.pp')).encode(),err),err.value
 n=Native(exe);native_light=Oracle(exe);profiles=submissions=snapshots=commands=0;records=[]
 try:
  for group in range(5):
   for variant in range(2):
    state=State();state.pose.world[:]=state.matrix[:]=I;presets=Presets();rng=C.c_uint32(123)
    owner=lib.bk_ending_normal_assets_create(store,group,variant,(C.c_uint32*4)(100,110,120,130),C.byref(rng),C.byref(state),C.byref(presets),err);assert owner,err.value
    environments=[]
    try:
     assert lib.bk_ending_normal_assets_load_background(owner,store,err),err.value
     models=[];worlds=[];payloads=[]
     for actor in [0,4]:
      pose=lib.bk_ending_normal_assets_pose(owner,actor);model=lib.bk_actor_pose_model(pose);count=C.c_size_t();ptr=lib.bk_actor_pose_world(pose,C.byref(count));assert ptr
      world=(C.c_float*count.value)(*ptr[:count.value]);models.append(model);worlds.append(world)
      env=lib.bk_model_environment_create(model,world,err);assert env,err.value;environments.append(env)
      off=next(c.offset for c in model.contents.chunks[:model.contents.chunk_count] if c.tag==b'LIGH')
      raw=C.string_at(model.contents.source,model.contents.source_size)
      payloads.append([raw[off+i*172+68:off+(i+1)*172] for i in range(env.contents.light_count)])
     for order in [(0,1),(1,0)]:
      src=(Source*2)(*[Source(models[i],World(worlds[i],len(worlds[i]))) for i in order]);registry=lib.bk_scene_light_registry_create(src,2,err);assert registry,err.value
      try:
       inp=Input();inp.mode=1;inp.scene_root=90;inp.objects[0]=4;inp.objects[4]=1;inp.objects[5]=2
       assert lib.bk_scene_light_registry_input(registry,C.byref(inp))
       assert inp.mode==1 and inp.objects[0]==4 and inp.scene_root==90
       flat=[(i,j,env.contents.lights[j]) for i in order for env in [environments[i]] for j in range(env.contents.light_count)]
       table=0x3009000;n.word(0x645614,table);n.word(0x645618,len(flat));n.u.mem_write(table,bytes(0x1000));n.u.mem_write(n.key,b'BK3_L\0');n.commands=[]
       for k,(i,j,light) in enumerate(flat):
        p=n.light+k*0x100;f=0x3004000+k*0x400;parent=0x3008000+k*0x100
        n.u.mem_write(p,bytes(0x100));n.word(p+4,0x3ef);n.word(p+0x6c,1);n.word(p+0xe0,f);n.word(f+0x22c,parent)
        n.u.mem_write(p+8,light.name+b'\0');n.u.mem_write(p+0x7c,bytes(light.diffuse))
        n.u.mem_write(parent+8,models[i].contents.frames[models[i].contents.frames[light.frame_index].parent_index].name+b'\0')
        n.word(table+k*0x88+0x80,p);n.word(table+k*0x88+0x84,0x3ef)
       n.call(0x4a4140,b'');n.call(0x4a435a,struct.pack('<I',n.key));n.call(0x4a4438,b'')
       assert inp.light_count==struct.unpack('<I',n.u.mem_read(0x705738,4))[0]==len(flat)
       groups=struct.unpack('<'+'i'*len(flat),n.u.mem_read(0x7056f8,len(flat)*4));ranks=struct.unpack('<'+'i'*len(flat),n.u.mem_read(0x70573c,len(flat)*4))
       assert [l.group for l in inp.lights[:inp.light_count]]==list(groups)
       assert [l.ambient_rank for l in inp.lights[:inp.light_count]]==list(ranks)
       caches=(World*2)(*[World(worlds[i],len(worlds[i])) for i in order]);out=Lighting()
       assert lib.bk_scene_light_registry_values(registry,caches,2,C.byref(out),err),err.value
       assert [round(v*255) for v in out.ambient]==[(n.commands[-1][2]>>shift)&255 for shift in [16,8,0]]
       for step in range(32):
        # Current-world reads must not freeze to construction-time values.
        expected=[]
        for i,j,light in flat:
         f=light.frame_index;w=worlds[i]
         if step:w[f*16+12]+=0.125;w[f*16+13]-=0.0625
         submitted,_=native_light.light_state(payloads[i][j],w[f*16:(f+1)*16])
         expected.append(None if submitted is None else struct.unpack('<I25f',submitted))
         submissions+=submitted is not None
        inp.mode=[0,1,2,10][step%4];inp.objects[20]=3
        want=n.run(inp);plan=Pass();assert lib.bk_lighting_pass(C.byref(inp),C.byref(plan))
        assert [(c.kind,c.target,c.value) for c in plan.commands[:plan.count]]==want
        enabled={};ambient=0
        for c in plan.commands[:plan.count]:
         if c.kind<=1:
          assert lib.bk_scene_light_registry_command(registry,C.byref(c))
          if c.kind==0:ambient=c.value
          else:enabled[c.target]=c.value
         elif c.kind==2:
          assert lib.bk_scene_light_registry_values(registry,caches,2,C.byref(out),err),err.value
          assert [round(v*255) for v in out.ambient]==[(ambient>>shift)&255 for shift in [16,8,0]]
          selected=[e for k,e in enumerate(expected) if e is not None and enabled.get(k)]
          assert out.point_count+out.spot_count==len(selected)
          pi=si=0
          for values in selected:
           kind,*v=values
           if kind==1:p=out.points[pi];pi+=1
           else:
            assert kind==2
            spot=out.spots[si];si+=1;p=spot.point
            assert list(spot.direction)==v[15:18]
            assert [spot.falloff,spot.theta,spot.phi]==[v[19],v[23],v[24]]
           assert list(p.position)==v[12:15]
           assert list(p.diffuse)==v[:3] and list(p.specular)==v[4:7] and list(p.ambient)==v[8:11]
           assert [p.range,p.attenuation0,p.attenuation1,p.attenuation2]==[v[18],*v[20:23]]
          snapshots+=1
        commands+=plan.count
       saved=bytes(out);caches[1].floats-=1
       assert not lib.bk_scene_light_registry_values(registry,caches,2,C.byref(out),err);assert bytes(out)==saved
       assert not lib.bk_scene_light_registry_command(registry,C.byref(Command(1,len(flat),1)))
       assert not lib.bk_scene_light_registry_command(registry,C.byref(Command(1,0,2)))
       records.append(dict(group=group,variant=variant,order=order,groups=groups,ranks=ranks));profiles+=1
      finally:lib.bk_scene_light_registry_destroy(registry)
    finally:
     for env in environments:lib.bk_model_environment_destroy(env)
     lib.bk_ending_normal_assets_destroy(owner)
 finally:lib.bk_resources_destroy(store)
 report=dict(passed=True,profiles=profiles,submissions=submissions,snapshots=snapshots,commands=commands,records=records,max_error=0,exe_sha256=hashlib.sha256(exe).hexdigest(),scope=__doc__)
 (ROOT/'local/original-ending-lighting-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',json.dumps(report),flush=True)
if __name__=='__main__':main()
