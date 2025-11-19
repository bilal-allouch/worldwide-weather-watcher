# 🌍 Worldwide Weather Watcher (WWW)

### 📡 Station météo embarquée pour navires — Projet CESI 2025

Ce projet transforme une **Arduino UNO (ATmega328P)** en **station météo embarquée intelligente** capable d’effectuer des mesures environnementales, de détecter des erreurs matérielles, d’enregistrer des données sur carte SD et de fonctionner selon plusieurs **modes avancés**.

Développé dans le cadre du **Projet Systèmes Embarqués A2 (CESI Rouen)**.

---

# 🔥 1. Fonctionnalités principales

✔ Mesure **Température / Humidité / Pression** via **BME280 (I2C)**
✔ Mesure **luminosité** via une **LDR (A0)**
✔ Stockage automatique sur **carte SD (SPI)** au format CSV
✔ Gestion de **4 modes** : Standard, Économique, Maintenance, Configuration
✔ LED RGB chaînable indiquant l'état et les erreurs
✔ Console série (9600 bauds) pour configuration avancée
✔ Sauvegarde des paramètres dans **EEPROM** avec contrôle CRC
✔ Rotation automatique des fichiers CSV (`LOG000.CSV`, `LOG001.CSV`, …)

---

# 🧩 2. Matériel utilisé

| Composant                | Rôle                            |
| ------------------------ | ------------------------------- |
| Arduino UNO (ATmega328P) | Microcontrôleur principal       |
| BME280                   | Température, Pression, Humidité |
| LDR sur A0               | Luminosité                      |
| RTC DS1307               | Horodatage                      |
| Carte SD (SPI)           | Stockage CSV                    |
| LED RGB chaînable        | État & Erreurs                  |
| 2 boutons poussoirs      | Navigation entre modes          |

---

# 🔀 3. Modes de fonctionnement

### 🟢 **STANDARD**

* LED : **verte**
* Mesures automatiques régulières
* Écriture sur carte SD

### 🔵 **ÉCONOMIQUE**

* LED : **bleue**
* Intervalle de mesure 2× plus long
* Idéal pour économiser la batterie

### 🟠 **MAINTENANCE**

* LED : **orange**
* Pas d’écriture SD
* Lecture des capteurs en direct via la console
* Retrait sécurisé de la SD

### 🟡 **CONFIGURATION**

* LED : **jaune**
* Commandes série disponibles (HELP, READ, RESET, etc.)
* Se déclenche en maintenant le **bouton rouge** au démarrage

---

# 📂 4. Structure du projet

```
worldwide-weather-watcher/
│
├── src/
│   └── worldwide_weather_watcher.ino
│
├── docs/
│   ├── doc_technique.md
│   ├── architecture_programme.md
│   ├── modes_de_fonctionnement.md
│   └── doc_utilisateur.md
│
└── README.md
```

---

# 🧰 5. Installation & utilisation

### 🔌 Connexion

* Connecter l’Arduino via USB ou alimentation 5V.
* Ouvrir le **Moniteur Série** → 9600 bauds.

### 📄 Fichiers CSV

Générés automatiquement dans la carte SD :

```
LOG000.CSV
LOG001.CSV
LOG002.CSV
```

Format d’une ligne CSV :

```
YYYY-MM-DD HH:MM:SS;TempC;Hum%;hPa;Lum
```

---

# 🧭 6. Commandes série (mode CONFIGURATION)

* `HELP` → aide
* `READ` → prendre une mesure immédiate
* `LOG_INTERVAL=<sec>`
* `FILE_MAX_SIZE=<octets>`
* `CAPTEUR=BME:on|off`
* `CAPTEUR=LUM:on|off`
* `DATE=YYYY-MM-DD HH:MM:SS`
* `RESET`
* `EJECT`
* `VERSION`

---

# 🚨 7. Diagnostic LED

| LED        | Signification      |
| ---------- | ------------------ |
| 🟢 Vert    | Mode Standard      |
| 🔵 Bleu    | Économique         |
| 🟠 Orange  | Maintenance        |
| 🟡 Jaune   | Configuration      |
| ⚪ Blanc    | Initialisation     |
| 🔴/⚪       | Erreur SD          |
| 🔴/🔵      | Erreur RTC         |
| 🔴/🟢      | Capteur manquant   |
| 🔴/🟢 long | Valeur incohérente |

---

# 👥 8. Travail en équipe

Projet réalisé en équipe de **4 étudiants CESI**.
**Rôle personnel : Chef d’équipe** :

* Coordination
* Répartition des tâches
* Validation technique
* Intégration finale
* Démonstration du projet

---

# 📚 9. Documentation complète

* 📘 Technique : `docs/doc_technique.md`
* 🏗 Architecture : `docs/architecture_programme.md`
* 🔀 Modes : `docs/modes_de_fonctionnement.md`
* 📗 Utilisateur : `docs/doc_utilisateur.md`

---

# 🏁 10. Auteur

**Bilal ALLOUCH** — Étudiant en informatique, CESI Rouen

📌 Projet académique — Systèmes embarqués — 2025
