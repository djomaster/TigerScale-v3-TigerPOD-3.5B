#pragma once
#include "pod_policy.h"
#include "pod_crc.h"
#include "pod_http_memory.h"
#include "pod_cloud_ca.h"
// One immutable NFC/cloud plan, retained through an uncertain commit response.
static const char* R7_JOB = "/pod7-job.json";
static const char* R7_STAGE = "/pod7-stage.json";
static const char* R7_BACKUP = "/pod7-backup.json";
static const char* R7_COMMIT = "/pod7-commit.json";
static volatile bool r7Active = false;
static bool r7Blocked = true;
static volatile bool r7WorkerPaused = false;
static String r7CloudError;
static bool r7Load(const char* path, InventoryJson& d) {
    File f=LittleFS.open(path,"r");
    if(!f || f.size()>192*1024 || deserializeJson(d,f,DeserializationOption::NestingLimit(96)))return false;
    uint32_t expected=d["checksum"] | 0U;d.remove("checksum");
    PodCrcWriter bytes;size_t written=serializeJson(d,bytes);
    bool ok=!d.overflowed() && written==measureJson(d) && expected==bytes.value();
    d["checksum"]=expected;return ok;
}
static bool r7Save(const char* path, InventoryJson& d) {
    if(d.overflowed())return false;
    d.remove("checksum");PodCrcWriter bytes;
    if(serializeJson(d,bytes)!=measureJson(d))return false;
    d["checksum"]=bytes.value();
    if(d.overflowed())return false;
    if(!inventorySaveJson(path,d.as<JsonVariantConst>()))return false;
    InventoryJson check(256*1024);
    return r7Load(path,check) && check.as<JsonVariantConst>()==d.as<JsonVariantConst>();
}
static String r7Hex(const uint8_t* p,size_t n) {
    String s;s.reserve(n*2);for(size_t i=0;i<n;i++){char h[3];snprintf(h,3,"%02X",p[i]);s+=h;}return s;
}
static bool r7Unhex(const String& s,uint8_t* p,size_t n) {
    if(s.length()!=2*n)return false;
    for(size_t i=0;i<n;i++){
        int a=s[2*i],b=s[2*i+1];
        auto hex=[](int c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:-1;};
        a=hex(a);b=hex(b);if(a<0||b<0)return false;p[i]=(a<<4)|b;
    }return true;
}
static bool r7Id(const String& s) {
    if(s.isEmpty()||s.length()>128)return false;
    for(size_t i=0;i<s.length();i++)if(!isalnum((unsigned char)s[i])&&s[i]!='_'&&s[i]!='-')return false;
    return true;
}
static int r7Get(const String& owner,const String& id,InventoryJson& d,const char* collection="inventory") {
    if(strcmp(collection,"inventory") && strcmp(collection,"rfidAliases"))return -2;
    if(owner!=firebaseUid||!r7Id(id))return -1;
    R86TlsLease lease;HTTPClient& h=lease.http();h.setTimeout(15000);if(!h.begin(lease.client(),String(INVENTORY_ROOT)+"/users/"+owner+"/"+collection+"/"+inventoryUrlEncode(id)))return -1;
    h.addHeader("Authorization","Bearer "+firebaseIdToken);int code=h.GET();
    if(code==200 && !r86HttpJson(h,d))code=-2;
    h.end();if(code<0)lease.client().stop();return code;
}
static bool r7Capture(InventoryJson& job,const String& id,bool mustExist,const char* collection="inventory") {
    InventoryJson d(128*1024);int code=r7Get(job["owner"].as<String>(),id,d,collection);
    if(code!=(mustExist?200:404)){r7CloudError=code==200?"UID already associated":"Inventory unavailable / changed";return false;}
    JsonObject item=job["docs"].as<JsonArray>().createNestedObject();item["id"]=id;item["collection"]=collection;
    if(mustExist){
        if(d["updateTime"].as<String>().isEmpty() || d["fields"].isNull())return false;
        item["before"].set(d.as<JsonVariantConst>());
    }
    return !job.overflowed();
}
// Pairs created before tag_index/tag_count existed carry neither field but are still linked both ways with one
// timestamp. Re-encoding such a pair is exactly what writes the missing fields, so both-absent is accepted;
// a half-filled or contradictory pair is not.
static bool r7PairIndexes(JsonObjectConst a,JsonObjectConst b) {
    double ia=inventoryNumber(a,"tag_index"),ib=inventoryNumber(b,"tag_index");
    double ca=inventoryNumber(a,"tag_count"),cb=inventoryNumber(b,"tag_count");
    bool modern=ia==1&&ib==2&&ca==2&&cb==2;
    bool legacy=ia==0&&ib==0&&ca==0&&cb==0;
    return modern||legacy;
}
static bool r7PairFields(JsonObjectConst a,JsonObjectConst b,const String* uid) {
    return inventoryText(a,"uid")==uid[0] && inventoryText(b,"uid")==uid[1]
        && inventoryText(a,"twin_tag_uid")==uid[1] && inventoryText(b,"twin_tag_uid")==uid[0]
        && r7PairIndexes(a,b)
        && inventoryNumber(a,"timestamp")==inventoryNumber(b,"timestamp")
        && !(a["deleted"]["booleanValue"]|false) && !(b["deleted"]["booleanValue"]|false);
}
static void r7Identity(JsonObject f,const String* uid,int slot,uint32_t stamp,const uint8_t* data) {
    const char* remove[]={"uid","twin_tag_uid","tag_index","tag_count","timestamp","id_tigertag","needUpdateAt","deleted","deleted_at"};
    for(auto k:remove)f.remove(k);
    f["uid"]["stringValue"]=uid[slot];f["twin_tag_uid"]["stringValue"]=uid[1-slot];
    f["tag_index"]["integerValue"]=String(slot+1);f["tag_count"]["integerValue"]="2";
    f["timestamp"]["integerValue"]=String(stamp);
    uint32_t marker=(uint32_t)data[0]<<24|(uint32_t)data[1]<<16|(uint32_t)data[2]<<8|data[3];
    f["id_tigertag"]["integerValue"]=String(marker);
}
static void r7Touch(JsonObject fields) {
    time_t now=time(nullptr);struct tm utc;gmtime_r(&now,&utc);char iso[25];
    strftime(iso,sizeof(iso),"%Y-%m-%dT%H:%M:%SZ",&utc);
    fields.remove("last_update");fields.remove("updatedAt");
    fields["last_update"]["integerValue"]=String((uint64_t)now*1000);
    fields["updatedAt"]["timestampValue"]=iso;
}
// Compare the actual server to a saved side of the plan. No optimistic overwrite.
static bool r7ServerMatches(InventoryJson& job,bool after) {
    for(JsonObjectConst item:job["docs"].as<JsonArrayConst>()) {
        InventoryJson d(128*1024);int code=r7Get(job["owner"].as<String>(),item["id"].as<String>(),d,item["collection"]|"inventory");
        JsonVariantConst expected=after?item["after"]:item["before"]["fields"];
        if(expected.isNull()){if(code!=404)return false;}
        else if(code!=200 || (after? d["fields"].as<JsonVariantConst>()!=expected : d["updateTime"].as<String>()!=item["before"]["updateTime"].as<String>()))return false;
    }return true;
}
static bool r7Commit(InventoryJson& job) {
    // An acknowledged state is durable before returning to normal weighing.
    if(r7ServerMatches(job,true)){job["state"]="done";return r7Save(R7_JOB,job);}
    if(!r7ServerMatches(job,false)){r7CloudError="Inventory conflict: keep tags and journal";return false;}
    InventoryJson batch(512*1024);JsonArray writes=batch.createNestedArray("writes");
    String prefix="projects/tigertag-connect/databases/(default)/documents/users/"+job["owner"].as<String>()+"/";
    for(JsonObjectConst item:job["docs"].as<JsonArrayConst>()) {
        JsonObject w=writes.createNestedObject();
        if(item["after"].isNull())w["delete"]=prefix+String(item["collection"]|"inventory")+"/"+item["id"].as<String>();
        else {w["update"]["name"]=prefix+String(item["collection"]|"inventory")+"/"+item["id"].as<String>();w["update"]["fields"].set(item["after"]);}
        if(item["before"].isNull())w["currentDocument"]["exists"]=false;
        else w["currentDocument"]["updateTime"]=item["before"]["updateTime"];
    }
    if(batch.overflowed()||!inventorySaveJson(R7_COMMIT,batch.as<JsonVariantConst>()))return false;
    File f=LittleFS.open(R7_COMMIT,"r");if(!f)return false;
    R86TlsLease lease;HTTPClient& h=lease.http();h.setTimeout(15000);
    if(!h.begin(lease.client(),String(INVENTORY_ROOT)+":commit"))return false;
    h.addHeader("Authorization","Bearer "+firebaseIdToken);h.addHeader("Content-Type","application/json");
    int code=h.sendRequest("POST",&f,f.size());h.end();if(code<0)lease.client().stop();f.close();
    if(code==200 || r7ServerMatches(job,true)){job["state"]="done";return r7Save(R7_JOB,job);}
    r7CloudError="Tags verified; cloud sync pending (Resume)";return false;
}
// Firestore weight writes use Timestamp; older records use epoch milliseconds.
static double r8WeightTime(JsonObjectConst fields) {
    double numeric=inventoryNumber(fields,"last_update",-1);
    if(numeric>=0)return numeric;
    const char* iso=fields["last_update"]["timestampValue"] | "";
    if(!*iso)iso=fields["updatedAt"]["timestampValue"] | "";
    int y,m,d,h,min;double sec;
    if(sscanf(iso,"%d-%d-%dT%d:%d:%lf",&y,&m,&d,&h,&min,&sec)!=6
       || m<1 || m>12 || d<1 || d>31 || !strchr(iso,'Z'))return 0;
    // Gregorian civil date -> UTC days, independent of the device timezone.
    y-=m<=2;int era=(y>=0?y:y-399)/400;unsigned year=y-era*400;
    unsigned day=(153*(m+(m>2?-3:9))+2)/5+d-1;
    int days=era*146097+(int)(year*365+year/4-year/100+day)-719468;
    return ((double)days*86400+h*3600+min*60+sec)*1000;
}
// Restore the last completed transaction only. Metadata edits conflict; the
// newest weight is carried forward, never replaced by the backup's old value.
static bool r7RestorePlan(InventoryJson& job) {
    InventoryJson old(512*1024),latest(128*1024);
    if(!r7Load(R7_JOB,old) || old["kind"]=="restore"
        || old["owner"].as<String>()!=firebaseUid
        || old["uid0"]!=job["uid0"] || old["uid1"]!=job["uid1"]){
        r7CloudError="No completed backup for these UIDs/account";return false;
    }
    bool rollback=old["state"]!="done" && r7ServerMatches(old,false);
    if(old["state"]!="done" && !rollback && !r7ServerMatches(old,true)){
        r7CloudError="Pending operation conflicts with inventory; no restore";return false;
    }
    job["rollback"]=rollback;
    job.remove("docs");job.createNestedArray("docs");
    double latestTime=-1;
    for(JsonObjectConst item:old["docs"].as<JsonArrayConst>()) {
        String id=item["id"].as<String>();InventoryJson current(128*1024);
        int code=r7Get(firebaseUid,id,current,item["collection"]|"inventory");
        JsonVariantConst expected=rollback?item["before"]["fields"]:item["after"];
        if(expected.isNull()) {if(code!=404){r7CloudError="Restore conflict: document recreated";return false;}}
        else {
            if(code!=200){r7CloudError="Restore: current inventory unavailable";return false;}
            InventoryJson a(128*1024),b(128*1024);a.set(current["fields"]);b.set(expected);
            // syncAt is stamped by Studio/Firestore after a write; it says nothing about the spool data.
            for(auto key:{"weight_available","last_update","updatedAt","syncAt"}){a.remove(key);b.remove(key);}
            if(a.as<JsonVariantConst>()!=b.as<JsonVariantConst>()){
                r7CloudError="Restore refused: metadata edited in Studio; use Reencode";return false;
            }
            double stamp=r8WeightTime(current["fields"]);
            if(String(item["collection"]|"inventory")=="inventory" && stamp>latestTime){latestTime=stamp;latest.set(current["fields"]);}
        }
        JsonObject reverse=job["docs"].as<JsonArray>().createNestedObject();reverse["id"]=id;reverse["collection"]=String(item["collection"]|"inventory");
        if(code==200)reverse["before"].set(current.as<JsonVariantConst>());
        if(!item["before"].isNull())reverse["after"].set(item["before"]["fields"]);
    }
    for(JsonObject item:job["docs"].as<JsonArray>())if(!item["after"].isNull() && String(item["collection"]|"inventory")=="inventory") {
        for(auto key:{"weight_available","last_update","updatedAt"}) {
            if(latest.containsKey(key))item["after"][key].set(latest[key]);
            else item["after"].remove(key);
        }
    }
    if(!rollback)for(JsonObject item:job["docs"].as<JsonArray>())if(!item["after"].isNull() && String(item["collection"]|"inventory")=="inventory")r7Touch(item["after"].as<JsonObject>());
    job["restoreJournal"]=old["journal"];job["state"]="staged";
    return r7Save(R7_STAGE,job);
}
// Completed verification tolerates normal weight updates; transaction guards stay exact.
static bool r8VerifyLive(InventoryJson& job) {
    if(job["state"]!="done")return r7ServerMatches(job,true);
    for(JsonObjectConst item:job["docs"].as<JsonArrayConst>()) {
        if(item["checkOnly"]|false)continue;
        InventoryJson current(128*1024);int code=r7Get(firebaseUid,item["id"].as<String>(),current,item["collection"]|"inventory");
        if(item["after"].isNull()){if(code==404)continue;r7CloudError="Document recree / lecture HTTP "+String(code);return false;}
        if(code!=200){r7CloudError="Lecture Firebase HTTP "+String(code);return false;}
        InventoryJson expected(128*1024),actual(128*1024);expected.set(item["after"]);actual.set(current["fields"]);
        // syncAt: stamped by the server right after our own write; not spool data.
        for(auto key:{"weight_available","last_update","updatedAt","syncAt"}){expected.remove(key);actual.remove(key);}
        if(expected.as<JsonVariantConst>()!=actual.as<JsonVariantConst>()){
            // Name the field(s) that differ so a false "changed in Studio" can be traced from the serial log.
            String diff;
            for(JsonPairConst kv:expected.as<JsonObjectConst>())
                if(actual[kv.key()]!=kv.value()){diff+=kv.key().c_str();diff+=' ';}
            for(JsonPairConst kv:actual.as<JsonObjectConst>())
                if(expected[kv.key()].isNull()){diff+="+";diff+=kv.key().c_str();diff+=' ';}
            Serial.printf("[POD VERIFY] differs id=%s fields=%s\n",item["id"].as<String>().c_str(),diff.c_str());
            r7CloudError="Tags lus. Les donnees de la bobine ont change dans Studio.";return false;
        }
    }return true;
}
// Move Studio's existing linked-chip references in the SAME conditional batch
// whenever encoding/formatting replaces the inventory document IDs.
static bool r8MoveAliases(InventoryJson& job,const String* oldIds,int count,const String& target) {
    String token;unsigned moved=0;
    do {
        InventoryJson page(192*1024);String url=String(INVENTORY_ROOT)+"/users/"+firebaseUid+"/rfidAliases?pageSize=100";
        if(token.length())url+="&pageToken="+inventoryUrlEncode(token);
        int code=inventoryGet(url,page);if(code!=200){r7CloudError="Lecture liens partages HTTP "+String(code);return false;}
        for(JsonObjectConst entry:page["documents"].as<JsonArrayConst>()) {
            String old=inventoryText(entry["fields"],"spoolId");bool match=false;
            for(int i=0;i<count;i++)if(old==oldIds[i])match=true;
            if(!match)continue;
            if(++moved>100){r7CloudError="Trop de liens pour une operation. Aucun tag ecrit.";return false;}
            String path=entry["name"].as<String>(),id=path.substring(path.lastIndexOf('/')+1);
            if(!r7Id(id)||entry["updateTime"].as<String>().isEmpty())return false;
            JsonObject item=job["docs"].as<JsonArray>().createNestedObject();item["id"]=id;item["collection"]="rfidAliases";
            item["before"].set(entry);item["after"].set(entry["fields"]);item["after"]["spoolId"]["stringValue"]=target;
        }
        token=page["nextPageToken"]|"";
    }while(token.length());
    return !job.overflowed();
}
// Executed only by the existing cloud worker; no NFC or UI work on this core.
static void r7CloudRun(InventoryRequest& r) {
    r.ok=false;r7CloudError="Cloud operation failed; no automatic retry";
    InventoryJson job(512*1024);
    if(!r7Load(r.podAction==0 || r.podAction==3 ? R7_STAGE : R7_JOB,job)||job["owner"].as<String>()!=firebaseUid){r7CloudError="Journal/account mismatch";return;}
    if(r.podAction==2){
        if(job["state"]!="written" && job["state"]!="done"){r7CloudError="NFC verification required";return;}
        r.ok=r7Commit(job);return;
    }
    if(r.podAction==4){r.ok=r8VerifyLive(job);return;}
    if(r.podAction==1){r.ok=r7ServerMatches(job,false)||r7ServerMatches(job,true);if(!r.ok)r7CloudError="Inventory changed; no NFC write";return;}
    // Stage a new job. All changes remain local until the final NFC verification.
    if(r.podAction==3){r.ok=r7RestorePlan(job);return;}
    String kind=job["kind"].as<String>();String uid[2]={job["uid0"].as<String>(),job["uid1"].as<String>()};
    job.remove("docs");job.createNestedArray("docs");uint8_t payload[144];
    uint32_t stamp=job["stamp"];
    if(kind=="reset") {
        for(int i=0;i<2;i++)if(!r7Capture(job,uid[i],false))return;
        memset(payload,0,144);payload[0]=0x6c;payload[1]=0x41;payload[2]=0xa2;payload[3]=0xe1;
    } else if(kind=="associate") {
        String source=job["source"].as<String>();
        if(!r7Capture(job,source,true)||!r7Capture(job,uid[0],false)||!r7Capture(job,uid[1],false))return;
        JsonObjectConst f=job["docs"][0]["before"]["fields"];
        if(!inventoryChipless(source,f)||job["version"].as<String>()!=job["docs"][0]["before"]["updateTime"].as<String>()
            ||!inventoryEncode(f,stamp,payload)){r7CloudError="Selected spool changed / invalid";return;}
        for(int i=0;i<2;i++) {
            JsonObject after=job["docs"][i+1].createNestedObject("after");after.set(f);r7Identity(after,uid,i,stamp,payload);
        }
    } else if(kind=="format"||kind=="reencode") {
        if(!r7Capture(job,uid[0],true)||!r7Capture(job,uid[1],true))return;
        JsonObjectConst a=job["docs"][0]["before"]["fields"],b=job["docs"][1]["before"]["fields"];
        uint8_t other[144];
        // Each failure names its own cause: one message for three different problems cannot be acted on.
        if(!r7PairFields(a,b,uid)){
            Serial.printf("[POD PAIR] fields: uid=%d/%d twin=%d/%d idx=%d/%d cnt=%d/%d ts=%d/%d\n",
                (int)(inventoryText(a,"uid")==uid[0]),(int)(inventoryText(b,"uid")==uid[1]),
                (int)(inventoryText(a,"twin_tag_uid")==uid[1]),(int)(inventoryText(b,"twin_tag_uid")==uid[0]),
                (int)inventoryNumber(a,"tag_index"),(int)inventoryNumber(b,"tag_index"),
                (int)inventoryNumber(a,"tag_count"),(int)inventoryNumber(b,"tag_count"),
                (int)inventoryNumber(a,"timestamp"),(int)inventoryNumber(b,"timestamp"));
            r7CloudError="Not a coherent Twin pair in this account (links, index or timestamp differ)";return;
        }
        if(!inventoryEncode(a,stamp,payload)||!inventoryEncode(b,stamp,other)){r7CloudError="Spool data cannot be encoded for the tags";return;}
        if(memcmp(payload,other,76)){r7CloudError="Not a coherent Twin pair in this account (the two spool documents differ)";return;}
        if(r8WeightTime(b)>r8WeightTime(a))memcpy(payload+76,other+76,4);
        if(kind=="reencode")for(int i=0;i<2;i++) {
            JsonObject after=job["docs"][i].createNestedObject("after");after.set(job["docs"][i]["before"]["fields"]);
            r7Identity(after,uid,i,stamp,payload);
            JsonObjectConst latest=r8WeightTime(b)>r8WeightTime(a)?b:a;
            after.remove("weight_available");if(latest.containsKey("weight_available"))after["weight_available"].set(latest["weight_available"]);
        }
        else {
            String source=job["source"].as<String>();if(!r7Capture(job,source,false))return;
            JsonObject after=job["docs"][2].createNestedObject("after");after.set(a);
            // Refuse divergent metadata rather than discard one twin's information.
            InventoryJson ca(128*1024),cb(128*1024);ca.set(a);cb.set(b);
            for(auto key:{"uid","twin_tag_uid","tag_index","last_update","updatedAt","weight_available","rfidBackup","rfidListed"}){ca.remove(key);cb.remove(key);}
            if(ca.as<JsonVariantConst>()!=cb.as<JsonVariantConst>()){r7CloudError="Twin metadata differs; resolve in Studio first";return;}
            if(r8WeightTime(b)>r8WeightTime(a))after.set(b);
            for(auto key:{"uid","twin_tag_uid","tag_index","tag_count","timestamp","id_tigertag","rfidBackup","rfidListed","needUpdateAt"})after.remove(key);
            after["uid"]["stringValue"]=source;memset(payload,0,144);
            payload[0]=0x6c;payload[1]=0x41;payload[2]=0xa2;payload[3]=0xe1;
        }
    } else {r7CloudError="Unsupported job";return;}
    if(kind=="associate" || kind=="reset") {
        for(int i=0;i<2;i++) {
            if(!r7Capture(job,uid[i],false,"rfidAliases"))return;
            job["docs"][job["docs"].size()-1]["checkOnly"]=true;
        }
    }
    if(kind=="associate") {String source=job["source"].as<String>();if(!r8MoveAliases(job,&source,1,uid[0]))return;}
    if(kind=="format" && !r8MoveAliases(job,uid,2,job["source"].as<String>()))return;
    for(JsonObject item:job["docs"].as<JsonArray>())if(!item["after"].isNull() && String(item["collection"]|"inventory")=="inventory")r7Touch(item["after"].as<JsonObject>());
    job["payload"]=r7Hex(payload,144);job["state"]="staged";
    if(!r7Save(R7_STAGE,job)){r7CloudError="Journal storage failure";return;}r.ok=true;
}
