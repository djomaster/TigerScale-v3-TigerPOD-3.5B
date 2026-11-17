# TigerScale 3.5B — TigerPOD Inventaire r8

> Document historique (r8). Etat actuel : `CHANGES-1.0.0.md`.

Prototype source du 13 novembre 2026. Identité :
`V3Plus-3.11.0-3.5B-RFID-3.13-POD-INVENTORY-r8`.
Base : r7.1 custom, elle-même dérivée de la r6 custom 3.5B ; aucune substitution
par le firmware générique. Les dossiers et livraisons précédents sont conservés.
Les documents r6/r7/r7.1 inclus restent des références historiques.

## Ce qui change

- Menu 480 × 320 guidé : Scanner, action adaptée, Pesée, puis Outils séparés.
- Tag reconnu directement ou par identifiant partagé : résolution de la bobine
  puis parcours habituel de pesée et de mise à jour du poids.
- Tag inconnu utilisé comme identifiant : choix d'une bobine avec ou sans
  TigerTags, confirmation physique, ajout du lien partagé. Le tag n'est jamais
  écrit et les TigerTags existants restent associés.
- Deux NTAG compatibles inconnus : associer une bobine sans TigerTag ou formater
  leurs données utilisateur. A devient 1/2, B devient 2/2, UID distincts.
- Outils : actualiser les tags depuis l'inventaire, formater une paire associée,
  restaurer l'état sauvegardé, vérifier la paire.
- Confirmation par relâchement puis maintien physique de 2 secondes. Le contrôle
  distant ne peut pas autoriser l'écriture.
- Transport HSU accéléré pendant les opérations POD : attente de l'ACK UART
  sans les sondages fixes de 10 ms de la bibliothèque. Rafraîchissement de la
  progression limité. UID frais avant chaque page changée, relecture de cette
  page, contrôle final de toute la mémoire et alternance RF conservés.
- Vérifier tolère les mises à jour normales de poids. La restauration conserve
  le poids récent, y compris lorsque sa date est un Timestamp Firebase.

## Lien réellement partagé

La r8 utilise la collection officielle `users/{compte}/rfidAliases/{UID}` et les
champs `spoolId`, `chip`, `vendor`, `linkedAt`, présents dans Studio Manager
2.51.0. La consultation **en lecture seule** du connecteur TigerSystem a confirmé
l'existence de liens de cette forme dans le compte. Aucun lien n'a été créé et
aucune bobine n'a été modifiée pendant cette consultation.

Une autre balance doit disposer d'un firmware capable de résoudre ces liens
(comme cette r8). Une balance équipée d'un ancien firmware ne gagne pas cette
capacité automatiquement. Studio officiel reste inchangé. Les droits de création
et de déplacement des liens par le compte connecté doivent encore être vérifiés
sur la balance : le connecteur en lecture seule ne permet pas de les prouver.
Toutes les requêtes r8 utilisent l'authentification existante et HTTPS avec
vérification du certificat ; aucun point d'entrée d'écriture distant n'est ajouté.

Les liens existants suivent le changement de fiche lors d'un encodage d'une
bobine sans tag ou du formatage d'une paire. Ils font partie du même lot
conditionnel et du plan de restauration. Ne modifier ni la bobine ni ses liens
simultanément dans Studio ou sur une autre balance pendant une opération POD.
Les modifications des documents capturés sont contrôlées ; la création d'un
nouveau lien par un autre client pendant le plan n'est pas verrouillée globalement.

## Limites du prototype

- L'encodage reste limité à deux NTAG213/215/216 NXP distincts, déverrouillés,
  sans protection ni miroir, avec mémoire au-delà du bloc TigerTag de 144 octets
  vide. Les pages UID, verrouillage, configuration et mot de passe ne sont jamais
  écrites. « Formater » réinitialise le bloc TigerTag ; ce n'est pas un effacement
  usine de toutes les pages. Une carte verrouillée n'est pas déverrouillée.
- Un seul tag, un tag protégé ou une mémoire hors de ce périmètre peut être
  proposé comme identifiant sans écriture. Une erreur de lecture peut aussi
  empêcher de qualifier la paire comme encodable : rescanner avant de choisir.
- Un tag connu et un tag inconnu ensemble bloquent la pesée automatique : retirer
  l'inconnu. Deux bobines différentes ne sont jamais fusionnées automatiquement.
- Une liaison déjà existante n'est pas remplacée. Les liens vers une bobine
  supprimée nécessitent une correction dans Studio.
- Connexion au compte nécessaire pour reconnaître une nouvelle pose ; une panne
  réseau n'autorise ni association ni envoi du poids vers une bobine supposée.
