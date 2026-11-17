# TigerScale 3.5B — TigerPOD Inventaire r7

> Document historique (r7). Etat actuel : `CHANGES-1.0.0.md`.

Version source expérimentale du 12 novembre 2026, issue de la r6 custom validée
sur votre balance. Identité au démarrage :
`V3Plus-3.11.0-3.5B-RFID-3.13-POD-INVENTORY-r7`.

Cette livraison ne contient pas de binaire r7. La compilation PlatformIO et les
essais sur le matériel sont à réaliser par l'utilisateur. Les tests décrits
comme simulés n'ont accédé ni à votre inventaire réel, ni aux lecteurs physiques.

## Fonctionnement

1. Créer une bobine **sans tag** dans l'inventaire de Studio officiel, avec ses
   caractéristiques et son contenant correctement renseignés.
2. Connecter la balance au même compte TigerTag et au Wi-Fi. Studio n'a pas
   besoin d'être ouvert ou sur le même réseau : l'échange utilise Firebase.
3. Poser la bobine avec deux NTAG213/215/216 vierges, déverrouillés et distincts.
   Après détection stable, la balance ouvre le choix de bobine sans tag. Le même
   parcours est accessible dans Paramètres → TigerPOD / Inventaire → Associer.
4. Choisir la bobine. L'écran présente l'opération, la bobine et les deux UID.
   Relâcher complètement l'écran, puis maintenir physiquement **HOLD 2s** dans
   l'angle inférieur droit. Un clic distant ne vaut pas confirmation.
5. La balance écrit puis relit les deux tags, avec un seul champ RF actif à la
   fois. A devient 1/2, B devient 2/2 ; les deux UID restent différents.
6. Après vérification, Firebase remplace atomiquement la fiche sans tag par les
   deux fiches UID liées par `twin_tag_uid`, `tag_index`, `tag_count` et le même
   `timestamp`. Les données de bobine et de contenant sont conservées.
7. Attendre **« Deux tags verifies. Inventaire synchronise. Operation terminee. »**,
   puis Quitter. La balance relit la paire et reprend son fonctionnement de pesée.

Une paire déjà associée continue à utiliser le parcours habituel de lecture et
mise à jour du poids. Les filtres, l'étalonnage, les réglages de charge et le
transport HSU de la r6 sont conservés. Pendant le menu, l'acquisition de poids
continue ; pendant une transaction inachevée, la synchronisation du poids est
bloquée pour éviter d'utiliser une paire partiellement écrite.

## Actions du menu

| Action | Résultat |
| --- | --- |
| Associer | Deux tags strictement vierges + une bobine sans tag du compte connecté. Aucune réécriture automatique d'une paire existante. |
| Réencoder | Reconstruit la paire déjà associée avec les données actuelles de l'inventaire. A doit porter 1/2, B 2/2. |
| Formater | Sauvegarde la paire, écrit le marqueur vierge TigerTag Init et des zéros dans les 144 octets de données, vérifie, puis conserve la bobine sous forme d'une fiche sans tag dans Studio. Ne supprime pas la bobine. |
| Restaurer | Remet les données des tags d'avant la dernière opération sur les mêmes UID et remet leur association cohérente. Conserve le poids récent du cloud ; refuse les conflits de métadonnées. Permet aussi de revenir aux données initiales après une écriture partielle, si l'inventaire n'a pas changé. |
| Reprendre | Termine la transaction enregistrée, avec une nouvelle confirmation physique ; relit les pages et saute celles déjà correctes. Réessaie ensuite la synchronisation cloud si nécessaire. |
| Vérifier | Relit les deux tags et compare l'inventaire au résultat enregistré. Aucune écriture NFC ou cloud. |
| Annuler | Arrête entre les opérations NFC ; conserve le journal. Ne signifie pas que les pages déjà écrites ont été annulées. |

La restauration est limitée à **un niveau**, avec la dernière opération comme
référence. Après une restauration réussie, une seconde pression ne fait pas une
bascule vers l'état suivant. Une restauration interrompue se termine par Reprendre.
Une nouvelle opération remplace la référence active ; une copie précédente reste
sur LittleFS pour diagnostic, sans sélection automatique dans le menu.

