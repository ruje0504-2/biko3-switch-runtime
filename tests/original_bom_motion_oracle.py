"""Native5234e5/4241e3/42363b + complete49aa50/49ad16/49afde and
49b28f/49b900/49b55f/49bde5. Real math and helpers execute unhooked.
Static constructor guard bits are pre-set; atexit registration is not run.
Explicit cached matrices and retained state; no ending phase/actor dispatch.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX
from original_bom_deform_oracle import Native as Base,Node,F16,identity
from model_binding import ROOT,library
Fp=C.POINTER(C.c_float)
class Manual(C.Structure):_fields_=[('offset',C.c_float*2),('angles',C.c_float*2)]
class Oscillator(C.Structure):_fields_=[('fresh',C.c_uint32),('direction',C.c_uint32)]+[(n,C.c_float) for n in ['amplitude','travel','bound','start']]
class Return(C.Structure):_fields_=[('axes',Oscillator*4),('offset',(C.c_float*2)*2),('complete',(C.c_uint32*2)*2)]
MANUAL=[(0x49aa50,0x704d50,0x704d28),(0x49ad16,0x704d78,0x704cf0),(0x49afde,0x704d40,0x704ce8)]
OSC=[(0x49b55f,[0x556a50,0x704d18,0x704d6c,0x704d00,0x704d80,0x704cf8]),
     (0x49bde5,[0x556a58,0x704d5c,0x704d08,0x704d30,0x704d9c,0x704d88])]
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
def bind(lib):
 lib.bk_matrix_axis_rotation.argtypes=[Fp,Fp,C.c_float]
 lib.bk_node_reference_rotation.argtypes=[C.POINTER(Node),Fp,Fp,C.c_float,C.c_void_p]
 lib.bk_node_local_rotation.argtypes=[C.POINTER(Node),C.c_int,Fp,C.c_float,C.c_void_p]
 lib.bk_bom_manual_step.argtypes=[C.POINTER(Manual),C.c_int,C.POINTER(Node),Fp,C.c_int32,C.c_int32,C.c_float,C.c_float,C.c_int32,C.c_void_p]
 lib.bk_bom_oscillator_step.argtypes=[C.POINTER(Oscillator),C.c_uint,C.c_float,C.c_uint32,Fp,C.POINTER(C.c_int),C.c_void_p]
 lib.bk_bom_return_init.argtypes=[C.POINTER(Return)];lib.bk_bom_return_init.restype=None
 lib.bk_bom_return_single.argtypes=[C.POINTER(Return),C.POINTER(Node),Fp,Fp,C.c_float,C.c_int32,C.c_uint32,C.c_int32,C.POINTER(C.c_int),C.c_void_p]
 lib.bk_bom_return_multiple.argtypes=[C.POINTER(Return),C.POINTER(C.POINTER(Node)),C.POINTER(Fp),C.c_uint,Fp,C.c_float,C.c_int32,C.c_uint32,C.POINTER(C.c_int),C.c_void_p]
class Native(Base):
 refs=[0x3004000,0x3006000];nodes=[0x3005000,0x3007000];scene=0x3009000;config=0x300a000;value=0x300b000
 def __init__(self,exe):
  super().__init__(exe)
  for p in [0x704de0,0x704d20,0x704d98,0x704d58,0x704d48]:self.word(p,3)
  self.word(0x645600,self.scene)
 def f(self,p,n):return struct.unpack('<'+'f'*n,self.u.mem_read(p,n*4))
 def vf(self,p,v):self.u.mem_write(p,struct.pack('<'+'f'*len(v),*v))
 def setup(self,nodes,refs,scene):
  self.vf(self.scene+0xc0,scene)
  for i in range(2):
   if nodes[i] is not None:self.u.mem_write(self.nodes[i]+0x80,bytes(nodes[i]))
   if refs[i] is not None:self.vf(self.refs[i]+0xc0,refs[i])
   self.word(self.config+0xc4+i*4,self.refs[i] if refs[i] is not None else 0)
   self.word(self.config+0x104+i*4,self.nodes[i] if nodes[i] is not None else 0)
 def node_result(self,i):return Node.from_buffer_copy(bytes(self.u.mem_read(self.nodes[i]+0x80,C.sizeof(Node))))
 def oscillator(self,s,which,index,write):
  addresses=OSC[which][1]
  if write:
   for j,(name,_) in enumerate(Oscillator._fields_):self.word(addresses[j]+4*index,getattr(s,name) if j<2 else bits(getattr(s,name)))
  else:
   for j,(name,_) in enumerate(Oscillator._fields_):setattr(s,name,self.read(addresses[j]+4*index) if j<2 else self.f(addresses[j]+4*index,1)[0])
 def returning(self,s,which,write):
  for i in range(2 if which==0 else 4):self.oscillator(s.axes[i],which,i,write)
  for i in range(1 if which==0 else 2):
   pos=0x704de8 if which==0 else 0x704db0+i*12
   done=0x704df4 if which==0 else 0x704dfc+i*8
   if write:self.vf(pos,s.offset[i]);self.u.mem_write(done,bytes(s.complete[i]))
   else:s.offset[i][:]=self.f(pos,2);s.complete[i][:]=struct.unpack('<2I',self.u.mem_read(done,8))

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
 exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);e=C.create_string_buffer(256);rng=random.Random(0x49aa50)
 worst=0;counts=dict(axis=0,node=0,manual=0,oscillator=0,single=0,multiple=0);finished=0
 def equal(a,b,label):
  nonlocal worst
  for i,(x,y) in enumerate(zip(a,b)):
   d=abs(x-y)/max(1,abs(y));worst=max(worst,d)
   assert math.isfinite(d) and d<3e-5,(label,i,x,y,d)
 def matrix():
  v=identity()
  for i in range(3):
   for j in range(3):v[i*4+j]=rng.uniform(-.1,.1)+(rng.uniform(.8,1.2) if i==j else 0)
  v[12:15]=[rng.uniform(-3,3) for _ in range(3)]
  return F16(*v)
 def node():return Node(matrix(),matrix(),matrix())
 def node_equal(a,b,label):
  assert bytes(a.parent)==bytes(b.parent),(label,'parent')
  equal(a.local,b.local,(label,'local'));equal(a.world,b.world,(label,'world'))
 def osc_equal(a,b,label):
  assert a.fresh==b.fresh and a.direction==b.direction,(label,'flags',a.fresh,b.fresh,a.direction,b.direction)
  equal([getattr(a,x) for x,_ in Oscillator._fields_[2:]],[getattr(b,x) for x,_ in Oscillator._fields_[2:]],label)
 def ret_equal(a,b,which,label):
  for i in range(2 if which==0 else 4):osc_equal(a.axes[i],b.axes[i],(label,'axis',i))
  for i in range(1 if which==0 else 2):
   equal(a.offset[i],b.offset[i],(label,'offset',i));assert bytes(a.complete[i])==bytes(b.complete[i]),(label,'done',i)
 for case in range(1500):
  axis=(C.c_float*3)(*[rng.uniform(-2,2) for _ in range(3)]);radians=C.c_float(rng.uniform(-10,10)).value
  if case%11==0:axis[:]=[0,0,0]
  if case%13==0:axis[:]=[1.000004,0,0]
  n.vf(n.value,axis);n.call(0x5234e5,[n.value+64,n.value,bits(radians)])
  got=F16();assert lib.bk_matrix_axis_rotation(got,axis,radians);equal(got,n.f(n.value+64,16),('axis',case));counts['axis']+=1
  source=node();ref=matrix()
  if case%5==0:source.parent[3]=.001;ref[15]=.9;source.world[11]=.002
  for mode in [-1,0,1,2]:
   dst=Node.from_buffer_copy(bytes(source));n.setup([dst,None],[ref,None],identity());n.vf(n.value,axis)
   if mode==-1:
    n.call(0x4241e3,[n.nodes[0],n.refs[0],n.value,bits(radians)])
    assert lib.bk_node_reference_rotation(C.byref(dst),ref,axis,radians,e),e.value
   else:
    n.call(0x42363b,[n.nodes[0],mode,n.value,bits(radians)])
    assert lib.bk_node_local_rotation(C.byref(dst),mode,axis,radians,e),e.value
   node_equal(dst,n.node_result(0),('node',case,mode));counts['node']+=1
 print('PASS axis/node',counts, 'max',worst,flush=True)
 for kind,(address,pos,ang) in enumerate(MANUAL):
  state=Manual()
  for case in range(2000):
   if case%23==0:state.offset[:]=[rng.uniform(-2,2),rng.uniform(-2,2)]
   dst=node();ref=matrix();dx=rng.choice([0,1,-1,2147483647,-2147483648,rng.randrange(-80,80)]);dy=rng.randrange(-90,90)
   radius=C.c_float([0,.09,-.15,1.000005,.000001][case%5]).value;degrees=C.c_float(rng.uniform(-90,90)).value;flip=case%3-1
   missing=case%29==0
   n.setup([None if missing else dst,None],[ref,None],identity());n.vf(pos,state.offset);n.vf(ang,state.angles)
   words=[0]*11;words[6]=dx&0xffffffff;words[7]=dy&0xffffffff
   if case%19==0 and not missing:
    ref=dst.world;n.word(n.config+0xc4,n.nodes[0])
   nodes=[n.nodes[0] if case%19==0 and not missing else n.refs[0],0 if missing else n.nodes[0]] if kind==2 else [0,n.config]
   n.call(address,words+nodes+[bits(radius),bits(degrees),flip&0xffffffff])
   assert lib.bk_bom_manual_step(C.byref(state),kind,None if missing else C.byref(dst),ref,dx,dy,radius,degrees,flip,e),e.value
   equal(state.offset,n.f(pos,2),('manual',kind,case,'offset'));equal(state.angles,n.f(ang,2),('manual',kind,case,'angles'))
   if not missing:node_equal(dst,n.node_result(0),('manual',kind,case))
   counts['manual']+=1
 print('PASS manual',counts['manual'],'max',worst,flush=True)
 for which,(address,_) in enumerate(OSC):
  for axis in range(2 if which==0 else 4):
   state=Oscillator(fresh=1);offset=C.c_float(17)
   for case in range(2000):
    if case%37==0:state=Oscillator(fresh=1)
    distance=C.c_float(rng.uniform(-.3,.3)).value;ms=rng.choice([0,1,16,33,100,1000,0xffffffff]);done=C.c_int()
    n.oscillator(state,which,axis,True);n.vf(n.value,[offset.value]);n.call(address,[bits(distance),n.value,axis,ms]);result=n.u.reg_read(UC_X86_REG_EAX)
    expected=Oscillator();n.oscillator(expected,which,axis,False)
    assert lib.bk_bom_oscillator_step(C.byref(state),axis,distance,ms,C.byref(offset),C.byref(done),e),e.value
    assert done.value==result;(osc_equal(state,expected,('oscillator',which,axis,case)))
    equal([offset.value],n.f(n.value,1),('oscillator output',which,axis,case));counts['oscillator']+=1
 print('PASS oscillators',counts['oscillator'],'max',worst,flush=True)
 for which in range(2):
  state=Return();lib.bk_bom_return_init(C.byref(state))
  for case in range(3000):
   if case%41==0:lib.bk_bom_return_init(C.byref(state))
   nodes=[node(),node()];refs=[matrix(),matrix()];scene=matrix();count=1 if which==0 else 1+case%2
   # Explicit near-convergence distances exercise latch completion/order.
   if case%5==0:
    for i in range(2):nodes[i].world[12:15]=refs[i][12:15]
   present=[case%43!=0,case%47!=0]
   n.setup([nodes[i] if present[i] else None for i in range(2)],refs,scene);n.returning(state,which,True)
   native_indices=[0,1]
   if all(present):
    if case%19==0:
     refs[0]=nodes[0].world;n.word(n.config+0xc4,n.nodes[0])
    elif which==1 and count==2 and case%19==1:
     refs=[nodes[1].world,nodes[0].world]
     n.word(n.config+0xc4,n.nodes[1]);n.word(n.config+0xc8,n.nodes[0])
    elif which==1 and count==2 and case%19==2:
     nodes[1]=nodes[0];native_indices[1]=0;n.word(n.config+0x108,n.nodes[0])
   degrees=C.c_float([.09,7.5,0,-15][case%4]).value;flip=case%3-1;ms=[0,16,33,333,0xffffffff][case%5];reset=int(case%17==0);done=C.c_int()
   n.call(0x49b28f if which==0 else 0x49b900,[0 if which==0 else count-1,n.config,bits(degrees),flip&0xffffffff,ms,reset]);result=n.u.reg_read(UC_X86_REG_EAX)
   expected=Return.from_buffer_copy(bytes(state));n.returning(expected,which,False)
   if which==0:ok=lib.bk_bom_return_single(C.byref(state),C.byref(nodes[0]) if present[0] else None,refs[0],scene,degrees,flip,ms,reset,C.byref(done),e)
   else:ok=lib.bk_bom_return_multiple(C.byref(state),(C.POINTER(Node)*2)(*[C.pointer(nodes[i]) if present[i] else None for i in range(2)]),(Fp*2)(*refs),count,scene,degrees,flip,ms,C.byref(done),e)
   assert ok,(which,case,e.value);assert done.value==result,('return result',which,case,done.value,result)
   ret_equal(state,expected,which,('return',which,case))
   for i in range(count):
    if present[i]:node_equal(nodes[i],n.node_result(native_indices[i]),('return',which,case,i))
   counts['single' if which==0 else 'multiple']+=1;finished+=done.value
 print('PASS returns',counts['single'],counts['multiple'],'completed',finished,'max',worst,flush=True)
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),counts=counts,completed=finished,max_normalized_error=worst,scope=__doc__)
 (ROOT/'local/original-bom-motion-oracle.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
