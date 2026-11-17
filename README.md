> **TigerScale 3.13.0-djo.1.0 — first release of the custom 3.5B build (sources only).**
> Personal build for the **Waveshare ESP32-S3-Touch-LCD-3.5B** with two PN532 readers in HSU mode,
> built on the official 3.13.0 firmware, the 3.11.0 fork's native Bambu Lab tag support, quick battery
> charging, RFID reader sleep with the screen, and TigerPOD tag management.
>
> - What changed compared with the official firmware: [`docs/CHANGES-vs-official-3.13.0.md`](docs/CHANGES-vs-official-3.13.0.md)
> - This revision's notes and test protocol (French): [`docs/CHANGES-djo-1.0.md`](docs/CHANGES-djo-1.0.md)
> - Install, VS Code and PlatformIO workflow, limits: [`docs/CUSTOM-3.5B.md`](docs/CUSTOM-3.5B.md)
> - RFID changes ported from the official 3.12 / 3.13: [`docs/RFID-3.13-PORT.md`](docs/RFID-3.13-PORT.md)
>
> Only `esp32s3_hsu_b` is supported. Over-the-air update is disabled in this build; install over USB
> (PlatformIO `Upload` and `Upload Filesystem Image` in VS Code keep Wi-Fi, account and calibration).
> No ESP32 build or bench validation has been run for this release.
> The older revision notes (`CORRECTIF-r8.x`, `POD-INVENTORY-r7/r8`, `POD-TEST`) are historical.
> The parent-fork descriptions below also discuss boards and transports unsupported here.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)"  srcset="assets/logo-tigertag-head-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/logo-tigertag-head.svg">
    <img src="assets/logo-tigertag-head.svg" alt="TigerTag" width="260">
  </picture>
</p>

<h1 align="center">TigerScale V3+</h1>

<p align="center">
  <strong>This is a fork of https://github.com/TigerTag-Project/Tiger-Scale-V3<br>
  This firmware adds many features and fixes not in the official firmware.</strong><br><br>

  ## Added Features

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

<p align="center">
  <a href="https://cad.onshape.com/documents/ffd8ea5573c21ddec7180c6f">
    <img src="https://upload.wikimedia.org/wikipedia/commons/4/45/Onshape_logo_full.png?utm_source=commons.wikimedia.org&utm_campaign=index&utm_content=original" alt="Onshape" width="400">
    Remix the design in Onshape
  </a>
</p>