Si les caractéristiques ont été modifiées dans Studio avant un réencodage,
l'ancien contenu physique peut déjà différer de la fiche sauvegardée. Restaurer
refuse alors de recréer une incohérence entre tags et inventaire. Réencoder est
le choix normal pour reconstruire avec les données actuelles ; Restaurer sert
au retour à l'état précédent lorsqu'il reste cohérent.

## Limites volontaires

- Deux tags requis. Pas d'écriture sur Bambu, Certified, UID non NXP, tags protégés,
  verrouillés, mémoire étendue non nulle ou données inconnues. Les pages UID,
  verrous, mot de passe et configuration ne sont jamais écrites.
- Un tag simplement « reformaté » par une application peut garder des données
  résiduelles : il n'est pas automatiquement considéré vierge. La fonction
  Formater concerne une paire de production reconnue et associée à ce compte.
- Pour réutiliser les tags **TIGERPOD TEST** de la r6, faire Restore dans la r6
  avant de passer à cette version, ou utiliser deux autres tags réellement vierges.
  La r7 conserve le journal NVS `pod-test` intact et utilise `pod-prod` séparément.
- Les opérations cloud exigent le compte Firebase de la balance et une connexion
  HTTPS dont le certificat est vérifié. Les droits du compte restent appliqués
  côté Firebase ; un refus de droits n'est jamais contourné.
- Aucun nouveau service réseau d'écriture NFC. Les écritures sont autorisées
  seulement par le moteur local après HOLD physique. Les services web historiques
  de la base custom ne constituent pas une nouvelle interface TigerPOD.
- Une modification concurrente dans Studio entraîne un refus de la validation
  conditionnelle, jamais l'écrasement silencieux de la modification. Garder les
  tags et le journal ; en cas de conflit persistant, conserver les diagnostics.
- Ne pas modifier la bobine dans Studio pendant une opération. Le cloud et deux
  tags ne peuvent pas être modifiés par une transaction matérielle unique : le
  journal permet la reprise de cet intervalle, sans prétendre l'éliminer.
- Les nouveaux libellés du prototype sont principalement français, avec certains
  diagnostics techniques du moteur en anglais. Ils ne sont pas encore traduits
  dans toutes les langues de l'application d'origine.
- Les temps réels ne sont pas mesurés pour la r7. Le moteur rapide r6 est conservé,
  mais la sélection et la synchronisation cloud ajoutent une durée variable.

## Compilation par l'utilisateur

Décompresser `SOURCES-TigerScale-POD-3.5B-r7.zip`. Ouvrir le dossier
`TigerScale-POD-3.5B-r7`, celui qui contient **platformio.ini** ; ne pas lancer la
compilation depuis le dossier `outputs` parent.

```sh
pio run -e esp32s3_hsu_b
```

Si une ancienne compilation a laissé des fichiers de dépendances incomplets,
arrêter toute autre compilation, puis :

```sh
pio run -e esp32s3_hsu_b -t clean
pio run -e esp32s3_hsu_b
```

Le résultat attendu est `.pio/build/esp32s3_hsu_b/firmware.bin`. Cette archive
n'inclut ni `.pio`, ni ancien firmware : aucun binaire r6 ne peut être confondu
avec un binaire r7. Les dépendances restent celles de la r6, dont la plateforme
pioarduino 55.03.39 et l'environnement exclusif 3.5B HSU.

Pour le flash, utiliser votre méthode de mise à jour d'application déjà validée
pour cette carte et conserver la table de partitions existante. Ne pas effacer
la flash, NVS ou LittleFS ; ne pas lancer `uploadfs`. Le système de fichiers
contient les journaux et l'application web existante. Le protocole de sauvegarde,
de comparaison des partitions et de flash app0 est conservé dans POD-TEST.md,
section installation : remplacer uniquement l'image application par la r7 que
vous venez de compiler, sans réutiliser un ancien `firmware.bin` par erreur.

## Protocole d'essai sur table

Utiliser d'abord une **bobine de test** créée dans Studio, deux tags dédiés et le
moniteur série à 115200. Les écritures Firebase de la r7 sont réelles.

1. Vérifier l'identité r7 au démarrage. Vérifier à vide, avec votre masse de 255 g,
   puis à vide : le comportement doit rester comparable à la r6. Conserver les
   réglages de calibration ; ne pas retarer avec la bobine posée.
