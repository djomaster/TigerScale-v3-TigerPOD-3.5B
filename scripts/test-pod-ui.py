#!/usr/bin/env python3
"""Render the actual r8 LVGL home screens; run test-picker.py first."""
import os,subprocess,tempfile,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
lv=Path(os.environ['LVGL_SOURCE'])
flags=['-DLV_CONF_SKIP=1','-DLV_USE_LARGE_COORD=1','-DLV_MEM_SIZE=1048576','-DLV_COLOR_DEPTH=16','-DLV_FONT_MONTSERRAT_20=1','-DLV_FONT_MONTSERRAT_28=1','-I'+str(lv)]
objdir=root/'.pio/picker-native'/hashlib.sha256(str(flags).encode()).hexdigest()[:12]
objs=list(objdir.glob('*.o'))
if len(objs)!=len(list((lv/'src').rglob('*.c'))):raise SystemExit('Run test-picker.py with the same LVGL_SOURCE first.')
home=(root/'TigerTagSplashESP32/pod_inventory.h').read_text().split('static void r8Home(bool tools=false) {')[1].split('// Retrying')[0]
shim=r'''
#include <lvgl.h>
#include <string>
#include <cstdio>
#include <cassert>
struct String:std::string {using std::string::string;String(){}String(const std::string&s):std::string(s){}String(int n):std::string(std::to_string(n)){} };
const lv_font_t&gFont14=lv_font_montserrat_14;const lv_font_t&gFont16=lv_font_montserrat_14;const lv_font_t&gFont20=lv_font_montserrat_20;
lv_obj_t*podStatus=nullptr;bool podCancel=false;void(*podService)()=nullptr;String podConfirmContext;
bool r8UiAlias[2]={false,false};bool r7Blocked=false,r8UiScanned=false,r8UiKnown=false,r8UiCompatible=false,r86Prompt=false;volatile bool gPodMemRefused=false;
uint32_t millis(){return lv_tick_get();}void delay(int n){lv_tick_inc(n);}
int touchMode=0;uint32_t touchStart=0;
bool tsRead(int16_t&x,int16_t&y){x=400;y=280;auto elapsed=millis()-touchStart;if(touchMode==0)return true;if(touchMode==2){x=40;return elapsed>40;}return elapsed>40;}
String(*podTranslate)(const String&)=nullptr;
#include "i18n.h"
Language gLanguage=LANG_EN;
#include "pod_ui.h"
'''
tail=r'''
static unsigned char pixels[480*320*3];
void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*c){for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++){auto rgb=lv_color_to32(*c++);if(x>=0&&x<480&&y>=0&&y<320){int i=(y*480+x)*3;pixels[i]=(rgb>>16)&255;pixels[i+1]=(rgb>>8)&255;pixels[i+2]=rgb&255;}}lv_disp_flush_ready(d);}
int checks=0;
void bounds(lv_obj_t*o){lv_area_t a;lv_obj_get_coords(o,&a);assert(a.x1>=0&&a.y1>=0&&a.x2<480&&a.y2<320);checks++;for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++)bounds(lv_obj_get_child(o,i));}
int main(int argc,char**argv){lv_init();static lv_color_t buf[480*40];static lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buf,nullptr,480*40);static lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=480;d.ver_res=320;d.draw_buf=&db;d.flush_cb=flush;lv_disp_drv_register(&d);
 r8Screen=lv_obj_create(nullptr);lv_scr_load(r8Screen);
 const char* names[]={"accueil","compatible","identifiant","reconnu","reprise","outils"};
 for(int mode=0;mode<6;mode++){r7Blocked=mode==4;r8UiScanned=mode>0;r8UiKnown=mode==3;r8UiCompatible=mode==1;r8Home(mode==5);lv_obj_update_layout(r8Screen);bounds(r8Screen);lv_refr_now(nullptr);std::string path=std::string(argv[1])+"/"+names[mode]+".ppm";FILE*f=fopen(path.c_str(),"wb");assert(f);fprintf(f,"P6\n480 320\n255\n");fwrite(pixels,1,sizeof(pixels),f);fclose(f);}
 // All four tool failures expose tools-back, same-action retry and home.
 for(int action:{2,3,4,6}) {
  r86Result(false,"Tag B non detecte. Verifiez sa position puis reessayez. Aucun tag modifie pendant cette tentative.",true,action);
  lv_obj_update_layout(r8Screen);bounds(r8Screen);
  if(action==2){lv_refr_now(nullptr);std::string path=std::string(argv[1])+"/echec-outil.ppm";FILE*f=fopen(path.c_str(),"wb");assert(f);fprintf(f,"P6\n480 320\n255\n");fwrite(pixels,1,sizeof(pixels),f);fclose(f);}
  int n=lv_obj_get_child_cnt(r8Screen);int expected[3]={11,14,7};
  for(int i=0;i<3;i++){auto*b=lv_obj_get_child(r8Screen,n-3+i);assert((intptr_t)lv_obj_get_user_data(b)==expected[i]);lv_event_send(b,LV_EVENT_CLICKED,nullptr);assert(r8Action==expected[i]);checks+=2;}
 }
 r86Result(true,"",true,2);assert((intptr_t)lv_obj_get_user_data(lv_obj_get_child(r8Screen,-2))==11);checks++;
 r8UiScanned=false;r7Blocked=false;r8Home();int scans=0;
 for(unsigned i=0;i<lv_obj_get_child_cnt(r8Screen);i++)if((intptr_t)lv_obj_get_user_data(lv_obj_get_child(r8Screen,i))==9)scans++;
 assert(scans==1);checks++;
 r8UiScanned=true;r8Home();scans=0;
 for(unsigned i=0;i<lv_obj_get_child_cnt(r8Screen);i++)if((intptr_t)lv_obj_get_user_data(lv_obj_get_child(r8Screen,i))==9)scans++;
 assert(scans==1);checks++;
 // A held finger on entry cannot confirm; release + 2-second hold is required.
 podConfirmContext="PLA bleu\nAucun tag ne sera ecrit.";
 touchMode=0;touchStart=millis();assert(!r8Confirm("Ajouter cet identifiant ?"));checks++;
 touchMode=1;touchStart=millis();assert(r8Confirm("Ajouter cet identifiant ?"));assert(millis()-touchStart>=2000);checks+=2;
 touchMode=2;touchStart=millis();assert(!r8Confirm("Ajouter cet identifiant ?"));checks++;
 printf("%d actual LVGL r8 bounds/physical-confirmation checks passed.\n",checks);
}
'''
out=root/'.pio/pod-ui-preview';out.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(shim+'static void r8Home(bool tools=false) {'+home+tail)
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++11',*flags,'-I'+str(root/'TigerTagSplashESP32'),str(p/'test.cpp'),*[str(x) for x in objs],'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(out)],check=True)
try:
 from PIL import Image
 for p in out.glob('*.ppm'):Image.open(p).save(p.with_suffix('.png'))
except ImportError:pass
print(out)
