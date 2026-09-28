"""Portable PCM DSP policy checked against an independent rational timeline."""
import ctypes as C
import math
import random
import struct
import unittest
from model_binding import library

def f32(v):return C.c_float(v).value
class Phase(C.Structure):
    _fields_=[('frame',C.c_size_t),('fraction',C.c_uint32)]
class PcmMix(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lib=library();l=cls.lib;fp=C.POINTER(C.c_float)
        for name,args,result in [('bk_pcm_decode',[C.c_void_p,C.c_size_t,C.c_void_p],C.c_void_p),('bk_pcm_destroy',[C.c_void_p],None),('bk_pcm_mix',[C.c_void_p,C.c_uint32,C.c_uint64,C.c_int,C.c_int32,C.c_int32,fp,C.c_size_t,C.c_void_p],C.c_int),('bk_pcm_position',[C.c_void_p,C.c_uint32,C.c_uint64,C.c_int,C.POINTER(C.c_size_t),C.POINTER(C.c_int)],C.c_int),('bk_pcm_quantize',[fp,C.POINTER(C.c_int16),C.c_size_t],C.c_int)]:
            fn=getattr(l,name);fn.argtypes=args;fn.restype=result
    def pcm(self,rate,channels,samples):
        data=struct.pack('<'+'h'*len(samples),*samples);fmt=struct.pack('<HHIIHH',1,channels,rate,rate*channels*2,channels*2,16)
        raw=b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(data))+data
        wav=b'RIFF'+struct.pack('<I',len(raw))+raw;err=C.create_string_buffer(256);p=self.lib.bk_pcm_decode(wav,len(wav),err);self.assertTrue(p,err.value);return p
    def test_rational_resampling(self):
        rng=random.Random(923)
        for rate in [11025,22000,22050,22090,44100,48000,96000]:
            for channels in [1,2]:
                samples=[rng.randrange(-32768,32768) for _ in range(37*channels)];p=self.pcm(rate,channels,samples)
                try:
                    for output_rate in [22050,44100,48000]:
                        for loop in [0,1]:
                            for start in [0,1,34,100,2**64-33]:
                                for volume,pan in [(0,0),(-2000,0),(-6000,-1500),(-10000,0),(0,10000),(0,-10000)]:
                                    out=(C.c_float*64)(*[.25]*64);error=C.create_string_buffer(256)
                                    self.assertTrue(self.lib.bk_pcm_mix(p,output_rate,start,loop,volume,pan,out,32,error),error.value)
                                    pos=C.c_size_t();ended=C.c_int();self.assertTrue(self.lib.bk_pcm_position(p,output_rate,start,loop,C.byref(pos),C.byref(ended)))
                                    base=start*rate//output_rate
                                    self.assertEqual((pos.value,ended.value),(base%37,0) if loop else (min(base,37),int(base>=37)))
                                    for frame in range(32):
                                        numerator=(start+frame)*rate;at,phase=divmod(numerator,output_rate)
                                        for channel in range(2):
                                            v=.25
                                            if loop or at<37:
                                                at%=37;next_=(at+1)%37 if loop else min(at+1,36)
                                                ix=0 if channels==1 else channel
                                                a,b=samples[at*channels+ix],samples[next_*channels+ix]
                                                shift=-pan if channel==0 and pan>0 else pan if channel==1 and pan<0 else 0
                                                def gain(x):return 0 if x<=-10000 else 10**(x/2000)
                                                v=f32(v+f32((a+(b-a)*(phase/output_rate))*(gain(volume)*gain(shift))))
                                            self.assertEqual(out[frame*2+channel],v)
                finally:self.lib.bk_pcm_destroy(p)
    def test_chunk_invariance(self):
        p=self.pcm(22090,1,[-32768,12345,32767,1,-1,1000])
        try:
            a=(C.c_float*400)();b=(C.c_float*400)();error=C.create_string_buffer(256)
            self.assertTrue(self.lib.bk_pcm_mix(p,48000,999,1,-33,1500,a,200,error))
            offset=0
            for frames in [1,7,31,3,100,58]:
                pointer=C.cast(C.byref(b,offset*8),C.POINTER(C.c_float))
                self.assertTrue(self.lib.bk_pcm_mix(p,48000,999+offset,1,-33,1500,pointer,frames,error));offset+=frames
            self.assertEqual(bytes(a),bytes(b))
            held=bytes(b)
            self.assertFalse(self.lib.bk_pcm_mix(p,48000,0,1,1,0,b,200,error));self.assertEqual(bytes(b),held)
        finally:self.lib.bk_pcm_destroy(p)
    def test_piecewise_frequency_phase(self):
        lib=self.lib
        lib.bk_pcm_phase_advance.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint64,C.c_int,Phase,C.POINTER(Phase)]
        lib.bk_pcm_mix_phase.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,Phase,C.c_int,C.c_int32,C.c_int32,C.POINTER(C.c_float),C.c_size_t,C.c_void_p]
        rng=random.Random(0x50de27);error=C.create_string_buffer(256)
        samples=[rng.randrange(-32768,32768) for _ in range(37)]
        p=self.pcm(22090,1,samples)
        try:
            for case in range(4000):
                rate=rng.choice([11025,44100,48000,192000]);hz=rng.choice([100,10001,22090,44100,100000,192000]);loop=case%2
                start=Phase(rng.randrange(37),rng.randrange(rate));elapsed=rng.choice([0,1,32,100000,2**64-1]);out=Phase()
                total=start.frame*rate+start.fraction+elapsed*hz
                if loop:total%=37*rate
                else:total=min(total,37*rate)
                self.assertTrue(lib.bk_pcm_phase_advance(p,rate,hz,elapsed,loop,start,C.byref(out)))
                self.assertEqual((out.frame,out.fraction),divmod(total,rate))
                mixed=(C.c_float*64)()
                self.assertTrue(lib.bk_pcm_mix_phase(p,rate,hz,out,loop,0,0,mixed,32,error),error.value)
                for i in range(32):
                    at,fraction=divmod(total,rate);expected=0
                    if at<37:
                        next_=(at+1)%37 if loop else min(at+1,36)
                        expected=f32(samples[at]+(samples[next_]-samples[at])*fraction/rate)
                        total+=hz
                        if loop:total%=37*rate
                        else:total=min(total,37*rate)
                    self.assertEqual(mixed[i*2],expected)
                    self.assertEqual(mixed[i*2+1],expected)
        finally:lib.bk_pcm_destroy(p)
    def test_quantization_and_invalid_destination(self):
        raw=[-1e20,-32768.6,-32767.5,-.5,.5,32766.5,32767,1e20];a=(C.c_float*8)(*raw);out=(C.c_int16*8)()
        self.assertTrue(self.lib.bk_pcm_quantize(a,out,4));self.assertEqual(list(out),[-32768,-32768,-32768,-1,1,32767,32767,32767])
        held=bytes(out);a[7]=float('nan');self.assertFalse(self.lib.bk_pcm_quantize(a,out,4));self.assertEqual(bytes(out),held)
        p=self.pcm(22050,1,[0,32767]);before=bytes(a);error=C.create_string_buffer(256)
        try:self.assertFalse(self.lib.bk_pcm_mix(p,48000,0,0,0,0,a,4,error));self.assertEqual(bytes(a),before)
        finally:self.lib.bk_pcm_destroy(p)
if __name__=='__main__':unittest.main()
