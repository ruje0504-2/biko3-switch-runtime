"""CPU-only XAN API, shared by native oracle and archive checks."""
import ctypes as C
from animation_binding import library as animation_library
class Definition(C.Structure):
    _fields_=[(s,C.c_int32) for s in ['active','loop','loop_start','duration','chain','next','chain_after']]+[(s,C.c_float) for s in ['start','end','blend_ticks']]
class State(C.Structure):
    _fields_=[(s,C.c_int32) for s in ['slot','requested','ended','looped','blend_done','loops']]+[(s,C.c_float) for s in ['elapsed','source','rate','blend_elapsed','blend_from','blend_to']]
class Sample(C.Structure):
    _fields_=[('blend',C.c_int32)]+[(s,C.c_float) for s in ['from_tick','to_tick','weight']]
def library():
    lib=animation_library()
    for name,args,result in [
        ('bk_clip_set_decode',[C.c_void_p,C.c_size_t,C.c_void_p],C.c_void_p),
        ('bk_clip_set_destroy',[C.c_void_p],None),
        ('bk_clip_model_name',[C.c_void_p],C.c_char_p),
        ('bk_clip_definition',[C.c_void_p,C.c_uint],C.POINTER(Definition)),
        ('bk_clip_player_create',[C.c_void_p,C.c_void_p],C.c_void_p),
        ('bk_clip_player_create_authored',[C.c_void_p,C.c_void_p],C.c_void_p),
        ('bk_clip_player_destroy',[C.c_void_p],None),
        ('bk_clip_select',[C.c_void_p,C.c_uint,C.c_int,C.c_void_p],C.c_int),
        ('bk_clip_request',[C.c_void_p,C.c_uint,C.c_void_p],C.c_int),
        ('bk_clip_request_mode',[C.c_void_p,C.c_uint,C.c_int,C.c_void_p],C.c_int),
        ('bk_clip_advance',[C.c_void_p,C.c_float,C.POINTER(Sample),C.c_void_p],C.c_int),
        ('bk_clip_state',[C.c_void_p,C.POINTER(State)],C.c_int)]:
        f=getattr(lib,name);f.argtypes=args;f.restype=result
    return lib
