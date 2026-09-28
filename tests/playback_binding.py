"""Host bindings for atomic model/timeline playback."""
import ctypes as C
from clip_binding import library as clip_library,State
from animation_binding import RootTransform
from model_binding import Model
def library():
    lib=clip_library()
    for name,args,result in [
        ('bk_model_playback_create',[C.POINTER(Model),C.c_void_p,C.c_int,C.c_void_p],C.c_void_p),
        ('bk_model_playback_destroy',[C.c_void_p],None),
        ('bk_model_playback_select',[C.c_void_p,C.c_uint,C.c_int,C.c_void_p],C.c_int),
        ('bk_model_playback_request',[C.c_void_p,C.c_uint,C.c_void_p],C.c_int),
        ('bk_model_playback_advance',[C.c_void_p,C.c_float,C.POINTER(RootTransform),C.c_void_p],C.c_int),
        ('bk_model_playback_step',[C.c_void_p,C.c_int,C.c_float,C.POINTER(RootTransform),C.c_void_p],C.c_int),
        ('bk_model_playback_place',[C.c_void_p,C.POINTER(RootTransform),C.c_void_p],C.c_int),
        ('bk_model_playback_frame',[C.c_void_p,C.c_uint32],C.POINTER(C.c_float)),
        ('bk_model_playback_state',[C.c_void_p,C.POINTER(State)],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
    return lib
