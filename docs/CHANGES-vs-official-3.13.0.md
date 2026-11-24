# Changes compared with the official TigerScale V3 3.13.0

Project: **TigerScale-V3-TigerPOD+**. A fork of https://github.com/TigerTag-Project/Tiger-Scale-V3 and of https://github.com/ijord/Tiger-Scale-V3.

Firmware identity: `3.14.0-pod.1.2.1` (build `3.14.0-pod.1.2.1+fork3.11+3.5B+POD`).

This build is not an upstream rebase. It has three layers:

1. **Official 3.13.0** — the starting reference for this comparison.
2. **Contributor fork 3.11.0** (ijord, "V3+") — native Bambu Lab tag reading, tag programming and erasing, and a zero function. It is the code line this build actually descends from.
3. **Custom work** — fast battery charging, RFID reader sleep with the screen, the RFID work ported back from official 3.13.0, TigerPOD, and the new interface.

Status: current version 3.13.0-pod.1.1.0. Hardware tests for 3.13.0-pod.1.1.0 were confirmed as validated on a real Waveshare 3.5B scale by the owner on 2026-11-19. This records the owner’s confirmation; no additional tests were run for this documentation update. Statements below come from the diff against the official folder and from the revision notes in `docs/`; where something is an inference, it says so.

## Summary

| Area | Official 3.13.0 | This build |
|------|-----------------|------------|
| Boards / transports | 3.5B and 3.5, HSU / SPI / I2C builds | 3.5B and 3.5, 2 PN532 readers in HSU. Guition and the SPI / I2C transports are refused at compile time |
| Native Bambu Lab tags | not supported | read, linked to an inventory spool, weight synced, spool shown on the weigh screen |
| Tag programming / erasing | not present | present (from the fork) |
| TigerPOD tag management | not present | new menu: read, new pair, refresh, verify, resume, format, restore |
| Battery charging | default | charge target 1000 mA, VBUS limit 1500 mA |
| RFID reader power-down | silence command fixed | PowerDown with HSU wake-up when the screen sleeps |
| Over-the-air update | enabled, automatic check, remote command | manual only (Settings > Update); this repository's release assets only, SHA-256 required; no automatic check, no cloud-triggered install |
| Interface | stock LVGL layout | redesigned weigh screen and TigerPOD home, new palette |
| Memory handling | stock | one shared TLS session, one worker task, `[MEM]` journal, memory gates |
| Version identity | `3.13.0` | `3.13.0-pod.1.1.0` |

## 1. Hardware scope and build

- Supported environments: `esp32s3_hsu_b` (Waveshare ESP32-S3-Touch-LCD-3.5B, bench-verified) and `esp32s3_hsu` (the 3.5 without the B, added in 3.13.0-pod.1.2.0, compiles, not bench-verified), both with two PN532 over UART. The Guition board and the SPI and I2C transports are rejected at compile time. Their branches stay in the source as inherited references, unsupported.
- `platformio.ini` reduced to that environment, with `-DARDUINO_LOOP_STACK_SIZE=32768` because POD journal parsing runs on the loop task and the default 8 KiB overflowed.
- `include/lv_conf.h`: `LV_USE_LARGE_COORD` set to 1, because inventory rows can exceed the 8191 px small-coordinate limit.

## 2. Power and sleep

- Charge target 1000 mA (about 0.33C on a 3000 mAh LiPo); VBUS input limit 1500 mA. Done by read-modify-write on AXP2101 registers 0x16 and 0x62 and read back; the log must show `verified=yes`. Charge voltage and thermal protections are untouched.
- When the screen sleeps, each detected reader receives PowerDown with HSU wake-up, and the full `D5 17 00` reply is checked before reporting `accepted`.
- On wake (touch, remote, or weight change) the screen is repainted, then the readers are re-initialised; an incomplete detection is retried once. RFID settings are not rewritten to NVS on every wake.
- Sleep is not entered during an RFID test or an inventory operation. The ESP32 and Wi-Fi stay on, and the module power rail stays on, so the red LEDs may stay lit. Actual current savings must be measured on the hardware.

## 3. RFID (ported from official 3.12 / 3.13)

- 16-level reader power scale and Settings > RFID > Auto, which favours the weaker reader and confirms the best levels. Old levels 0–4 keep their values; the fork's old level 5 becomes 15. A unit with no stored setting starts at level 7.
- Three initialisation attempts after a reset sequence without zeroing the chip between attempts. A reader that fails the boot probe is "unconfirmed", not absent: it is polled once a second and confirmed by a physical tag read, including blank and Bambu tags.
- An explicit single-reader selection is respected; a failed boot probe no longer turns it into a one-reader configuration.
- Field isolation during reads, writes and imports is kept; antenna on/off commands now control the real RF field, and an unconfirmed reader also drops its field while its neighbour reads.
- UART read budget 100 ms with two activation retries; variable-length frames from the fork are kept for NTAG and Classic UIDs. Configuration commands have a wider timeout and resynchronise after an error.
- The network-name fix of official 3.13.0 is present: `macSuffix4()` reads the hardware address from the eFuse (`esp_read_mac`), so the mDNS name is `tigerscale-XXXX` per unit.

## 4. Bambu Lab native tags (from the fork, extended)

- Original Bambu tags are read and can be linked to a spool in the Tiger Studio inventory through an alias record (`rfidAliases`), without writing anything to the tag.
- When a linked native tag is on the scale, the weight update goes to the matching inventory spool, and (1.0) the weigh screen now shows that spool's brand, material and colour, taken from the inventory document. TigerTag data still takes priority when present.
- Bambu import from the fork is kept, with a generated catalogue (`bambu_catalog.h`, `scripts/generate-bambu-catalog.py`).

