# Architecture du Programme

## 1. Vue d’ensemble

Ce document présente l’architecture logicielle du projet **Worldwide Weather Watcher**, tel qu’implémenté dans le code Arduino. L’objectif est de comprendre la structure interne du programme, l’organisation des modules, ainsi que les interactions entre les différents composants : capteurs, interfaces, stockage, modes de fonctionnement et gestion des erreurs.

---

## 2. Structure générale du programme

L’architecture du programme se décompose en plusieurs blocs logiques :

### **A. Initialisation matérielle**

* Initialisation des librairies : `Wire`, `SPI`, `SD`, `EEPROM`, `RTClib`, `ChainableLED`, `Adafruit_BME280`.
* Configuration des broches (LED, boutons, LDR, SD).
* Attachement des interruptions pour les boutons.
* Initialisation du RTC, SD, capteurs.

### **B. Gestion de la configuration (EEPROM)**

* Structure `Settings` contenant les paramètres :

  * Intervalle de log
  * Timeout
  * Taille max fichier SD
  * Capteurs activés
  * CRC d’intégrité
* Fonctions : `loadEEPROM()`, `saveEEPROM()`, `setDefaults()`, `crc16()`.

### **C. Capteurs (modèle générique via `struct Sensor`)**

Chaque capteur possède :

* Un nom
* Un pointeur vers un bool indiquant s’il est actif
* Un état `present`
* Une fonction d’initialisation
* Une fonction de lecture

Deux capteurs :

* `sBME` → BME280 (Température / Humidité / Pression)
* `sLUM` → Luminosité analogique (LDR)

### **D. Modes de fonctionnement**

Modes disponibles :

* `STANDARD`
* `ECONOMIQUE`
* `MAINTENANCE`
* `CONFIGURATION`

Chaque mode possède :

* Une couleur LED associée
* Un comportement dans la fonction `loop()`
* Un intervalle de log différent

Gestion via :

* `enterMode()`
* `tickStd()`, `tickEco()`, `tickMaint()`, `tickCfg()`

### **E. Système de log (carte SD)**

Fichiers nommés automatiquement : `LOG000.CSV`, `LOG001.CSV`, …

Fonctions :

* `openNewLog()` — création fichier + header CSV
* `rotateIfNeed()` — changement de fichier quand taille max atteinte
* `logOnce()` — lecture des capteurs + écriture CSV

### **F. Gestion des commandes (mode CONFIGURATION)**

Commandes via Moniteur Série :

* `LOG_INTERVAL=<sec>`
* `TIMEOUT=<s>`
* `FILE_MAX_SIZE=<octets>`
* `CAPTEUR=BME:on/off`
* `CAPTEUR=LUM:on/off`
* `DATE=YYYY-MM-DD HH:MM:SS`
* `READ`
* `RESET`
* `EJECT`
* `HELP`

Fonctions associées :

* `handleCmdChar()` — lecture caractère par caractère
* `handleCmdLine()` — parsing complet
* `showHelp()`

### **G. Gestion des interruptions (boutons)**

Deux interrupteurs :

* Bouton vert : passage STANDARD ↔ ÉCONOMIQUE
* Bouton rouge : passage MAINTENANCE ↔ retour mode précédent

Interruptions :

* `isrV()`
* `isrR()`

### **H. Gestion des erreurs (diagnostic via LED RGB)**

Erreurs possibles :

* `ERR_RTC` — horloge absente
* `ERR_SENSOR` — capteur manquant
* `ERR_INCOH` — valeurs incohérentes
* `ERR_SD_FULL` — carte pleine
* `ERR_SD_RW` — erreur lecture/écriture

Affichage via un clignotement codé : `updateErrorLED()`.

### **I. Boucle principale (loop)**

La fonction `loop()` exécute :

1. Gestion des flags de mode (issus des ISR)
2. Lecture des commandes série
3. Exécution du comportement du mode actif
4. Retour automatique CONFIG → STANDARD après timeout
5. Mise à jour LED d’erreur
6. Petite pause `delay(3)` pour stabiliser

---

## 3. Diagramme logique du programme

```
            +----------------+
            |    setup()     |
            +--------+-------+
                     |
                     v
+--------------------------------------+
|     Chargement configuration EEPROM   |
+--------------------------------------+
                     |
                     v
+--------------------------------------+
| Initialisation RTC / SD / Capteurs   |
+--------------------------------------+
                     |
                     v
+--------------+     +----------------------------+
| Bouton Rouge | --> | Entrer en CONFIGURATION    |
+--------------+     +----------------------------+
                     |
                     v
              +---------------+
              | Mode STANDARD |
              +-------+-------+
                      |
                      v
+--------------------------------------------------------+
| loop()                                                  |
|  • Gestion ISR (changement mode)                       |
|  • Lecture commandes série (si CONFIG)                 |
|  • Exécution du mode actif (tickStd/tickEco/...)       |
|  • Log capteurs si nécessaire                          |
|  • Mise à jour LED d’erreur                            |
+--------------------------------------------------------+
```

---

## 4. Organisation du code dans le fichier .ino

Le fichier Arduino est structuré comme suit :

```
1. Commentaires et description du projet
2. #define et constantes globales
3. Déclarations capteurs et structures
4. Fonctions de configuration (EEPROM)
5. Fonctions capteurs (init + read)
6. Système de log CSV
7. Commandes série (HELP, READ, etc.)
8. Gestion LED / erreurs
9. Modes de fonctionnement
10. setup()
11. loop()
```

---

## 5. Flux d’exécution simplifié

### Au démarrage :

1. LED blanche
2. Setup boutons + interruptions
3. Chargement configuration EEPROM
4. Initialisation RTC, SD, capteurs
5. Entrée dans le mode par défaut (STANDARD)

### En fonctionnement :

* Lecture des capteurs à intervalle régulier selon le mode
* Sauvegarde dans fichier CSV
* Réaction boutons (changement mode)
* Analyse de commandes série
* Surveillance erreurs

---

## 6. Conclusion

Cette architecture est conçue pour être :

* **modulaire** (capteurs génériques, modes séparés)
* **fiable** (EEPROM + CRC + LED erreur)
* **extensible** (ajout facile de nouveaux capteurs ou modes)
* **orientée utilisateur** (commands série pour config fine)

Elle offre une base solide pour une station météo embarquée évolutive et robuste utilisable dans un contexte maritime comme prévu dans le projet [WWW](http://WWW).
