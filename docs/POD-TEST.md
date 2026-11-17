# TigerScale + TigerPOD TEST r6 — Waveshare 3.5B seulement

> Document historique (r6). Etat actuel : `CHANGES-djo-1.0.md` et `CUSTOM-3.5B.md`.

## Base identifiée et sauvegardée

Projet local « ESP32-S3 TigerScale v3 ». Dernière livraison retrouvée dans le
chat « Adapter TigerScale au modèle 3.5 », dernière demande : conserver le fork,
les modifications de batterie et les améliorations RFID de 3.13.0.
Dossier source : Tiger-Scale-V3Plus-3.11.0-custom-3.5B-RFID-3.13.
Identité embarquée originale : V3Plus-3.11.0-3.5B-RFID-3.13-r1.
Identité TEST : V3Plus-3.11.0-3.5B-RFID-3.13-POD-TEST-r6.

Il s'agit du fork ijord 3.11.0 personnalisé avec portage RFID 3.13, PAS de la
version officielle 3.13. Aucun dossier .git dans cette base : aucun commit ou
nom de branche ne peut être certifié. La révision locale exacte est fixée par
le manifeste base-sha256.json et l'archive BASE-ORIGINALE-3.5B.zip.
SHA-256 du fichier .ino original :
0c93b8487016cbf312d96c8ba520789d44dd7d22aeb71654abc0c0844c40ced6

La conversation « Comparer ACR122U PN532 » ne contient aucune pièce jointe.
Les sources et le pack flash ont été retrouvés dans le projet local et recoupés
avec la dernière livraison du chat du projet. Aucun remplacement par un dépôt
public. L'archive originale inclut les binaires livrés précédemment ; elle ne
constitue PAS une sauvegarde de la mémoire de votre appareil.

## Fonctionnement du prototype

Paramètres > TigerPOD TEST. Les lecteurs restent en HSU/UART, câblage inchangé.
A = rfid1 (droite dans le fork), B = rfid2 (gauche) : vérifier le repérage réel.
La pesée est conservée en mode normal. Pendant l'écran POD, son traitement est
suspendu ; retour à la pesée à la sortie et réinitialisation des lecteurs.
Charge 1000 mA, limite USB 1500 mA, veille et profils RFID conservés.

- Read : lecture des UID, type/capacité et mémoire ; index et association dans
  le journal série. Aucune écriture. Les tags protégés contre la lecture peuvent
  être refusés ; aucune tentative de mot de passe.
- New pair : précontrôle des DEUX tags vierges et création du journal local.
  Nécessite une heure valide (NTP du firmware existant). La préparation n'écrit
  aucun tag. Maintenir physiquement la zone « HOLD 2s » en bas à droite 2 secondes après
  avoir relâché l'écran. Les UID et l'identifiant Twin sont affichés.
- Resume : écrire ou reprendre la paire journalisée, avec NOUVELLE confirmation
  physique. Les boutons réseau simulés ne peuvent pas donner cette permission.
- Verify : vérifier les deux UID, toutes les données et la paire journalisée.
- Cancel : interrompre entre opérations ; un WRITE déjà envoyé peut se terminer.
- Restore : restaurer les données vierges sauvegardées des deux mêmes tags,
  avec confirmation physique, puis supprimer le journal après vérification.
- Exit : retour à la pesée. Aucun effacement automatique du journal.

Une transaction à la fois, conservée dans une nouvelle zone NVS « pod-test ».
Après succès, Restore permet de réutiliser les deux tags. Pour une nouvelle paire
sans restaurer l'ancienne : présenter l'ancienne paire vérifiée, New pair, puis
échanger les tags contre les nouveaux pendant la confirmation « Replace ».
Le journal de l'ancienne paire est remplacé uniquement après préparation validée
et confirmation de la nouvelle. La paire ancienne n'est alors plus restaurable
par ce journal.

L'encodage est un enregistrement TigerTag standard NON certifié : produit
inconnu, champs de test, message « TIGERPOD TEST LOCAL », même identifiant Twin
à l'offset 32 (page 0x0C), index 0x12 / 0x22 à l'offset 39 (page 0x0D).
Les UID matériels restent distincts et ne sont jamais programmés.
Les données de test ne représentent pas un filament réel. Ne pas les importer
avec Tiger Studio dans votre inventaire de production.

