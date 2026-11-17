# Worklog — since the last checkpoint

The single source of truth for everything done since the last commit. Read it at
the start of a session; append to it the moment a change is done, never in a
batch at the end.

The headings match [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), so
at release time this file is synthesised into the `CHANGELOG.md` entry and the
release note in `docs/release-notes/` without being re-derived from the diff.

Keep it clean as you go: describe the **end state**, not the journey. An "Added
X" and a later "Fixed X" from the same cycle collapse into one entry. Anything
reverted disappears entirely — it never shipped. One bullet, one logical change,
always naming the files touched.

Reset this file to the header above at each checkpoint. Nothing is lost: the raw
log lives in git history and the synthesised entry lives in the changelog.

---

## Fixed

- Port the official 3.12/3.13 reader bring-up and background-recovery principles to the fork's full UART-frame reader. Configure modules even when their firmware-version query fails, retry three conversations after one reset, retain requested reader count, and promote readers from raw reads before blank/Bambu filtering. Keep an unconfirmed peer's RF field off when possible without making an absent module block the working reader (firmware).

- Make the native inventory test's fake Arduino String accept all integral types without ambiguous size_t conversion on macOS; firmware code is unchanged by this test-harness portability fix (`scripts/test-inventory.py`).

- The browser installer and firmware update channel use the fork's releases
  and Pages site, with explicit repository/site arguments in the deployment
  workflow. The release includes all three supported boards, including
  JC3248W535, and deployment verification checks each board's served binary
  against its release asset. The installer identifies the community fork and
  links the JC3248 wiring guide in every language
  (`.github/workflows/pages.yml`, `.github/workflows/release.yml`,
  `scripts/fetch-published-site.py`, `scripts/make-manifest.py`,
  `scripts/verify-published-site.py`, `TigerTagSplashESP32/TigerTagSplashESP32.ino`,
  `platformio.ini`, `web-installer/`, `docs/INSTALLATION.md`,
  `docs/JC3248W535.md`).

## Changed

- Personal edition: restrict PlatformIO, firmware board/transport guards, verification builds, CI matrices and manifest board lists to Waveshare ESP32-S3-Touch-LCD-3.5B with HSU PN532 readers (`platformio.ini`, firmware, `scripts/verify.sh`, publishing helper scripts, `.github/workflows/`, `README.md`, `llms.txt`, `docs/CUSTOM-3.5B.md`). Other-board source branches are retained as parent-fork history but cannot be built in this edition.
- Set AXP2101 charging to 1000 mA (about 0.33C for 3000 mAh) and VBUS input limit to 1500 mA after Wire1 initialisation; preserve unrelated register bits and verify readback (firmware).

- Prepared the first fork release, v3.11.0, with release notes covering the
  inventory/tag workflow, Bambu import, weight handling and supported boards
  (`TigerTagSplashESP32/TigerTagSplashESP32.ino`, `CHANGELOG.md`,
  `docs/release-notes/v3.11.0.md`, `llms.txt`, `scripts/check-llms-txt.py`,
  `docs/INSTALLATION.md`).

- The README's Onshape logo displays at 400 pixels wide and its browser-install
  button opens the fork's installer (`README.md`).

## Added

- Port the official 16-level RF table and translated Auto calibration menu, scoring the weaker reader first and confirming three finalists. Preserve the fork's write/erase/Bambu workflows and actual RF alternation. Keep a 100 ms UART read budget and two activation retries, and consume complete configuration responses instead of discarding variable-length tag data (firmware, `i18n.h`).
- Migrate the fork's former factory RF level 5 to the equivalent new level 15 exactly once; retain levels 0-4 and default fresh units to 7. Persist explicit single-reader selection separately from automatic detection. PowerDown also attempts unconfirmed configured readers during sleep (firmware).
- Document this hybrid build separately from the official release and add targeted RFID recovery/protocol regression coverage (`docs/RFID-3.13-PORT.md`, native test scripts).

