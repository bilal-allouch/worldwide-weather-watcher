# Modes de Fonctionnement — Worldwide Weather Watcher

Ce document décrit de manière claire et complète les **4 modes de fonctionnement** de la station météo embarquée WWW, tels qu’ils sont implémentés dans le code Arduino.

---

# 🌕 1. Vue d’ensemble

La station météo dispose de **4 modes principaux**, chacun ayant :

* un **comportement interne spécifique**
* une **couleur LED associée**
* une **logique d’activation** (boutons / logiciel)
* un **intervalle d’acquisition différent** (pour certains modes)

Les modes sont définis dans l’énumération suivante :

```cpp
enum Mode {
  STANDARD,
  ECONOMIQUE,
  MAINTENANCE,
  CONFIGURATION
};
```

---

# 🟢 2. Mode STANDARD (mode normal)

**Mode actif par défaut** au démarrage (si le bouton rouge n’est pas enfoncé).

### 🎯 Objectif

Réaliser des mesures régulières et les enregistrer dans la carte SD.

### 🟩 LED

**Verte** = fonctionnement normal

### ⏱ Intervalle de mesure

Basé sur la configuration :

```cpp
intStd() → cfg.logMin * 1000 ms
```

Par défaut : **10 secondes**.

### 🔧 Actions réalisées

* Lecture BME280 (température, humidité, pression)
* Lecture luminosité (LDR)
* Vérification cohérence des valeurs
* Écriture d’une ligne dans le fichier CSV
* Rotation automatique si taille dépasse `maxBytes`
* Affichage dans le port série (mode PRETTY ou brut)

### 🔁 Sortie / Entrée dans ce mode

toujours accessible via :

* appui long sur **bouton vert** si on est en mode ÉCONOMIQUE
* retour automatique depuis CONFIGURATION après 30s d’inactivité

---

# 🔵 3. Mode ÉCONOMIQUE

Mode activé pour économiser l’énergie.

### 🎯 Objectif

Réduire la consommation électrique.

### 🔵 LED

**Bleue**

### ⏱ Intervalle de mesure

2× plus long que le mode STANDARD :

```cpp
intEco() → cfg.logMin * 2000 ms
```

Par défaut : **20 secondes**.

### 🔧 Différences avec STANDARD

* mesures moins fréquentes
* LED bleue
* idéal en cas de batterie faible

### 🟦 Comment activer ce mode ?

Appui long sur **bouton vert** → `isrV()` active `fEco`.

### 🟩 Comment quitter ce mode ?

Appui long sur **bouton vert** → retour vers STANDARD.

---

# 🟠 4. Mode MAINTENANCE

Mode utilisé pour lire les valeurs en direct et manipuler la carte SD sans risque.

### 🎯 Objectif

Permettre :

* diagnostic technique
* test capteurs
* lecture des mesures en temps réel
* retrait sécurisé de la carte SD

### 🟠 LED

**Orange**

### 🔧 Caractéristiques

* **AUCUN log automatique**
* Les mesures sont affichées via `READ`
* Commande `EJECT` disponible pour sécuriser la SD

### 🟥 Utilité

* débogage
* lecture en direct
* consultation des données sans remplir la SD

### 🟧 Comment activer ce mode

Appui long sur **bouton rouge** → `isrR()` → `fMaint = true`.

### 🔁 Quitter ce mode

Appui long sur **bouton rouge** → retour au **mode précédent** (`modePrev`).

---

# 🟡 5. Mode CONFIGURATION

Mode permettant de configurer finement la station via le Moniteur Série.

### 🟨 LED

**Jaune**

### 🧩 Comment entrer dans ce mode

* **Au démarrage** : bouton rouge appuyé
* Ou depuis le code : passage manuel en CONFIGURATION

### 🕹 Interface série

Ce mode active une série de commandes spécifiques :

* `HELP` → afficher toutes les commandes
* `READ` → prendre une mesure immédiate
* `LOG_INTERVAL=<sec>`
* `TIMEOUT=<s>`
* `FILE_MAX_SIZE=<octets>`
* `CAPTEUR=BME:on|off`
* `CAPTEUR=LUM:on|off`
* `DATE=YYYY-MM-DD HH:MM:SS`
* `RESET` → paramètres par défaut
* `EJECT` → fermeture propre de la SD

### ⏱ Retour automatique

Après **30 secondes d’inactivité**, retour automatique en mode STANDARD.

---

# 🔺 6. Résumé visuel des modes

| Mode          | LED       | Logging    | Bouton activation    | Utilisation      |
| ------------- | --------- | ---------- | -------------------- | ---------------- |
| Standard      | 🟢 Vert   | Oui        | —                    | Normal           |
| Économique    | 🔵 Bleu   | Oui (lent) | Bouton vert          | Batterie faible  |
| Maintenance   | 🟠 Orange | Non        | Bouton rouge         | Diagnostique     |
| Configuration | 🟡 Jaune  | Non        | Bouton rouge au boot | Réglages système |

---

# 🔚 Fin du document

Ce document donne une vision complète des différents modes de fonctionnement implémentés dans la station météo WWW et permet à l’utilisateur ou au développeur de comprendre clairement chaque état du système.
