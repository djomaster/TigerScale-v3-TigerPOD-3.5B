# v3.13 fork custom 3.5B — 1.0

Révision source du 15 novembre 2026, identité
`3.13.0-pod.1.1.0+fork3.11+3.5B+POD` (1.0.0 : `3.13.0-pod.1.0.0`), version affichée `V3 v3.13.0 - POD v1.1.0`.
Base : copie de la r8.3 custom, sans modification des livraisons précédentes.
Ce firmware reste le fork historique 3.11 avec le port RFID 3.13 et les ajouts
custom ; il ne s'agit pas d'un remplacement par le firmware public 3.13.

Sources uniquement : aucune compilation ESP32, aucun flash et aucune opération
sur le compte ou les tags réels n'ont été exécutés pour cette livraison.

## Changements de cette révision

- Mises à jour Studio : une bobine reconnue (deux tags, `tag_count` à 2) portant `needUpdateAt` déclenche « Mise à jour des tags disponible — Actualiser / Plus tard ». Actualiser exige les deux tags et la confirmation physique, puis relit l'inventaire avant de clore la demande. Plus tard conserve la demande sans la répéter tant que la même bobine reste posée.
- Mémoire : contrôles distincts pour l'ouverture du menu et pour les opérations NFC/réseau (seuils d'origine conservés). Les messages nomment la cause : mémoire interne totale, bloc contigu interne, PSRAM totale ou bloc contigu PSRAM. Le conseil de fermer la vue distante est supprimé.
- Vue distante : pause après 1 s sous le seuil, 4 s minimum en pause, reprise après 5 s de mémoire saine continue.
- Changement de bobine : un nouvel UID invalide l'association des deux lecteurs ; une réponse réseau n'est acceptée que si UID, compte et génération correspondent encore.
- Icônes RFID : gris sans tag, vert tag détecté non associé, bleu association confirmée ; retour au gris ou au vert au retrait ou au changement.
- « Formater la paire » : si les tags ne forment pas une paire 1/2 + 2/2, effacement complet des deux tags, accepté seulement pour des UID absents de l'inventaire.
- « Actualiser » : en cas de paire invalide, le message affiche les octets lus (`A=.. B=..`).
- Aucune compilation ESP32, aucun flash ni test natif n'a été exécuté sur cette révision.

## Interface

- Bouton **TigerPOD** sur l'accueil de pesée, accessible même sans tags.
  L'entrée TigerPOD des paramètres est supprimée.
- Sous-titre **Gestion des tags**, sans « Local ».
- Un seul **Scanner les tags** au départ, puis **Rescanner** après lecture.
- **Pesée** retourne directement à l'accueil de pesée.
- Dans Outils, les résultats d'Actualiser, Formater, Restaurer et Vérifier ont
  un bouton **Retour** vers Outils. Un échec ajoute **Réessayer** pour la même action.
- Si une écriture est inachevée, Réessayer reprend le plan sauvegardé de cette
  action. Il ne remplace pas silencieusement le journal par un nouveau plan.
  La confirmation physique reste obligatoire pour écrire.
- L'échec de capture A/B est expliqué par un message de positionnement du tag.
  Les autres diagnostics techniques restent disponibles dans le moniteur série.

## Mises à jour

**Depuis la 1.1.0 (3.13.0-pod.1.1.0)** : la mise à jour par Réglages > Mise à jour est rétablie sur le canal de ce dépôt (manifeste `version.json` du site Pages, fichiers des releases de ce dépôt uniquement, SHA-256 obligatoire, aucune vérification automatique, commande cloud `ota_update` refusée). Ce qui suit décrit la révision 1.0.0, où tout était bloqué.

Les propositions du fork d'origine, dont 3.11, sont désactivées. Aucun canal custom
compatible et validé n'existe actuellement : **toutes les installations OTA sont
bloquées**, y compris les appels web et les commandes distantes d'installation.
Le menu indique une mise à jour manuelle par USB. Cette version ne configure pas
un nouveau service de distribution. Un futur canal devra contrôler la carte,
l'identité custom, la révision et l'authenticité avant d'autoriser l'installation.

## Mémoire et diagnostic

- Calcul de la somme de contrôle des journaux au fil de la sérialisation, sans
  créer une seconde copie complète en String. Format et somme de contrôle restent
  compatibles avec les journaux r8.3.
- Réponses réseau POD et recherche groupée reçues dans un tampon PSRAM borné
  (192 Kio), avec refus sur dépassement, réponse incomplète ou allocation impossible.
  Le chargement paginé général de l'inventaire conserve son tampon PSRAM existant.