- Send and validate the PN532 PowerDown response on each detected HSU reader during display sleep. Repaint the display and reinitialise readers on wake, retry incomplete wake once, preserve the requested reader configuration and skip wake NVS writes. Block standby during active RFID/inventory operations (firmware).
- Document current limits, powered module LEDs, installation offsets and the inherited upstream OTA channel (`docs/CUSTOM-3.5B.md`).
- Native fault-injection tests exercise the actual custom charge profile, PowerDown response checks, standby guards, wake retry and unsupported-board compilation rejection (`scripts/test-custom-power.py`, `scripts/verify.sh`).

## Added

- Separate local TigerPOD TEST screen, persistent UID-bound before/target journal,
  NTAG213/215/216 version and protection checks, strict blank policy, physical
  hold confirmation, alternating acknowledged RF fields, page verification,
  cancel/resume and restore (`pod_test.h`, `pod_policy.h`, firmware).

## Changed

- TEST build identity; disable legacy encoding/erasure entry points and gate all
  HSU writes to the local confirmed operation. Exclude marked TEST records from
  production weighing uploads. Preserve weighing, calibration, battery profile
  and reader recovery (`TigerTagSplashESP32.ino`).
- Retry test fixture explicitly enables the new low-level permission gate
  (`scripts/test-inventory.py`).

- Actual POD transaction fault-injection tests cover every interrupted page,
  lost acknowledgements, absent/swapped/same UID, memory/protection mismatch,
  corrupt journal, storage failure, cancel and missing physical confirmation
  (`scripts/test-pod.py`, `scripts/verify.sh`). Existing inventory fixture now
  checks the disabled automatic entry and low-level write gate.
- Document provenance, limitations, bench tests, flash backup and rollback
  (`docs/POD-TEST.md`, `README.md`).

## Fixed

- POD TEST r2: use InCommunicateThru (0x42, raw GET_VERSION 0x60) and validate
  response 0x43 instead of sending NTAG GET_VERSION through InDataExchange,
  which treats 0x60 as MIFARE authentication. Resynchronise UART after failure
  (`TigerTagSplashESP32.ino`).
- Show the failed selection/version/CC/protection/memory stage for each reader
  instead of the ambiguous "read failed" (`pod_test.h`).
- Test the actual GET_VERSION wire method with successful NTAG213/215/216
  replies and ACK, frame, response-code, length and status failures
  (`scripts/test-pod-transport.py`, `scripts/verify.sh`).

## Changed

- POD TEST r3 reports the reader, failed protection or blank check, page and
  four diagnostic bytes when New pair is refused. Distinguishes unrecognised
  header, residual user memory, static/dynamic lock, AUTH0, mirror and ACCESS.
  No blank policy or write-authorisation relaxation (`pod_test.h`, firmware).
- Actual prepare-path tests reject reformatted NTAG216 with residual high-page
  data and protected tags without writing NFC or creating a journal
  (`scripts/test-pod.py`).

## Fixed

- POD TEST r4 recognises the official TigerTag Init blank header 6C41A2E1 only
  when every subsequent user byte is zero. Production markers and Init with
  any residual product/data/signature/extended memory are still refused
  (`pod_policy.h`, `pod_test.h`, firmware, `docs/POD-TEST.md`).
- Exhaustively reject every nonzero byte after Init, reject production markers,
  and exercise prepare/write/verify/restore of an Init pair using actual POD
  functions (`scripts/test-pod.py`).

## Changed

- POD TEST r5 retains reader/page/stage/error through the final UI report,
  distinguishes missing journal and expired physical confirmation, and logs
  before/target/actual bytes on mismatches (`pod_test.h`, firmware).
- Consume all 16 bytes of each READ; inspect full capacity at preflight/final
  verification and the protected 144-byte record between page writes. Keep
  UID/protection checks, immediate page readback and the journal format.
- Test B's first-write failure and resume, read-call upper bound, last-block
  copy bounds, missing journal and confirmation timeout (`scripts/test-pod.py`).

## Changed

- POD TEST r6 keeps one reader's RF selection through its write block. Full
  checks run before confirmation, once per reader after confirmation, and at
  final verification; each changed page gets a fresh manufacturer-UID read,
  original/target page check, write and immediate readback. Header stays last,
  matching pages are skipped, and the other RF field remains off (`pod_test.h`).
