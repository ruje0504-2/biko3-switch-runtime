"""Compare every game WAV to Python's independent RIFF PCM decoder."""
import argparse,ctypes as C,hashlib,io,json,sys,wave
from pathlib import Path
from model_binding import ROOT,library
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('data',type=Path);a=p.parse_args();lib=library();error=C.create_string_buffer(256)
    for name,args,result in [('bk_pcm_decode',[C.c_void_p,C.c_size_t,C.c_void_p],C.c_void_p),('bk_pcm_destroy',[C.c_void_p],None),('bk_pcm_rate',[C.c_void_p],C.c_uint32),('bk_pcm_channels',[C.c_void_p],C.c_uint32),('bk_pcm_frames',[C.c_void_p],C.c_size_t),('bk_pcm_samples',[C.c_void_p],C.c_void_p)]:
        f=getattr(lib,name);f.argtypes=args;f.restype=result
    records=[];frames=0
    for path in sorted(a.data.glob('*.pp')):
        archive=Archive(path)
        for entry in archive.entries:
            if not entry.name.lower().endswith('.wav'):continue
            raw=archive.read(entry);pcm=lib.bk_pcm_decode(raw,len(raw),error);assert pcm,(path.name,entry.name,error.value)
            try:
                with wave.open(io.BytesIO(raw),'rb') as w:
                    assert w.getcomptype()=='NONE' and w.getsampwidth()==2
                    assert (lib.bk_pcm_rate(pcm),lib.bk_pcm_channels(pcm),lib.bk_pcm_frames(pcm))==(w.getframerate(),w.getnchannels(),w.getnframes())
                    expected=w.readframes(w.getnframes());actual=C.string_at(lib.bk_pcm_samples(pcm),len(expected));assert actual==expected,(entry.name,'samples')
                    records.append(dict(pack=path.name,name=entry.name,rate=w.getframerate(),channels=w.getnchannels(),frames=w.getnframes(),file_sha256=hashlib.sha256(raw).hexdigest(),pcm_sha256=hashlib.sha256(actual).hexdigest()));frames+=w.getnframes()
            finally:lib.bk_pcm_destroy(pcm)
        print(path.name,'PCM files checked',sum(r['pack']==path.name for r in records),flush=True)
    report=dict(passed=True,files=len(records),sample_frames=frames,rates=sorted(set(r['rate'] for r in records)),records=records,scope='CPU owned PCM16 decoded bytes and metadata match independent Python wave for all game WAVs; no playback/device claim.')
    (ROOT/'local/pcm-audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print('PASS',len(records),'WAVs',frames,'sample frames')
if __name__=='__main__':main()
