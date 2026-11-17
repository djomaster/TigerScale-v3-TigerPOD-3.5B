#!/usr/bin/env python3
"""Exercise discovery gates and removal recovery without cloud or hardware."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
code=r'''
#include <string>
#include <cstdint>
#include <cstdio>
#include <cassert>
struct String:std::string {using std::string::string;String(){}String(uint32_t n):std::string(std::to_string(n)){}String(const std::string&s):std::string(s){}bool isEmpty()const{return empty();}};
uint32_t tick=10000;uint32_t millis(){return tick;}
struct R8Seen{String uid,target;uint32_t at=0;bool checked=false,alias=false;}r8Seen[2];
String firebaseUid="owner",r8SeenOwner="owner",r7CloudError;
bool r7Active=false,r7Blocked=false,r8DiscoveryBusy=false,gBackgroundTasksStarted=true,lvglReady=true,gZeroRequired=false,r8WeighReady=false,r8Offered=false,firebaseAuth=true,gCloudSendPending=false,gInventoryRequest=false,gBlankLoadLatched=false,gBlankLoadWasPresent=false;
uint32_t r8DiscoveryAt=0,r8Generation=0,r83LookupAt=0,r83LookupGeneration=0,gBlankEmptySinceMs=0;bool loaded=true;
bool blankLoadPresent(){return loaded;}void netLog(const String&){}
struct InventoryRequest{enum{POD_RESOLVE}op;String owner,uids[2],resolved[2];bool alias[2]={},done=false,ok=false,needsUpdate=false;};
InventoryRequest r8DiscoveryRequest;bool r8DiscoveryPending=false;
int calls=0,menus=0;bool online=true;
void worker(){assert(r8DiscoveryPending);calls++;auto&r=r8DiscoveryRequest;bool known=false,unknown=false;for(int i=0;i<2;i++)if(!r.uids[i].empty()){if(r.uids[i]=="known"){r.resolved[i]="spool";known=true;}else unknown=true;}r.ok=online&&!(known&&unknown);r8DiscoveryPending=false;r.done=true;}
void runPodInventory(bool=false,bool=false){menus++;}
bool r86UpdateWanted=false,r86Prompt=false,gInventoryAssignmentPending=false;String r86UpdateKey,r86LaterKey;void r86ResetUpdate(){r86UpdateWanted=false;r86UpdateKey="";r86LaterKey="";}
#include "pod_discovery.h"
void observe(int slot,const char*uid){if(r8Seen[slot].uid!=uid){r8Seen[slot]=R8Seen();r8Seen[slot].uid=uid;r8Generation++;r8WeighReady=false;}r8Seen[slot].at=tick;}
int main(){int checks=0;
#define CHECK(x) do{assert(x);checks++;}while(0)
 observe(0,"known");r8DiscoveryAt=tick;r8DiscoveryTick();CHECK(!r8DiscoveryPending);tick+=499;r8DiscoveryTick();CHECK(!r8DiscoveryPending);tick++;r8DiscoveryTick();CHECK(r8DiscoveryPending&&!r8WeighReady&&calls==0);
 // The loop returns with HTTP still pending: sampling/rendering can continue.
 for(int i=0;i<30;i++){tick+=100;observe(0,"known");r8DiscoveryTick();CHECK(calls==0&&r8DiscoveryPending);}
 worker();r8DiscoveryTick();CHECK(r8WeighReady&&menus==0&&calls==1);
 // 2-5 second RF gaps do not requery or switch screens.
 for(int i=0;i<5;i++){tick+=5000;r8DiscoveryTick();CHECK(!r8WeighReady&&!r8DiscoveryPending);observe(0,"known");r8DiscoveryTick();CHECK(r8WeighReady);}
 tick+=6100;r8DiscoveryTick();CHECK(!r8WeighReady&&!r8DiscoveryPending);
 observe(0,"known");r8DiscoveryTick();CHECK(r8WeighReady&&calls==1); // cached same placement
 observe(1,"unknown");r8DiscoveryTick();CHECK(!r8WeighReady&&r8DiscoveryPending);worker();r8DiscoveryTick();CHECK(!r8WeighReady);
 tick+=6100;observe(0,"known");r8DiscoveryTick();CHECK(r8WeighReady); // unknown removed, known cache retained
 // Lift invalidates the placement and discards an in-flight result.
 observe(1,"unknown");tick+=11000;observe(0,"known");observe(1,"unknown");r8DiscoveryTick();CHECK(r8DiscoveryPending);
 loaded=false;r8DiscoveryTick();tick+=601;r8DiscoveryTick();CHECK(r8Seen[0].uid.empty());worker();r8DiscoveryTick();CHECK(!r8WeighReady);
 loaded=true;tick+=11000;observe(0,"unknown");r8DiscoveryTick();worker();r8DiscoveryTick();CHECK(menus==1&&!r8WeighReady);
 for(int i=0;i<5;i++){tick+=3000;observe(0,"unknown");r8DiscoveryTick();}CHECK(menus==1&&!r8DiscoveryPending);
 // New UID while a request runs: never accept the old response.
 observe(0,"known");r8DiscoveryTick();CHECK(r8DiscoveryPending);observe(0,"replacement");worker();r8DiscoveryTick();CHECK(!r8WeighReady);
 CHECK(r8DiscoveryPending);worker();r8DiscoveryTick();CHECK(!r8WeighReady);
 // Account switch invalidates even a successful late response.
 observe(0,"known");r8DiscoveryTick();CHECK(r8DiscoveryPending);firebaseUid="other";worker();r8DiscoveryTick();CHECK(!r8WeighReady&&r8Seen[0].uid.empty());
 tick+=11000;observe(0,"known");online=false;r8DiscoveryTick();worker();r8DiscoveryTick();CHECK(!r8WeighReady);
 tick+=4000;observe(0,"known");r8DiscoveryTick();CHECK(!r8DiscoveryPending);
 tick+=7000;observe(0,"known");online=true;r8DiscoveryTick();CHECK(r8DiscoveryPending);worker();r8DiscoveryTick();CHECK(r8WeighReady);
 observe(1,"new");r8DiscoveryAt=tick;r8DiscoveryTick();CHECK(!r8DiscoveryPending);
 tick+=149;r8DiscoveryTick();CHECK(!r8DiscoveryPending);tick++;r8DiscoveryTick();CHECK(r8DiscoveryPending);
 printf("%d asynchronous discovery/stability checks passed.\n",checks);
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++11','-I'+str(root/'TigerTagSplashESP32'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