- Envoi des associations UID depuis un fichier temporaire plutôt qu'une grosse
  String en mémoire interne ; ce fichier n'est pas rejoué automatiquement.
- Libération des documents de préparation avant la reprise imbriquée d'une écriture.
- Requêtes des conteneurs A et B traitées par le même worker réseau.
- Pendant TigerPOD, suspension de la vue distante, fermeture de ses connexions
  et libération de ses tampons avant de continuer ; reprise à la sortie.
  Les tâches réseau de fond attendent ; les requêtes nécessaires au POD continuent.
- Contrôle avant entrée POD et requête inventaire POD : au moins 64 Kio internes
  libres, un bloc interne de 32 Kio, 1 Mio PSRAM libre et un bloc PSRAM de 512 Kio.
  Sinon, arrêt explicite sans lancer la nouvelle opération. Ces seuils prudents
  ne garantissent pas à eux seuls l'absence de panne mémoire.
- Traces `[POD MEM]` : étape, mémoire interne libre, plus grand bloc interne,
  PSRAM libre, minimum de pile restant rapporté par le système. Trace de démarrage
  `[POD BOOT] reset_reason=...`.

Les traces entourent l'entrée/sortie, la capture, les requêtes cloud et l'écriture.
Le redémarrage observé après « Load inventory » n'a pas été reproduit ici : une
pénurie mémoire est une piste étayée par les anciens logs, pas une cause prouvée.
Les allocations n'ont pas toutes disparu et une panne matérielle reste possible.

## Fonctions conservées

Pesée, tare, calibration, filtrage et mise à jour du poids ; reconnaissance en
arrière-plan et optimisations r8.2/r8.3 ; deux PN532 HSU avec RF alternée ; deux
UID distincts obligatoires pour encoder ; vérification complète avant/après
écriture, annulation entre pages et récupération par journal. Aucun contournement
pour accélérer une écriture au détriment des contrôles. Aucun délai RF ou cloud
maximal n'est garanti par cette livraison.

## Compilation et installation

1. Conserver la r8.3/r6 et la sauvegarde de la carte. Terminer ou restaurer toute
   opération en attente avec ses deux tags avant de changer de firmware.
2. Extraire cette archive dans un nouveau dossier. Ouvrir le dossier qui contient
   `platformio.ini`, et non son dossier parent `outputs`.
3. Compiler vous-même la variante 3.5B :

   ```sh
   pio run -e esp32s3_hsu_b
   ```

4. Flasher uniquement l'application suivant votre procédure habituelle.
   Ne pas utiliser d'effacement complet ni réinstaller LittleFS : ses journaux,
   la NVS, le compte, le Wi-Fi et la calibration doivent être conservés.
5. Vérifier l'identité 1.0 à l'écran et ouvrir le moniteur série à 115200 bauds.

## Protocole d'essai sur la carte

1. **Pesée** : zéro sans bobine, puis masse connue (par exemple votre référence
   255 g). Vérifier stabilité, tare et retour au zéro. Poser une bobine reconnue,
   vérifier la bonne bobine et son poids dans Studio. Répéter cinq fois.
2. **Navigation** : ouvrir TigerPOD depuis l'accueil sans tags ; un seul bouton
   Scanner les tags. Scanner une paire ; il devient Rescanner. Pesée revient
   à l'accueil. Vérifier l'absence d'entrée TigerPOD dans les paramètres.
3. **Actualiser** : utiliser une paire d'essai déjà associée, confirmer sur
   l'écran, attendre la vérification des deux tags et de l'inventaire. Répéter
   dix fois, dont quelques fois après ouverture de la vue distante. Celle-ci
   doit se déconnecter pendant TigerPOD et pouvoir se reconnecter après sortie.
4. **Retour et Réessayer** : pour chacun des quatre outils, provoquer un échec
   de capture en retirant B avant la préparation. Retour doit aller à Outils.
   Replacer B, Réessayer doit relancer la même action. Vérifier ne doit pas écrire.
5. **Reprise** : sur une paire sacrifiable, annuler entre deux pages, garder les
   mêmes tags dans la même position puis Réessayer/Reprendre. Vérifier la nouvelle
   confirmation physique et l'absence de succès avant la vérification complète.
   Ne pas créer une nouvelle opération pour contourner un refus de journal.
