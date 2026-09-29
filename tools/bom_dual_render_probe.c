/* Actual4d2320 three-model registry, combined3+2 bindings and shared effects.
 * Numeric Vulkan readback against CPU ENVL and the original transfer formula
 * applied to independently native-verified mappings. No visual/loader claim.
 * Resource, animation, visibility and raw disable inputs are explicit fixtures.
 */
#include "scene/bom_render.h"
#include "scene/ending_tertiary_assets.h"
#include "model/skin.h"
#include "core/matrix.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { \
  fprintf(stderr,"dual BOM GPU profile%u line%d: %s: %s\n",profile,__LINE__,#x,e); \
  goto done; } } while (0)
typedef struct {
  BkBomMeshView view;
  BkSkinMesh *skin;
  BkGpuMesh *gpu;
  const BkBomAssetMesh *asset;
} Reference;
static unsigned frames, hidden_frames, alpha_frames, repeated, rejected, empty_bindings;
static uint64_t vertices, writes, upper_writes, lower_writes;
static double worst;
static int transfer(const BkModelVertex *in,BkModelVertex *out,const float *m) {
  float p[4];bk_matrix_point(p,in->position,m);
  if (!isfinite(p[3])||p[3]==0) return 0;
  if (fabs((double)p[3]-1)>(double)1e-5f)
    for (unsigned j=0;j<3;++j) p[j]=(float)((double)p[j]/p[3]);
  for (unsigned j=0;j<3;++j) {
    float n=(float)((double)in->normal[0]*m[j]+(double)in->normal[1]*m[4+j]+
                      (double)in->normal[2]*m[8+j]);
    if (!isfinite(p[j])||!isfinite(n)) return 0;
    out->position[j]=p[j];out->normal[j]=n;
  }
  return 1;
}
static int compare(const BkLitVertex *got,const BkModelVertex *want,unsigned count,
                      char e[256]) {
  for (unsigned i=0;i<count;++i) {
    float position[]={got[i].base.x,got[i].base.y,got[i].base.z};
    for (unsigned j=0;j<6;++j) {
      double a=j<3?position[j]:got[i].normal[j-3];
      double b=j<3?want[i].position[j]:want[i].normal[j-3];
      double error=fabs(a-b)/fmax(1,fabs(b));
      if (error>worst) worst=error;
      if (!isfinite(error)||error>3e-5) {
        snprintf(e,256,"vertex%u component%u got%.9g expected%.9g",i,j,a,b);
        return 0;
      }
    }
    if (got[i].base.u!=want[i].uv[0][0]||got[i].base.v!=want[i].uv[0][1]) {
      snprintf(e,256,"transfer changed original UV at vertex%u",i);return 0;
    }
    ++vertices;
  }
  return 1;
}
static int run(BkRenderer *renderer,BkResourceStore *store,BkLightSet *light,
                  unsigned profile,char e[256]) {
  int ok=0;
  BkEndingTertiaryAssets *assets=NULL;
  BkEndingBackgroundAssets *background=NULL;
  BkBomRender *bom=NULL;
  BkActorRender *renders[3]={0};
  BkActorPose *poses[3]={0};
  BkModelSkin *skins[3]={0};
  const BkModel *models[3]={0};
  BkActorRenderBatch *batch=NULL;
  Reference *reference=NULL;
  BkLitVertex *readback=NULL;
  unsigned meshes=0,max_vertices=0;
  BkMenuCamera camera;BkEndingCameraPresets presets;
  uint32_t random=321,clocks[4]={100,100,100,100};
  CHECK(bk_menu_camera_dialogue(&camera));
  if (profile&1) {
    background=bk_ending_background_assets_create(store,"m03_90.xan",e);
    CHECK(background);
    assets=bk_ending_tertiary_assets_create_reloaded(store,2,profile/2,background,
        clocks,&random,&camera,&presets,e);
  } else assets=bk_ending_tertiary_assets_create(store,2,profile/2,clocks,
                                                 &random,&camera,&presets,e);
  CHECK(assets&&bk_ending_tertiary_assets_load_background(assets,store,e));
  bk_ending_background_assets_destroy(background);background=NULL;
  BkBomDualAssets *owner=bk_ending_tertiary_assets_bom(assets);
  CHECK(owner&&bk_bom_dual_assets_count(owner)==5);
  BkActorForest *forest=bk_ending_tertiary_assets_forest(assets);
  uint32_t roots[3];
  for (unsigned a=0;a<3;++a) {
    poses[a]=bk_ending_tertiary_assets_pose(assets,a);
    models[a]=bk_actor_pose_model(poses[a]);CHECK(models[a]);
    uint32_t registry;
    CHECK(bk_actor_forest_binding(forest,bk_ending_tertiary_assets_root(assets,a),
                                    &registry,roots+a));
    CHECK(registry==bk_ending_tertiary_assets_registry(assets,a));
    renders[a]=bk_actor_render_create(renderer,store,"bk3_11",models[a],
        a?NULL:bk_ending_tertiary_assets_eyes(assets),e);
    CHECK(renders[a]);
    if (bk_model_chunk(models[a],"ENVL")) {
      skins[a]=bk_model_skin_create(models[a],e);CHECK(skins[a]);
    }
    CHECK(bk_actor_pose_request_mode(poses[a],a==0?4:a==1?7:9,
                                      BK_CLIP_REQUEST_CONFIGURED,e));
  }
  meshes=bk_bom_dual_assets_mesh_count(owner);
  CHECK(meshes&&meshes<=64);
  reference=calloc(meshes,sizeof(*reference));CHECK(reference);
  for (unsigned i=0;i<meshes;++i) {
    Reference *r=reference+i;
    r->asset=bk_bom_dual_assets_mesh(owner,i);
    CHECK(r->asset&&r->asset->actor<3);
    const BkModelSubmesh *s=models[r->asset->actor]->submeshes+r->asset->submesh;
    r->view=(BkBomMeshView){malloc((size_t)s->vertex_count*sizeof(BkModelVertex)),s->vertex_count,NULL};
    CHECK(r->view.vertices);
    if (s->vertex_count>max_vertices) max_vertices=s->vertex_count;
    r->gpu=bk_actor_render_mesh(renders[r->asset->actor],models[r->asset->actor],r->asset->submesh);
    CHECK(r->gpu);
    for (uint32_t j=0;j<bk_model_skin_count(skins[r->asset->actor]);++j)
      if (bk_model_skin_entry(skins[r->asset->actor],j)->submesh==r->asset->submesh) {
        r->skin=bk_skin_mesh_create(skins[r->asset->actor],j,s,e);CHECK(r->skin);
      }
  }
  BkBomDeformBinding plans[5];int32_t groups[5];const uint32_t *sources[5];
  for (unsigned i=0;i<5;++i) {
    size_t count;
    CHECK(bk_bom_dual_assets_plan(owner,i,plans+i));
    CHECK(bk_bom_dual_assets_mapping(owner,i,groups+i,sources+i,&count));
    CHECK(count==plans[i].count&&plans[i].source<meshes&&plans[i].target<meshes);
    /*The packaged mata.vix is absent;4a52bc preserves that fourth row with
     * no writes. Its lower-clothing sibling matafuku.vix is present.*/
    CHECK((count!=0)==(i!=3));
    empty_bindings+=count==0;
    CHECK(reference[plans[i].source].asset->actor==(i<3?1u:2u));
    CHECK(reference[plans[i].target].asset->actor==0&&reference[plans[i].target].skin);
  }
  bom=bk_bom_render_create_dual(renderer,owner,renders,e);CHECK(bom);
  CHECK(!bk_bom_render_create_dual(renderer,owner,renders,e));++rejected;
  BkActorRender *duplicate[]={renders[0],renders[1],renders[1]};
  CHECK(!bk_bom_render_create_dual(renderer,owner,duplicate,e));++rejected;
  batch=bk_actor_render_batch_create(renderer,models[0]->frame_count*8,e);CHECK(batch);
  readback=malloc((size_t)max_vertices*sizeof(*readback));CHECK(readback);
  float projection[16];memcpy(projection,bk_identity,sizeof(projection));
  projection[0]=projection[5]=.01f;projection[10]=.001f;projection[12]=100;
  BkRenderStats baseline=bk_renderer_stats(renderer);
  for (unsigned frame=0;frame<96;++frame) {
    int32_t disabled[5];
    for (unsigned i=0;i<5;++i) disabled[i]=(int32_t[]){0,1,2,-1}[(frame+i)%4];
    CHECK(!bk_bom_render_prepare(bom,disabled,4,e));++rejected;
    unsigned hidden=frame%12==7,alpha=frame%12==4;
    for (unsigned a=0;a<3;++a) {
      BkActorVisibilityEdit edit={roots[a],a?((frame+a)%8>=4):hidden};
      CHECK(bk_actor_pose_visibility(poses[a],&edit,1,e));
      float seconds=(float[]){0,1.f/60,.05f,.1f,.25f}[frame%5];
      if (frame%3==0)
        CHECK(bk_ending_tertiary_assets_advance(assets,a,seconds,e));
      else CHECK(bk_ending_tertiary_assets_advance_plain(assets,a,seconds,
          frame%3==1?BK_CLIP_PLAIN_SOURCE:BK_CLIP_PLAIN_FORCE_CHAIN,e));
    }
    CHECK(bk_actor_forest_refresh(forest,e));
    BkMaterialAlphaEdit alpha_edit={.frame=roots[0],.alpha=alpha?0:1};
    CHECK(bk_material_pose_alpha(bk_ending_tertiary_assets_materials(assets,0),&alpha_edit,1,e));
    for (unsigned a=0;a<3;++a)
      CHECK(bk_actor_render_prepare_effects(renders[a],poses[a],
          bk_ending_tertiary_assets_materials(assets,a),
          a?NULL:bk_ending_tertiary_assets_face(assets),
          bk_ending_tertiary_assets_morph(assets,a),bk_identity,projection,e));
    CHECK(bk_bom_render_prepare(bom,disabled,5,e));
    if (frame<3) {
      CHECK(bk_actor_render_prepare_effects(renders[frame],poses[frame],
          bk_ending_tertiary_assets_materials(assets,frame),
          frame?NULL:bk_ending_tertiary_assets_face(assets),
          bk_ending_tertiary_assets_morph(assets,frame),bk_identity,projection,e));
      CHECK(bk_renderer_begin(renderer,e));
      CHECK(!bk_actor_render_draw(renders[0],light,e));++rejected;
      CHECK(bk_renderer_end(renderer,e));
      CHECK(bk_bom_render_prepare(bom,disabled,5,e));
    }
    BkActorRender *submitted[]={renders[0],renders[0]};
    unsigned repeat=frame%3==0?2:1;
    CHECK(bk_actor_render_batch_prepare(batch,submitted,repeat,e));
    for (unsigned i=0;i<meshes;++i) {
      Reference *r=reference+i;unsigned a=r->asset->actor,m=r->asset->submesh;
      const BkModelSubmesh *s=models[a]->submeshes+m;
      const BkMorphMesh *morph=a?NULL:bk_face_assets_mesh(bk_ending_tertiary_assets_face(assets),m);
      if (!morph) morph=bk_ending_tertiary_assets_mesh(assets,a,m);
      const BkModelVertex *v=morph?bk_morph_mesh_vertices(morph):s->vertices;
      if (r->skin) {
        size_t floats;const float *world=bk_actor_pose_world(poses[a],&floats);
        CHECK(bk_skin_mesh_apply(r->skin,world,floats,v,s->vertex_count,e));
        v=bk_skin_mesh_vertices(r->skin);
      }
      memcpy(r->view.vertices,v,(size_t)s->vertex_count*sizeof(*v));
      r->view.world=bk_actor_pose_frame(poses[a],r->asset->frame);
    }
    for (uint32_t q=0;q<bk_actor_render_batch_count(batch);++q) {
      uint32_t actor,frame_id,submesh;
      CHECK(bk_actor_render_batch_item(batch,q,&actor,&frame_id,&submesh));
      (void)actor;(void)frame_id;
      int32_t group=-1;
      for (unsigned i=0;i<5;++i)
        if (reference[plans[i].target].asset->submesh==submesh) {group=groups[i];break;}
      if (group<0) continue;
      for (unsigned i=0;i<5;++i) {
        if (groups[i]!=group||disabled[i]==1) continue;
        Reference *source=reference+plans[i].source,*target=reference+plans[i].target;
        for (size_t j=0;j<plans[i].count;++j) {
          CHECK(sources[i][j]<source->view.count&&plans[i].indices[j]<target->view.count);
          CHECK(transfer(source->view.vertices+sources[i][j],
              target->view.vertices+plans[i].indices[j],source->view.world));
          ++writes;if (i<3) ++upper_writes;else ++lower_writes;
        }
      }
    }
    CHECK(bk_renderer_begin(renderer,e));
    CHECK(bk_actor_render_batch_draw(batch,light,e));
    CHECK(bk_renderer_end(renderer,e));
    for (unsigned i=0;i<meshes;++i) {
      CHECK(bk_lit_mesh_readback(renderer,reference[i].gpu,readback,reference[i].view.count,e));
      if (!compare(readback,reference[i].view.vertices,reference[i].view.count,e)) {
        fprintf(stderr,"dual frame%u registry%u actor%u\n",frame,i,reference[i].asset->actor);goto done;
      }
    }
    BkRenderStats current=bk_renderer_stats(renderer);
    CHECK(current.live_allocations==baseline.live_allocations&&current.live_bytes==baseline.live_bytes);
    ++frames;hidden_frames+=hidden;alpha_frames+=alpha;repeated+=repeat==2;
  }
  printf("dual BOM GPU profile%u meshes=%u frames=96\n",profile,meshes);fflush(stdout);
  ok=1;
done:
  bk_bom_render_destroy(bom);bk_actor_render_batch_destroy(batch);free(readback);
  if (reference) for (unsigned i=0;i<meshes;++i) {
    bk_skin_mesh_destroy(reference[i].skin);free(reference[i].view.vertices);
  }
  free(reference);
  for (unsigned a=0;a<3;++a) {bk_actor_render_destroy(renders[a]);bk_model_skin_destroy(skins[a]);}
  bk_ending_tertiary_assets_destroy(assets);bk_ending_background_assets_destroy(background);
  return ok;
}
int main(int argc,char **argv) {
  if (argc!=2) return 2;
  char e[256]={0},path[2048];int result=1;unsigned profile=0;
  BkRenderer *renderer=bk_renderer_create(32,32,stdout,e);
  BkResourceStore *store=NULL;BkLightSet *light=NULL;
  CHECK(renderer);store=bk_resources_create(e);CHECK(store);
  const char *packs[]={"bk3_11","fambom","bk3_04","bk3_03"};
  for (unsigned i=0;i<4;++i) {
    CHECK(snprintf(path,sizeof(path),"%s/%s.pp",argv[1],packs[i])<(int)sizeof(path));
    CHECK(bk_resources_mount(store,packs[i],path,e));
  }
  light=bk_light_set_create(renderer,&(BkLighting){.ambient={1,1,1}},e);CHECK(light);
  BkRenderStats baseline=bk_renderer_stats(renderer);
  for (;profile<4;++profile) {
    CHECK(run(renderer,store,light,profile,e));
    BkRenderStats current=bk_renderer_stats(renderer);
    CHECK(current.live_allocations==baseline.live_allocations&&current.live_bytes==baseline.live_bytes);
  }
  CHECK(upper_writes&&lower_writes&&frames==384);
  printf("PASS dual BOM GPU profiles=4 frames=%u vertices=%llu transfers=%llu upper=%llu lower=%llu hidden=%u alpha0=%u repeated=%u rejected=%u empty_bindings=%u relative=%.9g stable_allocations=1\n",
      frames,(unsigned long long)vertices,(unsigned long long)writes,
      (unsigned long long)upper_writes,(unsigned long long)lower_writes,
      hidden_frames,alpha_frames,repeated,rejected,empty_bindings,worst);
  result=0;
done:
  bk_light_set_destroy(renderer,light);bk_resources_destroy(store);bk_renderer_destroy(renderer);
  return result;
}
