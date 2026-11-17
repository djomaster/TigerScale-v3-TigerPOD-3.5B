# Fork V3+ 3.11.0 + ameliorations RFID de l'officielle 3.13.0

Cette edition reste le fork ijord V3+ 3.11.0, pour Waveshare 3.5B en HSU
uniquement. Elle conserve la charge a 1000 mA, la limite VBUS a 1500 mA,
PowerDown pendant la veille, la programmation/effacement des tags et l'import
direct Bambu. Ce n'est pas le firmware officiel 3.13.0.

## Changements RFID portes et adaptes

- Echelle de puissance de 16 niveaux et bouton **Parametres > RFID > Auto**.
  Auto evalue en priorite le lecteur le plus faible puis confirme les meilleurs
  niveaux. Placer une bobine avec deux TigerTags correctement alignes pour ce test.
- Les valeurs analogiques des anciens niveaux 0-4 restent identiques. L'ancien
  niveau 5 du fork (profil usine) devient 15. La migration est enregistree une
  seule fois en NVS ; un appareil sans reglage demarre au niveau 7.
- Trois tentatives d'initialisation apres une sequence de reset, sans remettre
  la puce a zero entre chaque tentative. Meme sans version confirmee, le module
  recoit la configuration SAM, la puissance et les deux retries d'activation.
- Un lecteur non confirme reste candidat a une lecture toutes les secondes.
  La lecture physique d'un tag le confirme, y compris les tags vierges et Bambu
  qui ne sont pas encore transmis au circuit de pesee normal.
- Une selection explicite d'un seul lecteur est respectee. Une interrogation de
  demarrage ratee ne transforme plus la configuration demandee en un seul lecteur.
- Le fork isolait deja les champs RF pendant les lectures : ce comportement est
  conserve pour les lectures, ecritures et imports. Les commandes antenne on/off
  commandent maintenant aussi le champ reel. Un lecteur non confirme est lui
  aussi invite a couper son champ pendant que son voisin lit.
- Budget de lecture UART de 100 ms et deux retries d'activation. La lecture des
  trames de longueur variable du fork est conservee pour les UID NTAG et Classic.
  Les operations de configuration ont leur propre delai plus large et une
  resynchronisation apres erreur pour eviter les reponses decalees.
- En veille, PowerDown est tente sur chaque lecteur configure meme s'il n'a pas
  confirme sa version au demarrage. Les LED d'alimentation restent alimentees.

## Identite et installation

Le numero de base reste `3.11.0`. Le journal de demarrage distingue cette edition
avec `V3Plus-3.11.0-3.5B-RFID-3.13-r1`. Le correctif du nom reseau de l'officielle 3.13.0 (`macSuffix4()` lit
l'adresse materielle dans l'eFuse) est present dans cette edition.

Pour installer firmware et interface web sans effacement complet :

```sh
bash scripts/flash.sh --env esp32s3_hsu_b --fs
```

Les adresses et precautions sont dans [le guide 3.5B](CUSTOM-3.5B.md).
La mise a jour en ligne (OTA) est desactivee dans cette edition ; installer par USB. Flasher une
version standard remplace cette personnalisation.

Les tests natifs utilisent des lecteurs simules. Confirmer sur la balance :
lecture des deux cotes apres demarrage, apres veille, Auto, programmation et
effacement d'une paire vierge, puis import d'une bobine Bambu. La compilation
ne mesure ni la fiabilite RF ni le courant de charge reel.

Validation de cette livraison : compilation firmware et LittleFS, controles de
traductions/polices/documentation, 6330 verifications natives reussies, dont 44
sur les trames UART, les tentatives d'initialisation et les profils RF. Les
fonctions d'inventaire, d'ecriture/effacement, d'import Bambu et de zero du fork
sont conservees ; leurs tests natifs sont rejoues avec ce portage.
