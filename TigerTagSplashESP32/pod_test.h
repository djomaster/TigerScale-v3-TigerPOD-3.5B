#pragma once
#include "pod_policy.h"
// Single local transaction. Persisted BEFORE the first NFC write. No cloud API.
struct PodJournal {
    uint32_t magic, pair;
    uint8_t uid[2][7], size[2], before[2][144], target[2][144];
    uint32_t crc;
};
static PodJournal podJournal = {};
// Logical journal slots are bound to physical readers by fresh UIDs per operation.
static bool podReadersReversed=false;
static bool podCancel = false;
static lv_obj_t* podStatus = nullptr;
static String podConfirmContext;
static String podOperationLabel;
static void (*podService)() = nullptr;
static bool (*podConfirmUi)(const char*) = nullptr;
static void (*podProgressUi)(int,int) = nullptr;
static uint32_t podRunStartedMs=0;
static unsigned podWrittenPages=0,podSkippedPages=0,podUidChecks=0;
static const char* podReadError = "unknown";
static char podReadDetail[100];
static int podErrorSlot=-1, podErrorPage=-1;
static const char* podErrorPhase="idle";
static void podAt(int slot, int page, const char* phase) {
    podErrorSlot=slot<0?slot:(slot^(podReadersReversed?1:0));podErrorPage=page;podErrorPhase=phase;
}
static String podErrorReport() {
    char location[70];
    if(podErrorPage>=0) snprintf(location,sizeof(location),"%c P%02X %s: ",podErrorSlot<0?'?':podErrorSlot?'B':'A',podErrorPage,podErrorPhase);
    else snprintf(location,sizeof(location),"%c %s: ",podErrorSlot<0?'?':podErrorSlot?'B':'A',podErrorPhase);
    return String(location)+podReadError;
}
static bool podFail(const char* why) {
    podReadError=why;Serial.println("[POD ERROR] "+podErrorReport());return false;
}
static bool podPageFailure(const char* why, uint8_t page, const uint8_t* bytes) {
    snprintf(podReadDetail,sizeof(podReadDetail),"%s P%02X: %02X %02X %02X %02X",
             why,page,bytes[0],bytes[1],bytes[2],bytes[3]);
    return podFail(podReadDetail);
}
static bool podBlank(const uint8_t* memory, uint16_t capacity) {
    if(PodPolicy::blank(memory,capacity)) return true;
    // Diagnostic only: the accepted blank layouts and write permission stay unchanged.
    const uint8_t headers[][8]={{0},{0x6c,0x41,0xa2,0xe1,0,0,0,0},{3,0,0xfe,0},{1,3,0xa0,0x0c,0x34,3,0,0xfe},{3,4,0xd8,0,0,0,0xfe,0}};
    bool headerKnown=false;
    for(const auto& h:headers) if(!memcmp(memory,h,8)) headerKnown=true;
    if(!headerKnown) return podPageFailure("Nonblank header",4,memory);
    for(uint16_t offset=8;offset<capacity;offset++) {
        if(memory[offset]) return podPageFailure("Residual data",4+offset/4,memory+(offset&~3));
    }
    return podFail("Blank layout not accepted");
}
static PN532Reader& podReader(int slot) { return (bool(slot)!=podReadersReversed) ? rfid2 : rfid1; }
static String podUid(const uint8_t* uid) {
    char text[15]; for(int i=0;i<7;i++) snprintf(text+i*2,3,"%02X",uid[i]); return String(text);
}
static String (*podTranslate)(const String&)=nullptr;   // optional display-only translation (set by pod_inventory.h)
static void podSay(const String& text) {
    if(podService)podService();
    Serial.println("[POD] " + text);
    if (podStatus) { String shown=podTranslate?podTranslate(text):text; lv_label_set_text(podStatus,shown.c_str()); lv_timer_handler(); }
}
static bool podOff() {
    bool a=rfid1.configureRfField(false), b=rfid2.configureRfField(false);
    if (!a || !b) {
        // Hardware reset is the fail-closed fallback if an OFF ACK is lost.
        digitalWrite(PN532_1_RST,LOW); digitalWrite(PN532_2_RST,LOW);
        podSay("RF OFF failed: exit POD then reopen"); return podFail("RF OFF / exit and reopen");
    }
    return true;
}
static bool podSelect(int slot, const uint8_t* expected=nullptr) {
    podReadError="unknown";
    if (!podOff()) return false;
    delay(15);
    auto& r=podReader(slot);
    if (!r.configureRfField(true)) return podFail("RF ON command failed");
    delay(15);
    if (!r.isNewCardPresent()) return podFail("no tag / selection timeout");
    if (!r.readCardSerial()) return podFail("UID read failed");
    if (r.uid.size!=7 || r.uid.uidByte[0]!=4) return podFail("not NXP NTAG (UID)");
    if (expected && memcmp(expected,r.uid.uidByte,7)) {
        Serial.println("[POD UID] expected="+podUid(expected)+" actual="+podUid(r.uid.uidByte));
        return podFail("UID changed");
    }
    return true;
}
// Keep the RF selection and read the immutable manufacturer UID pages before
// each changed page. This is a fresh RF read, never a cached UID comparison.
static bool podQuickUid(int slot) {
    auto& r=podReader(slot);podUidChecks++;
    uint8_t header[16],actual[7];
    if(!r.readPages16(0,header))return podFail("UID page read failed before write");
    memcpy(actual,header,3);memcpy(actual+3,header+4,4); // skip BCC0
    if(memcmp(actual,podJournal.uid[slot],7)) {
        Serial.println("[POD UID] expected="+podUid(podJournal.uid[slot])+" actual="+podUid(actual));
        return podFail("UID changed before page write");
    }
    return true;
}
static bool podPage(PN532Reader& r, uint8_t page, uint8_t* four) {
    uint8_t b[16]; if(!r.readPages16(page,b)) return false;
    memcpy(four,b,4); return true;
}
static bool podInspect(int slot, uint8_t* memory, uint8_t& size, bool protection=true, uint16_t readLimit=0) {
    auto& r=podReader(slot); uint8_t v[8], b[16];
    const uint8_t prefix[]={0,4,4,2,1,0};
    if(!r.podVersion(v)) return podFail("GET_VERSION exchange failed");
    if(memcmp(v,prefix,6) || v[7]!=3 || !PodPolicy::capacity(v[6])) return podFail("unsupported NTAG version");
    size=v[6]; uint16_t cap=PodPolicy::capacity(size); uint8_t lock=4+cap/4;
    if(!r.readPages16(0,b)) return podFail("READ page 00 failed");
    uint8_t actualUid[7];memcpy(actualUid,b,3);memcpy(actualUid+3,b+4,4);
    if(r.uid.size!=7 || memcmp(actualUid,r.uid.uidByte,7))return podFail("UID changed during inspection");
    if(b[12]!=0xe1 || (b[13]!=0x10 && b[13]!=0x11)
       || b[14]!=(size==0xf?0x12:size==0x11?0x3e:0x6d) || (protection && b[15])) return podPageFailure("CC / access mismatch",3,b+12);
    if(protection && (b[10] || b[11])) return podPageFailure("Static lock",2,b+8);
    if(!r.readPages16(lock,b)) return podFail("READ configuration failed");
    // Dynamic locks; AUTH0; mirror/counter features; CFGLCK/PROT/AUTHLIM.
    if(protection && (b[0] || b[1] || b[2])) return podPageFailure("Dynamic lock",lock,b);
    if(protection && b[7]!=0xff) return podPageFailure("AUTH0 enabled",lock+1,b+4);
    if(protection && (b[4]&0xc0)) return podPageFailure("Mirror enabled",lock+1,b+4);
    if(protection && (b[8]&0xdf)) return podPageFailure("ACCESS protection",lock+2,b+8);
    // READ returns four pages already. Copy only user bytes in the last block.
    // Full capacity before/after a transaction; 144-byte record between writes.
    uint16_t count=readLimit && readLimit<cap?readLimit:cap;
    for(uint16_t offset=0;offset<count;offset+=16) {
        if(podService)podService();
        if(podCancel) return podFail("cancelled");
        if(!r.readPages16(4+offset/4,b)) {
            podErrorPage=4+offset/4;
            return podFail("user memory READ failed");
        }
        uint16_t n=count-offset<16?count-offset:16;
        memcpy(memory+offset,b,n);
        lv_timer_handler(); delay(1);
    }
    return true;
}
static bool podLoad() {
    Preferences n; if(!n.begin("pod-prod",true)) return false;
    bool ok=n.getBytesLength("journal")==sizeof(podJournal)
        && n.getBytes("journal",&podJournal,sizeof(podJournal))==sizeof(podJournal);
    n.end(); return ok && podJournal.magic==0x504f4431
        && podJournal.crc==PodPolicy::checksum(&podJournal,offsetof(PodJournal,crc));
}
static bool podSave() {
    podJournal.crc=PodPolicy::checksum(&podJournal,offsetof(PodJournal,crc));
    Preferences n; if(!n.begin("pod-prod",false)) return false;
    bool ok=n.putBytes("journal",&podJournal,sizeof(podJournal))==sizeof(podJournal); n.end();
    PodJournal expected=podJournal;
    return ok && podLoad() && !memcmp(&expected,&podJournal,sizeof(expected));
}
static bool podConfirm(const char* action) {
    if(podConfirmUi)return podConfirmUi(action);
    podSay(String(action)+"\n"+podConfirmContext+"\nRelease screen, then HOLD lower-right HOLD box 2s.\nTouch upper-left to cancel (30s timeout).");
    uint32_t start=millis(), hold=0; bool released=false;
    // No LVGL input processing here. tsRead bypasses injected taps in POD mode.
    while(millis()-start<30000) {
        if(podService)podService();
        int16_t x,y; bool down=tsRead(x,y);
        if(!down) {released=true;hold=0;}
        else if(released) {
            if(x<160 && y<100) return false;
            if(x>=370 && y>=255) {if(!hold)hold=millis();if(millis()-hold>=2000)return true;}
            else hold=0;
        }
        delay(15);
    }
    return false;
}
static bool podMatchRecord(int slot, const uint8_t* mem, bool exact, bool restore=false) {
    for(int offset=0;offset<144;offset+=4) {
        const uint8_t* desired=restore?podJournal.before[slot]:podJournal.target[slot];
        bool good=exact?!memcmp(mem+offset,desired+offset,4):
            (!memcmp(mem+offset,podJournal.before[slot]+offset,4) || !memcmp(mem+offset,podJournal.target[slot]+offset,4));
        if(!good) {
            podErrorPage=4+offset/4;
            Serial.printf("[POD MISMATCH] before=%02X%02X%02X%02X target=%02X%02X%02X%02X\n",
              podJournal.before[slot][offset],podJournal.before[slot][offset+1],podJournal.before[slot][offset+2],podJournal.before[slot][offset+3],
              desired[offset],desired[offset+1],desired[offset+2],desired[offset+3]);
            return podPageFailure(exact?"Verify mismatch":"Foreign/torn page",4+offset/4,mem+offset);
        }
    }
    return true;
}
static bool podCheckPair(bool exact, bool restore=false) {
    uint8_t mem[888], size;
    for(int slot=0;slot<2;slot++) {
        podAt(slot,-1,exact?"final verify":"preflight");
        if(!podSelect(slot,podJournal.uid[slot]) || !podInspect(slot,mem,size))return false;
        if(size!=podJournal.size[slot])return podFail("NTAG type changed");
        if(!podMatchRecord(slot,mem,exact,restore)) return false;
        for(int i=144;i<PodPolicy::capacity(size);i++) if(mem[i]) {
            podErrorPage=4+i/4;
            return podPageFailure("Extended data changed",4+i/4,mem+(i&~3));
        }
    }
    return podOff();
}
static bool podPrepare() {
    // Never replace an incomplete transaction. Completed pairs can be replaced
    // only after both are present and re-verified, followed by physical consent.
    if(podLoad()) {
        if(!podCheckPair(true)) {podSay("Existing journal: Resume or Restore first. Keep same two tags.");return false;}
        if(!podConfirm("Replace completed journal with a NEW pair?")) return false;
    }
    PodJournal next={}; next.magic=0x504f4431;
    time_t now=time(nullptr);
    if(now<1704067200) {podSay("Clock invalid: connect Wi-Fi for time, then retry.");return false;}
    next.pair=(uint32_t)(now-946684800);
    if(next.pair<=podJournal.pair) next.pair=podJournal.pair+1;
    uint8_t mem[888];
    for(int slot=0;slot<2;slot++) {
        if(!podSelect(slot) || !podInspect(slot,mem,next.size[slot])
            || !podBlank(mem,PodPolicy::capacity(next.size[slot]))) {
            podSay(String(slot?"B refused: ":"A refused: ")+podReadError+"\nNo tag written. Keep this message."); return false;
        }
        memcpy(next.uid[slot],podReader(slot).uid.uidByte,7);
        memcpy(next.before[slot],mem,144);
        auto* d=next.target[slot];
        d[0]=0x5b;d[1]=0xf5;d[2]=0x92;d[3]=0x64; // Standard TigerTag, never Certified.
        memset(d+4,0xff,4);d[12]=142;d[19]=255;d[23]=21;
        for(int b=0;b<4;b++) d[32+b]=next.pair>>(24-8*b);
        d[39]=slot?0x22:0x12;
        memcpy(d+48,"TIGERPOD TEST LOCAL",18);
    }
    if(!memcmp(next.uid[0],next.uid[1],7)) {podSay("Refused: same UID on both readers");return false;}
    if(!podOff())return false;
    podConfirmContext="A 1/2: "+podUid(next.uid[0])+" B 2/2: "+podUid(next.uid[1])+"\nTwin ID: "+String(next.pair);
    // Confirmation is before saving and never authorises another transaction.
    if(!podConfirm("Prepare local TEST pair?"))return false;
    podJournal=next;
    if(!podSave()){podSay("Journal storage/readback failed: no write");return false;}
    return true;
}
static bool podWrite(bool restore) {
    podRunStartedMs=0;podWrittenPages=podSkippedPages=podUidChecks=0;
    podAt(-1,-1,"journal");
    if(!podLoad())return podFail("No valid journal. Prepare first; no NFC write.");
    if(!memcmp(podJournal.uid[0],podJournal.uid[1],7))return podFail("Duplicate journal UIDs");
    if(!podCheckPair(false))return false;
    podConfirmContext=podOperationLabel+"\nTag 1/2: "+podUid(podJournal.uid[0])+" Tag 2/2: "+podUid(podJournal.uid[1])+"\nTwin ID: "+String(podJournal.pair);
    podAt(-1,-1,"confirmation");
    if(!podConfirm(restore?"RESTORE original blank data?":"Confirmer cette operation ?"))return podFail("Cancelled / hold timeout. No NFC write.");
    podRunStartedMs=millis();
    podSay("Working. Touch Cancel to stop between pages.");
    uint8_t mem[888], size;
    for(int slot=0;slot<2;slot++) {
        // One acknowledged RF switch and full safety check per reader, after
        // physical confirmation. The peer stays OFF for this entire block.
        podAt(slot,-1,"reader preflight");
        if(!podSelect(slot,podJournal.uid[slot]) || !podInspect(slot,mem,size))return false;
        if(size!=podJournal.size[slot])return podFail("NTAG type changed");
        if(!podMatchRecord(slot,mem,false))return false;
        for(int i=144;i<PodPolicy::capacity(size);i++) if(mem[i])
            return podPageFailure("Extended data changed",4+i/4,mem+(i&~3));
        const uint8_t* target=restore?podJournal.before[slot]:podJournal.target[slot];
        for(int step=0;step<36;step++) {
            int page=step==35?4:5+step; int off=(page-4)*4;
            if(podProgressUi)podProgressUi(slot,step);
            else if(step%6==0 || step==35)podSay(String("Tag ")+String(slot+1)+"/2 page "+String(step+1)+"/36");
            if(podService)podService();
            lv_timer_handler();podAt(slot,page,"before write");
            if(podCancel)return podFail("Cancelled; journal kept");
            // Already-correct pages are not rewritten. Final full verification
            // will still catch any change made since the per-reader snapshot.
            if(!memcmp(mem+off,target+off,4)){podSkippedPages++;continue;}
            if(!podQuickUid(slot))return false;
            uint8_t actual[4];
            if(!podPage(podReader(slot),page,actual))return podFail("Pre-write page read failed");
            if(memcmp(actual,podJournal.before[slot]+off,4) && memcmp(actual,podJournal.target[slot]+off,4))
                return podPageFailure("Foreign/torn page",page,actual);
            if(!memcmp(actual,target+off,4)){podSkippedPages++;continue;}
            podAt(slot,page,"WRITE");gPodWriteAllowed=true;
            bool ok=podReader(slot).writePage(page,target+off);
            gPodWriteAllowed=false;
            if(!ok)return podFail("WRITE failed; see TAG WRITE log");
            podAt(slot,page,"readback");
            if(!podPage(podReader(slot),page,actual))return podFail("Page readback failed");
            if(memcmp(actual,target+off,4))return podPageFailure("Page differs",page,actual);
            memcpy(mem+off,actual,4);podWrittenPages++;
        }
    }
    if(!podCheckPair(true,restore))return false;
    if(restore) {
        podAt(-1,-1,"journal clear");
        Preferences n;if(!n.begin("pod-prod",false))return podFail("NVS open failed; tags restored");
        bool ok=n.remove("journal");n.end();
        return ok?true:podFail("NVS removal failed; tags restored");
    }
    return true;
}
static void runPodTest() {
    if(gInventoryAssignmentPending || gInventoryRequest || wfPhase!=WF_IDLE) {
        lvglNotice("Finish weighing / pending inventory operation first.");return;
    }
    bool oldPause=gLivePaused;gLivePaused=true;gPodPhysicalOnly=true;
    gLiveTapPhase=LIVE_TAP_IDLE; gLiveTapSteps=0;podCancel=false;
    // Loop is paused here: no weighing workflow, background NFC or sleep.
    setupRFID(false);
    lv_obj_t* previous=lv_scr_act();lv_obj_t* scr=lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr,LVCOL_BG,0);
    podStatus=lv_label_create(scr);lv_obj_set_width(podStatus,450);lv_obj_set_pos(podStatus,15,12);
    lv_obj_set_style_text_color(podStatus,LVCOL_TEXT,0);
    static int action;action=0;
    const char* names[]={"Read","New pair","Resume","Verify","Restore","Exit","Cancel"};
    for(int i=0;i<7;i++) {
        auto* b=lv_btn_create(scr);lv_obj_set_size(b,108,42);lv_obj_set_pos(b,12+(i%4)*116,200+(i/4)*55);
        lv_obj_set_user_data(b,(void*)(intptr_t)(i+1));
        lv_obj_add_event_cb(b,[](lv_event_t* e){int a=(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));if(a==7)podCancel=true;else action=a;},LV_EVENT_CLICKED,nullptr);
        auto* l=lv_label_create(b);lv_label_set_text(l,names[i]);lv_obj_center(l);
    }
    auto* holdHint=lv_label_create(scr);lv_obj_set_pos(holdHint,374,267);
    lv_label_set_text(holdHint,"HOLD 2s");lv_obj_set_style_text_color(holdHint,LVCOL_ORANGE,0);
    lv_scr_load(scr);podSay("TigerPOD TEST - local only\n2 blank NTAG213/215/216. No cloud association.\nRead first; New pair then Resume to write.");
    bool done=false;
    while(!done) {
        lv_timer_handler();delay(5);
        int a=action;action=0;if(!a)continue;podCancel=false;
        bool ok=false;
        if(a==6){done=true;continue;}
        if(a==1) {
            String result;
            for(int slot=0;slot<2;slot++) {
                uint8_t mem[888],size=0;
                bool read=podSelect(slot)&&podInspect(slot,mem,size,false);
                result+=String(slot?"B: ":"A: ")+(read?podUid(podReader(slot).uid.uidByte)+" / "+String(PodPolicy::capacity(size))+" bytes":String(podReadError))+"\n";
                if(read)Serial.printf("[POD READ] index/count=%02X pair=%02X%02X%02X%02X\n",mem[39],mem[32],mem[33],mem[34],mem[35]);
            }
            podOff();podSay(result);continue;
        }
        if(a==2) {ok=podPrepare();if(ok)podSay("Prepared and saved. Resume writes after physical confirmation.");}
        if(a==3 || a==5 || a==4) {
            if(a==4) {
                podAt(-1,-1,"journal");ok=podLoad();
                if(!ok)podFail("No valid journal");else ok=podCheckPair(true);
            } else ok=podWrite(a==5);
            if(a==3 || a==5)Serial.printf("[POD PERF] operation_ms=%lu written=%u skipped=%u uid_checks=%u ok=%d\n",
                podRunStartedMs?(unsigned long)(millis()-podRunStartedMs):0UL,podWrittenPages,podSkippedPages,podUidChecks,ok?1:0);
            String detail=podErrorReport();gPodWriteAllowed=false;
            bool off=podOff();
            if(!off && ok){ok=false;detail=podErrorReport();}
            if(ok)podSay(a==5?"Original blank data restored; journal cleared":"Both tags VERIFIED 1/2 + 2/2; local Twin pair OK");
            else podSay("NOT validated\n"+detail+"\nKeep same tags and existing journal.");
            action=0;continue;
        }
        gPodWriteAllowed=false;podOff();action=0;
    }
    gPodWriteAllowed=false;podOff();
    gLiveTapPhase=LIVE_TAP_IDLE;gLiveTapSteps=0;gPodPhysicalOnly=false;gLivePaused=oldPause;
    lv_scr_load(previous);lv_obj_del(scr);podStatus=nullptr;
    setupRFID(false);gLastActivityMs=millis();
}
