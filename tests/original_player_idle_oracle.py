"""Full4c1313 idle player: real29 collision meshes, original401b0a request,
wall/ground, matrix placement. Only allocation/lookup and head projection
services are supplied; no fabricated idle movement or cleared velocity.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from original_player_scene_oracle import Native as SceneNative, State as Scene, Input as Input
from original_player_spatial_oracle import State, Placement
from original_player_movement_oracle import State as Movement,FIELDS
from original_player_wall_oracle import Wall, Input as WallInput,Vec
from original_collision_oracle import bind
from model_binding import ROOT,library,decode
from bk3_assets import Archive
class Native(SceneNative):
 scene=0x726640;clip=0x3002000;root_group=0x3008000;root=0x3009000
 def step_idle(self,s,inp,idle,xan):
  self.prepare(s.scene,inp);u=self.u;p=self.player
  u.mem_write(p,struct.pack('<I',self.clip));u.mem_write(self.clip,xan[512:]);u.mem_write(self.clip+0x160,struct.pack('<I',self.root_group));u.mem_write(self.root_group+0x14,struct.pack('<I',self.root));u.mem_write(self.root,bytes(0x300));u.mem_write(self.root+0x100,struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1));u.mem_write(p+0x14,struct.pack('<i',idle))
  for name,offset,fmt in FIELDS:
   v=getattr(s.movement,name);u.mem_write(p+offset,struct.pack('<'+fmt,*(v if isinstance(v,C.Array) else [v])))
  u.mem_write(p+0x2a8,struct.pack('<f',s.vertical_position))
  self.call(0x4c1313,struct.pack('<II',0x3001000,p),limit=30000000)
  out=State.from_buffer_copy(s);out.scene=self.read(s.scene)
  for name,offset,fmt in FIELDS:
   v=struct.unpack('<'+fmt,u.mem_read(p+offset,struct.calcsize(fmt)))
   if isinstance(getattr(out.movement,name),C.Array):getattr(out.movement,name)[:]=v
   else:setattr(out.movement,name,v[0])
  out.vertical_position=struct.unpack('<f',u.mem_read(p+0x2a8,4))[0]
  assert struct.unpack('<i',u.mem_read(self.clip+0x148,4))[0]==idle
  return out,struct.unpack('<16f',u.mem_read(self.root+0xc0,64))
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);rng=random.Random(0x4c1313);err=C.create_string_buffer(256)
 lib.bk_player_idle_step.argtypes=[C.POINTER(State),C.c_int32,C.c_void_p,C.POINTER(Input),C.POINTER(Placement),C.c_void_p]
 archive=Archive(args.data/'bk3_03.pp');entries={e.name.lower():e for e in archive.entries};actors=Archive(args.data/'bk3_01.pp');xan=actors.read(next(e for e in actors.entries if e.name.lower()=='h00_80.xan'))
 counts=dict(scenes=0,frames=0,compared=0,undefined_geometry=0,walls=0,floors=0);worst=0
 def equal(a,b,context):
  nonlocal worst
  delta=abs(a-b)/max(1,abs(b));worst=max(worst,delta);assert math.isfinite(delta) and delta<4e-5,(context,a,b,delta)
 for file in sorted(args.data.glob('*.atr')):
  filename=file.with_suffix('.x').name;data=archive.read(entries[filename.lower()]);atr=file.read_bytes();ok,model,message=decode(lib,data);assert ok,message
  world=(C.c_float*(model.contents.frame_count*16))();assert lib.bk_model_world_matrices(model,world,len(world),err)
  collision=lib.bk_collision_create(model,world,len(world),filename.upper().encode(),atr,len(atr),err);assert collision,err.value
  try:
   n.create(model.contents,world,filename.upper().encode(),atr)
   meshes=[lib.bk_collision_mesh(collision,i).contents for i in range(lib.bk_collision_count(collision))]
   points=[Vec(*(sum(m.vertices[m.indices[i+k]][j] for k in range(3))/3 for j in range(3))) for m in meshes for i in range(0,m.index_count,3)]
   for case in range(32):
    point=Vec(*rng.choice(points));point[0]+=rng.uniform(-2,2);point[1]+=rng.choice([-14,0,1,14]);point[2]+=rng.uniform(-2,2)
    camera=Vec(point[0]+27,point[1]+28,point[2]+36)
    s=State(movement=Movement(position=point,previous=Vec(99,100,-10),velocity=Vec(1,-7,-2),yaw=rng.uniform(-30,390),pitch=48,turn=(C.c_float*2)(2,-3),acceleration=1.5,action=2,move_latch=1,interaction_mode=2),scene=Scene(wall=Wall(position=point,camera_distance=200,normal=Vec(.25,.5,.75),near_wall=1),wall_heading=77,wall_name=b'old',surface_name=b'ground',any_wall=1),vertical_position=point[1]+rng.choice([5,13,20]))
    inp=Input(wall=WallInput(previous=Vec(600,700,800),motion=Vec(999,888,777),height=999,camera=camera,rays=(Vec*7)(*(camera for _ in range(7)))),head_distance=rng.choice([99,100,101]),projected_depth=.9,screen_scale=(C.c_float*2)(.625,.625),screen_position=(C.c_int32*2)(320,240),npc_hidden=case%4==0,excluded_surface=b'')
    old=State.from_buffer_copy(s);idle=case%4;expected,root=n.step_idle(s,inp,idle,xan);placement=Placement();assert lib.bk_player_idle_step(C.byref(s),idle,collision,C.byref(inp),C.byref(placement),err),(file.name,case,err.value)
    assert s.movement.action==idle and s.movement.interaction_mode==0 and tuple(s.movement.previous)==tuple(old.movement.position) and s.vertical_position==old.vertical_position
    assert s.movement.move_latch==old.movement.move_latch and bytes(s.movement.velocity)==bytes(old.movement.velocity) and bytes(s.movement.turn)==bytes(old.movement.turn)
    counts['frames']+=1;counts['walls']+=s.scene.any_wall;counts['floors']+=bool(s.scene.surface_name)
    if s.scene.wall.missing_projection:
     counts['undefined_geometry']+=1;continue
    counts['compared']+=1
    assert (s.scene.npc_in_view,s.scene.any_wall,s.scene.wall.near_wall,s.scene.wall_name,s.scene.surface_name)==(expected.scene.npc_in_view,expected.scene.any_wall,expected.scene.wall.near_wall,expected.scene.wall_name,expected.scene.surface_name),(file.name,case,'scene')
    for a,b in zip([*s.movement.position,*s.scene.wall.normal,s.scene.wall_heading,*placement.world],[*expected.movement.position,*expected.scene.wall.normal,expected.scene.wall_heading,*root]):equal(a,b,(file.name,case))
    assert tuple(s.scene.wall.rays_blocked)==tuple(expected.scene.wall.rays_blocked)
    if not s.scene.wall.singular_camera:equal(s.scene.wall.camera_distance,expected.scene.wall.camera_distance,(file.name,case,'camera'))
   counts['scenes']+=1
  finally:lib.bk_collision_destroy(collision);lib.bk_model_destroy(model)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),**counts,max_relative_error=worst,scope=__doc__,hooks=['collision allocation/lookup','cached head screen projection'],undefined_geometry_policy='existing first zero-length edge correction skipped; do not claim original uninitialized locals equivalent')
 (ROOT/'local/original-player-idle-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
