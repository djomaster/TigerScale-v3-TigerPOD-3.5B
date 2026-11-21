#pragma once
#include "pod_ui.h"
static bool r8AcceptOccupied=false;
// On-device coordinator. The cloud worker never touches NFC or LVGL.
static void r7InitState() {
    static bool initialized=false;if(initialized)return;initialized=true;
    InventoryJson job(512*1024);
    r7Blocked=LittleFS.exists(R7_JOB) && (!r7Load(R7_JOB,job)||job["state"]!="done");
    if(r7Blocked)Serial.println("[POD r7] Pending/corrupt journal: open TigerPOD / Resume. Weight sync blocked.");
}
static bool r7Cloud(uint8_t action) {
    InventoryRequest r;r.op=InventoryRequest::POD_JOB;r.podAction=action;
    r7CloudError="Wi-Fi/account unavailable";
    r86Memory(action==2?"cloud-commit":"cloud-read");
    podSay(action==2?"Synchronisation de l'inventaire":"Lecture et controle de l'inventaire");
    bool ok=inventoryWait(r,action==2?I18N_INV_SYNCING:I18N_INV_LOADING,false);
    r86Memory("cloud-return");
    if(!ok)podSay(r7CloudError);return ok;
}
static bool r7Journal(InventoryJson& job,PodJournal& j,const char* key="journal") {
    if(!r7Unhex(job[key].as<String>(),(uint8_t*)&j,sizeof(j)) || j.magic!=0x504f4431
       || j.crc!=PodPolicy::checksum(&j,offsetof(PodJournal,crc))
       || !PodPolicy::capacity(j.size[0]) || !PodPolicy::capacity(j.size[1])
       || j.uid[0][0]!=4 || j.uid[1][0]!=4 || !memcmp(j.uid[0],j.uid[1],7)
       || podUid(j.uid[0])!=job["uid0"].as<String>() || podUid(j.uid[1])!=job["uid1"].as<String>()) {
        podSay("Journal invalide. Aucun tag ecrit. Conserver les fichiers.");return false;
    }return true;
}
static void r7Sample() { blankFlowTick(); }
static bool r7CaptureTags(PodJournal& next,bool blank) {
    podReadersReversed=false;
    next={};next.magic=0x504f4431;uint8_t mem[888];
    for(int slot=0;slot<2;slot++) {
        podAt(slot,-1,"capture");
        if(!podSelect(slot)||!podInspect(slot,mem,next.size[slot]))return false;
        memcpy(next.uid[slot],podReader(slot).uid.uidByte,7);
        memcpy(next.before[slot],mem,144);
        memcpy(next.target[slot],mem,144);
        if(blank) {if(!podBlank(mem,PodPolicy::capacity(next.size[slot])))return false;}
        else for(int i=144;i<PodPolicy::capacity(next.size[slot]);i++)if(mem[i])return podFail("Extended data: operation refused");
    }
    if(!memcmp(next.uid[0],next.uid[1],7))return podFail("Same UID on both readers");
    return podOff();
}
// Reorder the captured record, never the physical tag contents or cloud indices.
static void r86SwapCapture(PodJournal& j) {
    for(int i=0;i<7;i++)std::swap(j.uid[0][i],j.uid[1][i]);
    std::swap(j.size[0],j.size[1]);
    for(int i=0;i<144;i++) {
        std::swap(j.before[0][i],j.before[1][i]);
        std::swap(j.target[0][i],j.target[1][i]);
    }
}
// Bind once before an operation. A movement AFTER binding still fails UID checks.
static bool r86BindJournal(const PodJournal& j) {
    podReadersReversed=false;uint8_t seen[2][7];
    for(int slot=0;slot<2;slot++) {
        podAt(slot,-1,"orientation");
        if(!podSelect(slot))return false;
        memcpy(seen[slot],podReader(slot).uid.uidByte,7);
    }
    if(!podOff())return false;
    bool direct=!memcmp(seen[0],j.uid[0],7)&&!memcmp(seen[1],j.uid[1],7);
    bool reverse=!memcmp(seen[0],j.uid[1],7)&&!memcmp(seen[1],j.uid[0],7);
    if(!memcmp(j.uid[0],j.uid[1],7)||(!direct&&!reverse))return podFail("Les UID ne correspondent pas au journal. Aucun tag ecrit.");
    podReadersReversed=reverse;
    Serial.println(reverse?"[POD] Tag 1/2 sur B, tag 2/2 sur A":"[POD] Tag 1/2 sur A, tag 2/2 sur B");
    return true;
}
static bool r7ProductionPair(const PodJournal& j,InventoryJson& job) {
    for(int slot=0;slot<2;slot++) {
        const uint8_t* p=j.before[slot];
        const uint8_t standard[]={0x5b,0xf5,0x92,0x64},plus[]={0xbc,0x0f,0xcb,0x97};
        if((memcmp(p,standard,4)&&memcmp(p,plus,4)) || p[39]!=(slot?0x22:0x12)
           || !memcmp(p+48,"TIGERPOD TEST",12))return podFail("Deux tags de production requis: indices 1/2 et 2/2");
        uint32_t stamp=(uint32_t)p[32]<<24|(uint32_t)p[33]<<16|(uint32_t)p[34]<<8|p[35];
        if(stamp!=(uint32_t)inventoryNumber(job["docs"][slot]["before"]["fields"],"timestamp"))
            return podFail("Physical pair differs from inventory");
    }
    return true;
}
static bool r7Resume(bool verifyOnly=false) {
    InventoryJson job(512*1024);PodJournal next;
    if(!r7Load(R7_JOB,job)||job["owner"].as<String>()!=firebaseUid||!r7Journal(job,next)) {
        podSay("Aucun journal valide pour ce compte. Aucun tag ecrit.");return false;
    }
    podJournal=next;
    if(!r86BindJournal(next))return false;
    if(verifyOnly) {
        bool ok=podCheckPair(true);if(ok)ok=r7Cloud(4);
        podSay(ok?"Deux tags et inventaire verifies.":"Verification incomplete. Garder les tags et le journal.");return ok;
    }
    if(job["state"]=="done"){podSay("Operation deja terminee. Utiliser Verifier ou une nouvelle action.");return true;}
    if(!r7Cloud(1))return false;
    if(!podSave()){podSay("Sauvegarde NVS impossible. Aucun tag ecrit.");return false;}
    podOperationLabel=job["kind"].as<String>()+" : "+job["label"].as<String>();
    podSay("Ecriture puis verification des deux tags");r86Memory("nfc-write");
    if(!podWrite(false))return false;
    r86Memory("nfc-verified");
    Serial.printf("[POD PERF] nfc_ms=%lu written=%u skipped=%u uid_checks=%u\n",
        (unsigned long)(millis()-podRunStartedMs),podWrittenPages,podSkippedPages,podUidChecks);
    // Store proof of this readback before submitting the conditional batch.
    job["state"]="written";
    if(!r7Save(R7_JOB,job)){podSay("Tags verifies, journal non sauvegarde. Reprendre.");return false;}
    if(!r7Cloud(2))return false;
    r7Blocked=false;
    podSay("Deux tags verifies. Inventaire synchronise.\nOperation terminee.");return true;
}
static bool r7New(const char* kind) {
    bool associate=!strcmp(kind,"associate"),restore=!strcmp(kind,"restore");
    InventoryJson old(512*1024);
    bool hasJob=LittleFS.exists(R7_JOB);
    if(hasJob && (!r7Load(R7_JOB,old)||(!restore && old["state"]!="done"))) {
        podSay("Operation en attente. Reprendre ou Restaurer avant toute nouvelle action.");return false;
    }
    if(firebaseUid.isEmpty()||!firebaseAuth){podSay("Connecter le compte TigerTag et le Wi-Fi.");return false;}
    podSay("Controle complet des deux tags");r86Memory("capture");
    PodJournal next;
    if(!r7CaptureTags(next,associate && !r8AcceptOccupied))return false;
    bool rebuild=false,formatFallback=false;
    if(!strcmp(kind,"reencode") || !strcmp(kind,"format")) {
        // Only propose the canonical order. Cloud reciprocity, metadata, timestamp
        // and physical production-tag validation below must still all succeed.
        if(next.before[0][39]==0x22 && next.before[1][39]==0x12)r86SwapCapture(next);
        if(next.before[0][39]!=0x12 || next.before[1][39]!=0x22) {
            if(!strcmp(kind,"format")) {
                // Not a 1/2 + 2/2 production pair: "format" becomes a full blank erase of both
                // tags. The cloud step only accepts UIDs absent from the inventory, so a tag that
                // belongs to a spool is refused there and nothing is written.
                kind="reset";formatFallback=true;
            } else if(!strcmp(kind,"reencode") && ((PodPolicy::blank(next.before[0],144) && PodPolicy::blank(next.before[1],144))
                      || (!next.before[0][39] && !next.before[1][39]))) {
                // Both tags are blank or carry no index (byte 39 = 0), but the inventory may still describe them:
                // rewrite them FROM the inventory. The cloud step still requires a coherent
                // Twin pair of this account (reciprocal UIDs, indices 1/2 and 2/2).
                rebuild=true;
            } else {
                // Byte 39 = (index<<4)|count: 0x12 = tag 1 of 2, 0x22 = tag 2 of 2. Show what was read.
                static char why[200];
                snprintf(why,sizeof(why),"Paire invalide: tag 1/2 et tag 2/2 requis (lu A=%02X B=%02X, attendu 12 et 22; debut A=%02X%02X%02X%02X B=%02X%02X%02X%02X).",
                         next.before[0][39],next.before[1][39],next.before[0][0],next.before[0][1],next.before[0][2],next.before[0][3],next.before[1][0],next.before[1][1],next.before[1][2],next.before[1][3]);
                return podFail(why);
            }
        }
    } else if(restore && hasJob && podUid(next.uid[0])==old["uid1"].as<String>()
              && podUid(next.uid[1])==old["uid0"].as<String>()) {
        r86SwapCapture(next);
    }
    time_t now=time(nullptr);
    if(now<1704067200){podSay("Horloge invalide. Connecter le Wi-Fi puis reessayer.");return false;}
    next.pair=(uint32_t)(now-946684800);
    if(hasJob && next.pair<=(uint32_t)(old["stamp"]|0U))next.pair=(old["stamp"]|0U)+1;
    InventoryJson job(512*1024);
    job["owner"]=firebaseUid;job["kind"]=kind;job["stamp"]=next.pair;
    job["uid0"]=podUid(next.uid[0]);job["uid1"]=podUid(next.uid[1]);
    job["label"]="Twin "+podUid(next.uid[0]);
    if(associate) {
        InventoryRequest list;list.op=InventoryRequest::LIST;
        if(!inventoryWait(list,I18N_INV_LOADING,true))return false;
        if(list.spools.empty()){podSay("Aucune bobine sans tag. Creer la bobine dans Studio.");return false;}
        int choice=inventoryPickSpool(list.spools);if(choice<0)return false;
        job["source"]=list.spools[choice].id;job["version"]=list.spools[choice].version;job["label"]=list.spools[choice].label;
    } else if(!strcmp(kind,"format")) {
        job["source"]="TIGERDATA_"+String(next.pair)+"_"+podUid(next.uid[0]);
    }
    if(!r7Save(R7_STAGE,job)){podSay("Sauvegarde locale impossible. Aucun tag ecrit.");return false;}
    bool staged=r7Cloud(restore?3:0);
    if(!staged && rebuild) {
        // Blank tags carry no index: try the other order once; the inventory decides which is 1/2.
        r86SwapCapture(next);
        job["uid0"]=podUid(next.uid[0]);job["uid1"]=podUid(next.uid[1]);job["label"]="Twin "+podUid(next.uid[0]);
        if(!r7Save(R7_STAGE,job)){podSay("Sauvegarde locale impossible. Aucun tag ecrit.");return false;}
        staged=r7Cloud(0);
    }
    if(!staged) {
        if(formatFallback && r7CloudError=="UID already associated")
            r7CloudError="Ces UID existent deja dans l'inventaire. Utiliser Actualiser les tags pour les reecrire depuis l'inventaire.";
        else if(rebuild)
            r7CloudError="Tags vierges, mais aucune paire 1/2 + 2/2 coherente dans l'inventaire pour ces UID ("+r7CloudError+").";
        return false;
    }
    if(!r7Load(R7_STAGE,job)){podSay("Preparation illisible. Aucun tag ecrit.");return false;}
    if(!strcmp(kind,"format") && job["kind"]=="reset")kind="reset";
    if(restore) {
        PodJournal previous;
        if(!r7Journal(job,previous,"restoreJournal"))return false;
        for(int i=0;i<2;i++) {
            if(next.size[i]!=previous.size[i]||!PodPolicy::recoverable(next.before[i],previous.before[i],previous.target[i]))return podFail("Backup target differs: use same unchanged tags");
            memcpy(next.target[i],previous.before[i],144);
            // Restoring metadata must agree with the previous physical record.
            if(old["kind"]!="associate" && old["kind"]!="reset" && !PodPolicy::blank(next.target[i],144)) {
                bool found=false;
                for(JsonObjectConst doc:job["docs"].as<JsonArrayConst>())if(doc["id"].as<String>()==podUid(next.uid[i])&&!doc["after"].isNull()) {
                    uint8_t encoded[144];const uint8_t* p=next.target[i];
                    uint32_t stamp=(uint32_t)p[32]<<24|(uint32_t)p[33]<<16|(uint32_t)p[34]<<8|p[35];
                    if(!inventoryEncode(doc["after"],stamp,encoded))return podFail("Backup metadata cannot be encoded");
                    encoded[39]=i?0x22:0x12;
                    if(memcmp(encoded,p,76))return podFail("Backup tag differs from saved inventory metadata");
                    found=true;
                }
                if(!found)return podFail("Backup inventory document missing");
            }
        }
    } else {
        uint8_t payload[144];if(!r7Unhex(job["payload"].as<String>(),payload,144))return false;
        if(!associate && !rebuild && strcmp(kind,"reset") && !r7ProductionPair(next,job))return false;
        for(int i=0;i<2;i++) {
            memcpy(next.target[i],payload,(associate || !strcmp(kind,"format") || !strcmp(kind,"reset"))?144:80);
            if(strcmp(kind,"format") && strcmp(kind,"reset"))next.target[i][39]=i?0x22:0x12;
        }
    }
    next.crc=PodPolicy::checksum(&next,offsetof(PodJournal,crc));
    job["journal"]=r7Hex((const uint8_t*)&next,sizeof(next));job.remove("restoreJournal");job.remove("payload");
    job["state"]="pending";
    // Backup and authoritative plan are both persisted before any NFC write.
    if(hasJob&&!r7Save(R7_BACKUP,old)){podSay("Sauvegarde precedente impossible. Aucun tag ecrit.");return false;}
    if(!r7Save(R7_JOB,job)){r7Blocked=true;podSay("Journal non sauvegarde. Aucun tag ecrit. Garder les fichiers.");return false;}
    r7Blocked=true;old.clear();old.shrinkToFit();job.clear();job.shrinkToFit();
    r86Memory("plan-saved");return r7Resume();
}
// Read-only scan accepts 4/7/10-byte UIDs, including maker tags. NFC writes
// still pass the stricter NTAG/protection/identity checks in the journal engine.
static String r8UiUid[2],r8UiChip[2];
static bool r8UiCompatible=false,r8UiKnown=false,r8UiScanned=false;
static bool r8SelectRaw(int slot,const String& expected="") {
    if(!podOff())return false;delay(15);auto& reader=podReader(slot);
    if(!reader.configureRfField(true))return false;delay(15);
    if(!reader.isNewCardPresent()||!reader.readCardSerial())return false;
    String uid;for(int i=0;i<reader.uid.size && i<10;i++){char h[3];snprintf(h,3,"%02X",reader.uid.uidByte[i]);uid+=h;}
    return r8Uid(uid) && (expected.isEmpty() || uid==expected);
}
static bool r8UiResolved[2]={false,false};
static String r8UiLabel[2];
static bool r8UiAlias[2]={false,false};
static uint8_t r9Check[2][3]={{0,0,0},{0,0,0}};   // Verify content checks per tag: signature, index, date (0 not run, 1 ok, 2 differs)
static uint32_t r8UiStamp[2]={0,0};static int r8UiIndex[2]={0,0};
static bool r8Scan() {
    podReadersReversed=false;
    r8UiKnown=false;r8UiCompatible=true;r8UiScanned=false;r8UiResolved[0]=r8UiResolved[1]=false;r8UiLabel[0]=r8UiLabel[1]="";r8UiAlias[0]=r8UiAlias[1]=false;r8UiStamp[0]=r8UiStamp[1]=0;r8UiIndex[0]=r8UiIndex[1]=0;int present=0;
    InventoryRequest resolve;resolve.op=InventoryRequest::POD_RESOLVE;
    for(int i=0;i<2;i++) {
        r8UiUid[i]="";r8UiChip[i]="unknown";
        if(!r8SelectRaw(i)){r8UiCompatible=false;continue;}
        auto& reader=podReader(i);String uid;
        for(int b=0;b<reader.uid.size;b++){char h[3];snprintf(h,3,"%02X",reader.uid.uidByte[b]);uid+=h;}
        r8UiUid[i]=resolve.uids[i]=uid;present++;
        if(reader.uid.size==4 && reader.sak==8)r8UiChip[i]="mifare1k";
        // Preview only: full-capacity checks still run in r7CaptureTags before any write.
        uint8_t memory[16],size=0;
        if(reader.uid.size!=7 || reader.uid.uidByte[0]!=4 || !podInspect(i,memory,size,true,sizeof(memory)))r8UiCompatible=false;
        else {
            r8UiChip[i]=size==0xf?"NTAG213":size==0x11?"NTAG215":"NTAG216";

        }
    }
    podOff();podReadError="unknown";r8UiCompatible=r8UiCompatible&&present==2&&r8UiUid[0]!=r8UiUid[1];
    if(!present){r7CloudError="Aucun tag lisible. Placez la bobine puis scannez.";podSay(r7CloudError);return false;}
    // Reuse only the just-completed discovery for exactly these UIDs/account.
    // This is a menu hint, never permission to encode or commit an association.
    bool recent=!r8DiscoveryBusy && !r8DiscoveryPending && r8DiscoveryRequest.done
        && r8DiscoveryRequest.ok && r8DiscoveryRequest.owner==firebaseUid
        && r83LookupGeneration==r8Generation && millis()-r83LookupAt<3000;
    for(int i=0;i<2;i++)if(resolve.uids[i]!=r8DiscoveryRequest.uids[i])recent=false;
    if(recent)for(int i=0;i<2;i++){resolve.resolved[i]=r8DiscoveryRequest.resolved[i];resolve.resolvedLabel[i]=r8DiscoveryRequest.resolvedLabel[i];resolve.alias[i]=r8DiscoveryRequest.alias[i];resolve.docStamp[i]=r8DiscoveryRequest.docStamp[i];resolve.docIndex[i]=r8DiscoveryRequest.docIndex[i];}
    else if(!inventoryWait(resolve,I18N_INV_LOADING,false)){podSay(r7CloudError);return false;}
    for(int i=0;i<2;i++){r8UiResolved[i]=resolve.resolved[i].length()>0;r8UiLabel[i]=resolve.resolvedLabel[i];r8UiAlias[i]=resolve.alias[i];r8UiStamp[i]=resolve.docStamp[i];r8UiIndex[i]=resolve.docIndex[i];}
    r8UiKnown=r8UiResolved[0]||r8UiResolved[1];r8UiScanned=true;return true;
}
// Read-only result of the pair on the scale: what each reader holds and what the inventory knows.
static bool r9Reported=false;
static bool r9ChecksShown(){return r9Check[0][0]||r9Check[1][0];}
static void r9Report(bool verify,const char* note=nullptr) {
    r8Clear(verify?t(I18N_POD_VERIFY_T):t(I18N_POD_SCAN_T));
    int pitch=r9ChecksShown()?62:52;
    for(int i=0;i<2;i++) {
        char line[96];
        bool absent=r8UiUid[i].isEmpty();
        // Read tags lists what is on the readers and nothing else; Verify adds the inventory side.
        if(absent)snprintf(line,sizeof(line),"%c  %s",'A'+i,t(I18N_POD_SCAN_ABSENT));
        else if(verify)snprintf(line,sizeof(line),"%c  %s  %s  %s",'A'+i,r8UiUid[i].c_str(),r8UiChip[i].c_str(),r8UiResolved[i]?t(I18N_POD_SCAN_KNOWN):t(I18N_POD_SCAN_NEW));
        else snprintf(line,sizeof(line),"%c  %s  %s",'A'+i,r8UiUid[i].c_str(),r8UiChip[i].c_str());
        r8Text(r8Screen,line,18,70+i*pitch,444,R8_TEXT,&gFont16);
        if(verify && r8UiLabel[i].length()) {   // which inventory spool this tag belongs to, with its remaining weight
            auto* sp=r8Text(r8Screen,r8UiLabel[i],18,92+i*pitch,444,R8_ACCENT);
            lv_label_set_long_mode(sp,LV_LABEL_LONG_DOT);lv_obj_set_height(sp,18);
        }
        if(verify && r9Check[i][0]) {   // one line per tag: each content check with its result
            static const LangKey names[3]={I18N_POD_CHK_SIG,I18N_POD_CHK_IDX,I18N_POD_CHK_DATE};
            char chk[128];int n=0;chk[0]=0;
            for(int c=0;c<3;c++)n+=snprintf(chk+n,sizeof(chk)-n,"%s%s %s",c?"   ":"",t(names[c]),r9Check[i][c]==1?"OK":"X");
            bool good=r9Check[i][0]==1&&r9Check[i][1]==1&&r9Check[i][2]==1;
            r8Text(r8Screen,chk,18,112+i*pitch,444,good?0x6FCF8A:0xFF6B6B);
        }
    }
    if(verify) {
        int vy=r9ChecksShown()?198:182;
        bool a=r8UiResolved[0],b=r8UiResolved[1];
        const char* verdict=note?note:a&&b?t(I18N_POD_SCAN_OK):(a||b)?t(I18N_POD_SCAN_HALF):r8UiCompatible?t(I18N_POD_SCAN_FREE):t(I18N_POD_SCAN_NOPAIR);
        podStatus=r8Text(r8Screen,verdict,18,vy,444,R8_MUTED);
    }
    r8Button(t(I18N_POD_BACK),12,18,254,216,true);r8Button(t(I18N_POD_WEIGH),7,246,254);
    r9Reported=true;
}
// True when the saved journal describes exactly the two tags now on the readers (either order).
static bool r9JournalMatchesPair() {
    InventoryJson job(512*1024);PodJournal j;
    if(!LittleFS.exists(R7_JOB)||!r7Load(R7_JOB,job)||job["owner"].as<String>()!=firebaseUid)return false;
    if(!r7Unhex(job["journal"].as<String>(),(uint8_t*)&j,sizeof(j))||j.magic!=0x504f4431)return false;
    String x=podUid(j.uid[0]),y=podUid(j.uid[1]);
    return (x==r8UiUid[0]&&y==r8UiUid[1])||(x==r8UiUid[1]&&y==r8UiUid[0]);
}
// Verify: the pair on the scale decides. A matching journal gets the full NFC + inventory check;
// anything else is a read-only report, so a pair without journal can be checked too.
static bool r9Verify() {
    r9Check[0][0]=r9Check[0][1]=r9Check[0][2]=r9Check[1][0]=r9Check[1][1]=r9Check[1][2]=0;
    if(!r8Scan())return false;
    if(r9JournalMatchesPair()){bool ok=r7Resume(true);if(ok)r9Report(true);return ok;}
    // No journal: compare what the two tags physically hold with the inventory records of those UIDs.
    if(!(r8UiResolved[0]&&r8UiResolved[1])){r9Report(true);return true;}
    if(r8UiAlias[0]||r8UiAlias[1]){r9Report(true,t(I18N_POD_VER_LINKED));return true;}
    PodJournal cap={};uint8_t mem[888];
    for(int slot=0;slot<2;slot++) {
        podAt(slot,-1,"capture");
        if(!podSelect(slot)||!podInspect(slot,mem,cap.size[slot]))return false;
        memcpy(cap.uid[slot],podReader(slot).uid.uidByte,7);memcpy(cap.before[slot],mem,144);
    }
    if(!podOff())return false;
    char note[128];const char* verdict=t(I18N_POD_VER_OK);bool failed=false;
    for(int i=0;i<2;i++) {
        if(podUid(cap.uid[i])!=r8UiUid[i])return podFail("UID changed / tag absent before linking");
        const uint8_t* p=cap.before[i];
        const uint8_t standard[]={0x5b,0xf5,0x92,0x64},plus[]={0xbc,0x0f,0xcb,0x97};
        uint32_t stamp=(uint32_t)p[32]<<24|(uint32_t)p[33]<<16|(uint32_t)p[34]<<8|p[35];
        r9Check[i][0]=(memcmp(p,standard,4)&&memcmp(p,plus,4))?2:1;
        r9Check[i][1]=(r8UiIndex[i]>0 && p[39]!=(r8UiIndex[i]==1?0x12:0x22))?2:1;
        r9Check[i][2]=(r8UiStamp[i] && stamp!=r8UiStamp[i])?2:1;
        int key=r9Check[i][0]==2?I18N_POD_VER_SIG:r9Check[i][1]==2?I18N_POD_VER_IDX:r9Check[i][2]==2?I18N_POD_VER_STAMP:-1;
        if(key>=0 && !failed){failed=true;snprintf(note,sizeof(note),t((LangKey)key),'A'+i);verdict=note;}   // NOLINT: format comes from the table
    }
    r9Report(true,verdict);return true;
}
static bool r8Link() {
    podReadersReversed=false;
    InventoryRequest r;r.op=InventoryRequest::LIST;r.podAll=true;
    if(!inventoryWait(r,I18N_INV_LOADING,true))return false;
    if(r.spools.empty()){podSay("Aucune bobine disponible dans ce compte.");return false;}
    int chosen=inventoryPickSpool(r.spools);if(chosen<0)return false;
    r.source=r.spools[chosen].id;r.version=r.spools[chosen].version;
    podConfirmContext=r.spools[chosen].label+"\nLien partage avec Studio et vos balances.\nAucun tag ne sera ecrit.";
    if(!podConfirm("Ajouter cet identifiant ?"))return false;
    for(int i=0;i<2;i++)if(r8UiUid[i].length()) {
        if(!r8SelectRaw(i,r8UiUid[i]))return podFail("UID changed / tag absent before linking");
        r.uids[i]=r8UiUid[i];r.chip[i]=r8UiChip[i]=="mifare1k"?"mifare1k":"unknown";
    }
    podOff();r.op=InventoryRequest::POD_LINK;
    bool ok=inventoryWait(r,I18N_INV_SYNCING,false);
    podSay(ok?"Identifiant partage enregistre. Les TigerTags existants sont conserves.":r7CloudError);return ok;
}
// A maker chip (Bambu Lab...) already linked to a spool, typically a finished one: remove the shared
// link only. The spool stays in the inventory without a chip, and the chip can then be linked again.
static bool r8Unlink() {
    podReadersReversed=false;
    InventoryRequest r;String label;
    for(int i=0;i<2;i++)if(r8UiAlias[i] && r8UiUid[i].length()) {
        r.uids[i]=r8UiUid[i];
        if(label.length())label+="\n";
        label+=r8UiUid[i];
        if(r8UiLabel[i].length())label+=" - "+r8UiLabel[i];
    }
    if(r.uids[0].isEmpty() && r.uids[1].isEmpty()){podSay("Inventory unavailable / changed");return false;}
    podConfirmContext=label+"\nBobine conservee, sans puce.\nAucun tag ne sera ecrit.";
    if(!podConfirm("Supprimer le lien de cette puce ?"))return false;
    for(int i=0;i<2;i++)if(r.uids[i].length() && !r8SelectRaw(i,r.uids[i]))return podFail("UID changed / tag absent before unlinking");
    podOff();r.op=InventoryRequest::POD_UNLINK;
    bool ok=inventoryWait(r,I18N_INV_SYNCING,false);
    if(ok) {   // the scan on screen is stale: those chips are free now, so Nouvelle paire becomes the next step
        for(int i=0;i<2;i++)if(r8UiAlias[i]){r8UiAlias[i]=false;r8UiResolved[i]=false;r8UiLabel[i]="";}
        r8UiKnown=r8UiResolved[0]||r8UiResolved[1];
    }
    podSay(ok?"Lien supprime. La puce peut etre associee a une autre bobine.":r7CloudError);return ok;
}
// Display-only translation of the engine's status sentences. The journal logic and the serial
// log keep the original text; only what reaches the panel follows the chosen language.
struct PodMsg { const char* src; LangKey key; };
static const PodMsg kPodMsgs[]={
    {"Journal invalide. Aucun tag ecrit. Conserver les fichiers.",I18N_POD_M01},
    {"Aucun journal valide pour ce compte. Aucun tag ecrit.",I18N_POD_M02},
    {"Operation deja terminee. Utiliser Verifier ou une nouvelle action.",I18N_POD_M03},
    {"Sauvegarde NVS impossible. Aucun tag ecrit.",I18N_POD_M04},
    {"Ecriture puis verification des deux tags",I18N_POD_M05},
    {"Tags verifies, journal non sauvegarde. Reprendre.",I18N_POD_M06},
    {"Deux tags verifies. Inventaire synchronise.\nOperation terminee.",I18N_POD_M07},
    {"Operation en attente. Reprendre ou Restaurer avant toute nouvelle action.",I18N_POD_M08},
    {"Connecter le compte TigerTag et le Wi-Fi.",I18N_POD_M09},
    {"Controle complet des deux tags",I18N_POD_M10},
    {"Horloge invalide. Connecter le Wi-Fi puis reessayer.",I18N_POD_M11},
    {"Aucune bobine sans tag. Creer la bobine dans Studio.",I18N_POD_M12},
    {"Sauvegarde locale impossible. Aucun tag ecrit.",I18N_POD_M13},
    {"Preparation illisible. Aucun tag ecrit.",I18N_POD_M14},
    {"Sauvegarde precedente impossible. Aucun tag ecrit.",I18N_POD_M15},
    {"Journal non sauvegarde. Aucun tag ecrit. Garder les fichiers.",I18N_POD_M16},
    {"Aucun tag lisible. Placez la bobine puis scannez.",I18N_POD_M17},
    {"Aucune bobine disponible dans ce compte.",I18N_POD_M18},
    {"Ajouter cet identifiant ?",I18N_POD_M19},
    {"Supprimer le lien de cette puce ?",I18N_POD_UNLINK_Q},
    {"Lien supprime. La puce peut etre associee a une autre bobine.",I18N_POD_UNLINK_DONE},
    {"Confirmer cette operation ?",I18N_POD_CONFIRM_OP},
    {"RESTORE original blank data?",I18N_POD_CONFIRM_RESTORE},
    {"Replace completed journal with a NEW pair?",I18N_POD_CONFIRM_REPLACE},
    {"Prepare local TEST pair?",I18N_POD_CONFIRM_TEST},
    {"Identifiant partage enregistre. Les TigerTags existants sont conserves.",I18N_POD_M20},
    {"Tags verifies mais inventaire non synchronise. Reprendre ou Verifier.",I18N_POD_M21},
    {"Operation annulee ou non terminee. Scanner avant de recommencer.",I18N_POD_M22},
    {"Inventaire indisponible. Aucune ecriture NFC.",I18N_POD_M23},
    {"Deux bobines differentes detectees. Garder une seule bobine.",I18N_POD_M24},
    {"Un tag connu et un tag inconnu. Retirer le tag inconnu pour peser.",I18N_POD_M25},
    {"Bobine modifiee. Choisir de nouveau.",I18N_POD_M26},
    {"UID deja lie. Gerer le lien dans Studio.",I18N_POD_M27},
    {"Trop de liens pour une operation. Aucun tag ecrit.",I18N_POD_M28},
    {"Les UID ne correspondent pas au journal. Aucun tag ecrit.",I18N_POD_M29},
    {"Deux tags de production requis: indices 1/2 et 2/2",I18N_POD_M30},
    {"Wi-Fi/account unavailable",I18N_POD_M31},
    {"Not a coherent Twin pair in this account (links, index or timestamp differ)",I18N_POD_M32},
    {"Spool data cannot be encoded for the tags",I18N_POD_M33},
    {"Same UID on both readers",I18N_POD_M34},
    {"Physical pair differs from inventory",I18N_POD_M35},
    {"Tags lus. Les donnees de la bobine ont change dans Studio.",I18N_POD_M36},
    {"Lien vers une bobine absente / invalide. Verifier dans Studio.",I18N_POD_X01},
    {"Lien envoye, relecture impossible. Scanner avant de recommencer.",I18N_POD_X02},
    {"UID deja dans l'inventaire / lecture impossible",I18N_POD_X03},
    {"Ces UID existent deja dans l'inventaire. Utiliser Actualiser les tags pour les reecrire depuis l'inventaire.",I18N_POD_X04},
    {"Journal illisible. Conserver les tags et le journal.",I18N_POD_X05},
    {"UID changed / tag absent before linking",I18N_POD_X06},
    {"Extended data: operation refused",I18N_POD_X07},
    {"Backup target differs: use same unchanged tags",I18N_POD_X08},
    {"Backup metadata cannot be encoded",I18N_POD_X09},
    {"Backup tag differs from saved inventory metadata",I18N_POD_X10},
    {"Backup inventory document missing",I18N_POD_X11},
    {"Inventory conflict: keep tags and journal",I18N_POD_X12},
    {"Tags verified; cloud sync pending (Resume)",I18N_POD_X13},
    {"No completed backup for these UIDs/account",I18N_POD_X14},
    {"Pending operation conflicts with inventory; no restore",I18N_POD_X15},
    {"Restore conflict: document recreated",I18N_POD_X16},
    {"Restore: current inventory unavailable",I18N_POD_X17},
    {"Restore refused: metadata edited in Studio; use Reencode",I18N_POD_X18},
    {"Cloud operation failed; no automatic retry",I18N_POD_X19},
    {"Journal/account mismatch",I18N_POD_X20},
    {"NFC verification required",I18N_POD_X21},
    {"Inventory changed; no NFC write",I18N_POD_X22},
    {"Selected spool changed / invalid",I18N_POD_X23},
    {"Not a coherent Twin pair in this account (the two spool documents differ)",I18N_POD_X24},
    {"Twin metadata differs; resolve in Studio first",I18N_POD_X25},
    {"Unsupported job",I18N_POD_X26},
    {"Journal storage failure",I18N_POD_X27},
    {"UID already associated",I18N_POD_X28},
    {"Inventory unavailable / changed",I18N_POD_X29},
    {"Tag A non detecte. Verifiez sa position puis reessayez. Aucun tag modifie pendant cette tentative.",I18N_POD_X30},
    {"Tag B non detecte. Verifiez sa position puis reessayez. Aucun tag modifie pendant cette tentative.",I18N_POD_X31},
};
static String r8Translate(const String& text) {
    for(const auto& m:kPodMsgs) if(text==m.src) return String(t(m.key));
    // Memory refusals carry numbers: rebuild them from the translated pieces.
    static const PodMsg heads[]={
        {"Memoire interne totale insuffisante : ",I18N_POD_MEM_TOTAL},
        {"Memoire interne fragmentee (bloc contigu trop petit) : ",I18N_POD_MEM_BLOCK},
        {"PSRAM libre insuffisante : ",I18N_POD_MEM_PSRAM},
        {"PSRAM fragmentee (bloc contigu trop petit) : ",I18N_POD_MEM_PSRAM_BLOCK}};
    for(const auto& h:heads) if(text.startsWith(h.src)) {
        unsigned have=0,need=0;
        if(sscanf(text.c_str()+strlen(h.src),"%u Ko disponibles, %u Ko requis.",&have,&need)==2) {
            char fig[96];snprintf(fig,sizeof(fig),t(I18N_POD_MEM_FIG),have,need);   // NOLINT: format comes from the table
            return String(t(h.key))+fig+(strstr(text.c_str(),"rouvrira seul.")?t(I18N_POD_MEM_RESTART):t(I18N_POD_MEM_RETRY));
        }
    }
    // Confirmation context: replace the known fragments, keep spool labels and UIDs as they are.
    {
        struct Frag {const char* from;const char* to;};
        const Frag frags[]={
            {"Lien partage avec Studio et vos balances.\nAucun tag ne sera ecrit.",t(I18N_POD_LINK_NOTE)},
            {"Bobine conservee, sans puce.\nAucun tag ne sera ecrit.",t(I18N_POD_UNLINK_NOTE)},
            {"associate : ",nullptr},{"reset : ",nullptr},{"restore : ",nullptr},{"refresh : ",nullptr}};
        std::string work(text.c_str());bool changed=false;
        for(const auto& f:frags) {
            size_t at=work.find(f.from);if(at==std::string::npos)continue;
            std::string to;
            if(f.to)to=f.to;
            else {
                const char* kind=!strcmp(f.from,"associate : ")?t(I18N_POD_NEWPAIR):!strcmp(f.from,"reset : ")?t(I18N_POD_FORMAT):!strcmp(f.from,"restore : ")?t(I18N_POD_RESTORE):t(I18N_POD_REFRESH_TAGS);
                to=std::string(kind)+" : ";
            }
            work.replace(at,strlen(f.from),to);changed=true;
        }
        if(changed)return String(work.c_str());
    }
    // Failure reports are "<A|B|?> [Pxx ]<phase>: <message>": keep the location, translate the message.
    const char* raw=text.c_str();const char* sep=strstr(raw,": ");
    if(sep && sep-raw>2 && sep-raw<=20 && raw[1]==' ') {
        String rest=String(sep+2);
        for(const auto& m:kPodMsgs) if(rest==m.src) {
            char loc[24];size_t n=(size_t)(sep+2-raw);memcpy(loc,raw,n);loc[n]=0;
            return String(loc)+String(t(m.key));
        }
    }
    return text;
}
static void r8Home(bool tools=false) {
    r8Clear(tools?t(I18N_POD_TOOLS_SUB):t(I18N_POD_MGMT));
    if(tools) {
        r8Text(r8Screen,t(I18N_POD_TOOLS_NOTE),18,82,444,R8_MUTED);
        r8Button(t(I18N_POD_REFRESH_TAGS),2,18,126);r8Button(t(I18N_POD_FORMAT_PAIR),3,246,126);
        r8Button(t(I18N_POD_RESTORE),4,18,184);r8Button(t(I18N_POD_VERIFY),6,246,184);
        r8Button(t(I18N_POD_BACK),12,18,254,150);return;
    }
    if(r7Blocked) {
        r8Text(r8Screen,t(I18N_POD_BLOCKED_T),18,84,444,R8_TEXT,&gFont20);
        r8Text(r8Screen,t(I18N_POD_BLOCKED_D),18,120,444,R8_MUTED);
        r8Button(t(I18N_POD_RESUME),5,18,194,216,true);r8Button(t(I18N_POD_RESTORE),4,246,194);
    } else if(r86Prompt) {
        r8Text(r8Screen,t(I18N_POD_UPD_T),18,84,444,R8_TEXT,&gFont20);
        r8Text(r8Screen,t(I18N_POD_UPD_D),18,121,444,R8_MUTED);
        r8Button(t(I18N_POD_REFRESH),2,18,194,216,true);r8Button(t(I18N_POD_LATER),16,246,194);
    } else {
        // Mockup layout: back arrow + title, two columns of three cards. Restaurer lives under Outils.
        podStatus=nullptr;r8Bar=nullptr;lv_obj_clean(r8Screen);
        lv_obj_set_style_bg_color(r8Screen,lv_color_hex(0x0D131B),0);lv_obj_clear_flag(r8Screen,LV_OBJ_FLAG_SCROLLABLE);
        auto* back=r8Button(LV_SYMBOL_LEFT,7,6,4,44);lv_obj_set_size(back,44,44);
        r8Text(r8Screen,"TigerPOD",56,14,200,R8_TEXT,&gFont20);
        auto* tools=r8Button(t(I18N_POD_TOOLS),11,384,8,80);lv_obj_set_size(tools,80,36);
        (void)back;(void)tools;
        r8Card(t(I18N_POD_READ),t(I18N_POD_S_READ),&gUi_read,9,16,52,!r8UiScanned);
        r8Card(t(I18N_POD_NEWPAIR),t(I18N_POD_S_NEWPAIR),&gUi_link,r8UiCompatible?1:10,245,52,r8UiScanned&&!r8UiKnown);
        r8Card(t(I18N_POD_REFRESH),t(I18N_POD_S_REFRESH),&gUi_refresh,2,16,124,false);
        r8Card(t(I18N_POD_VERIFY),t(I18N_POD_S_VERIFY),&gUi_verify,6,245,124,false);
        r8Card(t(I18N_POD_RESUME),t(I18N_POD_S_RESUME),&gUi_resume,5,16,196,false);
        r8Card(t(I18N_POD_FORMAT),t(I18N_POD_S_FORMAT),&gUi_format,(r8UiScanned&&(r8UiAlias[0]||r8UiAlias[1]))?18:(r8UiCompatible&&!r8UiKnown)?13:3,245,196,false);
        return;
    }
    r8Button(t(I18N_POD_WEIGH),7,18,254,132);
    if(r8UiScanned)r8Button(t(I18N_POD_RESCAN),9,164,254,140);
    r8Button(t(I18N_POD_TOOLS),11,318,254,144);
}
// Retrying a partially written action must resume its immutable journal.
static bool r86RunTool(int action,bool retry) {
    if(action==6)return r7Resume(true);
    const char* kind=action==2?"reencode":action==3?"format":"restore";
    if(retry && LittleFS.exists(R7_JOB)) {
        InventoryJson job(512*1024);
        if(!r7Load(R7_JOB,job))return podFail("Journal illisible. Conserver les tags et le journal.");
        if(job["state"]!="done" && (job["kind"]==kind || (action==3 && job["kind"]=="reset"))) {
            job.clear();job.shrinkToFit();return r7Resume();
        }
    }
    return r7New(kind);
}
static String r86FailureText() {
    if(!strcmp(podErrorPhase,"capture") && !strcmp(podReadError,"no tag / selection timeout"))
        return String(podErrorSlot?"Tag B non detecte. ":"Tag A non detecte. ")+
            "Verifiez sa position puis reessayez. Aucun tag modifie pendant cette tentative.";
    return strcmp(podReadError,"unknown")?podErrorReport():r7CloudError;
}
// Re-measure after dropping our own caches and letting the network stack drain, before refusing.
static uint8_t r86Reclaim(bool operation,uint32_t* have,uint32_t* need) {
    r86TlsClose();   // an idle keep-alive session holds ~45 KB of internal RAM: free it before measuring
    uint8_t code=r86MemoryCheck(operation,have,need);
    if(!code)return code;
    r86FreeCaches();
    for(int attempt=0;code && attempt<5;attempt++) {
        uint32_t until=millis()+300;
        while((int32_t)(millis()-until)<0){r7Sample();lv_timer_handler();delay(10);}
        code=r86MemoryCheck(operation,have,need);
    }
    return code;
}
static void runPodInventory(bool associate,bool updatePrompt) {
    uint8_t opEntryCode=R86_MEM_OK;
    if(gInventoryAssignmentPending || gInventoryRequest || !gBackgroundTasksStarted || gCloudSendPending) {
        lvglNotice(t(I18N_POD_FINISH_FIRST));return;
    }
    bool oldPause=gLivePaused;gLivePaused=true;gPodPhysicalOnly=true;r7Active=true;
    while(!r7WorkerPaused || !gLiveSuspended){r7Sample();lv_timer_handler();delay(10);}
    r86Memory("pod-enter");
    {
        // The full operation gate is measured HERE, before the screens below take their share of
        // internal RAM (they cost several KB of contiguous block): measuring after the UI exists
        // made the same heap fail a check it passed at entry. The menu itself only needs the lenient limits.
        uint32_t opHave=0,opNeed=0;opEntryCode=r86Reclaim(true,&opHave,&opNeed);
        uint32_t have=0,need=0;uint8_t code=r86MemoryCheck(false,&have,&need);
        if(code) {
            gPodPhysicalOnly=false;r7Active=false;gLivePaused=oldPause;
            if(updatePrompt)r86LaterKey=r86UpdateKey;
            lvglNotice(r8Translate(r86MemoryMessage(code,have,need)).c_str());return;
        }
    }
    gLiveTapPhase=LIVE_TAP_IDLE;gLiveTapSteps=0;podCancel=false;podService=r7Sample;
    setupRFID(false);lv_obj_t* previous=lv_scr_act();r8Screen=lv_obj_create(nullptr);lv_scr_load(r8Screen);
    podTranslate=r8Translate;podConfirmUi=r8Confirm;podProgressUi=r8Progress;r8UiScanned=false;r8Action=associate?9:0;r8Busy=false;r8Home();
    bool toolsContext=false,touched=false,updated=false;int lastTool=0;
    while(true) {
        r7Sample();lv_timer_handler();delay(5);int a=r8Action;r8Action=0;if(!a)continue;
        if(a==7 || a==16)break;
        if(a==17)r86ScheduleResume();
        if(a==11 || a==12){toolsContext=a==11;r8Home(toolsContext);continue;}
        bool retry=a==14;if(retry){a=lastTool;if(!a)continue;}
        touched=true;
        if(a==2 || a==3 || a==4 || a==6)lastTool=a;
        r9Reported=false;gPodMemRefused=false;podCancel=false;podReadError="unknown";r7CloudError="Operation annulee ou non terminee. Scanner avant de recommencer.";r8Busy=true;bool ok=false;
        r8Working(a==9?t(I18N_POD_READING):t(I18N_POD_PREPARING));
        if(a==9) {for(int i=0;i<2;i++)r9Check[i][0]=r9Check[i][1]=r9Check[i][2]=0;ok=r8Scan();r8Busy=false;if(ok)r9Report(false);}
        else if(a==10)ok=r8Link();
        else if(a==18)ok=r8Unlink();
        else {
            // NFC and network operations: full memory gate, with the exact cause on refusal.
            uint32_t have=0,need=0;uint8_t code=r86Reclaim(true,&have,&need);
            // A contiguous-block shortfall that appeared only after the UI was built is not a refusal.
            if(code==R86_MEM_BLOCK && opEntryCode==R86_MEM_OK)code=R86_MEM_OK;
            if(code){r7CloudError=r86MemoryMessage(code,have,need);gPodMemRefused=true;}
            else if(a==1 || a==13) {r8AcceptOccupied=true;ok=r7New(a==1?"associate":"reset");r8AcceptOccupied=false;}
            else if(a==6)ok=r9Verify();
            else if(a==2 || a==3 || a==4)ok=r86RunTool(a,retry);
            else ok=r7Resume(a==6);
            // A refresh counts as handled only after both tags were written and read back
            // (physical confirmation inside r7Resume) AND the inventory re-read matches.
            if(ok && a==2) {
                ok=r7Cloud(4);
                if(ok){updated=true;r86UpdateWanted=false;r86LaterKey="";r86Prompt=false;}
                else podSay("Tags verifies mais inventaire non synchronise. Reprendre ou Verifier.");
            }
        }
        gPodWriteAllowed=false;bool off=podOff();r8Busy=false;
        if((a!=9 || !ok) && !r9Reported)r86Result(ok,r86FailureText(),toolsContext,a);
        if(!off && podStatus)lv_label_set_text(podStatus,t(I18N_POD_RF_ERROR));
        r8Action=0;
    }
    gPodWriteAllowed=false;podOff();podReadersReversed=false;podService=nullptr;podConfirmUi=nullptr;podProgressUi=nullptr;podOperationLabel="";
    // Declined or untouched refresh prompt: keep weighing state and the cached association.
    if(updatePrompt && !updated)r86LaterKey=r86UpdateKey;
    if(!(updatePrompt && !touched)) {
        String uids[2]={podUid(podJournal.uid[0]),podUid(podJournal.uid[1])};restartInventoryRead(uids);
        ++r8Generation;r8WeighReady=false;r8Seen[0]=R8Seen();r8Seen[1]=R8Seen();r8Offered=false;r8DiscoveryAt=millis();
    }
    gLiveTapPhase=LIVE_TAP_IDLE;gLiveTapSteps=0;gPodPhysicalOnly=false;gLivePaused=oldPause;
    lv_scr_load(lvScreen?lvScreen:previous);lv_obj_del(r8Screen);r8Screen=nullptr;podStatus=nullptr;setupRFID(false);
    r86Memory("pod-exit");r7Active=false;while(r7WorkerPaused){lv_timer_handler();delay(5);}gLastActivityMs=millis();
}
