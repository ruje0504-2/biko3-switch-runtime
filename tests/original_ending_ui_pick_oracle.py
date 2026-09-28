"""Unhooked4da3ca/4da4f7,4a7b26 and full4da76f including42d4b6.
Explicit cached node worlds, camera LOCAL and device matrices; no target
loading/animation/publication or full UI selection is implied. All24 required
nodes exist in defined native samples. Missing-node stack data is rejected.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX
from original_prop_route_oracle import Native
from model_binding import ROOT,library
F=C.c_float;I=C.c_int32;M=F*16;P=F*2
class Bindings(C.Structure):
 _fields_=[('world',C.POINTER(M)),('present',C.POINTER(C.c_uint8)),('count',C.c_size_t),
           ('camera_position',C.POINTER(F)),('view',C.POINTER(F)),('projection',C.POINTER(F)),
           ('viewport_matrix',C.POINTER(F)),('ring_width',F)]
def ident():return M(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);exe=ap.parse_args().exe.read_bytes()
 n=Native(exe);u=n.u;lib=library();rng=random.Random(0x4da76f);e=C.create_string_buffer(256)
 lib.bk_ending_ui_camera_sector.argtypes=[C.POINTER(F),I,I,I,C.POINTER(C.c_int),C.POINTER(C.c_int)]
 lib.bk_ending_ui_segment_hit.argtypes=[C.POINTER(F)]*3+[F,C.POINTER(C.c_int)]
 lib.bk_ending_ui_pick_targets.argtypes=[C.POINTER(Bindings),C.POINTER(F),C.POINTER(F),C.POINTER(I),C.c_void_p]
 def wi(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def wf(a,v):u.mem_write(a,struct.pack('<f',v))
 def ri():return I(u.reg_read(UC_X86_REG_EAX)).value
 def rf(a):return struct.unpack('<f',u.mem_read(a,4))[0]
 angles=segments=segment_hits=picks=pick_hits=zero_w=zero_depth=0
 categories=[0]*5;selected_counts=[0]*12
 wi(0x645604,n.actor)
 for case in range(4000):
  m=ident();angle=rng.choice([0,90,120,135,140,180,200,220,240,265,320,359,rng.uniform(-360,360)])
  m[8]=math.sin(math.radians(angle));m[10]=math.cos(math.radians(angle))
  if case%9==0:m[8]=rng.choice([-0.,0.,1.,-1.]);m[10]=rng.choice([-0.,0.,1.,-1.])
  # World deliberately differs;424004 copies local+80.
  u.mem_write(n.actor+0x80,bytes(m));u.mem_write(n.actor+0xc0,bytes(ident()))
  cached=rng.choice([-1,0,6,25,26,27,28]);lo,hi=rng.choice([(200,320),(120,240),(140,220),(359,0),(135,265),(0,0),(-1,1)])
  wi(0x721ed0,cached);n.call(0x4da3ca,struct.pack('<2i',lo,hi));category=ri()
  n.call(0x4da4f7,struct.pack('<2i',lo,hi));outside=ri();o,c=C.c_int(-9),C.c_int(-8)
  assert lib.bk_ending_ui_camera_sector(m,cached,lo,hi,C.byref(o),C.byref(c))
  assert (o.value,c.value)==(outside,category),(case,angle,m[8],m[10],lo,hi,o.value,c.value,outside,category)
  categories[category]+=1;angles+=1
 for case in range(16000):
  a=P(rng.uniform(-2000,2000),rng.uniform(-2000,2000));z=P(rng.uniform(-2000,2000),rng.uniform(-2000,2000))
  p=P(rng.uniform(-2200,2200),rng.uniform(-2200,2200));radius=F(rng.choice([0,1,30,100,1000,math.inf])).value
  if case%7==0:z=P(*a)
  if case%9==0:p=P(*a)
  if case%11==0:a=P(0,0);z=P(100,0);p=P(rng.choice([-1,0,50,100,101]),radius if math.isfinite(radius) else 5)
  n.call(0x4a7b26,struct.pack('<7f',*a,*z,*p,radius));wanted=ri();hit=C.c_int(-1)
  assert lib.bk_ending_ui_segment_hit(a,z,p,radius,C.byref(hit))
  assert hit.value==wanted,(case,list(a),list(z),list(p),radius,hit.value,wanted)
  segments+=1;segment_hits+=hit.value
 world=(M*39)();present=(C.c_uint8*39)(*[1]*39)
 for i in range(39):wi(0x721ef4+i*4,0x3001000+i*0x100)
 for case in range(1612):
  view=ident();projection=ident();screen=ident();camera=(F*3)(rng.uniform(-50,50),rng.uniform(-50,50),rng.uniform(-150,150))
  view[12]=rng.uniform(-40,40);view[13]=rng.uniform(-40,40);view[14]=rng.uniform(-100,100)
  if case%3:projection[0]=rng.uniform(.5,2);projection[5]=rng.uniform(.5,2);projection[11]=rng.choice([-.01,.01,.001]);projection[15]=rng.choice([0,1,-1])
  screen[0]=rng.choice([1,50,250]);screen[5]=-screen[0];screen[12]=320;screen[13]=240
  for i in range(39):
   world[i]=ident();world[i][12]=rng.uniform(-150,150);world[i][13]=rng.uniform(-150,150);world[i][14]=rng.uniform(-150,150)
  if case%13==0:
   for i in range(39):world[i][15]=0
   projection[11]=0
  if case%17==0:
   world[24][12:15]=camera[:]
  # Overlapping line targets with different camera distances.
  if case%5==0:
   view=ident();projection=ident();screen=ident();camera[:]=[0,0,100]
   for i in range(39):world[i]=ident();world[i][12]=i*10;world[i][14]=rng.choice([0,50,100])
  focused=None
  if case>=1600:
   view=ident();projection=ident();screen=ident();camera[:]=[0,0,-100]
   for i in range(39):world[i]=ident()
   for row,chain in enumerate([[24,20,18],[25,21,19],[33,35,31,37,29],[34,36,32,38,30]]):
    for column,node in enumerate(chain):world[node][12]=column*100;world[node][13]=row*100
   ids=[24,20,25,21,20,18,21,19,33,35,34,36,35,31,36,32,31,37,32,38,37,29,38,30]
   j=case-1600;left,right=ids[j*2:j*2+2]
   focused=[(world[left][12]+world[right][12])/2,(world[left][13]+world[right][13])/2]
  zero_w+=projection[11]==0 and all(world[i][15]==0 for i in range(39))
  zero_depth+=any(list(world[i][12:15])==list(camera) for i in [24,20,25,21,18,19,33,35,34,36,31,32,37,38,29,30])
  for i in range(39):u.mem_write(0x3001000+i*0x100+0xc0,bytes(world[i]))
  u.mem_write(0x71b40c,bytes(camera));u.mem_write(0x642fa8,bytes(view));u.mem_write(0x642830,bytes(projection));u.mem_write(0x642af0,bytes(screen))
  pointer=P(rng.uniform(-100,800),rng.uniform(-100,600));width=F(rng.choice([1,32,96,300])).value
  if case%5==0:pointer[:]=[rng.randrange(180,381),rng.choice([0,1,10])]
  distance=F(rng.choice([0,50,200,10000]))
  if focused is not None:pointer[:]=focused;width=F(.001).value;distance=F(10000)
  wf(0x73887c,width);wf(0x3000800,distance.value)
  n.call(0x4da76f,struct.pack('<I2f',0x3000800,*pointer));want=(ri(),rf(0x3000800))
  b=Bindings(world,present,39,camera,view,projection,screen,width);selected=I(-99)
  assert lib.bk_ending_ui_pick_targets(C.byref(b),pointer,C.byref(distance),C.byref(selected),e),(case,e.value)
  assert (selected.value,distance.value)==want,(case,selected.value,distance.value,want)
  if focused is not None:assert selected.value==case-1600,(case,selected.value)
  if selected.value>=0:pick_hits+=1;selected_counts[selected.value]+=1
  picks+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),angles=angles,categories=categories,segments=segments,segment_hits=segment_hits,picks=picks,pick_hits=pick_hits,selected_counts=selected_counts,zero_w_input_cases=zero_w,zero_distance_input_cases=zero_depth,focused_selection_cases=12,max_error=0,hooks=[],scope=__doc__)
 (ROOT/'local/original-ending-ui-pick-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