Le prototype exclut les tags portant ce message des remontées de pesée. Aucune
association Firestore n'est créée : Twin Tags est ici une association locale,
sur les tags et dans le journal. Aucune modification serveur ou Studio requise.
Le Wi-Fi et les fonctions existantes de la balance ne sont pas supprimés.
Il n'y a aucun nouveau point d'accès réseau d'écriture NFC. Les anciennes
entrées d'encodage automatique/effacement sont désactivées dans ce TEST ; toutes
les écritures HSU sont bloquées hors de l'opération locale confirmée.

## Précautions mises en œuvre et limites

GET_VERSION distingue NTAG213/215/216. Contrôle CC, verrouillages statiques et
dynamiques, AUTH0, configuration miroir/protection ; refus si lecture incertaine.
Définition volontairement stricte de « vierge » : mémoire utilisateur entièrement
nulle ou en-tête NDEF vide reconnu avec TOUS les octets restants nuls. Certains
tags neufs contenant des données d'usine seront donc refusés. Aucun effacement
préalable automatique et aucune écriture de mot de passe, CC, verrouillage ou UID.
Les pages écrites sont limitées à 0x04–0x27 (144 octets). Aucune signature créée.

Le champ du lecteur voisin doit confirmer OFF avant activation. Échec OFF :
arrêt et reset matériel des deux modules. UID revérifié avant chaque page ;
relecture de chaque page écrite, puis vérification complète des deux tags.
En-tête écrit en dernier. Un tag seul réussi n'est jamais annoncé comme paire
valide. Les écritures ne sont pas atomiques entre deux puces.

La reprise accepte uniquement des pages identiques à leur image originale ou
à leur cible enregistrée. Une page déchirée/inconnue est mise à l'écart : Resume
ET Restore refusent de la modifier. Conserver les tags et le journal pour
analyse ; ne pas les confondre avec une paire de production. Un journal absent
ou corrompu ne permet pas de reprendre des tags partiellement écrits.
Après coupure, aucune reprise automatique : ouvrir POD puis Resume/Restore.
Le premier test est volontairement lent : contrôles mémoire répétés à chaque
page, surtout sur NTAG216. Garder les tags immobiles pendant toute l'opération.

Compilation et tests simulés ne prouvent pas la fiabilité RF, le tactile réel,
la persistance lors d'une vraie coupure, les performances ou la précision de
pesée. Aucun appareil n'a été flashé pendant la préparation de cette livraison.

## Installation et sauvegarde réelle de l'appareil

1. Retirer toutes les bobines et tous les tags. Noter calibration, unité, zéro,
   configuration des deux lecteurs et niveau RF. Brancher en USB avec une
   alimentation stable. Fermer le moniteur série pendant le flash.
2. Installer/utiliser esptool. Les commandes ci-dessous correspondent à esptool
   5 ; remplacer PORT par le port USB de la carte (ex. /dev/cu.usbmodem...).
3. Faire DEUX lectures de toute la flash et comparer leurs SHA-256 avant
   d'écrire. Ne pas démarrer l'application entre ces lectures : mode bootloader.
   Garder cette sauvegarde privée (elle contient Wi-Fi et session cloud).

```sh
python3 -m esptool --chip esp32s3 --port PORT --after no-reset read-flash 0 0x1000000 appareil-avant-A.bin
python3 -m esptool --chip esp32s3 --port PORT --after no-reset read-flash 0 0x1000000 appareil-avant-B.bin
shasum -a 256 appareil-avant-A.bin appareil-avant-B.bin
cmp appareil-avant-A.bin appareil-avant-B.bin
```

4. Vérifier la carte 3.5B et la flash de 16 Mo. Comparer la table de partitions
   de l'appareil au partitions.bin fourni avant d'utiliser les adresses fixes :

```sh
python3 -c 'from pathlib import Path; p=Path("appareil-avant-A.bin").read_bytes(); q=Path("partitions.bin").read_bytes(); assert len(p)==0x1000000; assert p[0x8000:0x8000+len(q)]==q, "Table differente : ARRET"'
```

