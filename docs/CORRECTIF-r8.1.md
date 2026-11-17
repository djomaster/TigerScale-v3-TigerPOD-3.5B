# v3.13 fork custom — r8.1, carte 3.5B

> Document historique (r8.1). Etat actuel : `CHANGES-djo-1.0.md`.

Version source du 14 novembre 2026, issue exclusivement de la r8 custom.
Identité complète : `V3.13-FORK-CUSTOM-3.5B-POD-r8.1`.
L'écran de démarrage indique `v3.13.0 fork custom r8.1` ; le champ version
numérique devient `3.13.0`.

Ce renommage ne transforme pas le code en copie de la version officielle 3.13 :
la base historique reste V3Plus 3.11.0, avec le port RFID 3.13 et les ajouts custom
TigerPOD. Les versions précédentes restent intactes.

## Correctif

La découverte automatique ne passe plus par l'écran bloquant « Saving spool
assignment ». Elle soumet une demande persistante au traitement réseau existant,
puis rend immédiatement la main à la boucle de pesée, d'affichage et de lecture
RFID. Aucun écran de progression n'est ouvert pour cette reconnaissance.

Une interruption RFID inférieure à six secondes ne provoque pas de nouvelle
recherche. La résolution est conservée pour la même pose. Un UID différent,
un changement de compte ou un retrait confirmé de la bobine invalide la
reconnaissance ; une réponse réseau ancienne est ignorée. Sans lecture datant de moins de 1,5 seconde,
la synchronisation du poids est suspendue, sans effacer inutilement le résultat
mémorisé. Après une erreur réseau, la nouvelle tentative est espacée de dix
secondes et reste invisible.

La temporisation de dédoublonnage RFID de 1,5 seconde n'est pas modifiée. La
tolérance de six secondes appartient uniquement à la reconnaissance des liens.
L'acquisition, l'étalonnage, la tare, les filtres et les calculs de poids sont
inchangés. Un UID nouveau n'autorise aucun envoi de poids avant validation.

Les tags inconnus peuvent toujours ouvrir le menu d'association une fois par
pose. Les opérations explicitement choisies dans TigerPOD conservent leurs écrans
de progression ; les lectures y indiquent « Chargement du stock », tandis que
le message d'enregistrement reste réservé à l'envoi d'une association/opération.
Les confirmations physiques avant écriture sont conservées.

## Compilation et essai

Aucune compilation ESP32 ni aucun flash exécuté ici. Extraire dans un nouveau
dossier puis compiler depuis celui contenant `platformio.ini` :

```sh
pio run -e esp32s3_hsu_b
```

Conserver NVS et LittleFS ; terminer toute opération POD en attente avant de
changer de version. Ne pas effacer la flash ni envoyer un système de fichiers
vide. Lire également le protocole r8, qui reste applicable aux écritures.

1. Vérifier la nouvelle identité au démarrage, le zéro et un poids connu.
2. Poser une bobine reconnue pendant une minute : affichage du poids continu,
   mise à jour dans Studio, aucun écran « Saving spool assignment ».
3. Provoquer une brève perte de lecture d'un tag (2 à 5 s), sans substituer la
   bobine : pas de nouvel écran ni de recherche répétée.
4. Retirer la bobine puis en poser une autre : sa reconnaissance doit précéder
   tout envoi de son poids. Vérifier le bon destinataire dans Studio.
5. Ralentir/couper le Wi-Fi pendant une première reconnaissance : affichage et
   acquisition restent actifs, aucun poids envoyé à une bobine non validée.
   Rétablir le réseau : nouvelle tentative en arrière-plan.
6. Retirer/changer les tags pendant la requête : une réponse ancienne ne doit
   pas autoriser la nouvelle pose. Un tag connu et un inconnu restent bloqués
   tant que la combinaison n'est pas résolue.
7. Poser des tags inconnus : une seule proposition d'association par pose.
   Vérifier que l'encodage exige toujours la confirmation physique.

Retour arrière : reprendre/restaurer d'abord toute opération inachevée, puis
reflasher l'application r8 conservée sans effacer NVS/LittleFS. Aucune migration
du format des journaux ou des associations Firebase n'est introduite en r8.1.