2. Poser une paire déjà associée de test, sans entrer dans TigerPOD. Vérifier
   informations, poids et une mise à jour habituelle dans Studio. Retirer la bobine.
3. Poser les deux tags vierges. Choisir la bobine de test sans tag. Ne pas confirmer
   physiquement : après le délai de 30 s, aucun tag ne doit être écrit et Studio
   doit conserver la fiche sans tag. Le plan local peut rester en attente.
4. Reprendre avec les mêmes tags et confirmer HOLD. Contrôler UID A différent de B,
   index 1/2 et 2/2, puis le message final de synchronisation réussie. Dans Studio,
   contrôler qu'une seule bobine logique Twin remplace la fiche sans tag et que
   matière, couleur, poids et contenant sont corrects. Vérifier depuis la balance.
5. Quitter le menu ; vérifier 255 g, retrait et retour à zéro. Poser à nouveau la
   paire : elle doit être lue normalement, sans proposition d'encodage.
6. Sur cette paire de test, modifier un champ dans Studio, puis Réencoder et HOLD.
   Vérifier la lecture des nouvelles données, l'identité des UID et le résultat
   dans Studio. Une modification de poids récente ne doit pas revenir en arrière.
7. Formater et HOLD. Les deux tags doivent être vierges ; la bobine doit rester
   dans Studio sans tags. Restaurer avec les tags à la même place et HOLD : la
   paire et ses données doivent revenir. Vérifier et contrôler le poids récent.
8. Tester Annuler pendant un réencodage : ne pas retirer les tags ; aucune réussite
   ne doit être annoncée. Reprendre et HOLD doit terminer, en sautant les pages
   déjà correctes. Vérifier les deux tags et Studio.
9. Sur une autre opération de test, couper le Wi-Fi après la lecture des tags,
   avant la validation cloud. L'opération doit rester en attente, sans succès
   annoncé. Rétablir le Wi-Fi et Reprendre ; une seule paire logique doit subsister.
10. Tester le refus avec un autre UID ou un tag protégé : aucun succès, aucun
    remplacement de bobine. Remettre les tags attendus avant toute reprise.

Les tests 8 à 10 modifient potentiellement l'état de la paire de test : ne pas
les faire sur une bobine de production. La coupure électrique physique et le
comportement lors d'une panne réelle de flash n'ont pas été testés ici ; aucune
validation matérielle de ce type n'est revendiquée.

## En cas d'arrêt et retour à la r6

- Conserver les deux tags à leur place A/B. Ne pas les reformater avec un téléphone.
- Reprendre pour terminer, ou Restaurer pour revenir à l'état sauvegardé lorsque
  le cloud ne présente pas de conflit. Ne pas supprimer les fichiers `pod7-*`.
- Les fichiers `/pod7-job.json`, `/pod7-backup.json`, `/pod7-stage.json` et
  `/pod7-commit.json` contiennent des UID et des données d'inventaire ; ils ne
  contiennent pas le jeton Firebase. Ne pas les publier sans les examiner.
- Si le journal est illisible, arrêter les écritures et conserver une sauvegarde
  complète de l'appareil. La r7 refuse de deviner l'état à rétablir.
- Pour revenir à r6 : terminer ou restaurer d'abord toute opération r7. Si ce
  n'est pas possible, retirer les tags concernés avant le retour à r6 et les
  isoler : la r6 ne connaît pas le verrou de synchronisation du journal r7.
  Flasher seulement l'application r6 conservée, avec la sélection de partition
  adaptée à votre installation ; préserver NVS/LittleFS. Ce retour firmware ne
  restaure pas à lui seul les tags ou l'inventaire Firebase.

## Vérifications livrées

Contrôles statiques `scripts/verify.sh --fix --quick` et suites natives exécutées
sur ordinateur : inventaire/lecteurs/pesée, sélecteur LVGL réel, zéro/dérive,
alimentation, RFID, moteur NFC r6, transport GET_VERSION, planificateur cloud r7
et coordinateur r7 complet avec périphériques simulés. Voir VALIDATION-r7.txt
pour les résultats et la provenance. Aucune compilation ESP32, aucun flash et
aucune écriture dans votre inventaire réel n'ont été exécutés pour cette livraison.
