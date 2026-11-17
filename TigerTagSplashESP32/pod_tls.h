// 1.0 -- one TLS client and one HTTP client shared by every Firestore call.
//
// Before, each request built a WiFiClientSecure + HTTPClient on the stack, opened a
// session, and tore it down, so the ~33 KB contiguous TLS block was carved out and
// freed dozens of times per operation -- the pattern that fragments internal RAM.
// Now both objects exist once, calls hold a lease (so two tasks never share a socket),
// and a connection that is still warm is reused (keep-alive) instead of rebuilt.
//
// The HTTPClient is deliberately never destroyed: its destructor stops the socket,
// which would defeat the reuse. A lease is re-entrant for the task that holds it.
#pragma once
#ifdef ESP_PLATFORM
#define R86_SELF() ((uintptr_t)xTaskGetCurrentTaskHandle())
#else
#define R86_SELF() ((uintptr_t)1)
#endif
#ifndef R86_TLS_IDLE_MS
#define R86_TLS_IDLE_MS 8000UL   // a socket idle longer than this is probably closed by the server
#endif
#ifndef R86_TLS_CLOSE_MS
#define R86_TLS_CLOSE_MS 1500UL  // an idle socket keeps ~45 KB of internal RAM: give it back quickly
#endif
struct R86TlsState {
    WiFiClientSecure* client = nullptr;
    HTTPClient* http = nullptr;
    volatile uintptr_t owner = 0;
    int depth = 0;
    uint32_t lastMs = 0;
};
static R86TlsState gR86Tls;
// Stop the shared socket when nobody is using it. Other TLS users (pairing, update manifest, Bambu
// import) call this first so two sessions' buffers are never resident at once.
static void r86TlsClose() {
    if (!gR86Tls.client) return;
    if (!__sync_bool_compare_and_swap(&gR86Tls.owner, (uintptr_t)0, R86_SELF())) return;
    gR86Tls.client->stop();
    __sync_synchronize(); gR86Tls.owner = 0;
}
// Called from loop(): an idle shared session must not sit on internal RAM between bursts.
static void r86TlsIdleTick() {
    if (!gR86Tls.client || gR86Tls.owner) return;
    if (millis() - gR86Tls.lastMs < R86_TLS_CLOSE_MS) return;
    r86TlsClose();
}
class R86TlsLease {
public:
    R86TlsLease() {
        uintptr_t me = R86_SELF();
        if (gR86Tls.owner == me) { gR86Tls.depth++; return; }
        while (!__sync_bool_compare_and_swap(&gR86Tls.owner, (uintptr_t)0, me)) delay(5);
        gR86Tls.depth = 1;
        if (!gR86Tls.client) {
            gR86Tls.client = new WiFiClientSecure();
            gR86Tls.client->setCACert(POD_CLOUD_CA);
            gR86Tls.http = new HTTPClient();
        }
        if (millis() - gR86Tls.lastMs > R86_TLS_IDLE_MS) gR86Tls.client->stop();
        gR86Tls.http->setReuse(true);
        gR86Tls.http->useHTTP10(false);
    }
    ~R86TlsLease() {
        gR86Tls.lastMs = millis();
        if (--gR86Tls.depth == 0) { __sync_synchronize(); gR86Tls.owner = 0; }
    }
    R86TlsLease(const R86TlsLease&) = delete;
    R86TlsLease& operator=(const R86TlsLease&) = delete;
    WiFiClientSecure& client() { return *gR86Tls.client; }
    HTTPClient& http() { return *gR86Tls.http; }
};
