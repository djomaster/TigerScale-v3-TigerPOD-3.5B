#pragma once
// Official Studio 2.51 linked-chip schema; never write the physical maker chip.
struct R8Seen { String uid,target; uint32_t at=0; bool checked=false,alias=false; };
static R8Seen r8Seen[2];
static String r8SeenOwner;
static bool r8DiscoveryBusy=false,r8Offered=false;
static volatile bool r8WeighReady=false;
static uint32_t r8DiscoveryAt=0,r8Generation=0,r83LookupAt=0,r83LookupGeneration=0;
static InventoryRequest r8DiscoveryRequest;
static volatile bool r8DiscoveryPending=false;
// Studio update request: wanted flag + the pair it applies to; "later" remembers
// the pair for which the offer was declined so it is not repeated while it stays.
static bool r86UpdateWanted=false,r86Prompt=false;
static String r86UpdateKey,r86LaterKey;
static void r86ResetUpdate() {r86UpdateWanted=false;r86UpdateKey="";r86LaterKey="";}
static bool r86NeedUpdate(JsonObjectConst fields) {
    JsonObjectConst o=fields["needUpdateAt"].as<JsonObjectConst>();
    if(o.isNull() || o.containsKey("nullValue"))return false;
    if(o.containsKey("booleanValue"))return o["booleanValue"]|false;
    if(o.containsKey("integerValue"))return o["integerValue"].as<String>()!="0";
    return true;
}
static void r8Observe(int slot,const String& uid) {
    auto& s=r8Seen[slot];
    if(s.uid!=uid) {
        ++r8Generation;r8WeighReady=false;
        // The association belongs to the pair: a new UID in one slot invalidates the
        // other slot's old association too, until a fresh lookup confirms both.
        for(int i=0;i<2;i++)if(i!=slot){r8Seen[i].checked=false;r8Seen[i].target="";r8Seen[i].alias=false;}
        s=R8Seen();s.uid=uid;r8DiscoveryAt=millis();r8Offered=false;
        r86UpdateWanted=false;r86UpdateKey="";
    }
    s.at=millis();
}
static bool r8Allowed(int slot,const String& uid) {
    return r8SeenOwner==firebaseUid && r8Seen[slot].uid==uid && r8Seen[slot].checked && r8Seen[slot].target.length();
}
static bool r8AliasTarget(const String& id) {
    for(auto& s:r8Seen)if(s.alias && s.target==id && r8SeenOwner==firebaseUid)return true;
    return false;
}
static bool r8Uid(const String& uid) {
    if(uid.length()!=8 && uid.length()!=14 && uid.length()!=20)return false;
    for(size_t i=0;i<uid.length();i++)if(!((uid[i]>='0'&&uid[i]<='9')||(uid[i]>='A'&&uid[i]<='F')))return false;
    return true;
}
static int r8AliasGet(const String& uid,InventoryJson& d) {
    if(!r8Uid(uid))return -2;
    return inventoryGet(String(INVENTORY_ROOT)+"/users/"+firebaseUid+"/rfidAliases/"+uid,d);
}
static bool r8Spool(JsonObjectConst fields) {
    return !fields.isNull() && !(fields["deleted"]["booleanValue"]|false) && inventoryNumber(fields,"id_type",142)==142;
}
// One-line spool description for the Read / Verify screens: brand material - name (grams).
static String r9SpoolLabel(JsonObjectConst f) {
    String brand=inventoryText(f,"manufacturer");
    if(brand.isEmpty())brand=inventoryText(f,"brand");
    auto bi=gBrandDb.find((uint16_t)inventoryNumber(f,"id_brand"));
    if(brand.isEmpty()&&bi!=gBrandDb.end())brand=bi->second;
    String mat=inventoryText(f,"material");
    auto mi=gMaterialDb.find((uint16_t)inventoryNumber(f,"id_material"));
    if(mat.isEmpty()&&mi!=gMaterialDb.end())mat=mi->second;
    String color=inventoryText(f,"color_name");
    if(color.isEmpty())color=inventoryText(f,"name");
    if(color.isEmpty())color=inventoryText(f,"message");
    String s=brand+" "+mat;
    if(color.length())s+=" - "+color;
    if(f.containsKey("weight_available")) {
        char g[24];snprintf(g,sizeof(g)," (%.0f g)",(double)inventoryNumber(f,"weight_available"));s+=g;
    }
    return s;
}
static String r8Name(const String& id,const char* collection="inventory") {
    return "projects/tigertag-connect/databases/(default)/documents/users/"+firebaseUid+"/"+collection+"/"+id;
}
// Read two identities in one authenticated HTTPS exchange. Results are keyed
// by full document name: Firestore does not promise response ordering.
static bool r82Read(const String* ids,const char* collection,InventoryJson& result) {
    InventoryJson request(4096),response(128*1024);
    JsonArray names=request.createNestedArray("documents");
    for(int i=0;i<2;i++)if(ids[i].length()) {
        if(!r7Id(ids[i]))return false;
        if(!i || ids[i]!=ids[0])names.add(r8Name(ids[i],collection));
    }
    result.clear();if(!names.size())return true;
    JsonArray mask=request["mask"].createNestedArray("fieldPaths");
    for(auto field:{"id_type","deleted","twin_tag_uid","spoolId","needUpdateAt","tag_count","tag_index","timestamp",
                    "manufacturer","brand","id_brand","material","id_material","color_name","name","message","weight_available"})mask.add(field);
    String body;serializeJson(request,body);
    R86TlsLease lease;HTTPClient& http=lease.http();http.setTimeout(15000);
    bool valid=false;
    if(http.begin(lease.client(),String(INVENTORY_ROOT)+":batchGet")) {
        http.addHeader("Authorization","Bearer "+firebaseIdToken);http.addHeader("Content-Type","application/json");
        int code=http.POST(body);
        if(code==200) {
            valid=r86HttpJson(http,response) && response.is<JsonArray>();
        }
        http.end();if(code<0)lease.client().stop();
    }
    if(valid)for(JsonVariantConst name:names) {
        int matches=0;
        for(JsonObjectConst entry:response.as<JsonArrayConst>()) {
            if(entry["found"]["name"].as<String>()==name.as<String>()) {
                result[name.as<String>()]["code"]=200;result[name.as<String>()]["fields"].set(entry["found"]["fields"]);matches++;
            } else if(entry["missing"].as<String>()==name.as<String>()) {result[name.as<String>()]["code"]=404;matches++;}
        }
        if(matches!=1)valid=false;
    }
    if(valid && !result.overflowed())return true;
    // Compatibility fallback only reads; never infer absence from an HTTP error.
    netLog("[POD TIMING] batch fallback: individual reads");
    result.clear();
    for(int i=0;i<2;i++)if(ids[i].length()) {
        InventoryJson doc(128*1024);int code=r7Get(firebaseUid,ids[i],doc,collection);
        if(code!=200 && code!=404)return false;
        String name=r8Name(ids[i],collection);result[name]["code"]=code;
        if(code==200)result[name]["fields"].set(doc["fields"]);
    }
    return !result.overflowed();
}
static bool r8Post(InventoryJson& batch) {
    if(batch.overflowed())return false;
    const char* path="/pod8-link-commit.json";
    if(!inventorySaveJson(path,batch.as<JsonVariantConst>()))return false;
    File file=LittleFS.open(path,"r");if(!file)return false;
    R86TlsLease lease;HTTPClient& http=lease.http();http.setTimeout(15000);
    if(!http.begin(lease.client(),String(INVENTORY_ROOT)+":commit"))return false;
    http.addHeader("Authorization","Bearer "+firebaseIdToken);http.addHeader("Content-Type","application/json");
    int code=http.sendRequest("POST",&file,file.size());http.end();if(code<0)lease.client().stop();file.close();
    if(code!=200)r7CloudError="Firebase HTTP "+String(code)+" : lien non confirme. Scanner pour verifier.";
    return code==200;
}
static void r8LinkedRun(InventoryRequest& r) {
    r.ok=false;r.needsUpdate=false;r7CloudError="Inventaire indisponible. Aucune ecriture NFC.";
    if(r.op==InventoryRequest::POD_UNLINK) {
        // Frees a maker chip (Bambu Lab...) that is linked to a finished spool: only the shared link
        // document goes. The spool stays in the inventory, and no physical chip is ever written.
        // Each delete is conditional on the revision just read, so a link edited meanwhile in Studio is left alone.
        InventoryJson batch(64*1024);JsonArray writes=batch.createNestedArray("writes");
        for(int i=0;i<2;i++) {
            if(r.uids[i].isEmpty() || (i && r.uids[i]==r.uids[0]))continue;
            if(!r8Uid(r.uids[i]))return;
            InventoryJson d(8192);int code=r8AliasGet(r.uids[i],d);
            if(code==404)continue;
            if(code!=200 || d["updateTime"].as<String>().isEmpty()){r7CloudError="Inventory unavailable / changed";return;}
            JsonObject w=writes.createNestedObject();w["delete"]=r8Name(r.uids[i],"rfidAliases");
            w["currentDocument"]["updateTime"]=d["updateTime"];
        }
        if(!writes.size()){r.ok=true;return;}   // nothing is linked any more
        bool sent=r8Post(batch);
        bool gone=true;
        for(int i=0;i<2;i++)if(r.uids[i].length()) {
            InventoryJson d(8192);if(r8AliasGet(r.uids[i],d)!=404)gone=false;
        }
        r.ok=gone;   // a lost reply is reconciled by reading each actual link
        if(!r.ok && sent)r7CloudError="Lien envoye, relecture impossible. Scanner avant de recommencer.";
        return;
    }
    if(r.op==InventoryRequest::POD_RESOLVE) {
        String twin[2],missing[2],targets[2];
        for(int i=0;i<2;i++) {
            r.resolved[i]="";r.resolvedLabel[i]="";r.docStamp[i]=0;r.docIndex[i]=0;r.alias[i]=false;
            if(r.uids[i].length()&&!r8Uid(r.uids[i]))return;
        }
        InventoryJson direct(128*1024),aliases(128*1024),spools(128*1024);
        if(!r82Read(r.uids,"inventory",direct))return;
        for(int i=0;i<2;i++)if(r.uids[i].length()) {
            JsonObjectConst doc=direct[r8Name(r.uids[i])];
            if(doc["code"]==200) {
                if(!r8Spool(doc["fields"]))return;
                r.resolved[i]=r.uids[i];r.resolvedLabel[i]=r9SpoolLabel(doc["fields"]);
                r.docStamp[i]=(uint32_t)inventoryNumber(doc["fields"],"timestamp");r.docIndex[i]=(int)inventoryNumber(doc["fields"],"tag_index",0);
                twin[i]=inventoryText(doc["fields"],"twin_tag_uid");
                // Visible trace for "Studio asks for an update but the scale does not offer it".
                Serial.printf("[POD UPDATE] uid=%s need=%d tag_count=%d tag_index=%d twin=%s\n",r.uids[i].c_str(),
                    (int)r86NeedUpdate(doc["fields"]),(int)inventoryNumber(doc["fields"],"tag_count",0),
                    (int)inventoryNumber(doc["fields"],"tag_index",0),twin[i].c_str());
                if(r86NeedUpdate(doc["fields"]) && (inventoryNumber(doc["fields"],"tag_count",0)==2 || inventoryNumber(doc["fields"],"tag_count",0)==0))r.needsUpdate=true;
            } else missing[i]=r.uids[i];
        }
        if(!r82Read(missing,"rfidAliases",aliases))return;
        for(int i=0;i<2;i++)if(missing[i].length()) {
            JsonObjectConst doc=aliases[r8Name(missing[i],"rfidAliases")];
            if(doc["code"]==200) {
                targets[i]=inventoryText(doc["fields"],"spoolId");if(!r7Id(targets[i]))return;
            }
        }
        if(!r82Read(targets,"inventory",spools))return;
        for(int i=0;i<2;i++)if(targets[i].length()) {
            JsonObjectConst doc=spools[r8Name(targets[i])];
            if(doc["code"]!=200 || !r8Spool(doc["fields"])) {
                r7CloudError="Lien vers une bobine absente / invalide. Verifier dans Studio.";return;
            }
            r.resolved[i]=targets[i];r.resolvedLabel[i]=r9SpoolLabel(doc["fields"]);r.alias[i]=true;twin[i]=inventoryText(doc["fields"],"twin_tag_uid");
        }
        if(r.resolved[0].length()&&r.resolved[1].length()&&r.resolved[0]!=r.resolved[1]
           && (twin[0]!=r.resolved[1]||twin[1]!=r.resolved[0])) {
            r7CloudError="Deux bobines differentes detectees. Garder une seule bobine.";return;
        }
        // Unknown + recognized may be a loose tag: never auto-link it or weigh it as a pair.
        if(r.uids[0].length()&&r.uids[1].length() && (r.resolved[0].isEmpty()!=r.resolved[1].isEmpty())){
            r7CloudError="Un tag connu et un tag inconnu. Retirer le tag inconnu pour peser.";return;
        }
        // A refresh only applies to a complete direct pair; linked chips have nothing to re-encode.
        if(!(r.uids[0].length()&&r.uids[1].length()&&!r.alias[0]&&!r.alias[1]&&r.resolved[0].length()&&r.resolved[1].length()))r.needsUpdate=false;
        // Both documents must name each other as twin (older pairs have no tag_count to say so).
        if(twin[0]!=r.uids[1]||twin[1]!=r.uids[0])r.needsUpdate=false;
        r.ok=true;return;
    }
    if(r.source.isEmpty() || (!r.uids[0].length()&&!r.uids[1].length()))return;
    InventoryJson spool(128*1024);if(r7Get(firebaseUid,r.source,spool)!=200 || !r8Spool(spool["fields"]))return;
    if(spool["updateTime"].as<String>()!=r.version){r7CloudError="Bobine modifiee. Choisir de nouveau.";return;}
    InventoryJson batch(512*1024);JsonArray writes=batch.createNestedArray("writes");
    // Compare the selected spool revision atomically with alias creation.
    JsonObject keep=writes.createNestedObject();keep["update"]["name"]=r8Name(r.source);
    keep["update"]["fields"].set(spool["fields"]);keep["currentDocument"]["updateTime"]=r.version;
    for(int i=0;i<2;i++) {
        if(r.uids[i].isEmpty() || (i && r.uids[i]==r.uids[0]))continue;
        if(!r8Uid(r.uids[i]))return;
        InventoryJson existing(128*1024);int code=r7Get(firebaseUid,r.uids[i],existing);
        if(code!=404){r7CloudError="UID deja dans l'inventaire / lecture impossible";return;}
        existing.clear();code=r8AliasGet(r.uids[i],existing);
        if(code==200){r7CloudError="UID deja lie. Gerer le lien dans Studio.";return;}
        if(code!=404)return;
        JsonObject w=writes.createNestedObject();w["update"]["name"]=r8Name(r.uids[i],"rfidAliases");
        JsonObject f=w["update"].createNestedObject("fields");
        f["spoolId"]["stringValue"]=r.source;f["chip"]["stringValue"]=r.chip[i].length()?r.chip[i]:String("unknown");
        f["vendor"]["stringValue"]="unknown";
        auto transform=w.createNestedArray("updateTransforms").createNestedObject();
        transform["fieldPath"]="linkedAt";transform["setToServerValue"]="REQUEST_TIME";
        w["currentDocument"]["exists"]=false;
    }
    bool sent=r8Post(batch);bool verified=true;
    for(int i=0;i<2;i++)if(r.uids[i].length()) {
        InventoryJson d(8192);if(r8AliasGet(r.uids[i],d)!=200 || inventoryText(d["fields"],"spoolId")!=r.source)verified=false;
    }
    r.ok=verified; // Lost response is reconciled by reading each actual shared link.
    if(!r.ok && sent)r7CloudError="Lien envoye, relecture impossible. Scanner avant de recommencer.";
}
