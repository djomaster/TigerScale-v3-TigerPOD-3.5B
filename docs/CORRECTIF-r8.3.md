# v3.13 fork custom 3.5B — r8.3 : les deux parcours

> Document historique (r8.3). Etat actuel : `CHANGES-djo-1.0.md`.

Révision source du 14 novembre 2026, identité
`V3.13-FORK-CUSTOM-3.5B-POD-r8.3`. La r8.2 reste conservée.

La r8.3 conserve les accélérations de la r8.2 pour les bobines reconnues
(recherches Firebase groupées, attentes locales raccourcies), et accélère aussi
l'ouverture du choix d'association pour les tags inconnus.

## Proposition rapide, contrôle complet avant écriture

L'aperçu vérifie les UID, le type NTAG, le bloc constructeur, les protections et
16 octets de mémoire utilisateur par tag. Sur deux NTAG216, il effectue 6 commandes
READ au lieu de 116 pour les deux inspections complètes (hors sélection RF et
GET_VERSION, toujours présents). Le menu affiche « Deux NTAG detectes » et
annonce le contrôle complet avant toute écriture : ce n'est pas une déclaration
que toute la mémoire a déjà été validée.

La recherche d'association déjà terminée en arrière-plan peut être réutilisée
pendant moins de trois secondes, uniquement pour les mêmes UID, le même compte
et la même génération de détection. Sinon, une recherche normale est faite.
Ce résultat sert seulement à choisir le menu ; il n'autorise aucune écriture.

Après choix d'Associer ou de Formater, la lecture complète des deux mémoires et
les contrôles de cohérence restent inchangés. Données résiduelles hors du bloc
pris en charge, tag manquant, UID dupliqué ou protections provoquent un refus
avant écriture. L'encodage exige toujours deux tags distincts et la confirmation
physique ; la pesée n'est pas modifiée.

Le contrôle complet étant différé, une paire peut afficher le choix d'encodage
puis être refusée lors de la préparation si sa mémoire contient des données non
prises en charge. Aucun effacement automatique n'est alors effectué.

## Délai et vérification sur votre carte

L'objectif reste une réaction proche d'une seconde dans les deux parcours.
Ce délai total n'est pas garanti : aucune mesure RF/Wi-Fi/Firebase réelle n'a
été faite ici. La préparation complète après choix d'une action reste plus
longue que la simple ouverture du menu. Elle protège vos tags et votre inventaire.

1. Compiler depuis le dossier contenant `platformio.ini` :

   ```sh
   pio run -e esp32s3_hsu_b
   ```

2. Vérifier l'identité r8.3, le zéro et une pesée connue. Les bobines déjà
   associées doivent toujours recevoir le bon poids dans Studio.
3. Chronométrer cinq poses de bobines connues, puis cinq poses de deux NTAG
   inconnus, jusqu'à l'affichage du menu. Comparer à la r8.1/r8.2.
4. Choisir une bobine sans TigerTag : attendre la préparation complète, puis
   confirmer physiquement. Ne pas confondre ce délai avec celui du premier menu.
5. Sur des tags d'essai, vérifier qu'un tag manquant, deux UID identiques ou des
   données hors du bloc pris en charge n'autorisent aucune écriture. Ne pas
   effacer des données pour contourner un refus sans sauvegarde.
6. Tester le retrait/changement de paire et le Wi-Fi coupé : aucune réponse
   ancienne ne doit autoriser une association ou une mise à jour erronée.

Sources uniquement ; aucune compilation ESP32 ni aucun flash exécutés ici.
Conserver NVS/LittleFS et terminer toute opération POD en attente avant mise à
jour. Pour revenir à la r8.2, reprendre/restaurer d'abord une éventuelle opération
inachevée, puis reflasher l'application conservée sans effacer la mémoire.
Le format des journaux et des liens partagés est inchangé.
