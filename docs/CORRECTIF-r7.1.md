# Correctif r7.1

> Document historique (r7.1). Etat actuel : `CHANGES-1.0.0.md`.

Le journal fourni montre une restauration réussie, suivie d’une vérification
des deux tags et de l’inventaire. Il ne montre pas un nouvel encodage de bobine.
Les deux NTAG216 restaurés présentent l’en-tête vierge
`01 03 A0 0C 34 03 00 FE`. Le détecteur automatique r7 réservait cet en-tête
aux NTAG213, contrairement au moteur POD qui l’acceptait déjà sur les trois
capacités. Cela provoquait « unrecognized tag; upload blocked ».

La r7.1 reconnaît cet en-tête sur les trois capacités et reconnaît également
le marqueur vierge TigerTag Init. Les protections et la vérification de toute
la mémoire avant écriture restent inchangées. Aucun journal n’est effacé ou
migré ; r6 et r7 sont conservées. Aucun changement de calibration.

Compiler avec `pio run -e esp32s3_hsu_b` depuis le dossier contenant platformio.ini.
Mettre à jour seulement l’application par votre méthode habituelle ; préserver
NVS et LittleFS, ne pas utiliser erase-flash ou uploadfs.
Après démarrage, contrôler l’identité POD-INVENTORY-r7.1, retirer puis reposer
la bobine : attendre le choix de bobine sans tag, sélectionner la fiche de test
puis confirmer physiquement HOLD. Contrôler le succès final et Studio.
Le menu Associer reste utilisable manuellement. Une mémoire résiduelle ou une
protection doit toujours faire refuser l’écriture.

Le poids brut observé dans le journal est d’environ 253,6–253,7 g, puis revient
à 0 g au retrait. Ce journal seul ne permet pas de conclure à la précision de
l’étalonnage par rapport à une masse de référence de 255 g.

Compilation ESP32 et essais matériels de ce correctif à faire par l’utilisateur.
Voir POD-INVENTORY-r7.md pour le protocole complet et le retour arrière.
