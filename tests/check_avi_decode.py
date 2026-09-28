"""Independent FFmpeg RGB555 comparison for every MSV1 AVI frame and seeks.

FFmpeg is a host-only verification tool; it is not linked into the runtime.
This checks codec pixels, not the game's VFW texture-copy orientation/layout.
"""
import argparse
import array
import ctypes as C
import hashlib
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path
from model_binding import ROOT, library
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('pack',type=Path);p.add_argument('--ffmpeg',default='ffmpeg');a=p.parse_args()
    lib=library();error=C.create_string_buffer(256)
    for name in ['bk_avi_open','bk_avi_decoder_create','bk_avi_decoder_pixels']:
        getattr(lib,name).restype=C.c_void_p
    lib.bk_avi_open.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p]
    lib.bk_avi_decoder_create.argtypes=[C.c_void_p,C.c_void_p]
    lib.bk_avi_decoder_frame.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p]
    lib.bk_avi_decoder_pixels.argtypes=[C.c_void_p]
    lib.bk_avi_decoder_destroy.argtypes=[C.c_void_p];lib.bk_avi_destroy.argtypes=[C.c_void_p]
    class Info(C.Structure):
        _fields_=[(s,C.c_uint32) for s in ['width','height','frames','scale','rate']]
    lib.bk_avi_info.argtypes=[C.c_void_p];lib.bk_avi_info.restype=C.POINTER(Info)
    version=subprocess.check_output([a.ffmpeg,'-version'],text=True).splitlines()[0]
    pack=Archive(a.pack);results=[];rng=random.Random(20260928)
    with tempfile.TemporaryDirectory(prefix='avi-reference-',dir=ROOT/'local') as temp:
        for entry in pack.entries:
            if not entry.name.lower().endswith('.avi'):continue
            source=pack.read(entry);avi=lib.bk_avi_open(source,len(source),error);assert avi,error.value
            decoder=lib.bk_avi_decoder_create(avi,error);assert decoder,error.value
            try:
                info=lib.bk_avi_info(avi).contents;frame_size=info.width*info.height*2
                path=Path(temp)/'input.avi';path.write_bytes(source)
                raw=Path(temp)/'reference.rgb555'
                subprocess.run([a.ffmpeg,'-v','error','-y','-i',str(path),'-map','0:v:0',
                                '-pix_fmt','rgb555le','-f','rawvideo',str(raw)],check=True)
                words=array.array('H',raw.read_bytes())
                if sys.byteorder!='little':words.byteswap()
                # Bit15 is an unused RGB555 flag, not alpha/image content.
                reference=array.array('H',(x&0x7fff for x in words)).tobytes()
                assert len(reference)==info.frames*frame_size
                order=list(range(info.frames))+list(reversed(range(info.frames)))
                order += [rng.randrange(info.frames) for _ in range(120)]
                for index in order:
                    assert lib.bk_avi_decoder_frame(decoder,index,error),(entry.name,index,error.value)
                    image=C.string_at(lib.bk_avi_decoder_pixels(decoder),frame_size)
                    assert image==reference[index*frame_size:(index+1)*frame_size],(entry.name,index)
                results.append(dict(name=entry.name,source_sha256=hashlib.sha256(source).hexdigest(),
                    width=info.width,height=info.height,frames=info.frames,checked_seeks=len(order),
                    rgb555_sha256=hashlib.sha256(reference).hexdigest(),max_pixel_error=0))
            finally:
                lib.bk_avi_decoder_destroy(decoder);lib.bk_avi_destroy(avi)
    assert results,'no AVI assets'
    report=dict(passed=True,reference=version,checks=results,scope=__doc__)
    (ROOT/'local/avi-decode-verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
if __name__=='__main__':main()
