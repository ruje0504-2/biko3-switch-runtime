"""Execute original4097d6/409a94 with repeated targets bound to shared frames.

Both complete group loops and SRT math run unhooked. Synthetic reversed orders
have more tracks than frames; Japanese h02_55 contributes15 repeated targets.
No renderer, face deformation or gameplay timing is implied.
"""
import argparse, ctypes as C, hashlib, json, math, random, struct
from pathlib import Path
from original_placed_pose_oracle import Native
from animation_binding import library
from model_binding import ROOT, decode
from test_model import fixture, chunk
from bk3_assets import Archive

class Group(Native):
    group, array = 0x20152000, 0x20153000
    def bind(self, data, model):
        super().bind(data, model)
        self.word(self.group+0x78, len(self.tracks))
        self.word(self.group+0x7c, self.array)
        for index, (obj, _, frame, _) in enumerate(self.tracks):
            self.word(self.array+index*4, obj)
            self.word(obj+0x74, self.frames+frame*0x400)
    def sample_group(self, model, t, loop, blend):
        # This API compares samples, not the separate playback plain-time cache.
        self.vector(self.group+0x74, [-1])
        for obj, _, _, _ in self.tracks: self.word(obj+0x8c, loop)
        args = struct.pack('<If', self.group, t)
        if blend is not None: args += struct.pack('<2f', *blend)
        self.call(0x4097d6 if blend is None else 0x409a94, args)
        local = [struct.unpack('<16f', self.uc.mem_read(self.frames+i*0x400+0x80, 64))
                 for i in range(model.frame_count)]
        return self.compose_world(model, local)

def synthetic(reverse):
    tracks = []
    for track in range(3):
        keys = []
        for i in range(3):
            key = bytearray(220)
            struct.pack_into('<f', key, 0, i*(5.5+track))
            for offset in [4,20,36]: struct.pack_into('<I', key, offset, 1)
            struct.pack_into('<3f', key, 8, 100*track+10*i, track-i, i*3)
            struct.pack_into('<3f', key, 40, 1+track*.2, .8+i*.1, 1)
            angle=(i+track)*.23
            struct.pack_into('<4f', key, 116, 0, math.sin(angle), 0, math.cos(angle))
            if i==1: struct.pack_into('<I', key, 4, 0)  # independent sparse preparation
            keys.append(key)
        tracks.append(struct.pack('<6I',101,0,0,0,0,3)+b''.join(keys))
    return fixture(reverse_frames=True)+chunk(b'ANIM', bytes(64)+struct.pack('<2I',123,3)+
        b''.join(reversed(tracks) if reverse else tracks))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe',type=Path);parser.add_argument('data',type=Path)
    parser.add_argument('--output',type=Path,default=ROOT/'local/original-animation-duplicates.json')
    args=parser.parse_args();exe=args.exe.read_bytes();native=Group(exe);lib=library()
    archive=Archive(args.data/'bk3_14.pp')
    actual=archive.read(next(e for e in archive.entries if e.name=='h02_55.x'))
    entries=[('forward',synthetic(False)),('reverse',synthetic(True)),('h02_55.x',actual)]
    error=C.create_string_buffer(256);rng=random.Random(93048);rows=[];digest=hashlib.sha256()
    for name,data in entries:
        rc,model,message=decode(lib,data);assert rc==1,message
        animation=lib.bk_model_animation_create(model,error);assert animation,error.value
        try:
            m=model.contents;native.bind(data,m);end=lib.bk_model_animation_duration(animation)
            out=(C.c_float*(m.frame_count*16))();worst=0;checks=0
            times=[0,.001,5.5,7.5,11,15,end,end+.5,end+1.5,2*end+1.75]
            times += [rng.uniform(0,3*end) for _ in range(54)]
            for t in times:
                t=C.c_float(t).value
                for loop in [0,1]:
                    for blend in [None,(C.c_float(end*.63).value,C.c_float(.375).value)]:
                        if blend is None: ok=lib.bk_model_animation_sample(animation,t,loop,out,len(out),error)
                        else: ok=lib.bk_model_animation_blend(animation,t,*blend,loop,out,len(out),error)
                        assert ok,error.value
                        expected=native.sample_group(m,t,loop,blend)
                        for i,(got,want) in enumerate(zip(out,expected)):
                            delta=abs(got-want)/max(1,abs(want));worst=max(worst,delta)
                            assert math.isfinite(delta) and delta<3e-5,(name,t,loop,blend,i,got,want,delta)
                        digest.update(bytes(out));checks+=1
            rows.append(dict(name=name,sha256=hashlib.sha256(data).hexdigest(),frames=m.frame_count,
                tracks=len(native.tracks),duplicates=len(native.tracks)-len({t[2] for t in native.tracks}),
                samples=checks,max_relative_error=worst))
            print('PASS duplicate animation',name,checks,flush=True)
        finally:lib.bk_model_animation_destroy(animation);lib.bk_model_destroy(model)
    report=dict(passed=True,scope=__doc__,exe_sha256=hashlib.sha256(exe).hexdigest(),
        samples=sum(r['samples'] for r in rows),state_sha256=digest.hexdigest(),records=rows)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS animation duplicates:',json.dumps(report,sort_keys=True),flush=True)

if __name__=='__main__':main()
