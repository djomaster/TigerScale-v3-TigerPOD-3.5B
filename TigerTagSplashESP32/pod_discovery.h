#pragma once
// Core 1 never waits for HTTP or opens a progress screen for recognition.
// This grace belongs to discovery only; the reader's 1.5 s dedup is unchanged.
static void r8DiscoveryTick() {
    static uint32_t retryAt=0,emptySince=0,requestGeneration=0,requestStarted=0;
    static String resolvedUid[2];
    const uint32_t grace=6000;
    if(r8SeenOwner!=firebaseUid) {
        ++r8Generation;r8WeighReady=false;r8SeenOwner=firebaseUid;
        r8Seen[0]=R8Seen();r8Seen[1]=R8Seen();resolvedUid[0]="";resolvedUid[1]="";r8Offered=false;r86ResetUpdate();
    }
    bool loaded=blankLoadPresent();
    if(!loaded) {
        r8WeighReady=false;
        if(!emptySince)emptySince=millis();
        if(millis()-emptySince>600 && (r8Seen[0].uid.length()||r8Seen[1].uid.length())) {
            ++r8Generation;r8Seen[0]=R8Seen();r8Seen[1]=R8Seen();
            resolvedUid[0]="";resolvedUid[1]="";r8Offered=false;r86ResetUpdate();
        }
    } else emptySince=0;
    String present[2];
    for(int i=0;i<2;i++)if(r8Seen[i].uid.length() && millis()-r8Seen[i].at<grace)present[i]=r8Seen[i].uid;
    if(r8DiscoveryBusy) {
        if(!r8DiscoveryRequest.done)return;
        r8DiscoveryBusy=false;
        netLog("[POD TIMING] lookup_ms="+String(millis()-requestStarted));
        bool current=loaded && requestGeneration==r8Generation && r8DiscoveryRequest.owner==firebaseUid;
        for(int i=0;i<2;i++)if(present[i]!=r8DiscoveryRequest.uids[i] || (present[i].length() && r8Seen[i].uid!=present[i]))current=false;
        if(current && r8DiscoveryRequest.ok) {
            r83LookupAt=millis();r83LookupGeneration=r8Generation;
            for(int i=0;i<2;i++) {
                resolvedUid[i]=present[i];
                if(present[i].length()) {
                    r8Seen[i].checked=true;r8Seen[i].target=r8DiscoveryRequest.resolved[i];
                    r8Seen[i].alias=r8DiscoveryRequest.alias[i];
                }
            }
            r86UpdateKey=present[0]+"|"+present[1];
            r86UpdateWanted=r8DiscoveryRequest.needsUpdate && present[0].length() && present[1].length();
            retryAt=0;
        } else if(current) {
            r8WeighReady=false;retryAt=millis()+10000;
            netLog("[POD DISCOVERY] Lookup failed; retry in background");return;
        }
    }
    if(r7Active || r7Blocked || !loaded || !gBackgroundTasksStarted || !lvglReady || gZeroRequired)return;
    if(!firebaseAuth){r8WeighReady=false;return;}
    bool any=false,needs=false,known=false,fresh=false;
    for(int i=0;i<2;i++) {
        if(present[i]!=resolvedUid[i])needs=true;
        if(present[i].length()) {
            any=true;fresh=fresh || millis()-r8Seen[i].at<1500;
            if(!r8Seen[i].checked)needs=true;
            known=known||r8Seen[i].target.length();
        }
    }
    if(!any){r8WeighReady=false;return;} // Keep the cache across a short RF dropout.
    if(!needs) {
        r8WeighReady=known && fresh;
        if(!known && fresh && !r8Offered && !gInventoryRequest && !gCloudSendPending) {
            r8Offered=true;gBlankLoadLatched=true;gBlankLoadWasPresent=true;gBlankEmptySinceMs=0;
            runPodInventory(true);r8Offered=true;
        }
        // Studio asked for a tag refresh on this recognised pair: offer it once per
        // placement. "Plus tard" remembers the pair, so weighing carries on unprompted.
        else if(known && fresh && r86UpdateWanted && r86UpdateKey!=r86LaterKey
                && !gInventoryRequest && !gCloudSendPending && !gInventoryAssignmentPending) {
            r86Prompt=true;runPodInventory(false,true);r86Prompt=false;
        }
        return;
    }
    r8WeighReady=false;
    if((retryAt && (int32_t)(millis()-retryAt)<0) || millis()-r8DiscoveryAt<(present[0].length() && present[1].length()?150U:500U)
       || gCloudSendPending || gInventoryRequest || r8DiscoveryPending)return;
    r8DiscoveryRequest=InventoryRequest();r8DiscoveryRequest.op=InventoryRequest::POD_RESOLVE;
    r8DiscoveryRequest.owner=firebaseUid;
    for(int i=0;i<2;i++)r8DiscoveryRequest.uids[i]=present[i];
    requestGeneration=r8Generation;requestStarted=millis();r8DiscoveryBusy=true;
    r8DiscoveryPending=true; // Publish only after the complete request is ready.
}
