#!/usr/bin/env python3
"""Run the actual POD transaction functions against fake NFC/NVS/touch devices."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
header=(root/'TigerTagSplashESP32/pod_test.h').read_text().split('static void runPodTest() {')[0]
shim=r'''
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <cassert>
#include <algorithm>
struct String:std::string {using std::string::string;String(){}String(const std::string&s):std::string(s){}String(unsigned long n):std::string(std::to_string(n)){}String(unsigned int n):std::string(std::to_string(n)){}String(int n):std::string(std::to_string(n)){} };
struct SerialMock {void println(const String&){}template<class...T>void printf(const char*,T...){} } Serial;
uint32_t tick=0;uint32_t millis(){return tick;}void delay(int n){tick+=n;}
struct lv_obj_t{};void lv_label_set_text(lv_obj_t*,const char*){}void lv_timer_handler(){}
bool physical=true;int touches=0;
bool tsRead(int16_t&x,int16_t&y){x=430;y=280;return physical && (++touches%160)!=1;}
const int PN532_1_RST=1,PN532_2_RST=2,LOW=0;void digitalWrite(int,int){}
bool gPodWriteAllowed=false;int writes=0,failAt=-1,readCalls=0,rfCalls=0,injectAt=-1,injectMode=0;
void injectFault();bool lostAck=false,rfFail=false,storageFail=false;
std::vector<uint8_t> stored;
struct Preferences {
 bool begin(const char*,bool){return !storageFail;}void end(){}
 size_t getBytesLength(const char*){return stored.size();}
 size_t getBytes(const char*,void*p,size_t n){if(stored.size()!=n)return 0;memcpy(p,stored.data(),n);return n;}
 size_t putBytes(const char*,const void*p,size_t n){stored.assign((const uint8_t*)p,(const uint8_t*)p+n);return n;}
 bool remove(const char*){stored.clear();return true;}
};
struct PN532Reader {
 struct {uint8_t size=7,uidByte[7]={4,1,2,3,4,5,6};}uid;
 uint8_t mem[1024]={},size=0xf;bool present=true,on=false;
 bool configureRfField(bool b){rfCalls++;if(rfFail)return false;on=b;return true;}
 bool isNewCardPresent(){return present && on;}bool readCardSerial(){return present;}
 bool podVersion(uint8_t*v){uint8_t a[]={0,4,4,2,1,0,size,3};memcpy(v,a,8);return true;}
 bool readPages16(uint8_t p,uint8_t*b){readCalls++;if(!present||!on)return false;memcpy(b,mem+p*4,16);return true;}
 bool writePage(uint8_t p,const uint8_t*b){assert(gPodWriteAllowed);assert(p>=4&&p<=0x27);writes++;if(writes==failAt){if(lostAck)memcpy(mem+p*4,b,4);return false;}memcpy(mem+p*4,b,4);injectFault();return true;}
}rfid1,rfid2;
'''
tests=r'''
void injectFault(){
 if(writes!=injectAt)return;
 if(injectMode==1){rfid1.uid.uidByte[6]++;rfid1.mem[7]++;}
 if(injectMode==2)rfid1.present=false;
 if(injectMode==3)rfid1.mem[6*4]=0xe7;
 if(injectMode==4)podCancel=true;
 if(injectMode==5)rfid1.mem[0x80*4]=0xe7;
}
int checks=0;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
void reset(uint8_t size=0xf){
 rfid1=PN532Reader();rfid2=PN532Reader();rfid2.uid.uidByte[6]=7;
 stored.clear();podJournal={};writes=0;readCalls=rfCalls=0;injectAt=-1;injectMode=0;failAt=-1;rfFail=false;physical=true;touches=0;storageFail=false;podCancel=false;lostAck=false;
 for(auto*r:{&rfid1,&rfid2}){
  memcpy(r->mem,r->uid.uidByte,3);memcpy(r->mem+4,r->uid.uidByte+3,4);
  r->size=size;int cap=PodPolicy::capacity(size);r->mem[12]=0xe1;r->mem[13]=0x10;r->mem[14]=size==0xf?0x12:size==0x11?0x3e:0x6d;
  r->mem[(4+cap/4+1)*4+3]=0xff;
 }
 podJournal.magic=0x504f4431;podJournal.pair=123;
 for(int slot=0;slot<2;slot++){
  memcpy(podJournal.uid[slot],podReader(slot).uid.uidByte,7);podJournal.size[slot]=size;
  for(int i=0;i<144;i++)podJournal.target[slot][i]=(i+1)%255;
  podJournal.target[slot][39]=slot?0x22:0x12;
 }
 assert(podSave());
}
int main(){
 for(uint8_t size:{0xf,0x11,0x13}){
  reset(size);CHECK(podWrite(false));CHECK(writes==72);CHECK(!gPodWriteAllowed);CHECK(podCheckPair(true));CHECK(podWrite(true));CHECK(stored.empty());
 }
 // Every possible interrupted page, with and without a lost acknowledgement.
 for(int lost=0;lost<2;lost++)for(int cut=1;cut<=72;cut++){
  reset();failAt=cut;lostAck=lost;CHECK(!podWrite(false));CHECK(!stored.empty());CHECK(!gPodWriteAllowed);
  podJournal={};failAt=-1;CHECK(podWrite(false));CHECK(podCheckPair(true));
 }
 reset();rfid1.mem[7]^=1;CHECK(!podWrite(false));CHECK(writes==0);CHECK(strstr(podReadError,"UID changed")!=nullptr);
 reset();physical=false;CHECK(!podWrite(false));CHECK(writes==0); // remote activity cannot authorise
 reset();rfid2.uid.uidByte[6]++;CHECK(!podWrite(false));CHECK(writes==0);
 reset();rfid2.present=false;CHECK(!podWrite(false));CHECK(writes==0);
 reset();rfFail=true;CHECK(!podWrite(false));CHECK(writes==0);
 reset();rfid2.mem[10]=1;CHECK(!podWrite(false));CHECK(writes==0);
 reset();rfid2.mem[0x28*4]=1;CHECK(!podWrite(false));CHECK(writes==0);
 reset();rfid2.mem[0x29*4+3]=4;CHECK(!podWrite(false));CHECK(writes==0);
 reset();rfid2.mem[20]=0xe7;CHECK(!podWrite(false));CHECK(writes==0); // foreign/torn page
 reset();stored[22]^=1;CHECK(!podWrite(false));CHECK(writes==0);
 reset();podCancel=true;CHECK(!podWrite(false));CHECK(writes==0);
 reset();storageFail=true;CHECK(!podWrite(false));CHECK(writes==0);
 reset();rfid2.size=0x12;CHECK(!podWrite(false));CHECK(writes==0);
 reset();memcpy(podJournal.uid[1],podJournal.uid[0],7);CHECK(podSave());CHECK(!podWrite(false));CHECK(writes==0);
 uint8_t memory[888]={};CHECK(PodPolicy::blank(memory,888));
 for(int i=8;i<888;i++){memory[i]=1;CHECK(!PodPolicy::blank(memory,888));memory[i]=0;}
 reset(0x13);stored.clear();rfid1.mem[16]=3;rfid1.mem[18]=0xfe;rfid1.mem[0x80*4]=0x7b;
 CHECK(!podPrepare());CHECK(writes==0&&stored.empty());CHECK(strstr(podReadError,"Residual data P80: 7B")!=nullptr);
 reset(0x13);stored.clear();rfid1.mem[16]=0x5b;CHECK(!podPrepare());CHECK(writes==0&&stored.empty());CHECK(strstr(podReadError,"Nonblank header P04")!=nullptr);
 reset(0x13);stored.clear();rfid2.mem[0xe2*4]=1;CHECK(!podPrepare());CHECK(writes==0&&stored.empty());CHECK(strstr(podReadError,"Dynamic lock PE2")!=nullptr);
 reset(0x13);stored.clear();rfid2.mem[0xe3*4+3]=4;CHECK(!podPrepare());CHECK(writes==0&&stored.empty());CHECK(strstr(podReadError,"AUTH0 enabled PE3")!=nullptr);
 // Init is accepted only with a zero product and an entirely zero remainder.
 uint8_t init[888]={0x6c,0x41,0xa2,0xe1};CHECK(PodPolicy::blank(init,888));
 for(int i=4;i<888;i++){init[i]=1;CHECK(!PodPolicy::blank(init,888));init[i]=0;}
 init[0]=0x5b;init[1]=0xf5;init[2]=0x92;init[3]=0x64;CHECK(!PodPolicy::blank(init,888));
 init[0]=0xbc;init[1]=0x0f;init[2]=0xcb;init[3]=0x97;CHECK(!PodPolicy::blank(init,888));
 reset(0x13);stored.clear();
 const uint8_t magic[]={0x6c,0x41,0xa2,0xe1};
 for(auto*r:{&rfid1,&rfid2})memcpy(r->mem+16,magic,4);
 CHECK(podPrepare());CHECK(writes==0);CHECK(!memcmp(podJournal.before[0],magic,4));
 CHECK(podWrite(false));CHECK(podCheckPair(true));CHECK(podWrite(true));
 CHECK(!memcmp(rfid1.mem+16,magic,4)&&!memcmp(rfid2.mem+16,magic,4));CHECK(stored.empty());
 reset(0x13);stored.clear();memcpy(rfid1.mem+16,magic,4);rfid1.mem[20]=1;
 CHECK(!podPrepare());CHECK(writes==0&&stored.empty());
 reset(0x13);failAt=37;CHECK(!podWrite(false));CHECK(podErrorSlot==1&&podErrorPage==5);CHECK(!strcmp(podErrorPhase,"WRITE"));
 CHECK(podErrorReport().find("WRITE failed")!=std::string::npos);CHECK(!gPodWriteAllowed);CHECK(!stored.empty());
 failAt=-1;CHECK(podWrite(false));CHECK(podCheckPair(true));
 reset(0x13);CHECK(podWrite(false));CHECK(readCalls<2000);
 reset(0x13);physical=false;CHECK(!podWrite(false));CHECK(!strcmp(podErrorPhase,"confirmation"));CHECK(writes==0);
 reset();stored.clear();CHECK(!podWrite(false));CHECK(strstr(podReadError,"No valid journal")!=nullptr);CHECK(writes==0);
 for(uint8_t model:{0xf,0x11,0x13}){
  reset(model);uint8_t buffer[900],size;memset(buffer,0xa5,sizeof(buffer));CHECK(podSelect(0));
  CHECK(podInspect(0,buffer,size));CHECK(buffer[PodPolicy::capacity(model)]==0xa5);
  memset(buffer,0xa5,sizeof(buffer));CHECK(podInspect(0,buffer,size,true,144));CHECK(buffer[144]==0xa5);
 }

 // The active reader stays selected, but no next WRITE can use a substituted UID.
 for(int mode=1;mode<=4;mode++){
  reset(0x13);injectAt=1;injectMode=mode;CHECK(!podWrite(false));CHECK(writes==1);
  CHECK(!gPodWriteAllowed);CHECK(!stored.empty());
  if(mode==1)CHECK(strstr(podReadError,"UID changed")!=nullptr);
  if(mode==2)CHECK(strstr(podReadError,"Page readback failed")!=nullptr);
  if(mode==3)CHECK(strstr(podReadError,"Foreign/torn")!=nullptr);
  if(mode==4)CHECK(strstr(podReadError,"Cancelled")!=nullptr);
 }
 reset(0x13);injectAt=1;injectMode=5;CHECK(!podWrite(false));CHECK(strstr(podReadError,"Extended data changed")!=nullptr);CHECK(!stored.empty());
 reset(0x13);CHECK(podWrite(false));CHECK(readCalls<650);CHECK(rfCalls<=24);CHECK(podUidChecks==72);CHECK(podWrittenPages==72);
 int beforeWrites=writes;CHECK(podWrite(false));CHECK(writes==beforeWrites);CHECK(podSkippedPages==72);CHECK(podWrittenPages==0);
 printf("FAST transaction mock counts: full 72-page write bounded to <650 READs, <=24 RF commands\n");
 printf("POD actual transaction fault tests: %d checks passed\n",checks);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(shim+header+tests)
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-I'+str(root/'TigerTagSplashESP32'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
