# 📗 Documentation Utilisateur — Worldwide Weather Watcher

Cette documentation explique **comment utiliser la station météo embarquée Worldwide Weather Watcher (WWW)**. Elle est destinée à l’utilisateur final ou à un membre d’équipage sur un navire.

---

# 🟦 1. Présentation générale

La station météo WWW :

* mesure **température**, **humidité**, **pression atmosphérique**, **luminosité**,
* stocke les données sur **carte SD** au format CSV,
* affiche son état via une **LED RGB**,
* propose plusieurs **modes de fonctionnement**,
* possède une interface **console série** (USB) pour les réglages avancés,
* permet un retrait sécurisé de la carte SD.

---

# 🟢 2. Mise en marche

1. Connecter l’Arduino à une alimentation 5V (USB ou batterie).
2. La LED devient **blanche** quelques secondes (initialisation).
3. Le système active ensuite un des deux modes suivants :

   * 🟢 **STANDARD** si aucun bouton n’est appuyé.
   * 🟡 **CONFIGURATION** si le **bouton rouge** est maintenu appuyé pendant la mise sous tension.

---

# 🟦 3. Les boutons (interactions utilisateur)

La station possède **deux boutons poussoirs** :

## 🟩 Bouton vert (V)

* Appui long → Passage STANDARD ↔ ÉCONOMIQUE.

## 🟥 Bouton rouge (R)

* Appui long → Passage MAINTENANCE ↔ retour au mode précédent.
* Appui au démarrage → Entrer en CONFIGURATION.

Un **appui long** = environ **2 secondes**.

---

# 🟩 4. Modes de fonctionnement (résumé utilisateur)

## 🟢 Mode STANDARD — fonctionnement normal

* Mesures automatiques régulières.
* Enregistrement sur carte SD.
* LED **verte**.

## 🔵 Mode ÉCONOMIQUE — économie d’énergie

* Mesures moins fréquentes.
* LED **bleue**.
* Utile quand la batterie est faible.

## 🟠 Mode MAINTENANCE — diagnostic

* Pas d’écriture sur la carte SD.
* Lecture en direct via ordinateur.
* Sécurisation du retrait SD.
* LED **orange**.

## 🟡 Mode CONFIGURATION — réglages avancés

* Accessible uniquement via : appui du bouton rouge au démarrage.
* Configure l’intervalle de mesure, les capteurs, la date, etc.
* LED **jaune**.

---

# 🟥 5. Comprendre la LED RGB

La LED indique l’état actuel de la station.

| Couleur           | Signification                |
| ----------------- | ---------------------------- |
| 🟢 Vert           | Mode STANDARD (normal)       |
| 🔵 Bleu           | Mode ÉCONOMIQUE              |
| 🟠 Orange         | Mode MAINTENANCE             |
| 🟡 Jaune          | Mode CONFIGURATION           |
| ⚪ Blanc           | Initialisation               |
| 🔴/⚪ clignotant   | Erreur carte SD              |
| 🔴/🔵 clignotant  | Erreur RTC                   |
| 🔴/🟢 clignotant  | Capteur manquant             |
| 🔴/🟢 (vert long) | Incohérence dans les mesures |

---

# 💾 6. Lecture des données (fichiers CSV)

Les mesures sont enregistrées automatiquement sur la carte SD.

### 📄 Format d’un fichier

Chaque fichier se nomme :

```
LOG000.CSV
LOG001.CSV
LOG002.CSV
...
```

### 📑 Format d’une ligne CSV

```
YYYY-MM-DD HH:MM:SS;TempC;Hum%;hPa;Lum
```

Exemple :

```
2025-01-15 14:32:10;21.5;45;1013.2;230
```

Les fichiers changent automatiquement lorsqu’ils dépassent la taille limite configurée.

---

# 🧰 7. Interface USB (Console série)

La console série (Moniteur Arduino) permet :

* le diagnostic
* la configuration
* la prise de mesures immédiates

## 🔡 Vitesse : **9600 bauds**

## 🟨 Commandes disponibles

* `HELP` → liste des commandes
* `READ` → prendre une mesure immédiatement
* `LOG_INTERVAL=<sec>` → changer intervalle des mesures
* `FILE_MAX_SIZE=<octets>` → taille max d’un fichier
* `CAPTEUR=BME:on/off` → activer/désactiver BME280
* `CAPTEUR=LUM:on/off` → activer/désactiver LDR
* `DATE=YYYY-MM-DD HH:MM:SS` → régler date RTC
* `RESET` → réinitialiser configuration
* `EJECT` → fermer le fichier pour retirer la SD
* `VERSION` → affiche la version du programme

---

# 💿 8. Retirer la carte SD en sécurité

Avant de retirer la carte SD, taper :

```
EJECT
```

Cela ferme le fichier CSV proprement.

En mode MAINTENANCE, la SD peut être retirée sans risque car elle n’est pas utilisée.

---

# 🛠 9. Dépannage rapide

## ✔ Aucun affichage série

→ Vérifier que le Moniteur Arduino est réglé sur **9600 bauds**.

## ✔ LED rouge/blanc qui clignote

→ Carte SD pleine ou défaillante.

## ✔ LED rouge/bleu

→ RTC absente ou mal connectée.

## ✔ LED rouge/vert

→ Capteur manquant.

## ✔ Valeurs incohérentes

→ Vérifier le capteur BME280 et la LDR.

---

# 🔚 Fin de la documentation utilisateur

Ce document permet à tout utilisateur (équipage, technicien ou étudiant) d’utiliser la station météo WWW en sécurité et d’exploiter ses fonctionnalités.
