# TigerScale-V3-TigerPOD+

> **TigerScale-V3-TigerPOD+ 3.13.0-djo.1.0 — first release (sources only).**
> A custom firmware for the **Waveshare ESP32-S3-Touch-LCD-3.5B** with two PN532 readers in HSU mode.

## Fork notice

- This is a fork of https://github.com/TigerTag-Project/Tiger-Scale-V3 (the official TigerScale V3 firmware, here rebased on its 3.13.0 reference).
- This is a fork of https://github.com/ijord/Tiger-Scale-V3 (the contributor fork "V3+", 3.11.0), from which the native Bambu Lab tag support, tag programming and the zero function descend.
- Project name: **TigerScale-V3-TigerPOD+**. The GitHub repository is `djomaster/TigerScale-v3-TigerPOD-3.5B`, because GitHub repository names cannot contain a `+`.
- Licence: MIT, unchanged. The original copyright notice is kept in [`LICENSE`](LICENSE).

## What this fork changes

Compared with the official 3.13.0 firmware (full list in [`docs/CHANGES-vs-official-3.13.0.md`](docs/CHANGES-vs-official-3.13.0.md)):

- **TigerPOD** tag management menu: read tags, new pair, update, verify against the inventory, resume, format, restore. Verify names the matching inventory spool and its remaining weight.
- **Native Bambu Lab tags** (from the ijord fork, extended): read, linked to an inventory spool, weight synced.
- **Tag programming and erasing**, and a **zero** function (from the ijord fork).
- **RFID**: the work of the official 3.12 / 3.13 ported back, reader PowerDown with HSU wake-up when the screen sleeps.
- **Power**: battery charge target 1000 mA, VBUS limit 1500 mA.
- **Interface**: redesigned weigh screen and TigerPOD home, new icons and the Tiger System logo, 314 translation keys in 11 languages.
- **Robustness**: one shared TLS session, one worker task, memory journal and gates.
- **Scope**: only `esp32s3_hsu_b` is supported. Over-the-air update is disabled; install over USB.
- No ESP32 bench validation has been run for this release.

More: [`docs/CHANGES-djo-1.0.md`](docs/CHANGES-djo-1.0.md) (revision notes and test protocol, French), [`docs/CUSTOM-3.5B.md`](docs/CUSTOM-3.5B.md) (install and workflow), [`docs/RFID-3.13-PORT.md`](docs/RFID-3.13-PORT.md). The older `CORRECTIF-r8.x`, `POD-INVENTORY-r7/r8` and `POD-TEST` notes are historical. The upstream descriptions below also discuss boards and transports unsupported here.

---

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)"  srcset="assets/logo-tigertag-head-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/logo-tigertag-head.svg">
    <img src="assets/logo-tigertag-head.svg" alt="TigerTag" width="260">
  </picture>
</p>

<h1 align="center">TigerScale-V3-TigerPOD+</h1>

<p align="center">
  <strong>Based on the ijord V3+ fork of https://github.com/TigerTag-Project/Tiger-Scale-V3<br>
  This firmware adds many features and fixes not in the official firmware.</strong><br><br>

  ## Features inherited from the ijord fork

- **Write TigerTags** Put blank NTAG21X tags on both sides of your spool. When you drop this spool in the TigerScale, it will see that it is a blank tag and will scan your inventory for any spools which are not already tagged. You can then use the on-screen UI to match the spool and inventory slot. After that, the tag is written to be a standard TigerTag. 
- **Read Bambu RFID tags** Drop a Bambu tagged spool in, and it will look up what it is and add it to your inventory. After that it will track that specific spool
- **Support for the JC3248W535C** A 3.5" display board which is highly comparable to the Waveshare.
- **Support for either polarity of load scale wiring**
- Improved auto Tare
- Improved scale filtering

</p>

<p align="center">
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue.svg" alt="Platform: ESP32-S3">
  <img src="https://img.shields.io/badge/Build-PlatformIO-orange.svg" alt="Build: PlatformIO">
  <img src="https://img.shields.io/badge/UI-LVGL%20v8.4-6c3.svg" alt="UI: LVGL v8.4">
  <img src="https://img.shields.io/badge/Languages-11-informational.svg" alt="11 languages">
  <a href="https://discord.gg/3Qv5TSqnJH"><img src="https://img.shields.io/badge/Discord-Join-5865F2.svg" alt="Discord"></a>
</p>


<p align="center">
  <a href="https://djomaster.github.io/TigerScale-v3-TigerPOD-3.5B/">
    <img src="assets/install-button.svg" alt="Install TigerScale from your browser" width="420">
  </a>
</p>


