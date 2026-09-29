#include "scene/ending_secondary_presentation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Explicit real-resource frame fixture, not a natural47a5d0 playthrough.
 * Optional output uses the established PCM-window format, independently
 * replayed by original_ending_voice_oracle.py. */
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, e); goto done; } } while (0)
typedef struct { uint64_t submitted, consumed, hash; } Sink;
typedef struct { uint32_t now, calls; int fail; } Clock;
static uint64_t digest(uint64_t h, const void *data, size_t size) {
  const uint8_t *p = data;
  for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int submit(void *p, const int16_t *pcm, size_t n, char e[256]) {
  (void)e; Sink *s = p;
  s->hash = digest(s->hash, pcm, n * 2 * sizeof(*pcm)); s->submitted += n;
  return 1;
}
static int poll(void *p, uint64_t *n, char e[256]) {
  (void)e; *n = ((Sink *)p)->consumed; return 1;
}
static int clock_read(void *p, uint32_t *now, char e[256]) {
  Clock *c = p;
  if (c->fail) { snprintf(e, 256, "injected clock failure"); return 0; }
  *now = c->now + c->calls++; return 1;
}
static uint32_t bits(float value) { uint32_t n; memcpy(&n, &value, 4); return n; }
static int record(FILE *f, const uint32_t values[14], const uint8_t pcm[440]) {
  if (!f) return 1;
  uint8_t raw[56];
  for (unsigned i = 0; i < 14; ++i)
    for (unsigned b = 0; b < 4; ++b) raw[i*4+b] = (uint8_t)(values[i] >> (b*8));
  return fwrite(raw, sizeof(raw), 1, f) == 1 && fwrite(pcm, 440, 1, f) == 1;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  FILE *records = NULL;
  BkResourceStore *store = NULL;
  BkEndingSecondaryAssets *assets = NULL;
  BkEndingAudio *audio = NULL;
  BkAudio *mixer = NULL;
  BkClipSet *clips = NULL;
  BkBlob raw = {0};
  uint64_t geometry = UINT64_C(14695981039346656037), pcm_hash = geometry;
  uint64_t face_hash = geometry, state_hash = geometry;
  unsigned frames = 0, voiced = 0, ramps = 0, frozen = 0, rejected = 0, requested = 0;
  BkEndingSecondaryPresentationState retained = bk_ending_secondary_presentation_initial();
  BkEndingVoiceEnvelope voice = {0};
  if (argc == 3) {
    CHECK(records = fopen(argv[2], "wb"));
    CHECK(fwrite("BK3EP001", 8, 1, records) == 1);
  }
  CHECK(store = bk_resources_create(e));
  const char *packs[] = {"bk3_09", "bk3_04", "bk3_03", "fambom", "bk3_06"};
  for (unsigned i = 0; i < sizeof(packs)/sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned variant = 0; variant < 2; ++variant) {
      uint32_t random = 101 + group*2 + variant, clocks[4] = {100,100,100,100};
      BkMenuCamera camera = {0};
      for (unsigned i = 0; i < 4; ++i) camera.pose.world[i*5] = camera.matrix[i*5] = 1;
      BkEndingCameraPresets presets;
      BkEndingSecondaryPresentationState held_state = retained;
      BkEndingVoiceEnvelope held_voice = voice;
      CHECK(assets = bk_ending_secondary_assets_create(store,group,variant,clocks,&random,&camera,&presets,e));
      CHECK(bk_ending_secondary_assets_load_background(assets,store,e));
      CHECK(!memcmp(&held_state,&retained,sizeof(retained)) && !memcmp(&held_voice,&voice,sizeof(voice)));
      const BkEndingSecondaryConfig *config = bk_ending_secondary_assets_config(assets);
      CHECK(bk_resources_read(store,"bk3_09",config->primary,&raw,e) == BK_RESOURCE_OK);
      CHECK(clips = bk_clip_set_decode(raw.data,raw.size,e)); bk_blob_free(&raw);
      Sink sink = {.hash=UINT64_C(14695981039346656037)};
      BkAudioSink output = {&sink,48000,480,1920,submit,poll};
      CHECK(mixer = bk_audio_create(&output,e));
      CHECK(audio = bk_ending_audio_create(store,mixer,0,e));
      CHECK(bk_ending_audio_bind(audio,0,"bk3_06","PH11201.wav",e));
      BkEndingAudioCall play = {.operation=BK_ENDING_AUDIO_RESTART,.slot=0,.flags=1,.volume=-500};
      int started=0;
      CHECK(bk_ending_audio_call(audio,group,variant,0,&play,&started,e) && bk_audio_fill(mixer,e));
      BkEndingFrameState frame = {.phase=2,.state_721ee4=4,.group=(uint8_t)group};
      BkEndingAuxiliaryState aux = {.variant=(int32_t)variant};
      uint8_t toggles[8] = {0}; int32_t automatic=0;
      Clock clock = {.now=1000};
      BkEndingSecondaryPresentationScene scene = {
          assets,audio,&retained,&voice,&random,&clock,clock_read};
      BkActorPose *primary=bk_ending_secondary_assets_pose(assets,0);
      BkClipState tracks[2], after;
      for (unsigned i=0;i<2;++i) CHECK(bk_actor_pose_state(bk_ending_secondary_assets_pose(assets,i+1),tracks+i));
      for (unsigned step=0;step<420;++step) {
        const unsigned slots[]={1,2,3,9,14,15,16};
        if (!(step%60)) {
          unsigned slot=slots[step/60];
          const BkClipDefinition *definition=bk_clip_definition(clips,slot);
          CHECK(definition && definition->active);
          CHECK(bk_actor_pose_request_mode(primary,slot,BK_CLIP_REQUEST_CONFIGURED,e));
          ++requested;
        }
        frame.state_721ee4=step%70<10 ? 4 : step%70<40 ? 1 : step%70<55 ? 5 : 6;
        automatic=step%17!=0;
        toggles[0]=step%12<6 ? 0 : 2;
        toggles[1]=step%13<7 ? 0 : 255;
        toggles[3]=step%10<5 ? 0 : 255;
        toggles[5]=step%7<4 ? 0 : 1;
        aux.expression_a=6;aux.expression_b=step%30<15 ? 3 : 1;
        clock.now=step<210 ? 1000+step*17 : UINT32_MAX-700+(step-210)*17;
        clock.calls=0;
        if (step%100==20) CHECK(bk_audio_pause(mixer,0,e));
        if (step%100==25) CHECK(bk_audio_resume(mixer,0,1,e));
        int freeze=step%100>=40 && step%100<48;
        if (!freeze) sink.consumed+=480;
        else ++frozen;
        if (sink.consumed>sink.submitted) sink.consumed=sink.submitted;
        CHECK(bk_audio_poll(mixer,e));
        BkAudioCursor cursor;int playing=0;
        CHECK(bk_audio_playing(mixer,0,&playing) && bk_audio_cursor(mixer,0,&cursor));
        uint32_t bytes=cursor.pcm ? (uint32_t)(bk_pcm_frames(cursor.pcm)*bk_pcm_channels(cursor.pcm)*2) : 4096;
        uint32_t offset=cursor.pcm ? (uint32_t)(cursor.source_frame*bk_pcm_channels(cursor.pcm)*2) : 0;
        uint8_t window[440]={0};
        int sample=cursor.pcm && cursor.buffered && bytes>=443 && offset<bytes-443;
        if(sample) {
          const int16_t *pcm=bk_pcm_samples(cursor.pcm)+offset/2;
          for(unsigned i=0;i<220;++i) {window[i*2]=(uint8_t)pcm[i];window[i*2+1]=(uint8_t)((uint16_t)pcm[i]>>8);}
        }
        BkEndingVoiceEnvelope before=voice;
        int expression_changed=bk_ending_secondary_assets_face_state(assets)->expression!=aux.expression_b;
        CHECK(bk_ending_secondary_presentation_scene_step(&scene,&frame,&aux,&automatic,toggles,1.f/60,e));
        CHECK(clock.calls==(unsigned)(expression_changed ? 4 : 3));
        CHECK(bk_actor_pose_state(primary,&after));
        int live=after.slot==1 || after.slot==16 || (group==1 &&
            (after.slot==2 || after.slot==3 || after.slot==9 || after.slot==14 || after.slot==15));
        if(live) {
          uint32_t words[14]={group,variant,step,(uint32_t)playing,(uint32_t)cursor.playing,
              (uint32_t)!sample,bytes,offset,bits(before.target),bits(before.smoothed),
              bits(voice.target),bits(voice.smoothed),random,clock.calls};
          CHECK(record(records,words,window));++voiced;
        } else {CHECK(!memcmp(&before,&voice,sizeof(voice)));++ramps;}
        for(unsigned i=0;i<2;++i) {
          CHECK(bk_actor_pose_state(bk_ending_secondary_assets_pose(assets,i+1),&after));
          CHECK(!memcmp(tracks+i,&after,sizeof(after)));
        }
        uint32_t background_hidden;
        const BkModel *bg=bk_actor_pose_model(bk_ending_secondary_assets_pose(assets,3));
        for(uint32_t i=0;i<bg->frame_count;++i) if(bg->frames[i].parent_index==BK_MODEL_NONE) {
          CHECK(bk_actor_pose_hidden(bk_ending_secondary_assets_pose(assets,3),i,&background_hidden));
          CHECK(background_hidden==toggles[0]);
        }
        for(unsigned i=0;i<4;++i) {
          BkActorPose *pose=bk_ending_secondary_assets_pose(assets,i);size_t count=0;
          const float *world=bk_actor_pose_world(pose,&count);CHECK(world);
          for(size_t j=0;j<count;++j) CHECK(isfinite(world[j]));
          geometry=digest(geometry,world,count*sizeof(float));
        }
        face_hash=digest(face_hash,bk_ending_secondary_assets_face_state(assets),sizeof(BkFaceState));
        const uint32_t words[]={bits(retained.rate),(uint32_t)retained.remaining,
            (uint32_t)retained.active_clip,(uint32_t)retained.slow_phase,bits(retained.mouth_level),
            retained.mouth_descending,random};
        state_hash=digest(state_hash,words,sizeof(words));
        CHECK(bk_audio_fill(mixer,e));++frames;
      }
      held_state=retained;held_voice=voice;uint32_t saved_random=random;
      BkClipState before[4];
      for(unsigned i=0;i<4;++i) CHECK(bk_actor_pose_state(bk_ending_secondary_assets_pose(assets,i),before+i));
      clock.fail=1;
      CHECK(!bk_ending_secondary_presentation_scene_step(&scene,&frame,&aux,&automatic,toggles,1.f/60,e));
      CHECK(!memcmp(&retained,&held_state,sizeof(retained)) && !memcmp(&voice,&held_voice,sizeof(voice)) && random==saved_random);
      for(unsigned i=0;i<4;++i) {
        CHECK(bk_actor_pose_state(bk_ending_secondary_assets_pose(assets,i),&after));
        CHECK(!memcmp(before+i,&after,sizeof(after)));
      }
      ++rejected;pcm_hash=digest(pcm_hash,&sink.hash,sizeof(sink.hash));
      bk_ending_audio_destroy(audio);audio=NULL;bk_audio_destroy(mixer);mixer=NULL;
      bk_clip_set_destroy(clips);clips=NULL;bk_ending_secondary_assets_destroy(assets);assets=NULL;
    }
  CHECK(frames==4200 && requested==70 && voiced && ramps && frozen && rejected==10);
  printf("PASS secondary presentation: 10 entries %u frames %u requests %u PCM-envelope calls %u ramp/fixed-mouth frames %u frozen-consumption frames %u atomic clock rejections; geometry=%016llx face=%016llx state=%016llx pcm=%016llx; no parent/playability claim\n",
      frames,requested,voiced,ramps,frozen,rejected,(unsigned long long)geometry,
      (unsigned long long)face_hash,(unsigned long long)state_hash,(unsigned long long)pcm_hash);
  rc=0;
done:
  if(records && fclose(records)) rc=1;
  bk_blob_free(&raw);bk_ending_audio_destroy(audio);bk_audio_destroy(mixer);
  bk_clip_set_destroy(clips);bk_ending_secondary_assets_destroy(assets);bk_resources_destroy(store);
  return rc;
}
