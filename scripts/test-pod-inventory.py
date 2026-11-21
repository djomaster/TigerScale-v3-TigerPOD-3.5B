#!/usr/bin/env python3
"""Exercise actual r7 coordinator + actual NFC engine + actual cloud planner.
All peripherals, touch, file storage and HTTP are simulated. No firmware build.
"""
from pathlib import Path
import runpy,ast,os,subprocess,shutil,tempfile
ROOT=Path(__file__).resolve().parents[1]
cloud=runpy.run_path(str(ROOT/'scripts/test-pod-cloud.py'))
tree=ast.parse((ROOT/'scripts/test-pod.py').read_text())
nfc=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='shim' for t in n.targets))
nfc='\n'.join(l for l in nfc.splitlines() if not l.startswith('struct String:')).replace('struct PN532Reader {','struct PN532Reader {uint8_t sak=0;')
base=cloud['base'].replace('bool cancel=false,ok=false,', 'bool cancel=false,ok=false,done=false,')
engine=(ROOT/'TigerTagSplashESP32/pod_test.h').read_text().split('static void runPodTest() {')[0]
ui=r'''
void r86Memory(const char*){}const uint8_t R86_MEM_OK=0,R86_MEM_BLOCK=2;uint8_t r86MemoryCheck(bool,uint32_t*,uint32_t*){return 0;}String r86MemoryMessage(uint8_t,uint32_t,uint32_t){return String();}volatile bool gPodMemRefused=false;void r86FreeCaches(){}void r86ScheduleResume(){}
bool gLiveSuspended=true;lv_obj_t* lvScreen=nullptr;
void injectFault(){}
void blankFlowTick(){}
bool gInventoryAssignmentPending=false,gBackgroundTasksStarted=true,gCloudSendPending=false,gLivePaused=false,gPodPhysicalOnly=false,firebaseAuth=true;
InventoryRequest* gInventoryRequest=nullptr;
struct lv_img_hdr_t{int cf,z,r,w,h;};struct lv_img_dsc_t{lv_img_hdr_t header;int data_size;const unsigned char*data;};
const int LV_IMG_CF_ALPHA_8BIT=0,LV_IMG_CF_TRUE_COLOR_ALPHA=1,LV_OPA_COVER=255;
lv_obj_t* lv_img_create(lv_obj_t*p){return p;}void lv_img_set_src(lv_obj_t*,const void*){}
void lv_obj_set_style_img_recolor(lv_obj_t*,int,int){}void lv_obj_set_style_img_recolor_opa(lv_obj_t*,int,int){}
using lv_font_t=int;int gFont20=0,gFont16=0;
int gLiveTapPhase=0,gLiveTapSteps=0,LIVE_TAP_IDLE=0,LVCOL_BG=0,LVCOL_TEXT=0,LVCOL_ORANGE=0,gFont14=0;
uint32_t gLastActivityMs=0;
const int I18N_POD_READ=0,I18N_POD_NEWPAIR=0,I18N_POD_REFRESH=0,I18N_POD_VERIFY=0,I18N_POD_RESUME=0,I18N_POD_FORMAT=0,I18N_POD_S_READ=0,I18N_POD_S_NEWPAIR=0,I18N_POD_S_REFRESH=0,I18N_POD_S_VERIFY=0,I18N_POD_S_RESUME=0,I18N_POD_S_FORMAT=0,I18N_POD_TOOLS=0,I18N_POD_MGMT=0,I18N_POD_BLOCKED_T=0,I18N_POD_BLOCKED_D=0,I18N_POD_RESTORE=0,I18N_POD_UPD_T=0,I18N_POD_UPD_D=0,I18N_POD_LATER=0,I18N_POD_TOOLS_SUB=0,I18N_POD_TOOLS_NOTE=0,I18N_POD_REFRESH_TAGS=0,I18N_POD_FORMAT_PAIR=0,I18N_POD_BACK=0,I18N_POD_ST_SCAN=0,I18N_POD_ST_KNOWN=0,I18N_POD_ST_NTAG=0,I18N_POD_ST_ID=0,I18N_POD_WEIGH=0,I18N_POD_RESCAN=0;
typedef int LangKey;const int I18N_POD_X01=0,I18N_POD_X02=0,I18N_POD_X03=0,I18N_POD_X04=0,I18N_POD_X05=0,I18N_POD_X06=0,I18N_POD_X07=0,I18N_POD_X08=0,I18N_POD_X09=0,I18N_POD_X10=0,I18N_POD_X11=0,I18N_POD_X12=0,I18N_POD_X13=0,I18N_POD_X14=0,I18N_POD_X15=0,I18N_POD_X16=0,I18N_POD_X17=0,I18N_POD_X18=0,I18N_POD_X19=0,I18N_POD_X20=0,I18N_POD_X21=0,I18N_POD_X22=0,I18N_POD_X23=0,I18N_POD_X24=0,I18N_POD_X25=0,I18N_POD_X26=0,I18N_POD_X27=0,I18N_POD_X28=0,I18N_POD_X29=0,I18N_POD_X30=0,I18N_POD_X31=0,I18N_POD_CONFIRM_OP=0,I18N_POD_CONFIRM_RESTORE=0,I18N_POD_CONFIRM_REPLACE=0,I18N_POD_CONFIRM_TEST=0,I18N_POD_LINK_NOTE=0,I18N_POD_UNLINK_Q=0,I18N_POD_UNLINK_NOTE=0,I18N_POD_UNLINK_DONE=0,I18N_POD_CHK_SIG=0,I18N_POD_CHK_IDX=0,I18N_POD_CHK_DATE=0,I18N_POD_VER_OK=0,I18N_POD_VER_SIG=0,I18N_POD_VER_IDX=0,I18N_POD_VER_STAMP=0,I18N_POD_VER_LINKED=0,I18N_POD_SCAN_T=0,I18N_POD_VERIFY_T=0,I18N_POD_SCAN_ABSENT=0,I18N_POD_SCAN_KNOWN=0,I18N_POD_SCAN_NEW=0,I18N_POD_SCAN_OK=0,I18N_POD_SCAN_HALF=0,I18N_POD_SCAN_FREE=0,I18N_POD_SCAN_NOPAIR=0,I18N_POD_MEM_TOTAL=0,I18N_POD_MEM_BLOCK=0,I18N_POD_MEM_PSRAM=0,I18N_POD_MEM_PSRAM_BLOCK=0,I18N_POD_MEM_FIG=0,I18N_POD_MEM_RESTART=0,I18N_POD_MEM_RETRY=0,I18N_POD_STEP=0,I18N_POD_KEEP_SPOOL=0,I18N_POD_CANCEL=0,I18N_POD_PROGRESS=0,I18N_POD_CONFIRM_T=0,I18N_POD_CONFIRM_H=0,I18N_POD_CONFIRM_B=0,I18N_POD_OP_RUN=0,I18N_POD_OP_CANCELLED=0,I18N_POD_OP_DONE=0,I18N_POD_OP_NOT_DONE=0,I18N_POD_ALL_READY=0,I18N_POD_INTERRUPTED=0,I18N_POD_BACK_WEIGH=0,I18N_POD_RETRY=0,I18N_POD_RESTART_SCALE=0,I18N_POD_FINISH_FIRST=0,I18N_POD_RF_ERROR=0,I18N_POD_READING=0,I18N_POD_PREPARING=0,I18N_POD_M01=0,I18N_POD_M02=0,I18N_POD_M03=0,I18N_POD_M04=0,I18N_POD_M05=0,I18N_POD_M06=0,I18N_POD_M07=0,I18N_POD_M08=0,I18N_POD_M09=0,I18N_POD_M10=0,I18N_POD_M11=0,I18N_POD_M12=0,I18N_POD_M13=0,I18N_POD_M14=0,I18N_POD_M15=0,I18N_POD_M16=0,I18N_POD_M17=0,I18N_POD_M18=0,I18N_POD_M19=0,I18N_POD_M20=0,I18N_POD_M21=0,I18N_POD_M22=0,I18N_POD_M23=0,I18N_POD_M24=0,I18N_POD_M25=0,I18N_POD_M26=0,I18N_POD_M27=0,I18N_POD_M28=0,I18N_POD_M29=0,I18N_POD_M30=0,I18N_POD_M31=0,I18N_POD_M32=0,I18N_POD_M33=0,I18N_POD_M34=0,I18N_POD_M35=0,I18N_POD_M36=0;
static inline const char* t(int){return "";}
const int I18N_INV_LOADING=0,I18N_INV_SYNCING=0,LV_EVENT_CLICKED=1,LV_LABEL_LONG_WRAP=0,LV_LABEL_LONG_DOT=1,LV_OBJ_FLAG_SCROLLABLE=0,LV_PART_INDICATOR=0,LV_ANIM_OFF=0;
struct lv_event_t{};
void setupRFID(bool){}void lvglNotice(const char*){}void restartInventoryRead(const String*){}
void lv_obj_set_width(lv_obj_t*,int){}void lv_obj_set_pos(lv_obj_t*,int,int){}void lv_obj_set_size(lv_obj_t*,int,int){}
void lv_obj_set_style_bg_color(lv_obj_t*,int,int){}void lv_obj_set_style_text_color(lv_obj_t*,int,int){}
void lv_obj_set_style_text_font(lv_obj_t*,const int*,int){}void lv_obj_set_user_data(lv_obj_t*,void*){}
void* lv_obj_get_user_data(lv_obj_t*){return nullptr;}lv_obj_t* lv_event_get_target(lv_event_t*){return nullptr;}
void lv_obj_add_event_cb(lv_obj_t*,void(*)(lv_event_t*),int,void*){}
lv_obj_t* lv_scr_act(){return nullptr;}lv_obj_t* lv_obj_create(lv_obj_t*){return nullptr;}
lv_obj_t* lv_btn_create(lv_obj_t*){return nullptr;}lv_obj_t* lv_label_create(lv_obj_t*){return nullptr;}
void lv_obj_clean(lv_obj_t*){}void lv_obj_clear_flag(lv_obj_t*,int){}
int lv_color_hex(uint32_t c){return c;}void lv_label_set_long_mode(lv_obj_t*,int){}void lv_obj_set_height(lv_obj_t*,int){}
void lv_obj_set_style_radius(lv_obj_t*,int,int){}void lv_obj_set_style_border_width(lv_obj_t*,int,int){}void lv_obj_set_style_border_color(lv_obj_t*,int,int){}const int LV_OBJ_FLAG_CLICKABLE=0;const char*LV_SYMBOL_LEFT="";const char*LV_SYMBOL_EYE_OPEN="";const char*LV_SYMBOL_PLUS="";const char*LV_SYMBOL_REFRESH="";const char*LV_SYMBOL_OK="";const char*LV_SYMBOL_PLAY="";const char*LV_SYMBOL_TRASH="";void lv_obj_set_style_shadow_width(lv_obj_t*,int,int){}
void lv_refr_now(void*){}lv_obj_t* lv_bar_create(lv_obj_t*){return nullptr;}
void lv_bar_set_range(lv_obj_t*,int,int){}void lv_bar_set_value(lv_obj_t*,int,int){}
void lv_obj_center(lv_obj_t*){}void lv_scr_load(lv_obj_t*){}void lv_obj_del(lv_obj_t*){}
int inventoryPickSpool(std::vector<InventorySpool>&){return 0;}
bool inventoryWait(InventoryRequest& r,int,bool) {
 if(r.op==InventoryRequest::LIST){InventorySpool s;s.id="TIGERDATA_spool";s.version="v1";s.label="PLA blue";r.spools.push_back(s);return true;}
 if(r.op==InventoryRequest::POD_LINK||r.op==InventoryRequest::POD_RESOLVE||r.op==InventoryRequest::POD_UNLINK)r8LinkedRun(r);else r7CloudRun(r);return r.ok;
}
'''
tests=cloud['tests'].split('int main() {')[0]+r'''
void tags() {
 podReadersReversed=false;podConfirmUi=nullptr;rfid1=PN532Reader();rfid2=PN532Reader();rfid2.uid.uidByte[1]=9;
 for(auto* r:{&rfid1,&rfid2}) {
  r->size=0x13;memset(r->mem,0,sizeof(r->mem));
  memcpy(r->mem,r->uid.uidByte,3);memcpy(r->mem+4,r->uid.uidByte+3,4);
  r->mem[12]=0xe1;r->mem[13]=0x10;r->mem[14]=0x6d;r->mem[(4+888/4)*4+7]=0xff;
 }
 writes=0;failAt=-1;podCancel=false;physical=true;storageFail=false;touches=0;
}
int main() {
 seed();tags();r7Blocked=false;
 CHECK(r7New("associate"));CHECK(!r7Blocked);CHECK(writes>0&&posts==1&&server.size()==2);
 CHECK(rfid1.mem[16+39]==0x12&&rfid2.mem[16+39]==0x22);CHECK(r7Resume(true));
 int before=writes;CHECK(r7Resume());CHECK(writes==before);
 // Reencoding uses current inventory message, keeping UIDs and cloud weight.
 for(auto& entry:server){InventoryJson d(8192);deserializeJson(d,entry.second);d["fields"]["message"]["stringValue"]="Updated PLA";d["updateTime"]="edit";std::string out;serializeJson(d,out);entry.second=out;}
 CHECK(r7New("reencode"));CHECK(!memcmp(rfid1.mem+16+48,"Updated PLA",11));
 CHECK(!r7New("restore")); // backup tag data disagrees with edited pre-encode inventory
 CHECK(!r7Blocked); // failed staging must not replace completed journal
 CHECK(r7New("format"));CHECK(PodPolicy::blank(rfid1.mem+16,888));CHECK(server.size()==1);
 CHECK(r7New("restore"));CHECK(!memcmp(rfid1.mem+16+48,"Updated PLA",11));CHECK(server.size()==2);
 CHECK(!r7New("restore")); // one-level restore, no accidental toggle
 // Partial write, no cloud update, then physical rollback to original blank tags.
 seed();tags();r7Blocked=false;failAt=2;
 CHECK(!r7New("associate"));CHECK(r7Blocked&&posts==0&&server.size()==1);
 CHECK(!r7New("associate"));failAt=-1;
 CHECK(r7New("restore"));CHECK(!r7Blocked&&posts==0&&server.size()==1);
 CHECK(PodPolicy::blank(rfid1.mem+16,888)&&PodPolicy::blank(rfid2.mem+16,888));
 // Physical confirmation refusal creates only a recoverable plan, no write.
 seed();tags();r7Blocked=false;physical=false;
 CHECK(!r7New("associate"));CHECK(writes==0&&posts==0&&r7Blocked);
 physical=true;touches=0;CHECK(r7Resume());CHECK(!r7Blocked&&posts==1);
 // Cloud rejection after tags written retains recovery plan; retry skips pages.
 seed();tags();r7Blocked=false;denyCommit=true;
 CHECK(!r7New("associate"));CHECK(r7Blocked&&writes>0);before=writes;
 denyCommit=false;CHECK(r7Resume());CHECK(writes==before&&!r7Blocked);
 // Wrong UID cannot resume or restore and never sends a cloud commit.
 seed();tags();r7Blocked=false;failAt=2;CHECK(!r7New("associate"));before=writes;
 rfid2.uid.uidByte[1]++;failAt=-1;CHECK(!r7Resume());CHECK(writes==before&&posts==0);
 CHECK(!r7New("restore"));CHECK(writes==before&&posts==0);
 // Fast preview reads only header/configuration and 16 user bytes per tag.
 seed();tags();r7Blocked=false;r8DiscoveryBusy=r8DiscoveryPending=false;
 r8DiscoveryRequest=InventoryRequest();r8DiscoveryRequest.done=true;r8DiscoveryRequest.ok=true;r8DiscoveryRequest.owner=firebaseUid;
 for(int i=0;i<2;i++)r8DiscoveryRequest.uids[i]=podUid(podReader(i).uid.uidByte);
 r8Generation=r83LookupGeneration=7;r83LookupAt=millis();readCalls=0;
 CHECK(r8Scan());CHECK(r8UiCompatible&&!r8UiKnown);CHECK(readCalls==6);CHECK(batchReads==0&&writes==0&&posts==0);
 // Preview never authorizes a write: extended memory still refuses before NFC/cloud changes.
 rfid1.mem[0x80*4]=0x55;CHECK(r8Scan()&&r8UiCompatible);
 r8AcceptOccupied=true;CHECK(!r7New("associate"));r8AcceptOccupied=false;CHECK(writes==0&&posts==0);
 tags();rfid2.present=false;CHECK(r8Scan()&&!r8UiCompatible);CHECK(!r7New("associate")&&writes==0);
 tags();rfid2=rfid1;CHECK(r8Scan()&&!r8UiCompatible);CHECK(!r7New("associate")&&writes==0);
 tags();r83LookupAt=millis()-3001;int reads=batchReads;CHECK(r8Scan());CHECK(batchReads>reads);
 r83LookupAt=millis();r8DiscoveryRequest.owner="other";reads=batchReads;CHECK(r8Scan());CHECK(batchReads>reads);
 r8DiscoveryRequest.owner=firebaseUid;r8Generation++;reads=batchReads;CHECK(r8Scan());CHECK(batchReads>reads);
 // Retry reencoding resumes the exact existing plan after an interrupted page.
 seed();tags();r7Blocked=false;CHECK(r7New("associate"));
 for(auto& entry:server){InventoryJson d(8192);deserializeJson(d,entry.second);d["fields"]["message"]["stringValue"]="Retry material";d["updateTime"]="edit";std::string out;serializeJson(d,out);entry.second=out;}
 failAt=writes+1;CHECK(!r86RunTool(2,false));CHECK(r7Blocked);
 InventoryJson pending(512*1024);CHECK(r7Load(R7_JOB,pending));String journal=pending["journal"].as<String>();int priorPosts=posts;
 failAt=-1;CHECK(r86RunTool(2,true));CHECK(!r7Blocked&&posts==priorPosts+1);
 CHECK(r7Load(R7_JOB,pending));CHECK(pending["journal"].as<String>()==journal);
 // Format and restore retries preserve recovery, never start a new pair.
 failAt=writes+1;CHECK(!r86RunTool(3,false));failAt=-1;CHECK(r86RunTool(3,true));
 failAt=writes+1;CHECK(!r86RunTool(4,false));failAt=-1;CHECK(r86RunTool(4,true));CHECK(r86RunTool(6,true));
 podAt(1,-1,"capture");podReadError="no tag / selection timeout";CHECK(r86FailureText().find("Tag B non detecte")!=String::npos);

 // A production pair updates in either physical orientation without renumbering.
 seed();tags();r7Blocked=false;CHECK(r7New("associate"));
 String uidOne=podUid(rfid1.uid.uidByte),uidTwo=podUid(rfid2.uid.uidByte);
 auto edit=[](const char* msg){for(auto& entry:server){InventoryJson d(8192);deserializeJson(d,entry.second);d["fields"]["message"]["stringValue"]=msg;d["updateTime"]=msg;std::string out;serializeJson(d,out);entry.second=out;}};
 std::swap(rfid1,rfid2);edit("Reverse update");CHECK(r7New("reencode"));
 CHECK(podReadersReversed);CHECK(rfid1.mem[16+39]==0x22&&rfid2.mem[16+39]==0x12);
 CHECK(!memcmp(rfid1.mem+16+48,"Reverse update",14)&&!memcmp(rfid2.mem+16+48,"Reverse update",14));
 InventoryJson saved(512*1024);CHECK(r7Load(R7_JOB,saved));CHECK(saved["uid0"].as<String>()==uidOne&&saved["uid1"].as<String>()==uidTwo);
 InventoryJson doc(8192);deserializeJson(doc,server[uidOne]);CHECK(inventoryNumber(doc["fields"],"tag_index")==1);CHECK(inventoryText(doc["fields"],"twin_tag_uid")==uidTwo);
 deserializeJson(doc,server[uidTwo]);CHECK(inventoryNumber(doc["fields"],"tag_index")==2);CHECK(r7Resume(true));
 std::swap(rfid1,rfid2);CHECK(r7Resume(true));edit("Normal update");CHECK(r7New("reencode"));CHECK(!podReadersReversed);
 // Interrupted reverse update, rotation before retry: journal bytes are immutable.
 std::swap(rfid1,rfid2);edit("Resumed reverse");failAt=writes+1;CHECK(!r7New("reencode"));CHECK(r7Blocked);
 CHECK(r7Load(R7_JOB,saved));String immutable=saved["journal"].as<String>();priorPosts=posts;
 std::swap(rfid1,rfid2);failAt=-1;CHECK(r86RunTool(2,true));CHECK(!r7Blocked&&posts==priorPosts+1);
 CHECK(r7Load(R7_JOB,saved));CHECK(saved["journal"].as<String>()==immutable);CHECK(r7Resume(true));
 // Blank formatted tags are restored by journal UID, never by their erased index.
 std::swap(rfid1,rfid2);CHECK(r7New("format"));CHECK(PodPolicy::blank(rfid1.mem+16,888));
 std::swap(rfid1,rfid2);CHECK(r7New("restore"));CHECK(r7Resume(true));
 // Missing, duplicate, protected and mismatched indices still refuse writes.
 before=writes;priorPosts=posts;rfid2.present=false;CHECK(!r7New("reencode"));rfid2.present=true;
 CHECK(writes==before&&posts==priorPosts);
 auto keep=rfid2;rfid2=rfid1;CHECK(!r7New("reencode"));rfid2=keep;CHECK(writes==before&&posts==priorPosts);
 auto index=rfid2.mem[16+39];rfid2.mem[16+39]=rfid1.mem[16+39];CHECK(!r7New("reencode"));rfid2.mem[16+39]=index;
 rfid2.mem[10]=1;CHECK(!r7New("reencode"));rfid2.mem[10]=0;CHECK(writes==before&&posts==priorPosts);
 // Correct physical index is insufficient if cloud reciprocity has been edited.
 auto original=server[uidTwo];deserializeJson(doc,original);doc["fields"]["twin_tag_uid"]["stringValue"]="04000000000000";
 std::string altered;serializeJson(doc,altered);server[uidTwo]=altered;CHECK(!r7New("reencode"));server[uidTwo]=original;
 CHECK(writes==before&&posts==priorPosts);
 // A reversal after confirmation is not silently rebound during the write.
 edit("Moved after hold");podConfirmUi=[](const char*){std::swap(rfid1,rfid2);return true;};
 CHECK(!r7New("reencode"));CHECK(writes==before&&posts==priorPosts&&r7Blocked);podConfirmUi=nullptr;
 physical=false;CHECK(!r7Resume());CHECK(writes==before&&posts==priorPosts);
 physical=true;touches=0;CHECK(r7Resume());CHECK(r7Resume(true));
 printf("%d r7 coordinator checks passed (actual code, simulated peripherals).\n",checks);
}
'''
def main():
 inc=os.environ.get('ARDUINOJSON_INCLUDE')
 if not inc:
  candidates=list((ROOT/'.pio/libdeps').glob('*/ArduinoJson/src'));inc=str(candidates[0]) if candidates else None
 if not inc:raise SystemExit('Set ARDUINOJSON_INCLUDE to an existing ArduinoJson/src.')
 with tempfile.TemporaryDirectory(prefix='pod-inventory-') as td:
  p=Path(td);(p/'test.cpp').write_text((base+cloud['shim']+cloud['functions']+'\nstd::map<uint16_t,String> gBrandDb,gMaterialDb;\nstruct SerialMock {void println(const String&){}template<class...T>void printf(const char*,T...){} };extern SerialMock Serial;\nuint32_t millis();void delay(int);\n#include "pod_cloud_ca.h"\n#include "pod_tls.h"\n#include "pod_cloud.h"\n#include "pod_linked.h"\n'+nfc+'\n'+engine+ui+'\n#include "pod_inventory.h"\n'+tests).replace('struct SerialMock {void println(const String&){}template<class...T>void printf(const char*,T...){} } Serial;','SerialMock Serial;'))
  subprocess.run([os.environ.get('CXX') or shutil.which('g++'),'-std=c++17','-I'+inc,'-I'+str(ROOT/'TigerTagSplashESP32'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
if __name__=='__main__':main()