6. **Formatage/restauration** : sur cette paire d'essai seulement, formater puis
   restaurer ; contrôler les UID, la paire 1/2–2/2 et l'inventaire. Les règles de
   conflit doivent refuser des données cloud incompatibles plutôt que les écraser.
7. **Refus** : tag manquant, paire incorrecte, tag protégé et Wi-Fi coupé ne doivent
   jamais aboutir à un succès affiché ou à une association erronée. Garder les tags
   et le journal en cas d'écriture partielle.
8. **Mises à jour** : ouvrir Mise à jour. Aucune proposition 3.11 et aucune
   installation réseau ; le message doit indiquer USB/canal custom non configuré.
9. **En cas de redémarrage** : copier le log depuis avant l'action jusqu'au début
   du boot suivant, avec les lignes `[POD MEM]`, `[POD BOOT]`, le message de panic
   et toute backtrace. Noter l'action, la paire et l'état de la vue distante.

## Retour arrière

Terminer/reprendre ou restaurer d'abord l'opération inachevée avec les mêmes tags.
Reflasher ensuite l'application r8.3 conservée, sans effacer NVS/LittleFS. Les
journaux restent compatibles. Un retour au firmware précédent n'annule pas à
lui seul les modifications déjà effectuées sur les tags ou dans l'inventaire.
Conserver la r6 comme archive historique ; pour ce correctif, la r8.3 est la
référence directe de retour arrière.

## Vérifications effectuées hors matériel

Tests natifs du code réel avec périphériques simulés : inventaire/lecteur/poids
(3733), journal et cloud (111), coordinateur et reprises (68), liens partagés
(40), découverte (67), transactions et pannes NFC (2621), UART (29), transport
(30), zéro (81), port RFID (44), alimentation/carte (34), interface LVGL et
confirmation physique simulée (156), politique OTA/mémoire et câblage logiciel
(23). Total : 7037 contrôles. Les rendus LVGL ont également été inspectés.

Ces tests ne remplacent ni la compilation PlatformIO ni les essais RF, tactiles,
réseau et mémoire sur votre carte. Aucune performance ni disparition définitive
des redémarrages n'est déclarée validée sur matériel.

## Correctif de pile après diagnostic matériel

Le journal reçu confirme `Stack canary watchpoint triggered (loopTask)` après
`plan-saved`, avec une réserve minimale de pile de 196. Le fichier ELF de cette
compilation correspond exactement au SHA du crash (`8ffe3ef13264...`). Le décodage
montre `r7New -> r7Resume -> r7Load` : débordement pendant la relecture du journal.
Cette panne est distincte du refus d'une paire présentée dans l'ordre inverse.

`platformio.ini` réserve désormais 32 Kio à loopTask au lieu des 8 Kio par défaut
du coeur installé. Cela consomme 24 Kio internes supplémentaires ; les seuils de
contrôle mémoire POD sont conservés. La ligne `[POD BOOT] loop_stack_bytes=32768`
permet de vérifier que le nouveau binaire utilise cette allocation. Les valeurs
`stack` des traces restent un minimum historique depuis le démarrage de la tâche,
pas la quantité de pile libre exacte au moment de chaque message.

Recompiler normalement avec `pio run -e esp32s3_hsu_b`, puis flasher l'application
sans effacer NVS/LittleFS. Vérifier la nouvelle ligne de boot et refaire plusieurs
actualisations avec les deux tags, dans les deux sens de la bobine. Conserver le log
complet et observer une marge de pile nettement supérieure aux 196–276 précédents.
Si un journal reste en attente, utiliser Reprendre avec les mêmes tags et leur
orientation d'origine, sans formatage ni suppression du journal.

Le journal fourni montre déjà une reprise réussie après le redémarrage : deux
pages écrites, 70 inchangées ignorées, `nfc_ms=4063`, puis deux tags vérifiés et
inventaire synchronisé. Ce délai mesure la phase NFC, pas les contrôles initiaux,
la confirmation physique ou les requêtes cloud. La nouvelle allocation reste à
valider sur la carte après recompilation ; aucune compilation ESP32 effectuée ici.

## Actualisation indépendante du sens de la bobine

Les rôles 1/2 et 2/2 appartiennent aux UID, plus aux lecteurs A/B. L'actualisation
range la capture selon ces rôles puis relit les deux UID pour sélectionner le
lecteur correspondant à chaque entrée du journal. Aucun index n'est échangé sur
les tags ou dans l'inventaire. Les contrôles de paire réciproque, timestamp, contenu,
protection et compte restent obligatoires, ainsi que la confirmation physique.

