# 📘 Documentation Technique — Worldwide Weather Watcher

Projet : **Worldwide Weather Watcher (WWW)**
Carte : **Arduino UNO (ATmega328P)**
Version code : `WWW-Pretty 1.0`

Ce document décrit le fonctionnement interne de la station météo embarquée et la façon dont le code Arduino est organisé.

---

## 1. Objectif technique

Le programme transforme une carte **Arduino UNO** en mini-station météo embarquée capable de :

* mesurer **température**, **humidité** et **pression** avec un **BME280 (I2C)**
* mesurer la **luminosité** via une **LDR** connectée sur **A0**
* enregistrer régulièrement les mesures dans un **fichier CSV** sur **carte SD**
* gérer plusieurs **modes de fonctionnement** (STANDARD, ÉCONOMIQUE, MAINTENANCE, CONFIGURATION)
* indiquer l’état du système via une **LED RGB chaînable**
* **sauvegarder la configuration** dans l’EEPROM (intervalle, capteurs activés…)

---

## 2. Matériel et câblage

### 2.1. Microcontrôleur

* **Arduino UNO / ATmega328P**
* Alimentation : 5V
* Interfaces exploitées : **I2C**, **SPI**, **analogique**, **digital + interruptions**

### 2.2. Brochage

| Élément                | Broche Arduino    | Remarque                              |
| ---------------------- | ----------------- | ------------------------------------- |
| LED RGB DATA           | PIN_LED_DATA = 6  | Bus 2-wire pour ChainableLED          |
| LED RGB CLK            | PIN_LED_CLK  = 7  |                                       |
| Bouton vert            | PIN_BTN_V    = 2  | Entrée digitale + interruption (INT0) |
| Bouton rouge           | PIN_BTN_R    = 3  | Entrée digitale + interruption (INT1) |
| LDR (luminosité)       | PIN_LUM      = A0 | Entrée analogique                     |
| Carte SD - Chip Select | PIN_SD_CS    = 4  | Bus SPI                               |
| BME280 + RTC           | I2C (A4/A5)       | Bus I2C via la librairie `Wire`       |

### 2.3. Capteurs et modules utilisés

* **BME280** (Adafruit_BME280.h)  → température, humidité, pression
* **RTC DS1307** (RTClib.h)       → date & heure
* **LDR sur A0**                  → luminosité 0–1023
* **Carte SD** (SD.h + SPI)       → fichiers CSV `LOG000.CSV`, `LOG001.CSV`…
* **LED RGB chaînable** (ChainableLED.h)
* **EEPROM interne** (EEPROM.h)

---

## 3. Structure des données

### 3.1. Configuration (`Settings`)

Structure enregistrée en EEPROM contenant :

* intervalle entre deux logs (`logMin`)
* timeout générique (`toutSec`)
* taille max d’un fichier CSV (`maxBytes`)
* capteurs activés (`enMask`)
* vérification d’intégrité (`crc`)

### 3.2. Abstraction capteur (`Sensor`)

Une structure générique pour gérer BME et LDR de manière uniforme :

* nom du capteur
* booléen d’activation
* détection automatique
* pointeur fonction d’init
* pointeur fonction de lecture

Deux instances : `sBME` et `sLUM`.

---

## 4. Gestion des erreurs

Erreur stockées dans `gErrors` sous forme de flags :

* `ERR_SD_RW` → erreur SD
* `ERR_SD_FULL` → fichier/carte pleine
* `ERR_RTC` → RTC absente
* `ERR_SENSOR` → capteur manquant
* `ERR_INCOH` → mesures incohérentes

`updateErrorLED()` gère les priorités d’erreur et les motifs de clignotement.

---

## 5. Flux de fonctionnement

### 5.1. Initialisation (`setup()`)

1. Série 9600 bauds
2. LED blanche
3. Boutons + interruptions
4. Lecture configuration EEPROM
5. I2C
6. RTC
7. SD + ouverture du LOG CSV
8. Init capteurs
9. Choix du mode (bouton rouge → CONFIG)
10. Message HELP

### 5.2. Boucle (`loop()`)

1. Changement de mode via flags d’interruption
2. Lecture commandes série
3. Exécution périodique selon mode :

   * `tickStd()` → log toutes `logMin` secondes
   * `tickEco()` → log toutes `2×logMin` secondes
   * `tickMaint()` → silencieux
   * `tickCfg()` → silencieux
4. Retour auto en STANDARD après 30s en configuration
5. Gestion LED d’erreur
6. `delay(3)`

---

## 6. Mesures et log (`logOnce()`)

1. Lecture BME + LDR
2. Vérification des plages physiques
3. Affichage série (emoji ou brut)
4. Enregistrement CSV + rotation auto

Format CSV :

```
YYYY-MM-DD HH:MM:SS;TempC;Hum%;hPa;Lum
```

---

## 7. Modes de fonctionnement

* **STANDARD** → LED verte, log régulier
* **ÉCONOMIQUE** → LED bleue, log ralenti
* **MAINTENANCE** → LED orange, pas de log auto
* **CONFIGURATION** → LED jaune, commandes série (`HELP`, `READ`, `RESET`, etc.)

Changement via boutons (appui long) ou via le code.

---

## 8. Résumé général

```
[Capteurs] → [logOnce()] → [Série + SD] → [EEPROM conf] → [Gestion LED] → [Modes]
```