- Preserve the NVS journal format and explicit physical confirmation; abort on
  lost/different UID, foreign/torn page, cancellation or readback error. Existing
  low-level retries continue to reselect and compare the UID before retrying.
- Log measured operation milliseconds, written/skipped pages and fresh UID
  checks. No wall-clock performance claim without hardware measurement.
- Add active-write fault injection for tag removal/substitution, unexpected
  page/extended-memory change and cancellation, plus RF/READ count bounds and
  no-op resume checks (`scripts/test-pod.py`).

## Added

- r7 isolated source variant: authenticated, certificate-verified Firestore plans
  for chipless inventory association, format, restore and reencode. Conditional
  atomic commits follow physical confirmation and full two-tag readback.
  Immutable checksummed LittleFS job plus separate production NVS journal; r6
  TEST journal and original r6 sources remain unchanged (`pod_cloud.h`,
  `pod_cloud_ca.h`, `pod_inventory.h`, `pod_test.h`, firmware).
- Pending production transactions block automatic weight synchronization until
  explicit recovery. The normal weight acquisition/filter path is retained.
- Firmware compilation and all physical validation remain for the user.

## Changed

- Add the r7 on-device menu and automatic chipless-spool selection after blank
  twin detection. Reuse the existing picker and weigh sampling; pause background
  cloud activity while coordinating NFC. Reject weight pushes during pending POD
  transactions (`pod_inventory.h`, firmware).
- Preserve new cloud weight on backup restore; reject conflicting metadata and
  mismatches between old tag records and their saved inventory. Support rollback
  of partial NFC writes before cloud commit. Keep a one-level completed backup.
- Secure inventory GETs with the bundled Google Trust Services root certificates.
- Add cloud/coordinator native tests; allow native regression dependencies to be
  supplied from an existing installation without building ESP32 (`scripts/`).
- Document source-only compilation, migration from r6 TEST tags, real-account
  bench tests, limitations and rollback (`docs/POD-INVENTORY-r7.md`, README).

## Fixed

- r7.1 aligns automatic blank detection with POD's accepted reformatted NTAG
  layouts: empty Lock Control TLV on 213/215/216, plus TigerTag Init followed by
  zeros. This is a discovery hint only; full-capacity, lock, UID and physical
  confirmation checks remain unchanged. Adds 237 native regression assertions.

## Added

- r8 guided French 480x320 TigerPOD screens, contextual scan, maintenance tools,
  progress and release-then-hold physical confirmation (`pod_ui.h`, `pod_inventory.h`).
- Shared UID links matching Studio's official rfidAliases schema, authenticated
  conditional creation and cross-scale resolution. Preserve existing TigerTags;
  never write maker tags. Move captured links with encoding/format plans and
  restore them with the journal (`pod_linked.h`, `pod_cloud.h`).
- Native shared-link, discovery, UART and actual LVGL rendering/confirmation tests.

## Changed

- POD-only UART ACK reception removes library polling delays; per-page identity,
  immediate readback, full memory checks, cancellation and alternating RF stay
  enabled. Hardware timing remains unmeasured (`pod_uart.h`, firmware, `pod_test.h`).
- Resolve account links before allowing weight synchronization. Reject mixed
  known/unknown or unrelated spool tags; recover after removing an unknown tag.
  Preserve acquisition, filtering, tare and calibration (`pod_discovery.h`, firmware).
- Document source-only installation, shared-link requirements, test protocol and
  rollback; preserve previous deliveries (`docs/POD-INVENTORY-r8.md`, README).

## Fixed

- Fresh UID bytes are checked during each full inspection, including final
  verification; a swapped tag cannot pass using the cached selection.
- Completed pair verification tolerates ordinary weight updates; pending plan
  preconditions remain exact. Timestamp and numeric weight dates are ordered
  consistently for restore/reencode (`pod_cloud.h`).

## Fixed