5. Depuis TEST-3.5B, contrôler SHA256SUMS.txt. Pour cette mise à jour d'une base
   identique, conserver bootloader, partitions, NVS et LittleFS ; écrire seulement
   l'application et la sélection OTA app0. Ne pas utiliser erase-flash.

```sh
shasum -a 256 -c SHA256SUMS.txt
python3 -m esptool --chip esp32s3 --port PORT write-flash 0xE000 boot_app0.bin 0x10000 firmware.bin
```

Les autres images sont fournies pour référence ; l'interface web est inchangée.
Ne pas utiliser d'image factory fusionnée : elle pourrait effacer les réglages.
Après démarrage, contrôler dans le moniteur série à 115200 l'identité TEST exacte.
Ne pas lancer de mise à jour en ligne standard : elle remplacerait ce prototype.

## Protocole de validation sur table

Consigner pour chaque étape : type de tag, UID A/B, niveau RF, résultat, durée,
log série, heure, alimentation. Utiliser des tags de test sans valeur et garder
les bobines de production éloignées pendant les essais d'écriture.

1. Référence avant flash : zéro à vide, masses connues (ex. 100 g et 500 g),
   pesée d'une bobine connue, lecture chaque face, veille/réveil. Noter les
   écarts/répétabilité et comparer après flash avec les mêmes masses.
2. Après flash : même calibration, unité, connexion, batterie, zéro ; pas de
   tare imprévue. En mode pesée, deux orientations d'une bobine connue doivent
   identifier et synchroniser comme auparavant, sans aucun log TAG WRITE.
3. POD > Read : NTAG213, 215 et 216 (capacités 144/504/888). Comparer UID à un
   lecteur indépendant. Tester deux tags différents et une paire mixte.
4. Absence d'un tag, même tag vu par les deux antennes, tag non NTAG, tag déjà
   écrit : New pair doit refuser sans modifier aucun octet. Relecture externe.
5. Tag verrouillé ou protégé (préparé séparément avec un outil connu) : refus.
   Ne pas verrouiller un tag via ce prototype. Tag avec données après l'en-tête
   NDEF vide : refus, y compris dans la zone haute d'un 215/216.
6. Préparer deux vierges. Refuser/laisser expirer la confirmation : zéro écriture.
   Tenter les clics /api/tap et /tap existants sur la confirmation : aucune
   autorisation. Confirmer ensuite avec le doigt, relâchement puis maintien.
7. Resume puis confirmation physique. Attendre le résultat des DEUX tags.
   Avec un lecteur indépendant, vérifier UID uniques, mêmes octets 32–35,
   octet 39 = 0x12 et 0x22, message TEST, reste des données attendu, protections
   et UID inchangés. Aucun nouvel élément d'inventaire cloud ne doit apparaître.
8. Vérifier avec les tags inversés : refus de la paire journalisée (emplacements
   liés aux UID). Remettre les tags dans leur emplacement d'origine : succès.
9. Cancel avant écriture, pendant A et pendant B : état non validé, journal
   conservé. Resume exige confirmation et finit uniquement avec les mêmes UID.
10. Retirer B après écriture de A : pas de succès global. Remettre B puis Resume.
    Remplacer B par un autre vierge : refus sans écriture sur le remplaçant.
11. Couper l'alimentation pendant A puis pendant B sur des essais distincts.
    Redémarrer : aucune écriture spontanée. Resume doit vérifier le journal et
    l'état des pages ; reprise ou refus explicite en cas de page déchirée.
12. Restore : les DEUX tags doivent retrouver exactement leur image vierge ;
    journal supprimé seulement après relecture. Couper pendant Restore puis
    recommencer Restore avec les mêmes tags. Ne pas déclarer la restauration
    réussie si une seule puce est restaurée.
13. Débrancher un PN532 ou provoquer un défaut de communication avant une
    opération : refus ; pas d'activation non contrôlée du voisin. Mesurer si
    possible les champs RF avec un instrument adapté : jamais deux actifs
    pendant une opération POD. Les LED d'alimentation ne mesurent pas le champ.
