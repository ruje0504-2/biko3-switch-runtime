"""Image-absent50e6ba and cold mode0, plus the real4d66ab..4d66ed popup pair.
Defined native cases run without animation substitutions. Some original paths
call43f230/43f2f7 on null: prove the access fault separately, then suppress ONLY
those invalid device writes as an explicit portable policy. Report separately.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE,UcError,UC_ERR_READ_UNMAPPED
from unicorn.x86_const import *
from original_effect_sprite_oracle import Native as EffectNative,Effect,snapshot,bind as bind_effect
from original_item_notice_oracle import Fade
from original_ending_stage_ui_oracle import Native as StageNative,Stage,bind as bind_stage,install,same,RELEASE
import original_ending_ui_oracle as m
from model_binding import ROOT,library
class Frame(C.Structure):_fields_=[('count',C.c_uint),('draws',m.Draw*128)]
def null_write(n):
 def hook(u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP)
  if n.ri(sp+4):return
  n.null_calls.append(a);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,n.ri(sp))
 n.null_calls=[]
 for a in [0x43f230,0x43f2f7]:n.u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();lib=library();bind_effect(lib);bind_stage(lib);e=C.create_string_buffer(256);rng=random.Random(0x50e739)
 lib.bk_effect_sprite_advance_detached.argtypes=[C.POINTER(Effect),C.POINTER(C.c_float),C.c_float]
 lib.bk_ending_ui_dispatch_sprite.argtypes=[C.POINTER(m.Ui),C.POINTER(Stage),C.c_uint,C.c_float,C.POINTER(Frame),C.c_void_p]
 faults=[]
 for enter,exit,idle,phase in [(0,0,0,1),(0,0,0,5),(1,1,3,3),(1,1,5,3)]:
  raw=EffectNative(exe);s=Effect(Fade(.3,2,phase),(C.c_float*2)(1,1),(C.c_float*2)(0,0),.4,.01,(C.c_float*2)(.03,.04),(C.c_int32*2)(4,5),enter,exit,idle,0);r=(C.c_float*4)(0,0,96,96)
  raw.install(raw.base,0,s,r);raw.wi(raw.base+0x100,0);raw.wf(0x733700,.1)
  try:raw.call(0x50e6ba,struct.pack('<II',raw.base,0))
  except UcError as exc:
   ip=raw.u.reg_read(UC_X86_REG_EIP);assert exc.errno==UC_ERR_READ_UNMAPPED and ip in [0x43f239,0x43f2fd];faults.append(hex(ip))
  else:raise AssertionError('expected original null device access')
 n=EffectNative(exe);null_write(n);defined=policy=vertices=absent=0
 for case in range(13824):
  enter=case%4;exit=(case//4)%4;idle=[0,3,5][(case//16)%3];phase=(case//48)%6;present=(case//288)%2
  s=Effect(Fade(rng.random(),rng.choice([0,.01,.5,2,100]),phase),(C.c_float*2)(rng.uniform(-1,6),rng.uniform(-1,6)),(C.c_float*2)(rng.uniform(-1,2),rng.uniform(-1,2)),rng.uniform(-400,400),rng.uniform(-3,3),(C.c_float*2)(rng.uniform(-2,2),rng.uniform(-2,2)),(C.c_int32*2)(-1,987),enter,exit,idle,rng.choice([0,1,2,255]))
  r=(C.c_float*4)(rng.uniform(-200,1300),rng.uniform(-200,1000),rng.choice([0,1,97,rng.uniform(0,1300)]),rng.choice([0,1,73,rng.uniform(0,1000)]));dt=C.c_float(rng.choice([0,1/60,.00001,.5,1,10])).value
  n.install(n.base,0,s,r)
  if not present:n.wi(n.base+0x100,0)
  n.draws=[];n.null_calls=[];n.wf(0x733700,dt);n.call(0x50e6ba,struct.pack('<II',n.base,0))
  fn=lib.bk_effect_sprite_advance if present else lib.bk_effect_sprite_advance_detached
  assert fn(C.byref(s),r,dt),(case,snapshot(s));assert snapshot(s)==snapshot(n.read(n.base,0)),(case,present,snapshot(s),snapshot(n.read(n.base,0)))
  assert len(n.draws)==present
  if present:
   q=(C.c_float*8)();assert lib.bk_effect_sprite_quad(C.byref(s),r,q)
   for j,k in enumerate([0,1,2,3,0,2]):assert tuple(q[2*k:2*k+2])==struct.unpack_from('<2f',n.draws[0],32*j);vertices+=1
  else:absent+=1
  if n.null_calls:policy+=1
  else:defined+=1
 print('PASS detached independent',defined,policy,absent,flush=True)
 # Actual unconditional popup tail through cold start, load, release, other
 # resource profiles and reload. Native handle guards determine emitted draws.
 n=StageNative(exe);null_write(n);base=m.Ui();stage=Stage();install(n,base,stage)
 popup_draws=popup_policy=0;previous=None
 for t in range(2400):
  action={120:(2,0,0),400:None,600:(2,1,1),980:None,1400:(3,2,0),1800:None,2000:(2,4,1)}
  if t in action:
   if previous is not None:
    n.segment(*RELEASE[previous]);assert lib.bk_ending_stage_ui_release(C.byref(base),C.byref(stage),previous,e)
   profile=action[t];previous=profile[0] if profile else None
   if profile:
    kind,g,v=profile;n.load_stage(kind,g,v,1001);assert lib.bk_ending_stage_ui_initialize(C.byref(base),C.byref(stage),kind,g,v,1001,e)
  flags=[int(t%160<115),int(t%197<131)] if t>=120 else [0,0]
  dt=C.c_float([0,1/60,1/30,.1][t%4]).value;n.wf(0x733700,dt)
  for i in range(2):n.u.mem_write(m.BASE+(72+i)*0x16c+0x167,bytes([flags[i]]))
  n.draws=[];n.null_calls=[];n.segment(0x4d66ab,0x4d66ed);f=Frame()
  for i in range(2):
   p=stage.sprites[9+i];assert lib.bk_fade_sprite_request(C.byref(p.transform.fade),flags[i])
   assert lib.bk_ending_ui_dispatch_sprite(C.byref(base),C.byref(stage),72+i,dt,C.byref(f),e),(t,e.value)
  same(n,base,stage,('popup',t));assert f.count==len(n.draws),(t,f.count,len(n.draws))
  for i,(slot,_,raw) in enumerate(n.draws):
   assert f.draws[i].slot==slot
   for j,k in enumerate([0,1,2,3,0,2]):assert tuple(f.draws[i].xy[2*k:2*k+2])==struct.unpack_from('<2f',raw,j*32)
  popup_draws+=f.count;popup_policy+=bool(n.null_calls)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),independent_cases=13824,defined_native_cases=defined,policy_cases=policy,absent_cases=absent,loaded_vertices=vertices,proven_null_faults=faults,popup_frames=2400,popup_draws=popup_draws,popup_policy_frames=popup_policy,max_scalar_error=0,scope=__doc__)
 (ROOT/'local/original-detached-sprite-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
