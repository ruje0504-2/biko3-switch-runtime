"""Run472DCF's actual catalog blit decisions/rectangles in the pinned EXE.
Device surfaces, decoded images and file output are services. Compare portable
RGBA composition to an independent nearest sampler driven by those native
blits, including black-key borders. This is not Windows driver pixel equality.
"""
import argparse,ctypes as C,hashlib,json,struct,sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from model_binding import ROOT,library
from original_prop_route_oracle import Native as Base
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

class Image(C.Structure):
    _fields_=[('width',C.c_uint32),('height',C.c_uint32),('rgba',C.POINTER(C.c_uint8))]
class Native(Base):
    def __init__(self,exe):
        super().__init__(exe);self.u.mem_map(0x4400000,0x10000)
        for i in range(24):self.word(self.handle(i),0x440c000)
        self.word(0x440c008,0x440e000)
        self.word(0x53f0f8,0x440e100)
        for a in [0x44f0b1,0x47346d,0x450807,0x450352,0x4a736b,0x472846,0x440e000,0x440e100]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def handle(self,i):return 0x4400000+i*64
    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v))
    def hook(self,u,a,size,data):
        sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<11I',u.mem_read(sp+4,44));r=0;pop=4
        if a==0x44f0b1:
            assert args[:2]==(1024,768);r=self.handle(23)
        elif a==0x440e100:r=1;pop=12
        elif a==0x440e000:pop=8
        elif a==0x47346d:
            assert args[2]==self.page%5
            for i,p in enumerate(self.photos):
                self.word(0x69e638+i*4,self.handle(3+i) if p else 0)
                self.word(0x69eac8+i*4,p.width if p else 0);self.word(0x69eb18+i*4,p.height if p else 0)
        elif a==0x450807:
            assert args[0]==self.handle(23)
            self.blits.append(((args[5]-self.handle(0))//64,args[1:5],args[6:10],args[10]))
        elif a==0x450352:assert args[:2]==(self.handle(1),0)
        u.reg_write(UC_X86_REG_EAX,r);u.reg_write(UC_X86_REG_EIP,struct.unpack('<I',u.mem_read(sp,4))[0]);u.reg_write(UC_X86_REG_ESP,sp+pop)
    def run(self,page,photos):
        self.page=page;self.photos=photos;self.blits=[]
        self.word(0x69eabc,1);self.word(0x69eb68,1)
        for i in range(3):self.word(0x69e688+i*4,self.handle(i))
        self.call(0x472dcf,struct.pack('<2I',page%5,page))
        return self.blits

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);p.add_argument('data',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    exe=a.exe.read_bytes();assert hashlib.sha256(exe).hexdigest()=='a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    l=library();e=C.create_string_buffer(256)
    l.bk_image_decode.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(Image),C.c_void_p]
    l.bk_image_free.argtypes=[C.POINTER(Image)]
    l.bk_album_catalog_build.argtypes=[C.POINTER(Image)]*3+[C.POINTER(C.POINTER(Image)),C.POINTER(Image),C.c_void_p]
    archive=Archive(a.data/'bk3_19.pp');assets=[]
    for name in ['al_00.bmp','al_29.bmp','al_28.bmp']:
        raw=archive.read(next(x for x in archive.entries if x.name.lower()==name));im=Image()
        assert l.bk_image_decode(raw,len(raw),C.byref(im),e),e.value;assets.append(im)
    # Capture-like, deliberately differing dimensions and channel patterns.
    buffers=[];pictures=[]
    for i in range(20):
        w,h=37+i*13,29+i*7
        raw=bytes(c for y in range(h) for x in range(w) for c in [(x*7+i)%256,(y*11+i*3)%256,(x+y*3)%256,255])
        b=(C.c_uint8*len(raw)).from_buffer_copy(raw);buffers.append(b);pictures.append(Image(w,h,b))
    n=Native(exe);digest=hashlib.sha256();calls=0;pixels=0;cache={}
    def sample(im,rect,dw,dh):
        x0,y0,x1,y1=rect;key=(C.addressof(im.rgba.contents),rect,dw,dh)
        if key not in cache:
            raw=C.string_at(im.rgba,im.width*im.height*4);offset=[(x0+x*(x1-x0)//dw)*4 for x in range(dw)]
            cache[key]=[b''.join(raw[row+ix:row+ix+4] for ix in offset) for row in [(y0+y*(y1-y0)//dh)*im.width*4 for y in range(dh)]]
        return cache[key]
    try:
        for page in range(25):
            photos=[pic if (page+i)%3 and page!=0 else None for i,pic in enumerate(pictures)]
            blits=n.run(page,photos);calls+=len(blits);expected=bytearray(1024*768*4)
            for source,dest,rect,flags in blits:
                im=(assets+pictures)[source];x0,y0,x1,y1=dest;rows=sample(im,rect,x1-x0,y1-y0)
                assert flags in (0,0x8000)
                for y,row in enumerate(rows):
                    at=((y0+y)*1024+x0)*4
                    if not flags:expected[at:at+len(row)]=row
                    else:
                        for x in range(0,len(row),4):
                            if row[x:x+3]!=b'\0\0\0':expected[at+x:at+x+4]=row[x:x+4]
            ptrs=(C.POINTER(Image)*20)(*[C.pointer(im) if im else None for im in photos]);result=Image()
            assert l.bk_album_catalog_build(*[C.byref(im) for im in assets],ptrs,C.byref(result),e),e.value
            assert (result.width,result.height)==(1024,768)
            got=C.string_at(result.rgba,1024*768*4);l.bk_image_free(C.byref(result))
            assert got==expected,('pixels',page,next(i for i,(x,y) in enumerate(zip(got,expected)) if x!=y))
            digest.update(got);pixels+=1024*768
    finally:
        for im in assets:l.bk_image_free(C.byref(im))
    report=dict(passed=True,pages=25,native_blits=calls,pixels=pixels,max_error=0,digest=digest.hexdigest(),scope=__doc__)
    a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
