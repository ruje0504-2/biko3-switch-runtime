import ctypes as C
from model_binding import Model

class Point(C.Structure):
    _fields_=[('position',C.c_float*3),('range',C.c_float),('diffuse',C.c_float*3),
              ('attenuation0',C.c_float),('ambient',C.c_float*3),('attenuation1',C.c_float),('attenuation2',C.c_float),('specular',C.c_float*3)]
class Spot(C.Structure):
    _fields_=[("point",Point),("direction",C.c_float*3),("falloff",C.c_float),("theta",C.c_float),("phi",C.c_float)]
class Lighting(C.Structure):
    _fields_=[('ambient',C.c_float*3),('point_count',C.c_uint),('points',Point*8),('spot_count',C.c_uint),('spots',Spot*8)]
class Light(C.Structure):
    _fields_=[('name',C.c_char*65),('id',C.c_uint32),('type',C.c_uint32),('frame_index',C.c_uint32),
              ('diffuse',C.c_float*4),('specular',C.c_float*4),('ambient',C.c_float*4),
              ('position',C.c_float*3),('direction',C.c_float*3),('range',C.c_float),('falloff',C.c_float),
              ('attenuation',C.c_float*3),('theta',C.c_float),('phi',C.c_float)]
class Fog(C.Structure):
    _fields_=[(n,C.c_uint32) for n in ['enabled','mode','table','range_based','color']]+[(n,C.c_float) for n in ['start','end','density']]
class Environment(C.Structure):
    _fields_=[('lights',C.POINTER(Light)),('light_count',C.c_uint32),('ambient_count',C.c_uint32),('lighting',Lighting),('fog',Fog)]
def bind(lib):
    lib.bk_model_environment_create.argtypes=[C.POINTER(Model),C.POINTER(C.c_float),C.c_void_p]
    lib.bk_model_environment_create.restype=C.POINTER(Environment)
    lib.bk_model_environment_destroy.argtypes=[C.POINTER(Environment)]
    return lib
