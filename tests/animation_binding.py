"""Host-only bindings for the explicitly supported camera ANIM adapter."""
import ctypes as C
from model_binding import library as model_library, Model
class PoseSample(C.Structure):
    _fields_=[('from_tick',C.c_float),('to_tick',C.c_float),('weight',C.c_float),('blend',C.c_int),('loop',C.c_int)]
class RootTransform(C.Structure):
    _fields_=[('frame',C.c_uint32),('world',C.c_float*16)]

def library():
    lib=model_library()
    lib.bk_model_animation_create.argtypes=[C.POINTER(Model),C.c_void_p]
    lib.bk_model_animation_create.restype=C.c_void_p
    lib.bk_model_animation_destroy.argtypes=[C.c_void_p]
    lib.bk_model_animation_track_count.argtypes=[C.c_void_p]
    lib.bk_model_animation_track_count.restype=C.c_uint32
    lib.bk_model_animation_duration.argtypes=[C.c_void_p]
    lib.bk_model_animation_duration.restype=C.c_float
    lib.bk_model_animation_sample.argtypes=[C.c_void_p,C.c_float,C.c_int,C.POINTER(C.c_float),C.c_size_t,C.c_void_p]
    lib.bk_model_animation_sample.restype=C.c_int
    lib.bk_model_animation_blend.argtypes=[C.c_void_p,C.c_float,C.c_float,C.c_float,C.c_int,C.POINTER(C.c_float),C.c_size_t,C.c_void_p]
    lib.bk_model_animation_blend.restype=C.c_int
    lib.bk_model_animation_pose.argtypes=[C.c_void_p,C.POINTER(PoseSample),C.POINTER(RootTransform),C.POINTER(C.c_float),C.c_size_t,C.c_void_p]
    lib.bk_model_animation_pose.restype=C.c_int
    lib.bk_model_find_frame.argtypes=[C.POINTER(Model),C.c_char_p,C.POINTER(C.c_uint32),C.c_void_p]
    lib.bk_model_find_frame.restype=C.c_int
    return lib
