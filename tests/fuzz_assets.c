#include "resource/assets.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static uint32_t state=0x12345678;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void){
    uint8_t bytes[8192];char error[256];
    for(unsigned run=0;run<12000;run++){
        size_t size=next()%sizeof(bytes);
        for(size_t i=0;i<size;i++)bytes[i]=(uint8_t)next();
        if(size>=18 && run%3==0){
            bytes[0]=0;bytes[1]=0;bytes[2]=run%2?2:10;bytes[12]=(uint8_t)(next()%32);bytes[13]=0;
            bytes[14]=(uint8_t)(next()%32);bytes[15]=0;bytes[16]=run%2?24:32;bytes[17]=(uint8_t)(next()%64);
        }
        BkImage im;
        bk_image_decode(bytes,size,&im,error);bk_image_free(&im);
        if(size>=4){
            uint32_t declared=(2304+(next()%128)*80)^0xa67f54cb;
            for(unsigned j=0;j<4;j++)bytes[j]=(uint8_t)(declared>>(j*8));
        }
        uint8_t *out=NULL;size_t length=0;
        bk_tbl_decode(bytes,size,&out,&length,error);free(out);
    }
    puts("PASS: 12000 malformed image/TBL cases");return 0;
}