14. Exit : pesée normale, lecture deux côtés, zéro, veille/réveil, batterie et
    RFID Auto. Les tags TEST sont exclus des remontées de production.
15. Répéter trois cycles par type de tag, puis au moins dix paires au réglage RF
    retenu. Documenter tout timeout. Ne valider une utilisation régulière qu'après
    cette validation physique ; cette livraison reste un prototype TEST.

## Retour arrière

Retirer les tags avant toute restauration firmware. Le firmware original ne
connaît pas l'exclusion TEST ; ne pas lui présenter les tags de test encodés.

Retour exact à l'état d'avant : passer en mode bootloader, restaurer la lecture
complète appareil-avant-A.bin (16 Mo, SHA-256 vérifié) à l'adresse 0, sur LA MÊME
carte. Cette opération remplace toutes les données, y compris réglages/journal
modifiés depuis la sauvegarde. Elle ne restaure PAS les tags NFC.

```sh
python3 -m esptool --chip esp32s3 --port PORT write-flash 0 appareil-avant-A.bin
```

Alternative pour revenir au code original en conservant les réglages actuels :
extraire BASE-ORIGINALE-3.5B.zip et utiliser flash-3.5B/firmware.bin et
flash-3.5B/boot_app0.bin aux mêmes adresses 0x10000 et 0xE000. Vérifier leurs
empreintes avec le manifeste d'origine. Ne pas confondre ces fichiers avec TEST.
Contrôler l'identité originale, calibration, lecture des deux côtés et veille.

## Construction et sources de référence

PlatformIO : `pio run -e esp32s3_hsu_b` ; `pio run -e esp32s3_hsu_b -t buildfs`.
Validation : `bash scripts/verify.sh` (contrôles, compilation, suites natives,
fault injection POD). Dépendances résolues consignées dans la livraison.
Les .pio et paquets compilés ne sont pas inclus dans l'archive des sources.

Spécification TigerTag consultée à la révision
96f322a96363d9f921dc03f6b1eaef81aaa25433 :
https://github.com/TigerTag-Project/TigerTag-RFID-Guide/blob/96f322a96363d9f921dc03f6b1eaef81aaa25433/README.md
Documentation NXP utilisée pour les versions/protections NTAG :
https://docs.nxp.com/bundle/NTAG213_215_216/page/topics/get_version.html
https://docs.nxp.com/bundle/NTAG213_215_216/page/topics/password_verification_protection.html
https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf


## Correctif r2 — lecture NTAG216

La r1 envoyait GET_VERSION par InDataExchange (0x40), où 0x60 est interprété
comme une authentification MIFARE. La r2 utilise InCommunicateThru : D4 42 60,
après sélection du tag, et vérifie la réponse D5 43, le statut et les 8 octets
de version. Cela corrige une erreur de transport du prototype, sans modifier
les contrôles avant écriture ou la confirmation physique.

Les erreurs de lecture précisent désormais sélection, GET_VERSION, CC,
protection ou mémoire. Les tests initiaux simulaient GET_VERSION à trop haut
niveau ; un nouveau test contrôle la méthode de transport réelle.
Le journal NVS reste compatible. Aucun effacement de réglages requis.
Après installation de r2, commencer uniquement par Read avec vos NTAG216.
Attendu : deux UID distincts et 888 bytes. Si échec, relever le message exact
pour chaque lecteur et vérifier la détection dans Paramètres > RFID.

Référence technique NXP :
https://community.nxp.com/t5/NFC/I-can-t-read-the-session-register-in-NT3H1101-from-RF-side-with/td-p/558556


## Diagnostic r3 — New pair refuse des tags reformates

La r3 conserve exactement la politique de tags vierges. Elle affiche A/B,
la cause du refus, la page hexadecimale et quatre octets utiles au diagnostic.
Residual data : en-tete vide reconnu, mais donnees presentes plus loin.
Nonblank header : en-tete non reconnu comme vide (tag utilise ou autre format).
Static/Dynamic lock : verrouillage constate ; aucun deverrouillage tente.
AUTH0, ACCESS ou Mirror : configuration non acceptee pour une ecriture TEST.
Ces informations ne contiennent pas les pages de mot de passe.

