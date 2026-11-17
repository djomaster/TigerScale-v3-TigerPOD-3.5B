#pragma once
#include "ui_icons.h"
// 480 x 320, large actions and a separate tools screen. No UID dump on home.
static lv_obj_t *r8Screen=nullptr,*r8Bar=nullptr;
static int r8Action=0;
static bool r8Busy=false;
static uint32_t r8PaintAt=0;
static const uint32_t R8_BG=0x101820,R8_CARD=0x1D2A35,R8_TEXT=0xF3F6F8,R8_MUTED=0xA5B8C6,R8_ACCENT=0xFF9D32;
static lv_obj_t* r8Text(lv_obj_t* parent,const String& text,int x,int y,int w,uint32_t color=R8_TEXT,const lv_font_t* font=&gFont14) {
    auto* l=lv_label_create(parent);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,w);
    lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);lv_label_set_text(l,text.c_str());
    lv_obj_set_style_text_color(l,lv_color_hex(color),0);lv_obj_set_style_text_font(l,font,0);return l;
}
static void r8Clear(const char* subtitle) {
    podStatus=nullptr;r8Bar=nullptr;lv_obj_clean(r8Screen);
    lv_obj_set_style_bg_color(r8Screen,lv_color_hex(R8_BG),0);lv_obj_clear_flag(r8Screen,LV_OBJ_FLAG_SCROLLABLE);
    r8Text(r8Screen,"TigerPOD",18,12,250,R8_TEXT,&gFont20);
    r8Text(r8Screen,subtitle,18,42,440,R8_MUTED);
}
static lv_obj_t* r8Button(const char* title,int action,int x,int y,int width=216,bool primary=false) {
    auto* b=lv_btn_create(r8Screen);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,width,48);
    lv_obj_set_style_radius(b,12,0);lv_obj_set_style_bg_color(b,lv_color_hex(primary?R8_ACCENT:R8_CARD),0);
    lv_obj_set_style_shadow_width(b,0,0);lv_obj_set_user_data(b,(void*)(intptr_t)action);
    lv_obj_add_event_cb(b,[](lv_event_t* e){int a=(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));if(a==8)podCancel=true;else if(!r8Busy)r8Action=a;},LV_EVENT_CLICKED,nullptr);
    auto* l=lv_label_create(b);lv_label_set_text(l,title);lv_obj_set_style_text_font(l,&gFont14,0);
    lv_obj_set_style_text_color(l,lv_color_hex(primary?R8_BG:R8_TEXT),0);lv_obj_center(l);return b;
}
// Mockup tile: [34 px icon box] title + muted subtitle. Same click wiring as r8Button.
static lv_obj_t* r8Card(const char* title,const char* sub,const lv_img_dsc_t* icon,int action,int x,int y,bool primary) {
    auto* b=r8Button("",action,x,y,219,primary);lv_obj_set_size(b,219,62);lv_obj_set_style_radius(b,16,0);
    lv_obj_set_style_bg_color(b,lv_color_hex(primary?0x2A1D0E:0x121923),0);
    lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,lv_color_hex(primary?0xFF9A2E:0x243042),0);
    lv_obj_clean(b);
    auto* box=lv_obj_create(b);lv_obj_set_size(box,34,34);lv_obj_set_pos(box,6,6);lv_obj_set_style_radius(box,11,0);
    lv_obj_set_style_bg_color(box,lv_color_hex(primary?0x3A2610:0x1A2330),0);lv_obj_set_style_border_width(box,0,0);
    lv_obj_clear_flag(box,LV_OBJ_FLAG_SCROLLABLE);lv_obj_clear_flag(box,LV_OBJ_FLAG_CLICKABLE);
    auto* ic=lv_img_create(box);lv_img_set_src(ic,icon);lv_obj_set_style_img_recolor(ic,lv_color_hex(primary?0xFFB45E:0x9DB1C8),0);lv_obj_set_style_img_recolor_opa(ic,LV_OPA_COVER,0);lv_obj_center(ic);
    auto* t1=lv_label_create(b);lv_label_set_text(t1,title);lv_obj_set_style_text_font(t1,&gFont16,0);
    lv_obj_set_style_text_color(t1,lv_color_hex(R8_TEXT),0);lv_obj_set_pos(t1,54,6);
    auto* t2=lv_label_create(b);lv_label_set_text(t2,sub);lv_obj_set_style_text_font(t2,&gFont14,0);
    lv_obj_set_style_text_color(t2,lv_color_hex(0x8A9AAD),0);lv_obj_set_pos(t2,54,28);
    return b;
}
static void r8Working(const char* title) {
    r8Clear(t(I18N_POD_STEP));r8Text(r8Screen,title,18,83,440,R8_TEXT,&gFont20);
    podStatus=r8Text(r8Screen,t(I18N_POD_KEEP_SPOOL),18,122,440,R8_MUTED);
    r8Bar=lv_bar_create(r8Screen);lv_obj_set_pos(r8Bar,18,220);lv_obj_set_size(r8Bar,444,8);
    lv_bar_set_range(r8Bar,0,72);lv_bar_set_value(r8Bar,0,LV_ANIM_OFF);
    lv_obj_set_style_bg_color(r8Bar,lv_color_hex(R8_ACCENT),LV_PART_INDICATOR);
    r8Button(t(I18N_POD_CANCEL),8,18,254,150);lv_timer_handler();
}
static void r8Progress(int slot,int page) {
    if(!r8Bar)return;
    uint32_t now=millis();if(page!=35 && now-r8PaintAt<100)return;r8PaintAt=now;
    char text[96];snprintf(text,sizeof(text),t(I18N_POD_PROGRESS),slot+1,(slot*36+page+1)*100/72);
    if(podStatus)lv_label_set_text(podStatus,text);
    lv_bar_set_value(r8Bar,slot*36+page+1,LV_ANIM_OFF);
}
static bool r8Confirm(const char* action) {
    r8Clear(t(I18N_POD_CONFIRM_T));
    r8Text(r8Screen,podTranslate?podTranslate(String(action)).c_str():action,18,76,444,R8_TEXT,&gFont20);
    r8Text(r8Screen,podTranslate?podTranslate(podConfirmContext):podConfirmContext,18,115,444,R8_MUTED);
    r8Text(r8Screen,t(I18N_POD_CONFIRM_H),18,228,444,R8_MUTED);
    r8Button(t(I18N_POD_CANCEL),8,18,258,150);r8Button(t(I18N_POD_CONFIRM_B),0,260,258,202,true);
    auto* bar=lv_bar_create(r8Screen);lv_obj_set_pos(bar,260,251);lv_obj_set_size(bar,202,4);
    lv_bar_set_range(bar,0,2000);lv_bar_set_value(bar,0,LV_ANIM_OFF);lv_refr_now(nullptr);
    uint32_t start=millis(),hold=0;bool released=false,ok=false;
    while(millis()-start<30000) {
        if(podService)podService();int16_t x,y;bool down=tsRead(x,y);
        if(!down){released=true;hold=0;}
        else if(released) {
            if(x<180&&y>=250)break;
            if(x>=260&&y>=250){if(!hold)hold=millis();if(millis()-hold>=2000){ok=true;break;}}
            else hold=0;
        }
        lv_bar_set_value(bar,hold?millis()-hold:0,LV_ANIM_OFF);lv_refr_now(nullptr);delay(15);
    }
    // LVGL input is deliberately not pumped during the physical hold.
    r8Working(ok?t(I18N_POD_OP_RUN):t(I18N_POD_OP_CANCELLED));return ok;
}
// Result navigation is explicit: tools return to tools; retry repeats that tool.
static void r86Result(bool ok,const String& detail,bool tools,int action) {
    r8Clear(ok?t(I18N_POD_OP_DONE):t(I18N_POD_OP_NOT_DONE));
    r8Text(r8Screen,ok?t(I18N_POD_ALL_READY):t(I18N_POD_INTERRUPTED),18,80,444,R8_TEXT,&gFont20);
    podStatus=r8Text(r8Screen,ok?String(t(I18N_POD_BACK_WEIGH)):(podTranslate?podTranslate(detail):detail),18,118,444,R8_MUTED);
    if(!ok && (action==2 || action==3 || action==4 || action==6)) {
        r8Button(t(I18N_POD_BACK),11,18,254,132);r8Button(t(I18N_POD_RETRY),14,164,254,140,true);r8Button(t(I18N_POD_WEIGH),7,318,254,144);
    } else {r8Button(t(I18N_POD_BACK),tools?11:12,18,254,216,true);r8Button(t(I18N_POD_WEIGH),7,246,254);}
    if(!ok && gPodMemRefused)r8Button(t(I18N_POD_RESTART_SCALE),17,18,196,444,true);
}
