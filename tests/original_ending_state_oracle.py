"""Actual4cc582..4cc7e6 prefix: original memset/memcpy, fade request and table
selection. Only platform pointer warp is observed. All mapped scalar fields
are compared, including preserved addresses outside the native clear block.
No x86 resource-pointer clearing is claimed as portable object destruction.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
import original_ending_ui_frame_oracle as ui
import original_ending_reload_oracle as r
from original_ending_control_oracle import STATE_ADDR,FLAG_ADDR,Warp
from original_ending_auxiliary_oracle import ADDR
from model_binding import ROOT,library
I=C.c_int32;F=C.c_float;B=C.c_uint8;S=C.c_int8;P=C.POINTER
class State(C.Structure):
 _fields_=[('frame',ui.t.Frame),('control',ui.t.Control),('auxiliary',ui.t.Auxiliary),('controller',ui.Controller),('selected',I),('stage3_state',I),('open',I),('contact_index',I),('gauge_y',F),('node_state',I*39),('targets',(I*2)*39),('points',(I*2)*3),('choices',I*3),('working',(B*8)*5),('speech_names',(C.c_char*32)*2),('normal_inputs',I*14),('normal_processed',I*14),('normal_ready',I),('normal_target',I),('next_mode',I),('normal_side',S),('final_state',S),('saved_toggles',B*4),('aux_inputs',I*2),('aux_config',(I*6)*5),('alternate',I*2),('unavailable',I*2),('model_paths',(C.c_char*260)*10),('special_cameras',(F*4)*108)]
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('warp',Warp)]
REGIONS=[]
def field(path,addr,size=None):
 typ=State;off=0
 for name in path.split('.'):
  off+=getattr(typ,name).offset;typ=dict(typ._fields_)[name]
 REGIONS.append((off,addr,C.sizeof(typ) if size is None else size,path))
for name,a in ui.t.FIELDS+ui.t.BYTES:field('frame.'+name,a)
field('frame.camera_values',0x721e14);field('frame.camera_table',0x709fcc)
for (name,typ),a in zip(ui.t.Control._fields_[:7],STATE_ADDR):field('control.'+name,a)
for name,a in [('targets',0x70c8d8),('saved_camera',0x71b41c),('variant',0x721b3d),('toggles',0x7220f8),('pause_selection',0xbeeb7d)]:field('control.'+name,a)
for i,a in enumerate(FLAG_ADDR):REGIONS.append((State.control.offset+ui.t.Control.pause_flags.offset+i,a,1,'pause_flags'+str(i)))
for (name,typ),a in zip(ui.t.Auxiliary._fields_,ADDR):field('auxiliary.'+name,a)
for name,a in ui.HINT:field('controller.hints.'+name,a)
for (name,typ),a in zip(ui.t.Normal._fields_,ui.t.NORMAL):field('controller.normal.'+name,a)
for name,a in ui.t.AUX:field('controller.aux.'+name,a)
field('controller.aux.processed',0x6ea178)
for i in range(5):REGIONS.append((State.controller.offset+ui.Controller.aux.offset+ui.t.Aux.group_seen.offset+4*i,0x6ea18c+64*i,4,'group_seen'+str(i)))
for name,addr in [('selected',0x721ed8),('stage3_state',0x721ee8),('open',0x72210c),('contact_index',0x721ed4),('gauge_y',0x721e24),('node_state',0x721e28),('targets',0x721f90),('points',0x7220c8),('choices',0x7220e4),('working',0x721dc6),('normal_inputs',0x709c70),('normal_processed',0x719b64),('normal_ready',0x719b0c),('normal_target',0x719444),('next_mode',0x719b20),('normal_side',0x719b4c),('final_state',0x6d1c0c),('saved_toggles',0x70c8d0),('aux_inputs',0x6ea170),('aux_config',0x6e9fa8),('alternate',0x6afd38),('unavailable',0x6afd0c),('model_paths',0x70c8fc),('special_cameras',0x71944c)]:field(name,addr)
for i in range(2):REGIONS.append((State.speech_names.offset+i*32,0x722224+i*0x120,32,'speech'+str(i)))
# Guarantee the fixture itself does not unknowingly alias two owners.
for i,x in enumerate(REGIONS):
 for y in REGIONS[i+1:]:assert max(x[1],y[1])>=min(x[1]+x[2],y[1]+y[2]),(x,y)
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);self.u.hook_add(UC_HOOK_CODE,self.warp,begin=0x4b768d,end=0x4b768d)
 def write(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def snapshot(self):return b''.join(bytes(self.u.mem_read(a,size)) for _,a,size,_ in REGIONS)
 def warp(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
  self.warped=struct.unpack('<2f',u.mem_read(sp+4,8));self.before=self.snapshot()
  if self.mutate:self.u.mem_write(0x70c8cc,b'\x80');self.write(0x719c50,33)
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,group,variant,scale,origin,overlay,mutate):
  raw=bytes(s)
  self.u.mem_write(0x721b28,b'\x5a'*0x3bec)
  for off,a,size,name in REGIONS:self.u.mem_write(a,raw[off:off+size])
  self.u.mem_write(0xb537e8+0x134,bytes([overlay.stage]));self.u.mem_write(0xb537e8+0x12c,struct.pack('<f',overlay.alpha));self.u.mem_write(0xb537e8+0x138,struct.pack('<f',overlay.speed))
  self.u.mem_write(0x721ad0,struct.pack('<f',scale));self.u.mem_write(0x689f98,bytes(origin));self.u.mem_write(self.stack,struct.pack('<IIIi',self.stop,group,variant&255,0));self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x37f);self.mutate=mutate
  self.u.emu_start(0x4cc582,0x4cc7e6,count=1000000);assert self.u.reg_read(UC_X86_REG_EIP)==0x4cc7e6
  return self.snapshot(),self.u.mem_read(0xb537e8+0x134,1)[0]
def snapshot(s):return b''.join(bytes(s)[off:off+size] for off,_,size,_ in REGIONS)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256);rng=random.Random(0x4cc582)
 lib.bk_ending_state_begin.argtypes=[P(State),P(ui.t.Fade),C.c_uint,S,F,P(I),P(Ops),C.c_void_p]
 frames=6000;mutations=0;amount=sum(x[2] for x in REGIONS);table_hash=hashlib.sha256()
 for case in range(frames):
  s=State.from_buffer_copy(rng.randbytes(C.sizeof(State)));initial=bytes(s);group=case%5;variant=[0,1,-1,-128,127][(case//5)%5];scale=F([.25,.5,961/1280,1,1.3333333,2][case%6]).value
  origin=(I*2)(rng.randrange(-10000000,10000000),rng.randrange(-10000000,10000000));overlay=ui.t.Fade(.75,2,case%6);mutate=case%7==0
  expected,stage=n.run(s,group,variant,scale,origin,overlay,mutate)
  @Warp
  def warp(_,x,y,err):
   assert (x,y)==n.warped and snapshot(s)==n.before
   if mutate:s.controller.hints.once_flags=128;s.frame.finish_elapsed=33
   return 1
  assert lib.bk_ending_state_begin(C.byref(s),C.byref(overlay),group,variant,scale,origin,C.byref(Ops(None,warp)),e),(case,e.value)
  actual=snapshot(s)
  if actual!=expected:
   off=0
   for _,addr,size,name in REGIONS:
    assert actual[off:off+size]==expected[off:off+size],(case,name,hex(addr),actual[off:off+size].hex(),expected[off:off+size].hex())
    off+=size
  assert overlay.stage==stage and overlay.alpha==.75 and overlay.speed==2
  # Struct padding and any newly added unregistered fields cannot change silently.
  masked=bytearray(initial)
  for off,addr,size,name in REGIONS:masked[off:off+size]=bytes(s)[off:off+size]
  assert bytes(masked)==bytes(s)
  table_hash.update(bytes(s.model_paths)+bytes(s.special_cameras));mutations+=mutate
  if case%1000==999:print('PASS ending prefix',case+1,flush=True)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,mapped_regions=len(REGIONS),bytes_per_snapshot=amount,compared_bytes=amount*frames,mutable_warp_callbacks=mutations,tables_sha256=table_hash.hexdigest(),max_error=0,scope=__doc__)
 (ROOT/'local/original-ending-state-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
