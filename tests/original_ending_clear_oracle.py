"""Original BeginScene/EndScene guards, viewport construction and D3DX Clear
forwarding. Execute constructor529b9c and52ab5d..52ab8f up to the real D3D7
Clear slot; only the device boundary is replaced. No Windows GPU claim.
"""
import argparse,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native
from model_binding import ROOT

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);u=n.u;rng=random.Random(0x52ab4c)
 def word(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def read(a,k=1):return struct.unpack('<'+'I'*k,u.mem_read(a,4*k))
 device,table,context=0x3001000,0x3002000,0x3003000
 word(0x6455a0,device);word(device,table)
 for slot,hook in [(0x14,0x300f100),(0x18,0x300f200),(0x28,0x300f300)]:word(table+slot,hook)
 events=[]
 def hook(u,addr,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);count=7 if addr==0x300f300 else 1;ret,*args=read(sp,count+1);events.append((addr,tuple(args)))
  u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4*(count+1));u.reg_write(UC_X86_REG_EIP,ret)
 for addr in [0x300f100,0x300f200,0x300f300]:u.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr)
 # Actual D3DX context defaults and dispatch table.
 u.reg_write(UC_X86_REG_ECX,context);n.call(0x529b9c,b'')
 assert read(context)==(0x540270,) and read(0x540270+0x54)==(0x52ab4c,)
 assert read(context+0xa8)==(0x3f800000,)
 wrappers=0
 for ready in [0,1,2]:
  for rendering in [0,1,2]:
   for active in [0,1,2]:
    for fun in [0x429f86,0x429fd6]:
     events.clear();word(0x645594,ready);word(0x645598,rendering);word(0x6456bc,active);n.call(fun,b'')
     called=bool(ready and rendering and (fun==0x429f86 or active))
     assert events==([(0x300f100 if fun==0x429f86 else 0x300f200,(device,))] if called else [])
     assert read(0x6456bc)==((1 if fun==0x429f86 else 0) if called else active,)
     wrappers+=1
 for case in range(2000):
  flags=2;depth=struct.unpack('<I',struct.pack('<f',rng.random()))[0];color=rng.getrandbits(32);stencil=rng.getrandbits(32)
  word(context+0x78,device);word(context+0xa4,color);word(context+0xa8,depth);word(context+0xac,stencil)
  ebp,esp=0x2007000,0x2006000;word(ebp+8,context);word(ebp+12,flags);u.reg_write(UC_X86_REG_EBP,ebp);u.reg_write(UC_X86_REG_ESP,esp);events.clear()
  u.emu_start(0x52ab5d,0x52ab8f,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52ab8f
  assert events==[(0x300f300,(device,0,0,flags,color,depth,stencil))]
 for case in range(500):
  values=(0,0,640,480,0,0x3f800000) if case==0 else tuple(rng.getrandbits(32) for _ in range(6))
  n.call(0x4a9f10,struct.pack('<7I',0x3004000,*values));assert read(0x3004000,6)==values
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),wrapper_cases=wrappers,clear_cases=2000,viewport_cases=500,clear_slot='IDirect3DDevice7 vtable+0x28',flags=2,default_depth=1,byte_exact=True,scope=__doc__)
 (ROOT/'local/original-ending-clear-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report)
if __name__=='__main__':main()
