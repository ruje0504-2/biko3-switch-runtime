"""Actual4db3e4/4797dc and4d5e60..4d6420, with unhooked geometry.
Only graphics IO, keys and DirectSound status are boundaries. Shared original
normal action tables, actual camera-local/target math, live callbacks and ring
snapshots are compared. Node matrices remain explicit fixtures, not loaders.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_ending_stage_ui_oracle import Native,Stage,bind,install,same
from original_detached_sprite_oracle import Frame as DrawFrame,null_write
from original_ending_ui_cursor_oracle import Bindings as CursorBindings,Notices
from original_ending_frame_oracle import State as Frame,FIELDS,BYTES
from original_ending_control_oracle import State as Control
from original_ending_auxiliary_oracle import State as Auxiliary
from original_ending_ui_pick_oracle import Bindings as Pick,ident,M,P
import original_ending_ui_oracle as m
from model_binding import ROOT,library
F=C.c_float;I=C.c_int32;Pair=I*2
class Bindings(C.Structure):
 _fields_=[('frame',C.POINTER(Frame)),('control',C.POINTER(Control)),('auxiliary',C.POINTER(Auxiliary)),
 ('stage3',C.POINTER(I)),('clip',C.POINTER(I)),('open',C.POINTER(I)),('actions',C.POINTER(I)),
 ('targets',C.POINTER(Pair)),('target_count',C.c_size_t),('camera',C.POINTER(F)),('pick',C.POINTER(Pick)),
 ('unavailable',C.POINTER(I)),('item',C.POINTER(C.c_uint8))]
Voice=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int),C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('key',m.KEY),('voice',Voice)]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('--cursors',action='store_true');a=ap.parse_args();with_cursors=a.cursors;exe=a.exe.read_bytes()
 n=Native(exe);null_write(n) if with_cursors else None;lib=library();bind(lib);rng=random.Random(0x4db3e4);e=C.create_string_buffer(256)
 args=[C.POINTER(m.Ui),C.POINTER(Stage),C.POINTER(Bindings),C.POINTER(F),F]
 for name in ['normal','third']:getattr(lib,'bk_ending_ui_select_'+name).argtypes=args+[C.POINTER(I),C.POINTER(DrawFrame),C.c_void_p]
 lib.bk_ending_ui_select.argtypes=args+[C.c_uint8,C.POINTER(Ops),C.POINTER(I),C.POINTER(DrawFrame),C.c_void_p]
 lib.bk_ending_ui_cursor.argtypes=[C.POINTER(m.Ui),C.POINTER(Stage),C.POINTER(CursorBindings),I,C.c_uint8,C.POINTER(F),F,C.POINTER(DrawFrame),C.c_void_p]
 n.wi(0x300e024,0x300e100);n.wi(0x300d800,0x300e000)
 def mutate_native():
  n.wi(0x721ee4,2);n.wi(0x721eec,4);n.wi(0x721ef0,2);n.wi(0x7220e0,1);n.wi(0x721ed0,4);n.wi(0x72210c,0)
 def hook(u,addr,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=n.ri(sp);pop=4
  if addr==0x4b76c2:
   code,mode,extra=struct.unpack('<3I',u.mem_read(sp+4,12));assert mode==2 and extra==0
   n.trace.append(('key',code,I(n.ri(0x722110)).value));value=n.keys[code]
   if n.mutate:mutate_native()
  else:
   ptr,dst=struct.unpack('<2I',u.mem_read(sp+4,8));assert ptr==0x300d800
   n.trace.append(('voice',));n.wi(dst,n.flags);value=n.hr;pop=12
   if n.mutate:mutate_native()
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
 n.u.hook_add(UC_HOOK_CODE,hook,begin=0x4b76c2,end=0x4b76c2)
 n.u.hook_add(UC_HOOK_CODE,hook,begin=0x300e100,end=0x300e100)
 # Observe child executions without replacing them.
 n.children={a:0 for a in [0x4db3e4,0x4797dc,0x4da76f,0x4da3ca,0x4da4f7]}
 def observe(u,a,size,_):n.children[a]+=1
 for addr in n.children:n.u.hook_add(UC_HOOK_CODE,observe,begin=addr,end=addr)
 policy_frames=0
 frames=[0]*3;draws=vertices=keys=voices=absent_voice=mutated=0;results=[0]*9;skipped_unknown=0
 # No dynamic DLL rebuild while this oracle runs.
 for case in range(2400 if with_cursors else 6000):
  mode=2 if with_cursors else case%3;g=(case//3)%5;v=(case//15)%2
  base,stage=m.Ui(),Stage();flags=(C.c_uint8*6)();gauge=F()
  assert lib.bk_ending_ui_initialize(C.byref(base),1280,flags,C.byref(gauge),e)
  if mode!=0:assert lib.bk_ending_stage_ui_initialize(C.byref(base),C.byref(stage),3,g,v,1280,e)
  base.sprites[50].transform.fade.stage=rng.randrange(6);base.sprites[50].transform.fade.alpha=rng.random()
  f,c,a=Frame(),Control(),Auxiliary();f.group=g
  f.phase=rng.choice([1,2,3,4,5,6,7,8,9,0,10]);f.state_721ee0=rng.choice([0,1,1,3,4]);f.state_721ee4=rng.choice([0,1,2,3]);c.state_721eec=rng.randrange(5)
  a.gate=rng.randrange(6);a.progress=rng.choice([0,.38999995,.39,.5,1]);f.camera_request=rng.choice([-7,0,1])
  s3=I(rng.choice([0,1,1,2,3]));clip=I(rng.choice([-1,0,1,2,3,4,6,7,8,9,10,20]));opened=I(rng.choice([0,0,1,-1]));unavail=(I*2)(rng.randrange(2),rng.randrange(2));item=C.c_uint8(rng.choice([0,1,2,255]))
  # Normal entry chooses4d2320 when variant is nonzero. Do not pair
  # its fallback=-1 with a variant0 action table containing absent rows.
  if mode==1 or (mode==2 and f.phase==3):v=1
  raw=bytes(n.u.mem_read(0x56f7f4+(g*2+v)*320,320));actions=(I*80).from_buffer_copy(raw)
  assert actions[70]==-1
  f.camera_cached=rng.choice([-1,-1,0,4,6,17,32,26,27,38,actions[(case//30)%16*5]])
  f.camera_event=rng.choice([0,1,2,3]);point=P(rng.uniform(0,400),rng.uniform(-5,305));dt=F(rng.choice([0,1/60,.1,.5])).value
  targets=(Pair*39)(*[Pair(rng.randrange(1280),rng.randrange(960)) for _ in range(39)])
  camera=ident();yaw=rng.choice([0,90,180,210,270,330]);camera[8]=math.sin(math.radians(yaw));camera[10]=math.cos(math.radians(yaw))
  world=(M*39)(*[ident() for _ in range(39)]);present=(C.c_uint8*39)(*[1]*39);pos=(F*3)(0,0,-100);matrix=ident()
  for row,chain in enumerate([[24,20,18],[25,21,19],[33,35,31,37,29],[34,36,32,38,30]]):
   for col,node in enumerate(chain):world[node][12]=col*100;world[node][13]=row*100
  if case%7==0:point[:]=[50,0]
  pick=Pick(world,present,39,pos,matrix,matrix,matrix,base.sprites[50].rect[2])
  b=Bindings(C.pointer(f),C.pointer(c),C.pointer(a),C.pointer(s3),C.pointer(clip),C.pointer(opened),actions,targets,39,camera,C.pointer(pick),unavail,C.pointer(item))
  install(n,base,stage)
  for name,addr in FIELDS:n.wi(addr,getattr(f,name))
  for name,addr in BYTES:n.u.mem_write(addr,bytes([getattr(f,name)]))
  n.wi(0x721ee8,s3.value);n.wi(0x721eec,c.state_721eec);n.wi(0x721ef0,a.gate);n.wf(0x721e20,a.progress);n.wi(0x72210c,opened.value)
  n.wi(0x721b28,n.actor);n.wi(n.actor+0x140,clip.value);n.u.mem_write(0x709db8,bytes(actions));n.u.mem_write(0x721f90,bytes(targets));n.u.mem_write(0x6afd0c,bytes(unavail));n.u.mem_write(0x71bcdc,bytes([item.value]))
  n.wi(0x645604,0x300c000);n.u.mem_write(0x300c080,bytes(camera));n.u.mem_write(0x300c0c0,bytes(ident()))
  for i in range(39):n.wi(0x721ef4+i*4,0x3001000+i*0x100);n.u.mem_write(0x3001000+i*0x100+0xc0,bytes(world[i]))
  for addr,value in [(0x71b40c,pos),(0x642fa8,matrix),(0x642830,matrix),(0x642af0,matrix)]:n.u.mem_write(addr,bytes(value))
  n.wf(0x733700,dt);n.keys=[rng.choice([0,1,255,256,0xffffffff]) for _ in range(2)];n.hr=rng.choice([0,0,1]);n.flags=rng.choice([0,1,2,3]);n.mutate=case%11==0
  n.present=case%5!=0;n.wi(0x722454,0x300d800 if n.present else 0)
  visible=rng.choice([0,0,1,2,255]);n.trace=[];n.draws=[];n.null_calls=[]
  ready=I(rng.choice([0,1]));notices=Notices();n.wi(0x719b0c,ready.value)
  for slot in [53,54,55,56,72,73]:n.u.mem_write(m.BASE+slot*0x16c+0x167,b"\0")
  if mode<2:
   n.call(0x4db3e4 if mode==0 else 0x4797dc,struct.pack('<2f',*point));wanted=I(n.u.reg_read(UC_X86_REG_EAX)).value
  else:
   if f.phase in [0,7,10] and visible!=1:
    before=bytes(f);out=DrawFrame();selected=I(777)
    assert not lib.bk_ending_ui_select(C.byref(base),C.byref(stage),C.byref(b),point,dt,visible,None,C.byref(selected),C.byref(out),e)
    assert selected.value==777 and out.count==0 and bytes(f)==before
    skipped_unknown+=1;continue
   n.wf(n.stack-0x124,point[0]);n.wf(n.stack-0x128,point[1]);n.u.mem_write(n.stack-8,bytes([visible]));n.wi(n.stack-0x11c,0x76543210)
   n.segment(0x4d5e60,0x4d677b if with_cursors else 0x4d6420);wanted=I(n.ri(n.stack-0x11c)).value
  trace=[]
  def mutate_port():
   f.state_721ee4=2;c.state_721eec=4;a.gate=2;f.camera_event=1;f.camera_cached=4;opened.value=0
  @m.KEY
  def key(_,code,mode,value,err):
   trace.append(('key',code,f.camera_request));value[0]=n.keys[code]
   if n.mutate:mutate_port()
   return 1
  @Voice
  def voice(_,playing,err):
   if n.present:
    trace.append(('voice',))
    if n.mutate:mutate_port()
   playing[0]=int(n.present and not n.hr and n.flags&1);return 1
  ops=Ops(None,key,voice);out=DrawFrame();selected=I(777)
  if mode<2:ok=getattr(lib,'bk_ending_ui_select_'+['normal','third'][mode])(C.byref(base),C.byref(stage),C.byref(b),point,dt,C.byref(selected),C.byref(out),e)
  else:ok=lib.bk_ending_ui_select(C.byref(base),C.byref(stage),C.byref(b),point,dt,visible,C.byref(ops),C.byref(selected),C.byref(out),e)
  assert ok,(case,mode,e.value)
  assert (selected.value,f.camera_request,trace)==(wanted,I(n.ri(0x722110)).value,n.trace),(case,mode,f.phase,selected.value,wanted,trace,n.trace)
  if with_cursors:
   cb=CursorBindings(C.pointer(f),C.pointer(a),C.pointer(clip),C.pointer(ready),C.pointer(notices))
   assert lib.bk_ending_ui_cursor(C.byref(base),C.byref(stage),C.byref(cb),selected,visible,point,dt,C.byref(out),e),e.value
   policy_frames+=bool(n.null_calls)
  same(n,base,stage,('select',case));assert out.count==len(n.draws)
  for i,(slot,sp,raw) in enumerate(n.draws):
   d=out.draws[i];assert d.slot==slot and d.alpha==sp.alpha and tuple(d.uv)==tuple(sp.uv)
   for j,k in enumerate([0,1,2,3,0,2]):assert tuple(d.xy[2*k:2*k+2])==struct.unpack_from('<2f',raw,32*j);vertices+=1
  frames[mode]+=1;draws+=out.count;results[selected.value]+=1
  keys+=sum(x[0]=='key' for x in trace);voices+=sum(x[0]=='voice' for x in trace);mutated+=n.mutate and bool(trace)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),normal_frames=frames[0],third_frames=frames[1],caller_frames=frames[2],unknown_phase_rejections=skipped_unknown,selected_counts=results,draws=draws,vertices=vertices,key_calls=keys,voice_calls=voices,mutated_callback_frames=mutated,native_child_calls={hex(k):v for k,v in n.children.items()},retail_tables=10,row14_targets=[-1]*10,max_error=0,cursor_composition=with_cursors,null_device_policy_frames=policy_frames,scope=__doc__)
 (ROOT/('local/original-ending-ui-select-cursor-oracle.json' if with_cursors else 'local/original-ending-ui-select-oracle.json')).write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
