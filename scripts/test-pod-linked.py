#!/usr/bin/env python3
"""Actual shared-alias coordinator against an isolated in-memory Firestore."""
import os,runpy,subprocess,tempfile,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
c=runpy.run_path(str(ROOT/'scripts/test-pod-cloud.py'))
tests=c['tests'].split('int main() {')[0]+r'''
uint32_t millis(){return 10000;}
int main(){
 seed();InventoryRequest link;link.op=InventoryRequest::POD_LINK;link.source="TIGERDATA_spool";link.version="v1";
 link.uids[0]="12345678";link.uids[1]="87654321";link.chip[0]=link.chip[1]="mifare1k";
 r8LinkedRun(link);CHECK(link.ok&&posts==1&&server.size()==3);
 InventoryRequest scan;scan.op=InventoryRequest::POD_RESOLVE;scan.uids[0]=link.uids[0];scan.uids[1]=link.uids[1];r8LinkedRun(scan);
 CHECK(scan.ok&&scan.alias[0]&&scan.alias[1]);CHECK(batchReads==3);CHECK(scan.resolved[0]==link.source&&scan.resolved[1]==link.source);
 // A second balance has no local map: the cloud alone resolves the same UIDs.
 r8Seen[0]=R8Seen();r8Seen[1]=R8Seen();r8LinkedRun(scan);CHECK(scan.ok&&scan.resolved[0]==link.source);
 int before=posts;r8LinkedRun(link);CHECK(!link.ok&&posts==before); // never overwrite a previous alias
 offline=true;r8LinkedRun(scan);CHECK(!scan.ok);offline=false;
 server.erase("TIGERDATA_spool");r8LinkedRun(scan);CHECK(!scan.ok); // deleted target does not authorize weighing
 seed();lostReply=true;r8LinkedRun(link);CHECK(link.ok&&posts==1); // lost ACK reconciled
 seed();link.version="stale";r8LinkedRun(link);CHECK(!link.ok&&posts==0);link.version="v1";
 seed();denyCommit=true;r8LinkedRun(link);CHECK(!link.ok&&server.size()==1);denyCommit=false;
 // Existing TigerTag pair is kept, two maker UIDs become extra references.
 seed();InventoryJson job(512*1024);stage(job);commit(job);
 InventoryJson d(8192);deserializeJson(d,server["04112233445566"]);link.source="04112233445566";link.version=d["updateTime"].as<String>();
 r8LinkedRun(link);CHECK(link.ok);CHECK(server.count("04112233445566")&&server.count("04778899AABBCC"));
 r8LinkedRun(scan);CHECK(scan.ok&&scan.resolved[0]=="04112233445566");
 scan.uids[1]="04778899AABBCC";r8LinkedRun(scan);CHECK(scan.ok&&scan.alias[0]&&!scan.alias[1]);
 scan.uids[1]="00000000";r8LinkedRun(scan);CHECK(!scan.ok); // mixed known/unknown cannot weigh automatically
 scan.uids[0]="00000000";scan.uids[1]="FFFFFFFF";r8LinkedRun(scan);CHECK(scan.ok&&scan.resolved[0].isEmpty()&&scan.resolved[1].isEmpty());
 scan.uids[0]="BAD/UID";r8LinkedRun(scan);CHECK(!scan.ok);
 // Direct known Twin pair resolves in one read-only HTTPS request, reversed order.
 seed();InventoryJson fastJob(512*1024);stage(fastJob);commit(fastJob);
 scan.uids[0]="04112233445566";scan.uids[1]="04778899AABBCC";batchReads=0;int savedPosts=posts,savedGets=getCalls;
 r8LinkedRun(scan);CHECK(scan.ok&&batchReads==1&&posts==savedPosts&&getCalls==savedGets);
 CHECK(scan.resolved[0]==scan.uids[0]&&scan.resolved[1]==scan.uids[1]);
 for(int fault=1;fault<=4;fault++){batchFault=fault;r8LinkedRun(scan);CHECK(scan.ok&&posts==savedPosts);}
 batchFault=0;server.erase("04778899AABBCC");r8LinkedRun(scan);CHECK(!scan.ok); // not silently a valid pair
 scan.uids[0]=scan.uids[1]="00000000";batchReads=0;r8LinkedRun(scan);CHECK(scan.ok&&batchReads==2); // duplicate names elided
 printf("%d shared UID alias checks passed. No physical tags or account touched.\n",checks);
}
'''
inc=os.environ.get('ARDUINOJSON_INCLUDE')
if not inc:raise SystemExit('Set ARDUINOJSON_INCLUDE to ArduinoJson/src.')
with tempfile.TemporaryDirectory(prefix='pod-linked-') as td:
 p=Path(td);(p/'test.cpp').write_text(c['base']+c['shim']+c['functions']+'\nstd::map<uint16_t,String> gBrandDb,gMaterialDb;\nuint32_t millis();void delay(int){}\nstruct {template<class...A>void printf(const char*,A...){}} Serial;\n#include "pod_cloud_ca.h"\n#include "pod_tls.h"\n#include "pod_cloud.h"\n#include "pod_linked.h"\n'+tests)
 subprocess.run([os.environ.get('CXX') or shutil.which('g++'),'-std=c++17','-I'+inc,'-I'+str(ROOT/'TigerTagSplashESP32'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
