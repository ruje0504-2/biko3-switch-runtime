#include "scene/ending_secondary_ui.h"
#include "world/placement.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e) snprintf(e,256,"secondary menu: %s",why);
  return 0;
}
static int valid(const BkEndingSecondaryMenuGeometry *g, char e[256]) {
  return g && g->width && g->height && g->width<=INT32_MAX &&
         g->height<=INT32_MAX && isfinite(g->scale) && g->scale>=0 &&
         isfinite(g->menu_width) && g->menu_width>=0 &&
         (double)g->menu_width/2<2147483648.0
      ? 1 : fail(e,"invalid logical window/scale/menu width");
}
int bk_ending_secondary_menu_zone(const BkEndingSecondaryMenuGeometry *g,
                                   const int32_t point[2], int32_t *result,
                                   char e[256]) {
  if (!point || !result || !valid(g,e)) return fail(e,"invalid zone input");
  if (point[0]<0 || point[1]<0 || (uint32_t)point[0]>g->width ||
      (uint32_t)point[1]>g->height) { *result=-1; return 1; }
  unsigned zone[2]={0},extent[2]={g->width,g->height};
  int32_t half=(int32_t)((double)g->menu_width/2);
  for (unsigned i=0;i<2;++i) {
    float high=(float)((double)point[i]+400.0*g->scale+half);
    if ((double)extent[i]<high) zone[i]=1;
    else if ((double)point[i]-400.0*g->scale-half<0) zone[i]=2;
  }
  static const int32_t table[3][3]={{0,7,8},{5,1,2},{6,3,4}};
  *result=table[zone[0]][zone[1]];
  return 1;
}
static int32_t add(int32_t value, int32_t amount) {
  uint32_t bits=(uint32_t)value+(uint32_t)amount;
  memcpy(&value,&bits,4);
  return value;
}
static int candidate(const BkEndingSecondaryMenuGeometry *g, int32_t angle,
                       const int32_t center[2], int32_t point[2], char e[256]) {
  /*Authored double. Do not replace with pi/180 or swap sin/cos.*/
  double radians=(double)angle*.01745;
  double offset[2]={sin(radians)*(400.0*g->scale),cos(radians)*(400.0*g->scale)};
  for (unsigned i=0;i<2;++i) {
    if (!isfinite(offset[i]) || offset[i]<=-2147483649.0 || offset[i]>=2147483648.0)
      return fail(e,"menu offset outside native integer range");
    point[i]=add(center[i],(int32_t)offset[i]);
  }
  return 1;
}
static int fits(const BkEndingSecondaryMenuGeometry *g, const int32_t point[2]) {
  double half=(double)g->menu_width/2;
  return (double)g->width>(float)((double)point[0]+half) &&
         (double)point[0]-half>0 &&
         (double)g->height>(float)((double)point[1]+half) &&
         (double)point[1]-half>0;
}
static int place(const BkEndingSecondaryMenuGeometry *g, int32_t *angle,
                  int32_t *correction, const int32_t center[2],
                  int32_t point[2], char e[256]) {
  for (unsigned tries=0;tries<=4096;++tries) {
    if (!candidate(g,*angle,center,point,e)) return 0;
    if (fits(g,point)) return 1;
    *angle=add(*angle,1);
    *correction=add(*correction,1);
  }
  return fail(e,"placement exceeded4096 angle increments");
}
int bk_ending_radial_menu_place(const BkEndingSecondaryMenuGeometry *g,
                                 int32_t base, int32_t step,
                                 const int32_t center[2], int32_t points[3][2],
                                 char e[256]) {
  if (!center || !points || !valid(g, e)) return fail(e, "missing radial menu geometry");
  const int32_t origin[2] = {center[0], center[1]};
  int32_t angle = base, correction = 0;
  if (!place(g, &angle, &correction, origin, points[0], e)) return 0;
  angle = add(add(base, step) % 360, correction);
  if (!place(g, &angle, &correction, origin, points[1], e)) return 0;
  angle = add(add(base, add(step, step)) % 360, correction);
  return place(g, &angle, &correction, origin, points[2], e);
}
int bk_ending_secondary_menu(const BkEndingSecondaryMenuGeometry *g,
                              int32_t event, int32_t automatic, int32_t selected,
                              const int32_t targets[39][2],
                              const int32_t alternate[2], int32_t choices[3],
                              int32_t points[3][2], char e[256]) {
  if (!targets || !alternate || !choices || !points || !valid(g,e))
    return fail(e,"missing menu owners");
  const int32_t *center;
  if (event==1) {
    if (automatic>=0 && automatic<4) {
      unsigned slot=0;
      for (int32_t i=0;i<4;++i) if (i!=automatic) choices[slot++]=i;
    }
    if (selected<0 || selected>=39) return fail(e,"selected menu target outside range");
    center=targets[selected];
  } else if (event==2) {
    choices[0]=automatic ? 4 : 5;
    choices[1]=automatic ? 5 : -1;
    choices[2]=-1;
    center=alternate;
  } else return 1;
  int32_t zone;
  if (!bk_ending_secondary_menu_zone(g,center,&zone,e)) return 0;
  if (zone<0) return 1;
  int32_t angle,correction=0;
  if (!zone) {
    angle=120;
    if (!place(g,&angle,&correction,center,points[0],e)) return 0;
    angle=add(correction,240);
    if (!place(g,&angle,&correction,center,points[1],e)) return 0;
    angle=correction;
    return place(g,&angle,&correction,center,points[2],e);
  }
  static const int32_t bases[8]={180,270,90,0,225,45,135,315};
  int32_t base=bases[zone-1];
  angle=base;
  if (!place(g,&angle,&correction,center,points[0],e)) return 0;
  int32_t step=event==1 ? 45 : 90;
  angle=add((base+step)%360,correction);
  if (!place(g,&angle,&correction,center,points[1],e)) return 0;
  angle=add((base+2*step)%360,correction);
  return place(g,&angle,&correction,center,points[2],e);
}
int bk_ending_selected_menu(const BkEndingSecondaryMenuGeometry *g,
                              const BkEndingUiPickBindings *projection,
                              int32_t event, int32_t selection,
                              int32_t selected, int32_t targets[39][2],
                              const int32_t alternate[2], int32_t choices[3],
                              int32_t points[3][2], char e[256]) {
  if (!projection || !targets || !alternate || !choices || !points || !valid(g,e))
    return fail(e,"missing selected menu owners");
  if (!bk_ending_ui_project_target(projection,4,targets[4],e)) return 0;
  const int32_t *center;
  int32_t heading=0;
  if (event==1) {
    if (selection>=0 && selection<3) {
      unsigned slot=0;
      for (int32_t i=0;i<3;++i) if (i!=selection) choices[slot++]=i;
      choices[2]=-1;
    }
    if (selected<0 || selected>=39) return fail(e,"selected menu target outside range");
    center=targets[selected];
  } else if (event==2) {
    choices[0]=3;
    choices[1]=choices[2]=-1;
    float angle;
    if (!bk_route_heading(&angle,(float)alternate[0],(float)alternate[1],
                            (float)targets[4][0],(float)targets[4][1]) ||
        !isfinite(angle) || (double)angle<=-2147483649.0 ||
        (double)angle>=2147483648.0)
      return fail(e,"selected menu bearing outside native integer range");
    heading=(int32_t)angle;
    center=alternate;
  } else return 1;
  int32_t zone;
  if (!bk_ending_secondary_menu_zone(g,center,&zone,e)) return 0;
  if (zone<0) return 1;
  int32_t angle,correction=0;
  if (!zone) {
    if (event==1) {
      angle=120;
      if (!place(g,&angle,&correction,center,points[0],e)) return 0;
      angle=add(correction,240);
      if (!place(g,&angle,&correction,center,points[1],e)) return 0;
    } else {
      angle=heading;
      if (!place(g,&angle,&correction,center,points[1],e) ||
          !place(g,&angle,&correction,center,points[0],e)) return 0;
    }
    angle=correction;
    return place(g,&angle,&correction,center,points[2],e);
  }
  static const int32_t ordinary[8]={180,270,90,0,225,45,135,315};
  static const int32_t alternate_bases[8]={225,315,135,45,270,90,180,0};
  if (event==1)
    return bk_ending_radial_menu_place(g,ordinary[zone-1],90,center,points,e);
  int32_t point[2];
  if (!candidate(g,heading,center,point,e)) return 0;
  angle=fits(g,point) ? heading : alternate_bases[zone-1];
  return bk_ending_radial_menu_place(g,angle,10,center,points,e);
}