Apres flash, essayer New pair sur les memes tags. Si le controle refuse,
aucune ecriture NFC et aucun nouveau journal ne sont crees. Relever le message
exact avant de choisir une methode de preparation des tags. Ne pas forcer
l'ecriture et ne pas modifier les pages de protection pour contourner le refus.
Si les deux tags satisfont finalement tous les controles, New pair presente
la confirmation et ne fait toujours aucune ecriture NFC ; Resume reste separe.


## Correctif r4 — TigerTag Init vierge

Ajoute le format initialise TigerTag Init : page 04 = 6C 41 A2 E1, puis TOUS
les octets de la memoire utilisateur restants a zero (y compris produit,
message, signature et extension NTAG215/216). Le marqueur seul n'autorise
jamais l'ecriture. Les TigerTag/TigerTag+ de production restent refuses.
Reference : https://github.com/TigerTag-Project/TigerTag-RFID-Guide#21-id-tigertag

Le journal sauvegarde aussi cet en-tete Init. Restore restaure exactement
l'image Init d'origine. Protections, UID distincts, confirmation physique et
verification des deux tags sont inchanges. Apres installation, refaire Read
puis New pair. Si preparation reussie, Resume necessite une nouvelle
confirmation physique. Si refus, conserver le message precis A/B.


## r5 — reprise et diagnostic d'une paire partielle

Compatible avec le journal r4 existant. Ne pas lancer New pair apres ecriture
partielle. Garder les deux tags dans les memes emplacements. Installer sans
effacer NVS, puis Resume avec confirmation physique. Verify est une lecture
seule : sur une paire partielle, un refus est attendu.

La r5 conserve a l'ecran et en serie le lecteur A/B, la page hexadecimale,
l'etape et la cause. Une difference indique les octets observes et imprime
les octets originaux/cibles en serie. Les erreurs de confirmation et l'absence
de journal ne sont plus annoncees comme un echec d'ecriture avec journal retenu.

Optimisation : READ fournit deja 16 octets ; ils sont maintenant utilises,
avec copie limitee a la memoire utilisateur pour le dernier bloc. La capacite
entiere est controlee avant et apres la transaction. Entre deux pages ecrites,
le firmware recontrole UID, type, protections et les 144 octets du record,
au lieu de relire 888 octets pour un NTAG216. Chaque page ecrite est relue.
Les changements hors du record sont refuses aux controles complets.
La vitesse reelle et la cause de l'echec observe en r4 restent a confirmer
sur la carte : les tests simules ne permettent pas d'affirmer la cause RF.


## r6 — variante rapide, r5 conservee comme reference

Conserve le champ du lecteur actif pendant le traitement de ses pages. Le
lecteur voisin reste coupe. Controles complets avant confirmation, a l'entree
de chaque lecteur apres confirmation, et apres les deux blocs d'ecriture.
Avant CHAQUE page modifiee : lecture physique des pages fabricant pour verifier
l'UID (en sautant BCC0), lecture de la page et comparaison original/cible.
Apres WRITE : relecture de la page. Pas de confiance dans un UID seulement
mis en cache. Les pages deja correctes ne sont pas reecrites. Entete en dernier.
Une erreur interrompt l'operation ; Resume refait les controles et demande une
nouvelle confirmation physique. Format du journal compatible avec r5.

La ligne [POD PERF] affiche operation_ms, written, skipped, uid_checks et ok.
operation_ms commence APRES le maintien physique et inclut la verification
finale ; il exclut le precontrole avant confirmation. Relever aussi la duree
percue. Ne pas comparer un Resume sans modification (written=0) a une ecriture.
La reduction des commandes est testee, pas une promesse de duree materielle.

Essai : flash sans effacement de NVS ; Verify sur la paire existante. Puis,
uniquement sur cette paire TEST : Restore, New pair, Resume, Verify. Chronometrer
Resume apres confirmation et conserver [POD PERF]. Refaire annulation/reprise
et cinq Verify. Ne pas tester une vraie coupure avant ces controles.
En cas de regression, reinstaller l'application r5 sans effacer NVS. Retirer
les tags pendant le flash puis les remettre aux memes emplacements. La paire
et son journal restent utilisables par r5. Aucun changement serveur/Studio.