- r8.1 discovery submits a persistent request to the cloud worker instead of
  blocking the main loop with inventoryWait. Reject stale replies after UID,
  load or account changes; retry offline lookups silently (`pod_discovery.h`,
  `pod_linked.h`, firmware). Retain a placement cache across short RF dropouts;
  six-second discovery grace leaves the reader's 1.5-second dedup unchanged.
- Manual POD reads use the loading label; only commits/links use the saving
  label (`pod_inventory.h`). Weight sampling/filtering and NFC writes unchanged.

## Changed

- Display v3.13.0 fork custom r8.1 and expose the full custom build identity;
  document the historical hybrid provenance and source-only hardware tests.

## Changed

- r8.2 groups identity lookups with authenticated Firestore batchGet, masks the
  necessary fields and matches unordered results by document name. Fall back
  to existing individual GETs on incompatible or incomplete responses; never
  treat a failed read as absence (`pod_linked.h`, native cloud/link tests).
- Reduce discovery settle to 150 ms for two UIDs / 500 ms for one, and accepted
  read pauses to 150 ms. Preserve RF alternation, dedup, fresh-read authorization,
  weighing calculations and two-tag encoding. Log queued lookup milliseconds;
  no sub-second timing claim without hardware measurement (firmware,
  `pod_discovery.h`, discovery tests, docs/CORRECTIF-r8.2.md).

## Changed

- r8.3 separates the quick unknown-tag menu preview (16 user bytes, header and
  protection checks) from full-memory write preflight. Label the result as NTAG
  detection, not complete encodability (`pod_inventory.h`).
- Reuse a completed discovery for under three seconds only when account, UIDs
  and generation match. This is a menu hint, never write authorization
  (`pod_linked.h`, `pod_discovery.h`). Keep all r8.2 known-spool optimizations.
- Test bounded preview reads, complete preflight refusal on extended data,
  missing/duplicate tags, and stale/account/generation cache invalidation.

## Changed

- 1.0 custom identity; home TigerPOD entry, Gestion des tags subtitle, a single
  scan/rescan action, direct weighing return, and tools result/retry navigation
  (`TigerTagSplashESP32.ino`, `pod_inventory.h`, `pod_ui.h`).
- Fail closed at all OTA installation gates; clear upstream proposals. No custom
  feed is configured, so updates require manual USB (`TigerTagSplashESP32.ino`).
- Stream journal CRC, receive POD HTTP JSON in bounded PSRAM, send alias commits
  from file, release preparation documents before resuming, serialize container
  TLS requests, suspend/free live view and defer heartbeat during POD
  (`pod_crc.h`, `pod_http_memory.h`, `pod_cloud.h`, `pod_linked.h`, firmware).

## Added

- Memory admission checks, stage/heap/stack diagnostics and reset reason; native
  tests for legacy journal compatibility, allocation failures, retry recovery,
  actual LVGL buttons and custom OTA/memory gates (`scripts/test-*.py`).
- Source-only installation, test and rollback guide (`docs/CHANGES-djo-1.0.md`),
  README/release identity updates and inclusion of the custom native test in
  `scripts/verify.sh`. Hardware reboot cause remains unproven.

## Fixed

- 1.0 compilation hotfix: diagnostic function bodies now follow the early type
  declarations and the original first-function location. PlatformIO had inserted
  automatic prototypes before InventorySpool/BambuTag, causing undeclared-type
  errors. Add `scripts/test-ino-prototypes.py` using the installed converter and
  GCC preprocessing only; reproduce the failure on the initial 1.0 and verify
  the corrected source. No ESP32 compilation, linking or flash performed.

## Fixed

- Confirmed loopTask stack overflow in the user's 1.0 ELF (SHA256 starts
  8ffe3ef13264): r7New -> r7Resume -> r7Load. Reserve 32 KiB for loopTask via
  ARDUINO_LOOP_STACK_SIZE (previous default 8 KiB); print the actual core-reported
  stack allocation at boot. NFC, journal, cloud transaction and weight logic are
  unchanged. Add stack flag inheritance/diagnostic regression checks. The user
  recompiles; no new firmware build or hardware validation performed here.

## Changed

