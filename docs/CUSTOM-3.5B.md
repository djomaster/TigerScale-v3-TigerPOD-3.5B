# TigerScale-V3-TigerPOD+ - V3 v3.13.0 - POD v1.2.0 - Waveshare 3.5B et 3.5

Cette copie inclut le portage RFID de l'officielle 3.13.0. Lire aussi
[les changements RFID et la migration](RFID-3.13-PORT.md).

Cette copie du fork ijord cible uniquement la **Waveshare
ESP32-S3-Touch-LCD-3.5B** et deux PN532 en HSU/UART. L'environnement unique est
`esp32s3_hsu_b`. Les cartes 3.5 sans B et Guition, et les transports SPI/I2C,
sont refuses a la compilation. Leurs branches et documents herites sont conserves
comme references du fork parent, sans support dans cette edition.

## Interface et TigerPOD (1.0)

Version `3.13.0-pod.1.2.0`. La 1.0.0 apportait : interface refaite d'apres les maquettes, menu TigerPOD traduit dans les 11 langues,
icones des lecteurs et logo Tiger System generes dans `ui_icons.h`. Details, protocole d'essai et retour arriere :
[CHANGES-1.0.0.md](CHANGES-1.0.0.md) ; comparaison avec l'officielle :
[CHANGES-vs-official-3.13.0.md](CHANGES-vs-official-3.13.0.md). La mise a jour en ligne se fait depuis Reglages > Mise a jour (depuis la 1.1.0).

## Choisir la carte (3.5B ou 3.5)

Depuis la 1.2.0, deux environnements sont proposés. Se fier à la serigraphie de la carte :

| Carte | Environnement | Etat |
|-------|---------------|------|
| Waveshare ESP32-S3-Touch-LCD-3.5**B** | `esp32s3_hsu_b` (par defaut) | verifiee sur banc |
| Waveshare ESP32-S3-Touch-LCD-3.5 (sans B) | `esp32s3_hsu` | compile, non verifiee sur banc |

```bash
bash scripts/flash.sh --fs                    # 3.5B
bash scripts/flash.sh --fs --env esp32s3_hsu  # 3.5 sans B
```

L'image d'une carte sur l'autre donne un ecran noir sans message : les deux cartes ont le meme
cablage mais pas le meme controleur d'ecran (AXS15231B en QSPI sur la B, ST7796 en SPI sur l'autre).
La mise a jour en ligne choisit toute seule la bonne image grace a l'identifiant de carte du
manifeste (`3.5b` ou `3.5`).

## Validation sur matériel réel

Les tests de la version `3.13.0-pod.1.1.0` ont été validés sur une vraie balance
Waveshare 3.5B, selon la confirmation du propriétaire du 19 novembre 2026.
Cette mise à jour documentaire ne comporte aucune nouvelle compilation ni aucun nouvel essai.

## Batterie et veille

- Batterie utilisateur : LiPo 3000 mAh. Consigne de charge : **1000 mA**, environ
  **0,33C**. Limite de courant d'entree VBUS : **1500 mA**. Le courant reel varie
  selon la charge du systeme, la batterie et la regulation du PMIC.
- Les registres 0x16 et 0x62 de l'AXP2101 sont modifies par lecture-modification-
  ecriture et relus pour verification. La tension de charge et les protections
  thermiques ne sont pas modifiees. Le log doit afficher `verified=yes`.
- En veille d'ecran, chaque lecteur detecte recoit PowerDown avec reveil HSU.
  La reponse complete `D5 17 00` est verifiee avant d'annoncer `accepted`.
  Reference : [NXP UM0701-02, section 7.2.11](https://www.nxp.com/docs/en/user-guide/141520.pdf).
- Au reveil tactile, distant ou par changement de poids, l'ecran est repeint,
  puis les lecteurs sont reinitialises. Une detection incomplete est retentee
  une fois. Les reglages RFID ne sont pas reecrits en NVS a chaque reveil.
- La veille ne s'engage pas pendant un test RFID ou une operation d'inventaire
  en cours. L'ESP32 et le Wi-Fi restent actifs.
- Le VCC des modules reste alimente : les LED rouges peuvent rester allumees.
  Une reduction effective du courant doit etre mesuree sur le montage.

La programmation/effacement des tags, l'import Bambu, la pesee et l'extinction
depuis les parametres proviennent du fork parent et sont conserves.

## Installation USB en conservant les reglages

Depuis ce dossier, avec PlatformIO installe et la carte branchee :

```sh
bash scripts/flash.sh --env esp32s3_hsu_b --fs
```

Cette commande installe aussi l'interface web du fork. Ne pas demander
d'effacement complet pour conserver le Wi-Fi, la session et la calibration.
Le dossier de livraison `flash-3.5B/` regroupe ces fichiers. Les images de
compilation sont dans `.pio/build/esp32s3_hsu_b/` ; `boot_app0.bin` est fourni
par le framework ESP32 et copie dans le dossier de livraison :

| Fichier | Adresse USB |
|---|---|
| bootloader.bin | 0x0000 |
| partitions.bin | 0x8000 |
| boot_app0.bin | 0xE000 |
| firmware.bin | 0x10000 |
| littlefs.bin | 0x810000 |

Un flash USB a ces adresses epargne la NVS. `firmware.factory.bin` est une image
fusionnee : l'utiliser a 0x0000 efface les reglages contenus dans sa plage.

Premiere installation : par USB avec PlatformIO ou par la page d'installation. Ensuite, Reglages > Mise a jour
verifie le manifeste du depot (aucune verification automatique) et installe uniquement les fichiers publies
dans les releases de ce depot, avec controle SHA-256. Flasher un firmware officiel remplace les
personnalisations ; conserver ces sources.
Le depot GitHub est https://github.com/djomaster/TigerScale-v3-TigerPOD-3.5B.

La compilation et les tests logiciels ne valident pas la carte reelle. Verifier
les deux lecteurs avant et apres veille, le log PMIC et la charge sur batterie.

## Verification logicielle

Le firmware 3.5B et l'image LittleFS sont verifies avec les suites natives du
fork et les tests supplementaires du portage RFID : inventaire/lecture/ecriture,
selection de bobines, filtrage et zero, charge/veille, restrictions de carte,
trames UART et recuperation des lecteurs.
Les tests de charge et de veille utilisent des composants simules ; ils ne
mesurent pas le courant reel et ne remplacent pas l'essai sur la balance.

## Working in VS Code with PlatformIO

Open the folder that contains `platformio.ini` in VS Code (File > Open Folder) with the PlatformIO IDE extension installed. The only environment is `esp32s3_hsu_b`.

- Build: PlatformIO sidebar > `esp32s3_hsu_b` > General > Build, or `pio run -e esp32s3_hsu_b` in the PlatformIO terminal.
- Flash firmware: `Upload`. Flash the web interface: `Upload Filesystem Image`. Neither erases NVS (Wi-Fi, account, calibration). To flash with the script instead: `bash scripts/flash.sh --env esp32s3_hsu_b --fs`.
- Serial monitor: `Monitor` (115200). Look for the `[MEM]`, `[META]` and `[POD ...]` lines.
- `.vscode/c_cpp_properties.json` and `launch.json` are generated by PlatformIO and contain absolute paths of the machine that created them. Delete them and let PlatformIO regenerate them (PlatformIO: Rebuild IntelliSense Index) after moving the folder.
- After adding or changing a Chinese string, regenerate the CJK subset fonts in the PlatformIO terminal (network needed): `bash scripts/make-cjk-font.sh`. Then run `bash scripts/verify.sh --fix`.