- Une seule sauvegarde d'opération est conservée. Migration limitée à 100 liens
  concernés et à la taille autorisée du journal ; au-delà, refus avant écriture.
- Les écrans POD de ce prototype sont en français ; la traduction multilingue
  du nouveau menu n'est pas encore intégrée.
- Aucun temps de 5 secondes n'est garanti. Le délai total comprend les contrôles,
  les confirmations et Firebase. La mesure matérielle reste nécessaire.

## Compilation et installation

Aucun binaire r8 n'est fourni et aucune compilation ESP32 n'a été exécutée ici,
conformément à votre demande. Extraire l'archive dans un nouveau dossier. Depuis
le dossier contenant `platformio.ini` :

```sh
pio run -e esp32s3_hsu_b
```

Conserver la r6 et le dossier précédent. Terminer ou restaurer toute opération
r7/r7.1 en attente avant de changer de version. Sauvegarder la flash actuelle,
notamment NVS et LittleFS qui contiennent configuration et journaux.
Flasher uniquement l'application compilée selon votre méthode habituelle.
Ne pas effacer la flash, ne pas envoyer une image LittleFS vide et ne pas lancer
une restauration usine. Le journal de production garde son format et son espace
`pod-prod` ; le journal TEST r6 `pod-test` reste distinct.

## Protocole d'essai sur la balance

1. Vérifier l'identité r8 au démarrage, connexion au bon compte, zéro à vide,
   puis pesée connue (par exemple 255 g). Contrôler gross/net et poids du contenant.
2. Poser une paire déjà associée. Attendre reconnaissance puis mise à jour dans
   Studio. Retirer et reposer cinq fois : aucune proposition de réencodage.
3. Avec deux NTAG d'essai distincts, choisir Scanner puis Associer une bobine
   sans TigerTag. Vérifier la bobine choisie. Un doigt déjà maintenu ne doit pas
   confirmer ; relâcher puis maintenir CONFIRMER 2 s.
4. Attendre le succès complet, vérifier les deux UID et indices dans Studio,
   puis utiliser Vérifier après une mise à jour du poids : pas de faux conflit.
5. Mesurer séparément le délai total et la ligne `[POD PERF] nfc_ms=...` du
   moniteur série, avec `written`, `skipped`, `uid_checks`. Comparer à r6 sur les
   mêmes tags et données. Une reprise sans changement doit éviter les pages
   déjà correctes ; cela ne constitue pas la mesure d'une écriture complète.
6. Utiliser un tag tiers de test : Choisir une bobine avec des TigerTags
   existants, confirmer, vérifier dans Studio que ceux-ci sont conservés.
   Reposer le tag tiers sur une seconde balance équipée de r8 et du même compte :
   même bobine, mise à jour du poids, aucun ajout de fiche parasite.
7. Refaire avec une bobine sans TigerTag. Encoder ensuite cette bobine avec une
   paire NTAG et vérifier que l'identifiant tiers pointe toujours vers elle.
8. Sur une paire d'essai, tester Actualiser, Formater, puis Restaurer. Formatage
   d'une paire associée : une fiche sans tag demeure dans l'inventaire.
   Restaurer remet l'état sauvegardé tout en conservant le poids récent.
9. Tester Annuler entre pages, tag absent, UID échangé, Wi-Fi coupé. Aucun succès
   ne doit être annoncé avant vérification complète. Garder les mêmes tags aux
   mêmes positions A/B ; utiliser Reprendre ou Restaurer avec confirmation.
   Ne pas créer une nouvelle paire tant que le journal reste en attente.
10. Tester un tag connu avec un inconnu, puis retirer seulement l'inconnu :
    reprise de la reconnaissance du connu. Vérifier aussi qu'une bobine supprimée
    ou une autre paire ne reçoit jamais son poids.

Les tests d'interruption et de restauration concernent exclusivement des tags et
bobines d'essai. Une erreur `HTTP 403` signifie que les droits du compte ne
permettent pas l'opération ; ne pas assouplir les règles Firebase pour la contourner.

## Retour arrière

Si une opération reste incomplète, utiliser d'abord Reprendre ou Restaurer en r8,
avec les mêmes UID, le même compte et le journal intact. Revenir à la r6 ne répare
pas une association cloud partiellement traitée. Après résolution, reflasher
l'application r6 conservée sans effacer NVS/LittleFS. La r6 ne sait pas exploiter
les nouveaux identifiants partagés ; les TigerTags traditionnels restent le
parcours de retour à privilégier. Une sauvegarde flash ne restaure pas Firebase.
