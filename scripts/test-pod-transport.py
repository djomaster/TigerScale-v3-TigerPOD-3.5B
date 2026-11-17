#!/usr/bin/env python3
"""Exercise the actual GET_VERSION transport, not a high-level NTAG mock."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'TigerTagSplashESP32/TigerTagSplashESP32.ino').read_text()
a=s.index('    bool podVersion(');b=s.index('\n    bool writePage(',a);method=s[a:b]
shim=r'''
#include <cstdint>
#include <cstring>
#include <cstdio>
#define PN532_COMMAND_INCOMMUNICATETHRU 0x42
void resyncPn532Uart(int& n){n++;}
struct Transport {
 bool ack=true;uint8_t sent[8]={};size_t count=0;int timeout=0;
 bool sendCommandCheckAck(uint8_t* p,size_t n,int ms){memcpy(sent,p,n);count=n;timeout=ms;return ack;}
};
struct Probe {
 bool podCommandAck(uint8_t* p,size_t n){return _pn532.sendCommandCheckAck(p,n,100);}
 Transport _pn532;int _serial=0;bool frame=true;uint8_t length=11,reply=0x43,status=0,size=0x0f;
 bool readResponse(uint8_t*b,size_t,uint8_t&n){uint8_t data[]={0xd5,reply,status,0,4,4,2,1,0,size,3};memcpy(b,data,sizeof(data));n=length;return frame;}
'''
tests=r'''
};
int checks=0;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 for(uint8_t size:{0x0f,0x11,0x13}){
  Probe p;p.size=size;uint8_t v[8]={};CHECK(p.podVersion(v));
  CHECK(p._pn532.count==2 && p._pn532.sent[0]==0x42 && p._pn532.sent[1]==0x60);
  CHECK(v[0]==0 && v[1]==4 && v[6]==size && v[7]==3);CHECK(p._serial==0);
 }
 uint8_t v[8];
 for(int fault=0;fault<6;fault++){
  Probe p;memset(v,0xee,8);
  if(fault==0)p._pn532.ack=false;if(fault==1)p.frame=false;
  if(fault==2)p.length=3;if(fault==3)p.length=12;
  if(fault==4)p.reply=0x41;if(fault==5)p.status=1;
  CHECK(!p.podVersion(v));CHECK(p._serial==1);
  CHECK(v[0]==0xee && v[7]==0xee);
 }
 printf("POD GET_VERSION wire transport: %d checks passed\n",checks);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text('#include <initializer_list>\n'+shim+method+tests)
 subprocess.run(['c++','-std=c++17',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
