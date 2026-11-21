#!/usr/bin/env python3
"""Compile and exercise the actual r7 cloud planner with in-memory Firestore/FS.
No network, account, tag writes or PlatformIO build.
"""
from pathlib import Path
import runpy, os, shutil, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
legacy=runpy.run_path(str(ROOT/'scripts/test-inventory.py'))
base=legacy['shim'].split('std::map<uint16_t,String> gBrandDb')[0]
base=base.replace('const char* POD_CLOUD_CA="fixture";','').replace('bool r7Blocked=false;','').replace('int r7Prompts=0;','').replace('void runPodInventory(bool=false,bool=false){r7Prompts++;}','').replace('void r7CloudRun(InventoryRequest&){}','').replace('void r8LinkedRun(InventoryRequest&){}','').replace('struct R8TestSeen{bool alias=false;String target;};R8TestSeen r8Seen[2];','').replace('void r8Observe(int,const String&){}bool r8Allowed(int,const String&){return true;}','')
base=base.replace('uint8_t releaseCount=0;', 'uint8_t releaseCount=0,podAction=0;')
shim=r'''
#include <cassert>
String firebaseUid="owner",firebaseIdToken="token";
class Stream {public:virtual ~Stream(){}virtual int available()=0;virtual int read()=0;virtual int peek()=0;virtual void flush()=0;virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t*,size_t)=0;};
#define MALLOC_CAP_SPIRAM 2
#define MALLOC_CAP_8BIT 4
bool failResponseAlloc=false;
void* heap_caps_realloc(void* p,size_t n,int){return failResponseAlloc?nullptr:realloc(p,n);}
void heap_caps_free(void* p){free(p);}
void netLog(const String&){}
const char* INVENTORY_ROOT="https://firestore.googleapis.com/v1/projects/tigertag-connect/databases/(default)/documents";
std::map<std::string,std::string> disk,server;
bool failSave=false,offline=false,lostReply=false,denyCommit=false;int posts=0,getCalls=0,serial=1,batchReads=0,batchFault=0;
struct File {
 std::string data;size_t at=0;bool valid=false;
 File(){}File(std::string d):data(d),valid(true){}operator bool()const{return valid;}
 size_t size(){return data.size();}void close(){}int read(){return at<data.size()?(unsigned char)data[at++]:-1;}
 size_t readBytes(char* p,size_t n){size_t k=std::min(n,data.size()-at);memcpy(p,data.data()+at,k);at+=k;return k;}
};
struct {File open(const char* p,const char*){auto it=disk.find(p);return it==disk.end()?File():File(it->second);}bool exists(const char* p){return disk.count(p);} }LittleFS;
bool inventorySaveJson(const char* p,JsonVariantConst d){if(failSave)return false;std::string s;serializeJson(d,s);disk[p]=s;return true;}
struct WiFiClientSecure {bool secure=false;void setCACert(const char* p){secure=p&&strstr(p,"BEGIN CERTIFICATE");}void stop(){}};
struct HTTPClient {
 std::string url,body;bool auth=false;
 void setTimeout(int){}void setReuse(bool){}void useHTTP10(bool){}bool begin(WiFiClientSecure& c,const String& u){assert(c.secure);url=u;return true;}
 void addHeader(const char* k,const String& v){if(!strcmp(k,"Authorization"))auth=v=="Bearer token";}
 int GET(){assert(auth);getCalls++;if(offline)return -1;auto i=server.find((url.find("/rfidAliases/")!=std::string::npos?"alias:":"")+url.substr(url.find_last_of('/')+1));if(i==server.end())return 404;body=i->second;return 200;}
 int writeToStream(Stream* sink){return sink->write((const uint8_t*)body.data(),body.size())==body.size()?(int)body.size():-1;}
 String getString(){return body;}void end(){}
 int POST(const String& s){
  if(url.find(":batchGet")!=std::string::npos){
   assert(auth);batchReads++;if(offline || batchFault==1)return -1;
   if(batchFault==3){body="invalid";return 200;}
   InventoryJson q(8192),out(128*1024);assert(!deserializeJson(q,s));auto rows=out.to<JsonArray>();
   auto names=q["documents"].as<JsonArray>();
   for(int i=(int)names.size()-1;i>=0;i--){
    if(batchFault==2 && i==0)continue;
    String name=names[i];std::string id=(name.find("/rfidAliases/")!=std::string::npos?"alias:":"")+name.substr(name.find_last_of('/')+1);
    auto row=rows.createNestedObject();auto found=server.find(id);
    if(found==server.end())row["missing"]=name;
    else {InventoryJson doc(128*1024);deserializeJson(doc,found->second);row["found"].set(doc.as<JsonVariantConst>());row["found"]["name"]=name;}
   }
   if(batchFault==4 && rows.size()){auto extra=rows.createNestedObject();extra.set(rows[0]);}
   body.clear();serializeJson(out,body);return 200;
  }
  File f(s);return sendRequest("POST",&f,f.size());
 }
 int sendRequest(const char*,File* f,size_t) {
  assert(auth);posts++;if(offline)return -1;if(denyCommit)return 403;
  InventoryJson d(512*1024);assert(!deserializeJson(d,f->data));
  // Enforce Firestore atomic preconditions, then apply all writes together.
  for(JsonObject w:d["writes"].as<JsonArray>()) {
   String name=w.containsKey("delete")?w["delete"].as<String>():w["update"]["name"].as<String>();
   std::string id=(name.find("/rfidAliases/")!=std::string::npos?"alias:":"")+name.substr(name.find_last_of('/')+1);auto it=server.find(id);
   if(w["currentDocument"].containsKey("exists")){if(it!=server.end())return 412;}
   else {InventoryJson current(128*1024);if(it==server.end()||deserializeJson(current,it->second)||current["updateTime"]!=w["currentDocument"]["updateTime"])return 412;}
  }
  for(JsonObject w:d["writes"].as<JsonArray>()) {
   String name=w.containsKey("delete")?w["delete"].as<String>():w["update"]["name"].as<String>();
   std::string id=(name.find("/rfidAliases/")!=std::string::npos?"alias:":"")+name.substr(name.find_last_of('/')+1);
   if(w.containsKey("delete"))server.erase(id);
   else {InventoryJson n(128*1024);n["fields"].set(w["update"]["fields"]);n["updateTime"]=String(serial++);std::string out;serializeJson(n,out);server[id]=out;}
  }return lostReply?-1:200;
 }
};
'''
shim+=r'''
int inventoryGet(const String& url,InventoryJson& d,bool=true) {
 if(offline)return -1;
 if(url.find("?")==String::npos) {
  WiFiClientSecure c;c.setCACert("BEGIN CERTIFICATE");HTTPClient h;h.begin(c,url);h.addHeader("Authorization","Bearer token");
  int code=h.GET();d.clear();if(code==200)deserializeJson(d,h.getString());return code;
 }
 d.clear();auto docs=d.createNestedArray("documents");
 for(auto& item:server)if(item.first.rfind("alias:",0)==0) {
  InventoryJson source(8192);deserializeJson(source,item.second);
  auto entry=docs.createNestedObject();entry.set(source.as<JsonVariantConst>());
  entry["name"]="projects/tigertag-connect/databases/(default)/documents/users/owner/rfidAliases/"+item.first.substr(6);
 }
 return 200;
}
'''
functions=''.join(legacy['extract'](x) for x in ['static double inventoryNumber(', 'static String inventoryText(', 'static bool inventoryChipless(', 'static bool inventoryEncode(', 'static String inventoryUrlEncode(', 'static String inventoryDocUrl('])
tests=r'''
int checks=0;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void saveServer(const char* id,InventoryJson& d){std::string s;serializeJson(d,s);server[id]=s;}
void seed() {
 server.clear();disk.clear();offline=failSave=lostReply=denyCommit=false;posts=getCalls=batchReads=batchFault=0;
 InventoryJson d(8192);d["updateTime"]="v1";auto f=d.createNestedObject("fields");
 f["id_type"]["integerValue"]="142";f["id_unit"]["integerValue"]="21";f["measure"]["integerValue"]="1000";
 f["weight_available"]["integerValue"]="900";f["last_update"]["integerValue"]="1";f["message"]["stringValue"]="PLA blue";
 saveServer("TIGERDATA_spool",d);
}
void stage(InventoryJson& job,const char* kind="associate") {
 job.clear();job["owner"]="owner";job["kind"]=kind;job["uid0"]="04112233445566";job["uid1"]="04778899AABBCC";
 job["stamp"]=1234;job["source"]="TIGERDATA_spool";job["version"]="v1";
 CHECK(r7Save(R7_STAGE,job));InventoryRequest r;r7CloudRun(r);CHECK(r.ok);CHECK(r7Load(R7_STAGE,job));
}
void commit(InventoryJson& job) {
 job["state"]="written";CHECK(r7Save(R7_JOB,job));InventoryRequest r;r.podAction=2;r7CloudRun(r);CHECK(r.ok);CHECK(r7Load(R7_JOB,job));CHECK(job["state"]=="done");
}
int main() {
 seed();InventoryJson job(512*1024);stage(job);
 CHECK(posts==0&&server.size()==1);CHECK(job["docs"].size()==5);
 CHECK(inventoryNumber(job["docs"][1]["after"],"tag_index")==1);
 CHECK(inventoryNumber(job["docs"][2]["after"],"tag_index")==2);
 CHECK(inventoryText(job["docs"][1]["after"],"twin_tag_uid")=="04778899AABBCC");
 CHECK(!disk.count(R7_JOB)); // staging must not replace active recovery journal
 job["state"]="pending";CHECK(r7Save(R7_JOB,job));InventoryRequest r;r.podAction=2;r7CloudRun(r);CHECK(!r.ok&&posts==0);
 lostReply=true;commit(job);CHECK(posts==1&&server.size()==2&&!server.count("TIGERDATA_spool"));
 r7CloudRun(r);CHECK(r.ok&&posts==1); // lost-response reconciliation, no repeated POST
 r.podAction=4;r7CloudRun(r);CHECK(r.ok);offline=true;r7CloudRun(r);CHECK(!r.ok);offline=false;
 // Reencoding reads latest metadata and weight.
 stage(job,"reencode");CHECK(posts==1);CHECK(job["docs"].size()==2);
 // Formatting keeps a chipless document and conditionally deletes both UIDs.
 stage(job,"format");CHECK(job["docs"].size()==3);CHECK(job["docs"][0]["after"].isNull());
 CHECK(inventoryText(job["docs"][2]["after"],"uid")=="TIGERDATA_spool");
 job["journal"]="backup bytes";commit(job);CHECK(server.size()==1&&server.count("TIGERDATA_spool"));
 // Restoration preserves the newest weight in both twin documents.
 InventoryJson current(8192);deserializeJson(current,server["TIGERDATA_spool"]);
 current["fields"]["weight_available"]["integerValue"]="255";current["fields"]["last_update"]["integerValue"]="99";
 current["updateTime"]="newWeight";saveServer("TIGERDATA_spool",current);
 InventoryJson restore(512*1024);restore["kind"]="restore";restore["owner"]="owner";
 restore["uid0"]="04112233445566";restore["uid1"]="04778899AABBCC";
 CHECK(r7RestorePlan(restore));CHECK(inventoryNumber(restore["docs"][0]["after"],"weight_available")==255);
 CHECK(inventoryNumber(restore["docs"][1]["after"],"weight_available")==255);
 CHECK(restore["restoreJournal"]=="backup bytes");
 current["fields"]["message"]["stringValue"]="user changed material";saveServer("TIGERDATA_spool",current);
 CHECK(!r7RestorePlan(restore)); // never lose later metadata edits
 // An incomplete NFC operation can be rolled back before cloud commit.
 seed();stage(job);job["state"]="pending";job["journal"]="backup";CHECK(r7Save(R7_JOB,job));
 CHECK(r7RestorePlan(restore));CHECK(restore["rollback"]==true);
 CHECK(r7ServerMatches(restore,true));CHECK(posts==0);
 // Old selected version, occupied UID and account mismatch refuse planning.
 seed();stage(job);current.clear();deserializeJson(current,server["TIGERDATA_spool"]);current["updateTime"]="v2";saveServer("TIGERDATA_spool",current);
 r.podAction=1;CHECK(r7Save(R7_JOB,job));r7CloudRun(r);CHECK(!r.ok&&posts==0);
 r.podAction=0;r7CloudRun(r);CHECK(!r.ok); // selected v1 changed
 seed();stage(job);server["04112233445566"]=server["TIGERDATA_spool"];r7CloudRun(r);CHECK(!r.ok);
 seed();stage(job);job["owner"]="other";CHECK(r7Save(R7_STAGE,job));r7CloudRun(r);CHECK(!r.ok);
 // Corruption and persistent write failure must fail closed.
 seed();stage(job);disk[R7_STAGE][15]^=1;r7CloudRun(r);CHECK(!r.ok);
 failSave=true;CHECK(!r7Save(R7_JOB,job));failSave=false;
 uint8_t bytes[4]={0,1,128,255},round[4];CHECK(r7Unhex(r7Hex(bytes,4),round,4)&&!memcmp(bytes,round,4));
 CHECK(!r7Unhex("000180FG",round,4));CHECK(!r7Unhex("000180",round,4));
 // Completed Verify ignores weight changes but not identity/metadata changes.
 seed();stage(job);commit(job);current.clear();deserializeJson(current,server["04112233445566"]);
 current["fields"]["weight_available"]["integerValue"]="2";current["fields"]["last_update"]["timestampValue"]="2026-11-12T14:00:00Z";
 saveServer("04112233445566",current);CHECK(r8VerifyLive(job));CHECK(!r7ServerMatches(job,true));
 current["fields"]["tag_index"]["integerValue"]="2";saveServer("04112233445566",current);CHECK(!r8VerifyLive(job));
 // Timestamp and legacy epoch values must order identically for restore/reencode.
 current.clear();current["last_update"]["timestampValue"]="1970-01-02T00:00:00Z";
 CHECK(r8WeightTime(current.as<JsonObjectConst>())==86400000);
 current["last_update"]["timestampValue"]="2026-11-13T12:30:00Z";
 double stamp=r8WeightTime(current.as<JsonObjectConst>());CHECK(stamp==1794573000000.0);
 current["last_update"].clear();current["last_update"]["integerValue"]=String((uint64_t)stamp);
 CHECK(r8WeightTime(current.as<JsonObjectConst>())==stamp);
 // Shared alias follows chipless -> Twin -> chipless in conditional batches.
 seed();current.clear();current["fields"]["spoolId"]["stringValue"]="TIGERDATA_spool";
 current["fields"]["chip"]["stringValue"]="mifare1k";current["fields"]["vendor"]["stringValue"]="unknown";current["updateTime"]="aliasV1";
 saveServer("alias:12345678",current);stage(job);CHECK(job["docs"].size()==6);commit(job);
 current.clear();deserializeJson(current,server["alias:12345678"]);CHECK(inventoryText(current["fields"],"spoolId")=="04112233445566");
 stage(job,"format");commit(job);current.clear();deserializeJson(current,server["alias:12345678"]);
 CHECK(inventoryText(current["fields"],"spoolId")=="TIGERDATA_spool");
 // Streamed CRC must remain byte-for-byte compatible with old journals.
 InventoryJson crcDoc(8192);crcDoc["text"]="escaped \" value";crcDoc["number"]=123;
 String legacy;serializeJson(crcDoc,legacy);uint32_t expected=PodPolicy::checksum(legacy.c_str(),legacy.length());
 PodCrcWriter crc;CHECK(serializeJson(crcDoc,crc)==legacy.length());CHECK(crc.value()==expected);
 crcDoc["checksum"]=expected;CHECK(inventorySaveJson("legacy",crcDoc.as<JsonVariantConst>()));InventoryJson decoded(8192);CHECK(r7Load("legacy",decoded));
 {PodResponse response;uint8_t bytes[4096]={};CHECK(response.write(bytes,512)==512);CHECK(response.size==512);failResponseAlloc=true;CHECK(response.write(bytes,4096)==0&&response.full);failResponseAlloc=false;}
 {PodResponse response;uint8_t b=0;CHECK(response.write(&b,192*1024+1)==0&&response.full);}
 seed();failResponseAlloc=true;InventoryJson unavailable(8192);CHECK(r7Get(firebaseUid,"TIGERDATA_spool",unavailable)!=200);CHECK(posts==0);failResponseAlloc=false;
 // Documents left by a spool deleted in Studio: tombstones and dangling links must not block a blank/associate.
 {
  auto tomb=[&](const char* id){InventoryJson t(8192);t["updateTime"]="t1";t["fields"]["deleted"]["booleanValue"]=true;t["fields"]["uid"]["stringValue"]=id;saveServer(id,t);};
  auto dangling=[&](const char* uid,const char* spool){InventoryJson a(8192);a["updateTime"]="a1";a["fields"]["spoolId"]["stringValue"]=spool;a["fields"]["chip"]["stringValue"]="mifare1k";saveServer((std::string("alias:")+uid).c_str(),a);};
  seed();tomb("04112233445566");tomb("04778899AABBCC");dangling("04112233445566","GONE_spool");
  stage(job,"reset");CHECK(job["docs"].size()==4);CHECK(job["docs"][0]["after"].isNull());CHECK(!job["docs"][0]["before"].isNull());
  commit(job);CHECK(!server.count("04112233445566")&&!server.count("04778899AABBCC")&&!server.count("alias:04112233445566"));CHECK(server.count("TIGERDATA_spool"));
  seed();tomb("04112233445566");tomb("04778899AABBCC");stage(job,"associate");commit(job);
  current.clear();deserializeJson(current,server["04112233445566"]);CHECK(inventoryText(current["fields"],"uid")=="04112233445566");CHECK(!(current["fields"]["deleted"]["booleanValue"]|false));
  seed();tomb("04112233445566");tomb("04778899AABBCC");stage(job,"format");CHECK(job["kind"]=="reset");commit(job);CHECK(!server.count("04112233445566")&&!server.count("04778899AABBCC"));
  // A link whose spool is a tombstone is stale too; a link to a live spool still protects the chip.
  seed();tomb("GONE_spool");dangling("04112233445566","GONE_spool");stage(job,"reset");commit(job);CHECK(!server.count("alias:04112233445566"));
  seed();dangling("04112233445566","TIGERDATA_spool");job.clear();job["owner"]="owner";job["kind"]="reset";job["uid0"]="04112233445566";job["uid1"]="04778899AABBCC";job["stamp"]=1234;
  CHECK(r7Save(R7_STAGE,job));{InventoryRequest rr;r7CloudRun(rr);CHECK(!rr.ok);}
  // A live spool on the UID is never treated as stale.
  seed();{InventoryJson live(8192);deserializeJson(live,server["TIGERDATA_spool"]);saveServer("04112233445566",live);}
  job.clear();job["owner"]="owner";job["kind"]="reset";job["uid0"]="04112233445566";job["uid1"]="04778899AABBCC";job["stamp"]=1234;
  CHECK(r7Save(R7_STAGE,job));{InventoryRequest rr;r7CloudRun(rr);CHECK(!rr.ok);}
 }
 printf("%d r7 cloud checks passed (actual planner, mocked FS/HTTP).\n",checks);
}
'''
def main():
 inc=os.environ.get('ARDUINOJSON_INCLUDE')
 candidates=list((ROOT/'.pio/libdeps').glob('*/ArduinoJson/src'))
 if not inc and candidates:inc=str(candidates[0])
 if not inc:raise SystemExit('Set ARDUINOJSON_INCLUDE to an existing ArduinoJson/src (no firmware build needed).')
 with tempfile.TemporaryDirectory(prefix='pod-cloud-') as td:
  p=Path(td);(p/'test.cpp').write_text(base+shim+functions+'\nuint32_t millis(){return 0;}void delay(int){}\nstruct {template<class...A>void printf(const char*,A...){}} Serial;\n#include "pod_cloud_ca.h"\n#include "pod_tls.h"\n#include "pod_cloud.h"\n'+tests)
  subprocess.run([os.environ.get('CXX') or shutil.which('g++'),'-std=c++11','-I'+inc,'-I'+str(ROOT/'TigerTagSplashESP32'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
if __name__=='__main__':main()