Vérifier et Reprendre retrouvent aussi les deux tags dans les deux sens. Formater
et Restaurer utilisent le même mécanisme ; après formatage, la restauration se
base sur les UID sauvegardés plutôt que sur un index effacé. Les anciens journaux
restent inchangés et compatibles. L'ordre peut changer AVANT une nouvelle tentative,
jamais pendant une écriture : toute différence d'UID après sélection arrête l'action.
Le retour à la pesée et les scans ordinaires réinitialisent cette correspondance.

Essai après compilation et flash de l'application seulement :

1. Actualiser une paire d'essai dans le sens habituel ; attendre la validation
   des deux tags et de l'inventaire.
2. Retourner la bobine et recommencer. Vérifier que chaque UID conserve son index
   et son lien réciproque, puis contrôler une pesée et le poids dans Studio.
3. Vérifier la paire dans les deux sens. Annuler une actualisation entre pages,
   retourner la bobine une fois l'action arrêtée et utiliser Réessayer/Reprendre :
   une nouvelle confirmation physique doit précéder la reprise.
4. Sur tags d'essai, retirer un tag avant l'action ou présenter une autre paire :
   refus sans écriture. Ne pas contourner un refus de protection ou de journal.

109 contrôles du coordinateur passent, dont 41 nouveaux contrôles d'orientation
et de sécurité. Les tests restent simulés ; la compilation ESP32 et les essais
sur la carte sont à effectuer par l'utilisateur. La pile 32 Kio est conservée.

## Gestion de la mémoire (1.0)

- **Journal `[MEM]`** : toutes les 30 s sur la console série, et immédiatement quand le plus grand bloc interne
  perd 4 Ko ou plus. Champs : `int_free`, `int_largest`, `int_largest_min`, `int_low`, `psram_free`, `psram_largest`.
  Désactivable avec `-DTIGERSCALE_MEM_LOG_MS=0`.
- **Tâche de travail unique** : créée au démarrage, quand le tas est intact. Elle exécute la synchronisation des
  bases de tags, la vérification de mise à jour et la lecture du manifeste, qui créaient chacune une tâche à pile
  de 8 à 12 Ko. Si elle est occupée, l'appelant retombe sur son comportement d'avant (lecture du manifeste
  bloquante) ou réessaie à la boucle suivante.
- **Connexion TLS partagée** (`pod_tls.h`) : tous les appels Firestore avec certificat passent par un seul
  client TLS et un seul client HTTP, avec réutilisation de la connexion. Les autres usages TLS (jumelage,
  manifeste de mise à jour, import Bambu) ferment d'abord ce socket inactif. Un socket inactif depuis plus de 8 s
  est refermé avant réutilisation.
- Non compilé pour l'ESP32 et non testé sur la balance ; tests simulés passés (inventaire, cloud, coordinateur,
  liens, découverte, politique OTA/mémoire).
- **Seuil de bloc contigu des opérations : 28 Ko** (32 Ko avant). Mesure sur banc : les connexions TLS réussissent à
  31,7 Ko (plusieurs fois de suite) et échouent par manque de mémoire vers 24 Ko ; le seuil de 32 Ko refusait 31,7 Ko
  sans raison. Le total interne (64 Ko) et la PSRAM sont inchangés. Constante `intBlock` de `r86MemoryCheck`.
- La porte d'entrée de l'opération cloud ferme la session TLS inactive et patiente jusqu'à 1,5 s avant de refuser, pour
  ne pas bloquer la synchronisation après l'écriture des tags (le journal reste récupérable via Reprendre).
- **Paires anciennes sans `tag_index` / `tag_count`** : un couple de documents liés dans les deux sens (`twin_tag_uid`)
  avec le même `timestamp`, mais sans ces deux champs, est maintenant accepté par Actualiser / Formater la paire
  (la réécriture ajoute justement les champs manquants) et la balance propose l'actualisation demandée par Studio.
  Un couple à moitié renseigné ou contradictoire reste refusé, avec un message qui nomme la cause.
- La lecture du poids container est réessayée (4 fois) sur échec de transport au lieu d'afficher « -- g ».


## Interface (1.0)

Visual rebuild following the design mockups; same data, same actions, same touch handlers.

