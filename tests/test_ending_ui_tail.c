#include "game/ending_sound.h"
#include "scene/ending_ui_tail.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char e[256], name[32] = "retained", names[2][32] = {{0}};
  assert(!bk_ending_sound_normal_voice(0, 1, 2, name, e));
  assert(!strcmp(name, "retained"));
  assert(bk_ending_sound_normal_voice(4, 12, 2, name, e));
  assert(!strcmp(name, "PH50226.wav"));
  assert(bk_ending_sound_contact_voice(3, 1, 12, 1, name, e));
  assert(!strcmp(name, "PH40259.wav"));
  assert(!bk_ending_sound_contact_voice(0, 1, INT_MAX, 1, name, e));
  BkEndingUi ui = {0};
  BkEndingStageUi stage = {0};
  BkEndingUiFrame out = {0};
  BkEndingFrameState f = {.phase = 1, .camera_cached = 11};
  BkEndingControlState c = {0};
  BkEndingAuxiliaryState a = {0};
  BkEndingUiNoticeState notices = {0};
  BkEndingUiNormalNotice normal = {0};
  BkEndingUiAuxNotice aux = {0};
  int8_t side = 1, final = 9;
  int32_t target = 2, inputs[14] = {0}, processed[14] = {0}, volume = -1000,
          contact = 2;
  uint8_t flash = 1;
  float gauge;
  BkEndingUiTailBindings b = {.frame = &f,
                              .control = &c,
                              .auxiliary = &a,
                              .notices = &notices,
                              .flash_wanted = &flash,
                              .final_state = &final,
                              .normal_side = &side,
                              .normal_target = &target,
                              .normal_inputs = inputs,
                              .normal_processed = processed,
                              .gauge_y = &gauge,
                              .contact_index = &contact,
                              .speech_names = names,
                              .voice_volume = &volume};
  assert(bk_ending_ui_initialize(&ui, 1280, c.pause_flags, &gauge, e));
  inputs[5] = 1;
  assert(
      bk_ending_ui_tail(&ui, &stage, &normal, &aux, &b, NULL, 1, .1f, &out, e));
  assert(processed[5] == 1 && notices.notices[0] == 1 && a.progress == .1f &&
         gauge == 231);
  assert(
      bk_ending_ui_tail(&ui, &stage, &normal, &aux, &b, NULL, 1, .1f, &out, e));
  assert(a.progress == .1f); /* consume once */
  /* Native full-value clamp still subtracts the complete Y increment. */
  for (unsigned width=640;width<=1280;width+=320) {
    float scale=width/1280.f;
    assert(bk_ending_ui_initialize(&ui,width,c.pause_flags,&gauge,e));
    a.progress=.98f; gauge=55*scale; processed[5]=0; out.count=0;
    assert(bk_ending_ui_tail(&ui,&stage,&normal,&aux,&b,NULL,scale,0,&out,e));
    assert(a.progress==1 && gauge==35*scale);
    ui.sprites[9].rect[1]=gauge;ui.sprites[9].transform.scale[1]=a.progress;
    out.count=1;
    assert(bk_ending_ui_sprite_step(&ui,9,0,&out.draws[0],e));
    assert(out.draws[0].xy[1]<51*scale); /* reproduced above-frame fill */
    BkEndingUi saved=ui;
    assert(bk_ending_ui_fit_gauge(&out,width,e));
    assert(out.draws[0].xy[1]==51*scale && out.draws[0].xy[5]==251*scale);
    assert(a.progress==1 && gauge==35*scale && !memcmp(&saved,&ui,sizeof(ui)));
    /* A retained value with the layout's initial Y puts the bar too low. */
    ui.sprites[9].rect[1]=251*scale;ui.sprites[9].transform.scale[1]=.5f;
    assert(bk_ending_ui_sprite_step(&ui,9,0,&out.draws[0],e));
    assert(out.draws[0].xy[5]==351*scale);
    assert(bk_ending_ui_fit_gauge(&out,width,e));
    assert(out.draws[0].xy[1]==151*scale && out.draws[0].xy[5]==251*scale);
  }
  puts("PASS gauge overflow/low-position reproduction and display-only correction at 640/960/1280");
  out.count=0; a.progress=.1f;

  ui.sprites[53].transform.fade.stage = 3;
  normal.cycles = 3;
  assert(!bk_ending_ui_tail(&ui, &stage, &normal, &aux, &b, NULL, 1, .1f, &out,
                            e));
  assert(!strcmp(names[0], "PH10218.wav") && !a.pending && !notices.notices[0]);
  /* Missing services preserve the native prefix, including filename. */
  f.phase = 8;
  ui.sprites[52].transform.fade.stage = 0;
  ui.sprites[52].transform.fade.alpha = 0;
  assert(bk_ending_ui_tail(&ui, &stage, NULL, NULL, &b, NULL, 1, .1f, &out, e));
  assert(ui.sprites[52].transform.fade.stage == 1 && out.count == 1);
  ui.loaded &= ~(UINT64_C(1) << 52);
  b.flash_wanted = NULL;
  assert(bk_ending_ui_tail(&ui, &stage, NULL, NULL, &b, NULL, 1, .1f, &out, e));
  assert(out.count ==
         1); /* final phase guards absent handle before flag read */
  f.phase = 6;
  a.gate = 5;
  assert(bk_ending_ui_tail(&ui, &stage, NULL, NULL, &b, NULL, 1, .1f, &out, e));
  a.gate = 3;
  aux.mode = 5;
  assert(
      !bk_ending_ui_tail(&ui, &stage, NULL, &aux, &b, NULL, 1, .1f, &out, e));
  BkCommonHudState common = {0};
  BkCommonHudFrame cf;
  bk_common_hud_initialize(&common);
  common.blocked = 1;
  common.curtain.stage = 0;
  assert(bk_ending_ui_curtain(&common, .1f, &cf, e));
  assert(cf.curtain_alpha == 0 && common.curtain.stage == 1);
  common.curtain = (BkFadeSprite){1, 2, 3};
  common.blocked = 0;
  assert(bk_ending_ui_curtain(&common, .1f, &cf, e));
  assert(cf.curtain_alpha == 1 && common.curtain.stage == 4);
  puts("ending UI tail boundaries PASS");
  return 0;
}