## 5. Tag programming and TigerPOD

- Programming and erasing blank tags (fork feature) is kept, with extra confirmation and a journal so an interrupted write can be resumed or restored.
- TigerPOD, new in this build (files `pod_*.h`):
  - Read tags, New pair, Refresh tags, Verify, Resume, Format pair; Restore is under Tools.
  - Automatic spool recognition runs asynchronously and tolerates short RF dropouts.
  - Refresh handles Twin pairs, including older documents without `tag_index` / `tag_count`, and ignores the server-stamped `syncAt` when comparing.
  - A linked (alias) tag can serve as a shared identifier without rewriting tags.
  - Studio's refresh request is offered on the scale; "later" is remembered per pair.
  - Error messages name the cause (incoherent pair, data that cannot be encoded, memory refusal) and the serial log carries `[POD ...]` diagnostics.

## 6. Memory and network robustness

- One shared TLS session (`pod_tls.h`) for every certificate-checked Firestore call, instead of overlapping sessions.
- One worker task created once at boot while the heap is whole.
- `[MEM]` journal on the serial console every 30 s and on heap drops.
- Separate memory gates for opening the menu and for NFC/network operations; the contiguous-block threshold for operations is 28 KB, with a retry before refusing. Messages name the cause.
- Container weight lookup retries on transport failure instead of showing "-- g".
- Known limit: internal-RAM fragmentation can still produce `SSL - Memory allocation failed (-32512)` when several sessions overlap during weighing. Moving mbedTLS buffers to PSRAM was recommended but not done.

## 7. Interface

- Weigh screen rebuilt following the design mockups: status badge and name in one row, a 448×204 card with both readers at the edges of the detection section, weight, brand with material chip, Filament in amber, Container, rack and position pills, and three icon buttons (Zero, TigerPOD, Settings).
- TigerPOD home as a grid of six cards with a back arrow.
- New palette (`LVCOL_BG`, `LVCOL_CARD`, `LVCOL_BORDER`, `LVCOL_MUTED`); rounder corners in Settings rows.
- Icons come from `ui_icons.h` (generated by `scripts/make-ui-icons.py` from the mockup SVGs): reader chips coloured by state, six TigerPOD tile icons, and the Tiger System logo on the orange TigerPOD button.
- Weight uses the 40 px font (largest available), smaller than the 68 px in the mockup.
- 317 interface string keys in `i18n.h` (official: 120) across the 11 languages: inventory, tag writing, Bambu import, zero, tag pair messages and the full TigerPOD menu. `LangKey` is 16 bits because the table passed 255 entries. The CJK font subset differs from the official one; the reason is not checked here, most likely it was regenerated for these strings.

## 8. Weighing and web interface

- Zero function and a "zero required" state from the fork.
- Calibration factor: either polarity is accepted, it only has to be finite and nonzero (official refuses negative values). This matches a swapped load-cell wire.
- Weighing state, auto-tare and weight computation rules are unchanged.

## 9. Over-the-air update

- From 3.13.0-pod.1.1.0 the Settings > Update screen works again, against this repository's own manifest (`https://djomaster.github.io/TigerScale-v3-TigerPOD-3.5B/version.json`). Installing a standard release would replace the customisation, which is why the channel points here only.
- Only URLs under this repository's GitHub release downloads are accepted, and a SHA-256 is mandatory, in the screen path, the local `/api/ota/update` and `otaApply()` itself.
- No automatic check (`TIGERSCALE_OTA_AUTOCHECK` is 0): the owner opens Settings > Update.
- The Firestore command `ota_update` is refused: an update cannot be started from the cloud.
- Version scheme: `<official base>-pod.<POD version>`, so a scale on 3.13.0-pod.1.0.0 is offered 3.13.0-pod.1.1.0.

## 10. Tooling and documentation

- Added tests: `test-pod-cloud`, `test-pod-inventory`, `test-pod-linked`, `test-pod-discovery`, `test-pod-transport`, `test-pod-uart`, `test-pod-ui`, `test-custom-r86`, `test-custom-power`, `test-rfid-313`, `test-inventory`, `test-picker`, `test-zero`, `test-ino-prototypes`.
- Added `scripts/make-ui-icons.py` and `svg/ui/*.svg` for the interface icons; VS Code / PlatformIO workflow documented in `docs/CUSTOM-3.5B.md`.
- Version scheme: `<official base>-pod.<POD version>`, shown as "V3 v3.13.0 - POD v1.0.0". The base follows the official firmware (3.13.0); the POD version is this fork's own and starts at 1.0.0.
- Per-revision notes live in `docs/` (CUSTOM-3.5B, RFID-3.13-PORT, POD-INVENTORY, CHANGES-1.0.0 and the older correction notes), plus `SHA256-SOURCES.json` for source hashes.

## 11. Not carried over, or unchecked

- Official docs `docs/RFID-PERFORMANCE.md` and `docs/release-notes/v3.12.0.md` are not in this build.
- The `docs/release-notes/v3.13.0.md` file is rewritten for this build, so it no longer describes the official release.
- Differences exist in `.github/workflows`, `web-installer/`, the official `CHANGELOG.md`, `CONTRIBUTING.md`, `AGENTS.md`, `CLAUDE.md`, `scripts/make-manifest.py`, `scripts/verify.sh` and some logo files. I did not review them line by line. The web-installer and workflows are not meant to be used for this build.
- Three checks of `verify.sh --quick` (`check-device-names`, `check-emoji`, `check-file-format`) could not run in the delivery folder because it is not a git repository; run `bash scripts/verify.sh --all` in the real repository.