- 1.0 orientation update: capture production pairs in logical tag-index order,
  then bind journal slots to physical readers by fresh UIDs before resume/verify.
  Preserve tag indices, cloud reciprocity, full memory/protection checks and the
  immutable recovery journal. Restore aligns captures by saved UID, including
  formatted tags with no remaining index. Physical confirmation is unchanged;
  moving/swapping tags after binding still aborts (`pod_inventory.h`, `pod_test.h`).
- Reset reader mapping for raw scans/UID links and when leaving POD; diagnostics
  retain physical A/B labels while confirmation identifies logical tags 1/2, 2/2.
- Coordinator regression suite now passes 109 checks, including reverse updates,
  immutable-journal retry after rotation, restore, duplicate/missing/protected
  tags, cloud mismatch and movement after confirmation. No firmware build.

## 1.0

- Studio refresh offer ("Actualiser / Plus tard") for recognised pairs flagged `needUpdateAt`;
  refresh closes only after both tags are verified and the inventory is re-read.
- Memory gates split between menu opening and NFC/network operations, with the exact cause
  (total, contiguous block, PSRAM) in the message; remote-view pause/resume hysteresis.
- A new UID invalidates both readers' associations and stale lookup results; RFID icons are
  grey / green / blue (none / detected / confirmed association).
- "Format pair" erases two non-1/2+2/2 tags completely (UIDs absent from the inventory only);
  "refresh" reports the bytes read on an invalid pair.
- Memory: one worker task created at boot now runs the tag-DB sync, update check and manifest fetch (no more
  8-12 KB task stacks created and freed per run); Firestore calls share one TLS client and one HTTP client
  (pod_tls.h, keep-alive, idle socket closed before any other TLS user); `[MEM]` journal on the serial console
  every 30 s and at once when the largest internal block shrinks by 4 KB (`-DTIGERSCALE_MEM_LOG_MS=0` disables it).
- Memory: before refusing an operation, caches are freed and the heap re-measured; a persistent fragmentation
  failure offers "Redemarrer la balance", which restarts and reopens TigerPOD on a clean heap.
- Identity, documents and internal identifier prefixes renamed to 1.0. No ESP32 build, flash or test run.

- Interface: weigh screen and TigerPOD home follow the mockups; reader chips, TigerPOD tile icons and the Tiger System
  logo come from `ui_icons.h` (generated by `scripts/make-ui-icons.py` from `svg/ui/` and `assets/`); the TigerPOD
  button is orange; the Update row is hidden from Settings (`TigerTagSplashESP32.ino`, `pod_ui.h`, `pod_inventory.h`).
- i18n: all TigerPOD labels and status sentences translated in 11 languages (314 keys, `LangKey` now `uint16_t`);
  engine sentences are translated at display time by `r8Translate()` so the journal logic and tests keep the source text.
- Documentation: README, CHANGELOG, WHATSNEW, llms.txt, CLAUDE.md, AGENTS.md, docs updated for 1.0 and the VS Code /
  PlatformIO workflow; `docs/CHANGES-vs-official-3.13.0.md` added. Chinese subset fonts must be regenerated with
  `bash scripts/make-cjk-font.sh` before the font guard passes.
- Failure screens translate the engine message after its location prefix ("B orientation: ..."); the Tag A not-detected message had a missing space and never matched its translation (`pod_inventory.h`).

- Read tags lists only the UID and chip of each reader. Verify names the inventory spool and its remaining weight and, without a journal, compares each tag (signature, index, timestamp) with its inventory record; the resolve request now fetches the label fields (`pod_linked.h`, `pod_inventory.h`, 8 new i18n keys, 314 total).- Confirmation screens translated: the "Confirm this operation?" title, the restore / replace / test titles and the context lines (operation name, "link shared with Studio" note); spool labels and UIDs stay as they are (`pod_inventory.h`, `pod_ui.h`, 5 new i18n keys, 314 total).
- Repository identity: links, workflows, web installer, scripts and documentation now point to `djomaster/TigerScale-v3-TigerPOD-3.5B`; `.github/FUNDING.yml` (upstream sponsor button) removed; `.vscode/settings.json` ignored (machine-local path); OTA wording corrected (disabled in this build).
