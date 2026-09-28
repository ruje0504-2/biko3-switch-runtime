"""Opening ma_05 enter1/exit1/idle11 pulse, actual50e633/50e6ba instructions.
Null native sprite handles suppress only device submission, not transition math.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from original_prop_route_oracle import Native
from original_item_notice_oracle import Fade
from model_binding import ROOT,library
class Pulse(C.Structure):_fields_=[('fade',Fade),('direction',C.c_uint8)]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x51158a);total=0
 lib.bk_pulse_sprite_step.argtypes=[C.POINTER(Pulse),C.c_uint8,C.c_float];lib.bk_pulse_sprite_initialize.argtypes=[C.POINTER(Pulse)]
 def check(s,wanted,seconds):
  nonlocal total
  raw=bytearray(0x16c);struct.pack_into('<f',raw,0x12c,s.fade.alpha);struct.pack_into('<f',raw,0x138,s.fade.speed);raw[0x134]=s.fade.stage;raw[0x148:0x14b]=bytes([1,1,11]);raw[0x160]=s.direction
  n.u.mem_write(n.actor,bytes(raw));n.u.mem_write(0x733700,struct.pack('<f',seconds));n.call(0x50e633,struct.pack('<2I',n.actor,wanted));n.call(0x50e6ba,struct.pack('<2I',n.actor,0))
  expected=(struct.unpack('<f',n.u.mem_read(n.actor+0x12c,4))[0],n.u.mem_read(n.actor+0x134,1)[0],n.u.mem_read(n.actor+0x160,1)[0])
  assert lib.bk_pulse_sprite_step(C.byref(s),wanted,seconds)
  assert (s.fade.alpha,s.fade.stage,s.direction)==expected,(total,expected,(s.fade.alpha,s.fade.stage,s.direction));total+=1
 for case in range(3600):
  s=Pulse(Fade(rng.random(),rng.choice([0,.1,2,100]),rng.randrange(6)),rng.choice([0,1,2,255]));check(s,rng.choice([0,1,2,255]),C.c_float(rng.choice([0,1/60,.25,.5,1,10])).value)
 for direction in [0,1,2,255]:
  s=Pulse(Fade(.7,8,5),direction);lib.bk_pulse_sprite_initialize(C.byref(s));assert s.direction==direction and s.fade.stage==1
  for tick in range(600):check(s,1 if tick<300 or tick>=400 else 0,C.c_float(1/60).value)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=total,scope=__doc__)
 (ROOT/'local/original-opening-prompt-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS opening prompt',total,'steps')
if __name__=='__main__':main()