- Weigh screen: header row (status badge + name, status icons), a 448x204 card with the readers at both edges of the detection section, weight, brand row with material chip, right column Filament (amber) / Container / rack and position pills, and three icon buttons (Zero, TigerPOD, Settings).
- TigerPOD home: back arrow, title, two columns of three cards (Lire, Nouvelle paire, Actualiser, Verifier, Reprendre, Formater). Restaurer stays under Outils.
- Settings: palette and row radius only; layout not rebuilt.
- Settings: the Update row is hidden.
- Native Bambu tags linked to an inventory spool now fill brand, material and colour on the weigh screen from the inventory document (weight sync unchanged).
- Not compiled or flashed (no PlatformIO in the build sandbox). Check the weigh screen on the panel first.

## TigerPOD colour and translations (1.0)

- The TigerPOD button on the weigh screen is tiger orange (`0xF28C28`, border `0xFFB45E`, dark text) so it stands out from Zero and Settings.
- Every visible label of the TigerPOD menu (home, tools, blocked operation, Studio update prompt, buttons) now goes through `t(I18N_POD_*)` and follows the language chosen in Settings. 31 keys were added to `i18n.h` in all 11 languages (314 keys in total).
- Every TigerPOD sentence is now translated in the 11 languages (314 keys in total, `LangKey` widened to 16 bits because the table passed 255 entries). Fixed texts (confirmation screen, progress, results, buttons, memory notices) use `t(I18N_POD_*)` directly. The engine's status and error sentences are translated at display time by `r8Translate()` in `pod_inventory.h`, which maps 66 known sentences to `I18N_POD_M*` / `I18N_POD_X*` keys and rebuilds the memory refusal messages with their figures. The journal logic, the serial log and the host tests keep the original text.
- Still shown as is: raw diagnostics (HTTP codes, tag page errors, `pod_test.h` developer messages, the `A P03 phase:` error prefix) and the `POD_ST_*` scan hints, which are defined but not displayed.
- The new Chinese strings use 27 Han characters missing from the compiled CJK subset fonts. `scripts/check-ui-fonts.py` therefore fails until you run `bash scripts/make-cjk-font.sh` on your machine (it downloads the font; no network here). Chinese labels would show blank boxes until then.

## Icons from the mockups (1.0)

- `TigerTagSplashESP32/ui_icons.h` holds the interface icons as LVGL images, generated by `scripts/make-ui-icons.py` from `svg/ui/*.svg` (the mockup's stroke icons) and `assets/tigersystem_icon.svg`. Regenerate with `python3 scripts/make-ui-icons.py > TigerTagSplashESP32/ui_icons.h` (needs ImageMagick and Pillow, development time only; never edit the header by hand).
- Reader chips on the weigh screen: 30 px rounded box with the three-arc antenna icon, border, tint and icon coloured by state (grey = no tag, green = tag read, blue = linked to an inventory spool). The old drawn circular antenna and the red cross are gone.
- TigerPOD button: the Tiger System logo (full colour, rounded square) in front of the caption, on the orange button.
- TigerPOD menu tiles: Read tags, New pair (link), Refresh (two arrows), Verify (shield and check), Resume (play) and Format (eraser) use the mockup icons; the highlighted tile is tinted orange.
- The 8-bit alpha icons are recoloured at run time (`lv_obj_set_style_img_recolor`), so one bitmap serves every state. Not shown on a real panel yet: judge size and colour on the balance.

## Read tags and Verify (1.0)

- **Read tags** only lists what is on the readers: for A and B the UID and chip type. No inventory status, verdict, spool or weight. Nothing is written.
- **Verify** is a real check of the pair on the scale and names the inventory spool each tag belongs to (brand, material, colour, remaining grams, in orange under the UID):
  - a saved journal that describes exactly these two tags (either order) gets the full NFC + inventory check as before;
  - otherwise the two tags are read and compared with the inventory records of their UIDs: production signature, tag index (1/2 or 2/2) and the timestamp stored on the tag against the inventory timestamp. A difference (for example the spool was edited in Studio) is named per tag with the advice to use Update tags;
  - linked tags (a maker tag attached to a spool) have no comparable content: the spool is named and the report says so.
  Not compared: colour, weight and other fields encoded in the tag.
  The report shows one line per tag with each check and its result (`Signature OK   Index OK   Date OK`, green when all pass, red with an `X` on the failing one), then the verdict.
- The spool label comes from the inventory fields `manufacturer` / `brand` / `id_brand`, `material` / `id_material`, `color_name` / `name` / `message` and `weight_available`; those fields were previously filtered out of the resolve request, which is why an earlier build showed an empty name and `(0 g)`.
- Failure reports keep their location prefix (`B orientation:`) and translate the message that follows.
