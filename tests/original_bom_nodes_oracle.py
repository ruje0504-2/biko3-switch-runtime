"""Real actor instances: BOM reference alignment preserves caches and topology.
Every call executes native422c49+4230bd; full publications use native522d9a.
Root motion and publication cadence are explicit test inputs, not ending AI.
"""
import argparse,ctypes as C,hashlib,json,math
from pathlib import Path
from original_bom_deform_oracle import Native,Node,F16,Config,Archive,NONE,identity
from original_matrix_oracle import multiply
from model_binding import ROOT,library,decode

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();error=C.create_string_buffer(256)
 lib.bk_bom_decode.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(Config),C.c_void_p]
 lib.bk_clip_set_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.bk_clip_set_decode.restype=C.c_void_p
 lib.bk_clip_set_destroy.argtypes=[C.c_void_p]
 lib.bk_actor_pose_create_loaded.argtypes=[C.c_void_p,C.c_void_p,C.c_uint32,C.POINTER(C.c_float),C.c_float,C.c_void_p];lib.bk_actor_pose_create_loaded.restype=C.c_void_p
 lib.bk_actor_pose_destroy.argtypes=[C.c_void_p]
 for name in ['frame','local','parent_world']:
  f=getattr(lib,'bk_actor_pose_'+name);f.argtypes=[C.c_void_p,C.c_uint32];f.restype=C.POINTER(C.c_float)
 lib.bk_actor_pose_publish.argtypes=[C.c_void_p];lib.bk_actor_pose_publish.restype=None
 lib.bk_actor_pose_root_local.argtypes=[C.c_void_p,C.POINTER(C.c_float),C.c_void_p]
 lib.bk_actor_pose_align_reference.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.c_float),C.c_void_p]
 lib.bk_model_find_frame.argtypes=[C.c_void_p,C.c_char_p,C.POINTER(C.c_uint32),C.c_void_p]
 arc=Archive(a.data/'fambom.pp');packs=[Archive(a.data/f'bk3_{i:02}.pp') for i in range(8,15)];nodes=matrices=0;worst=0;records=[]
 def read(pack,name):return pack.read(next(e for e in pack.entries if e.name.encode().lower()==name.lower()))
 def compare(got,want,label):
  nonlocal worst,matrices
  for i,(c,o) in enumerate(zip(got,want)):
   diff=abs(c-o)/max(1,abs(o));worst=max(worst,diff);assert math.isfinite(diff) and diff<2e-5,(label,i,c,o)
  matrices+=1
 for e in arc.entries:
  if not e.name.lower().endswith('.bom'):continue
  raw=arc.read(e);config=Config();assert lib.bk_bom_decode(raw,len(raw),C.byref(config),error)
  pack=next(p for p in packs if all(any(x.name.encode()==name for x in p.entries) for name in [config.primary,config.secondary]));models=[];clips=[];poses=[];roots=[]
  try:
   for name in [config.primary,config.secondary]:
    raw=read(pack,name);clip=lib.bk_clip_set_decode(raw,len(raw),error);assert clip,error.value;clips.append(clip);ok,model,msg=decode(lib,read(pack,raw[:256].split(b'\0')[0]));assert ok,msg;models.append(model)
    root=[i for i,f in enumerate(model.contents.frames[:model.contents.frame_count]) if f.parent_index==NONE];assert len(root)==1;roots.append(root[0]);pose=lib.bk_actor_pose_create_loaded(model,clip,root[0],(C.c_float*3)(0,0,0),0,error);assert pose,error.value;poses.append(pose)
    assert lib.bk_actor_pose_root_local(pose,model.contents.frames[root[0]].local,error);lib.bk_actor_pose_publish(pose)
   links=[]
   for b in config.bindings[:config.count]:
    parent=C.c_uint32();child=C.c_uint32();assert lib.bk_model_find_frame(models[0],b.parent,C.byref(parent),error);assert lib.bk_model_find_frame(models[1],b.child,C.byref(child),error);links.append((parent.value,child.value))
   count=models[1].contents.frame_count
   for step in range(40):
    root=F16(*models[0].contents.frames[roots[0]].local);root[12]+=(step%7)-3;root[13]+=.2*(step%3)
    assert lib.bk_actor_pose_root_local(poses[0],root,error)
    if step%3==0:lib.bk_actor_pose_publish(poses[0])
    for parent,child in links:
     worlds=[list(lib.bk_actor_pose_frame(poses[1],i)[:16]) for i in range(count)];parents=[list(lib.bk_actor_pose_parent_world(poses[1],i)[:16]) for i in range(count)]
     node=Node(F16(*lib.bk_actor_pose_local(poses[1],child)[:16]),F16(*worlds[child]),F16(*parents[child]));node.local[12:15]=[0,0,0];ref=F16(*lib.bk_actor_pose_frame(poses[0],parent)[:16])
     expected=n.node(node,ref,position=(C.c_float*3)(0,0,0));expected=n.node(expected,ref,forward=(C.c_float*3)(0,0,1),up=(C.c_float*3)(0,1,0))
     assert lib.bk_actor_pose_align_reference(poses[1],child,ref,error),(e.name,child,error.value)
     compare(lib.bk_actor_pose_local(poses[1],child)[:16],expected.local,(e.name,step,child,'local'))
     for i in range(count):
      compare(lib.bk_actor_pose_frame(poses[1],i)[:16],expected.world if i==child else worlds[i],(e.name,step,i,'world'))
      assert list(lib.bk_actor_pose_parent_world(poses[1],i)[:16])==parents[i]
     nodes+=1
    if step%4==0:
     native={};pending=set(range(count))
     while pending:
      ready=[i for i in pending if models[1].contents.frames[i].parent_index not in pending];assert ready
      for i in ready:
       p=models[1].contents.frames[i].parent_index;local=list(lib.bk_actor_pose_local(poses[1],i)[:16]);native[i]=multiply(n.u,local,native[p]) if p!=NONE else local;pending.remove(i)
     lib.bk_actor_pose_publish(poses[1])
     for i in range(count):compare(lib.bk_actor_pose_frame(poses[1],i)[:16],native[i],(e.name,step,i,'publish'))
   records.append(dict(config=e.name,alignments=40*len(links),secondary_frames=count));print('PASS actor BOM nodes',records[-1],flush=True)
  finally:
   for pose in poses:lib.bk_actor_pose_destroy(pose)
   for clip in clips:lib.bk_clip_set_destroy(clip)
   for model in models:lib.bk_model_destroy(model)
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),alignments=nodes,matrices=matrices,max_normalized_error=worst,records=records,scope=__doc__)
 (ROOT/'local/original-bom-nodes-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',nodes,matrices,worst,flush=True)
if __name__=='__main__':main()
