# v3.13 fork custom 3.5B — r8.2 : reconnaissance accélérée

> Document historique (r8.2). Etat actuel : `CHANGES-1.0.0.md`.

Cette révision du 14 novembre 2026 conserve la r8.1, la pesée, la reconnaissance
en arrière-plan et l'obligation de deux tags distincts pour encoder une paire.
Identité : `V3.13-FORK-CUSTOM-3.5B-POD-r8.2`.

## Attentes réduites

- Délai de stabilisation avant recherche : 150 ms lorsque les deux UID sont
  présents, au lieu de 1000 ms. Avec un seul lecteur/tag détecté : 500 ms pour
  laisser arriver la seconde lecture, sans imposer deux tags à la simple pesée.
- Pause après lecture acceptée : 150 ms au lieu de 1200 ms. Les antennes restent
  alternées, jamais activées ensemble. La fenêtre de dédoublonnage de 1,5 s,
  les contrôles de fraîcheur et la tolérance aux pertes brèves sont conservés.
- Recherche directe des deux fiches TigerTag groupée en une requête HTTPS au
  lieu de deux. Pour des identifiants supplémentaires : recherches groupées par
  étape (fiches UID, liens, bobines cibles), jusqu'à trois requêtes au lieu de six.
- Les champs demandés sont limités à ceux nécessaires à la reconnaissance.
  Les réponses sont rapprochées par nom de document, jamais par position.
  Une réponse incomplète, dupliquée ou illisible déclenche le repli vers les
  lectures individuelles existantes. Une erreur réseau ne signifie pas « tag
  inconnu » et n'autorise aucune écriture.

Référence de l'API de lecture groupée :
https://firebase.google.com/docs/firestore/reference/rest/v1/projects.databases.documents/batchGet

## Ce qui n'est pas garanti

Le délai total inférieur à une seconde est un objectif matériel à mesurer,
**pas un résultat validé ni une garantie**. Il reste dépendant de l'accrochage RF,
du Wi-Fi, de TLS/Firebase, du traitement des métadonnées et de la disponibilité
du traitement réseau. Une première reconnaissance interroge toujours le compte
réel ; aucun ancien lien n'est utilisé pour envoyer le poids sans validation.
Les chiffres ci-dessus désignent les attentes programmées et le nombre de
requêtes, pas une durée mesurée sur votre balance.

Le scan manuel de compatibilité et les contrôles avant encodage restent complets.
Les temps de préparation d'une paire inconnue ne se confondent donc pas avec
ceux de reconnaissance d'une bobine déjà associée.

## Compilation et essai

Sources uniquement : aucune compilation ESP32 et aucun flash exécutés ici.
Extraire dans un nouveau dossier, puis depuis `platformio.ini` :

```sh
pio run -e esp32s3_hsu_b
```

Flasher l'application sans effacer NVS/LittleFS ; terminer toute opération POD
inachevée auparavant. Garder la r8.1 pour revenir en arrière par reflash de
l'application, après reprise/restauration d'une éventuelle opération en attente.

1. Vérifier au démarrage l'identité r8.2, le zéro et un poids connu.
2. Poser une bobine déjà associée et chronométrer jusqu'à son affichage. Répéter
   cinq fois, puis tester une autre bobine. Contrôler le destinataire du poids.
3. Relever `[POD TIMING] lookup_ms=...` : durée entre mise en file de la recherche
   et récupération de sa réponse, incluant l'attente du traitement réseau.
   Ce chiffre exclut la détection RF initiale et l'affichage des métadonnées.
4. Tester une brève perte RFID, le retrait/changement de bobine pendant la
   recherche, et le Wi-Fi coupé : aucun écran répétitif, aucune association
   supposée, aucune mise à jour du poids vers une autre bobine.
5. Tester un identifiant supplémentaire partagé, puis des tags inconnus. Une
   écriture exige toujours deux NTAG distincts et la confirmation physique.
6. Comparer la réactivité de l'affichage du poids et les erreurs RFID à la r8.1 :
   la cadence de lecture étant plus élevée, ce point doit être confirmé au banc.
