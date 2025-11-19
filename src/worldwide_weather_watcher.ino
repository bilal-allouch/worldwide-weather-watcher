// ============================================================================
// Projet : Worldwide Weather Watcher (WWW)
// Carte  : Arduino UNO (ou compatible)
// ---------------------------------------------------------------------------
// Ce programme transforme l'Arduino en mini-station météo :
//   - Mesure température / humidité / pression avec un BME280 (I2C)
//   - Mesure la luminosité avec une LDR sur l'entrée analogique A0
//   - Enregistre régulièrement les mesures dans un fichier CSV sur carte SD
//   - Gère plusieurs modes de fonctionnement (STANDARD, ÉCONOMIQUE, etc.)
//   - Affiche un retour texte sur le moniteur série
//   - Utilise une LED RGB chaînable pour indiquer l'état / les erreurs
//   - Sauvegarde la configuration dans l'EEPROM (intervalle de log, capteurs actifs...)
// ============================================================================

#define PRETTY_UI 1   // Mettre à 0 pour une sortie série plus « brute » (sans emoji)

#include <Wire.h>      // Bus I2C → utilisé pour le BME280 et l'horloge RTC
#include <SPI.h>       // Bus SPI → utilisé pour la carte SD
#include <SD.h>        // Gestion de la carte SD
#include <EEPROM.h>    // Mémoire EEPROM interne pour stocker la config
#include <RTClib.h>    // Librairie pour le module RTC DS1307
#include <Adafruit_BME280.h>  // Capteur BME280 (température, humidité, pression)
#include <ChainableLED.h>     // LED RGB chaînable (type Grove Chainable RGB LED)

/* ---------- Définition des broches matérielles ---------- */
#define PIN_LED_DATA 6   // Broche DATA de la LED chaînable
#define PIN_LED_CLK  7   // Broche CLOCK de la LED chaînable
#define PIN_BTN_V    2   // Bouton « vert » (entrée avec interruption)
#define PIN_BTN_R    3   // Bouton « rouge » (entrée avec interruption)
#define PIN_LUM      A0  // Entrée analogique pour la luminosité (LDR)
#define PIN_SD_CS    4   // Chip Select de la carte SD

/* ---------- Type enum pour les couleurs de la LED ---------- */
// On nomme les couleurs pour rendre le code plus lisible.
enum LedColor {
  LED_OFF,  // LED éteinte
  LED_G,    // Vert
  LED_B,    // Bleu
  LED_Y,    // Jaune
  LED_O,    // Orange
  LED_R,    // Rouge
  LED_W     // Blanc
};

/* ---------- Modes de fonctionnement de la station ---------- */
// Chaque mode correspond à un comportement différent :
//   - STANDARD    : mesures régulières à intervalle normal
//   - ECONOMIQUE  : mesures moins fréquentes pour économiser l'énergie
//   - MAINTENANCE : pas de log automatique, pour manipulation / debug
//   - CONFIGURATION : mode « console » pour régler les paramètres

enum Mode {
  STANDARD,
  ECONOMIQUE,
  MAINTENANCE,
  CONFIGURATION
};

/* ---------- Structure de configuration sauvegardée en EEPROM ---------- */
// Cette structure contient tous les paramètres persistants du système.
// Elle est écrite / lue en bloc dans l'EEPROM.
struct Settings {
  uint16_t logMin;   // Intervalle entre deux logs (en secondes)
  uint16_t toutSec;  // Timeout générique (en secondes) → non exploité partout mais prévu
  uint32_t maxBytes; // Taille maximale d'un fichier log avant rotation
  uint8_t  enMask;   // Masque de capteurs activés (bit 0 = BME, bit 1 = LUM)
  uint16_t crc;      // CRC simple pour vérifier l'intégrité de la structure
};

// Déclaration anticipée de la structure Sensor (utilisée plus bas)
struct Sensor;

/* ---------- Prototypes des fonctions ---------- */
// Fonctions liées à la configuration / EEPROM
uint16_t crc16(const Settings &s);
void     setDefaults(void);
void     saveEEPROM(void);
void     loadEEPROM(void);

// Fonctions LED
void     setLed(enum LedColor c);

// Fonctions d'interruption (gestion des boutons)
void     isrV(void);
void     isrR(void);

// Fonctions capteurs
bool     initBME(struct Sensor *s);
void     readBME(struct Sensor *s, float *t, float *h, float *p);
bool     initLUM(struct Sensor *s);
void     readLUM(struct Sensor *s, float *t, float *h, float *p);

// Fonctions de log et modes
void     logOnce(void);
void     enterMode(enum Mode m);
void     tickStd(void);
void     tickEco(void);
void     tickMaint(void);
void     tickCfg(void);
bool     openNewLog(void);
void     rotateIfNeed(void);

// Gestion des commandes série
void     handleCmdChar(char c);
void     handleCmdLine(void);
void     showHelp(void);

// Fonctions d'affichage / formatage
void     printDateTime(Print &out);
void     printNum(Print &out, float v, uint8_t prec);
void     labelVal(
          Print &out,
          const __FlashStringHelper *label,
          float v,
          uint8_t prec,
          const __FlashStringHelper *unit
        );

/* ---------- Flags d'erreur pour la LED ---------- */
// Chaque bit représente une catégorie d'erreurs. On pourra combiner plusieurs
// erreurs en même temps et la LED affichera la plus prioritaire.

enum ErrFlags {
  ERR_NONE   = 0,        // Aucun problème
  ERR_RTC    = 1 << 0,   // Problème d'horloge temps réel
  ERR_SENSOR = 1 << 2,   // Capteur manquant ou défectueux
  ERR_INCOH  = 1 << 3,   // Mesures incohérentes (valeurs hors plage)
  ERR_SD_FULL= 1 << 4,   // Fichier / carte SD pleine ou limite atteinte
  ERR_SD_RW  = 1 << 5    // Erreur lecture/écriture SD
};

// Variable globale contenant les erreurs en cours (combinaison de bits)
volatile uint8_t gErrors = ERR_NONE;

// Met à jour l'état de la LED en fonction des erreurs
void updateErrorLED(void);

/* ---------- Objets globaux des librairies ---------- */
//  - led : une LED RGB chaînable
//  - rtc : horloge temps réel DS1307
//  - bme : capteur BME280

ChainableLED   led(PIN_LED_DATA, PIN_LED_CLK, 1);
RTC_DS1307     rtc;
Adafruit_BME280 bme;

/* ---------- Flags de demande de changement de mode (depuis les ISR) ---------- */
// Ces variables sont "volatile" car modifiées dans les interruptions.

volatile bool fEco   = false;  // Demande de passage en mode ÉCONOMIQUE
volatile bool fMaint = false;  // Demande de passage en mode MAINTENANCE
volatile bool fStd   = false;  // Demande de retour en STANDARD
volatile bool fQuitM = false;  // Demande de quitter MAINTENANCE et revenir au mode précédent

// Mode courant et mode précédent (pour MAINTENANCE)
enum Mode modeCur  = STANDARD;
enum Mode modePrev = STANDARD;

// Détection d'appui long sur les boutons
const unsigned long HOLD_MS = 2000;  // Durée (ms) d'appui pour valider un changement de mode
volatile unsigned long tV   = 0;      // Timestamp de l'appui sur bouton vert
volatile unsigned long tR   = 0;      // Timestamp de l'appui sur bouton rouge

// Présence des périphériques / capteurs
bool haveRTC = false;
bool haveSD  = false;
bool enBME   = true;   // BME activé ou non (peut être modifié par commande)
bool enLUM   = true;   // Capteur luminosité activé ou non

// Instance globale de configuration
Settings cfg;

// Valeurs par défaut de configuration
const uint16_t DEF_LOG_MIN = 10;   // Intervalle de log par défaut (en secondes)
const uint16_t DEF_TOUT    = 3;    // Timeout (en secondes)
const uint32_t DEF_MAXB    = 2048; // Taille max d'un fichier log en octets

// Gestion des fichiers de log sur la carte SD
File   logFile;                   // Fichier CSV courant
char   logName[16] = "LOG000.CSV"; // Nom du fichier courant
uint16_t logIdx    = 0;           // Compteur de fichiers (LOG000, LOG001, ...)

/* ---------- Structure générique pour décrire un capteur ---------- */
// Cela permet de traiter différents capteurs de manière uniforme.
// - name   : nom du capteur (pour debug)
// - en     : pointeur vers un bool global indiquant s'il est activé
// - present: vrai si le capteur est détecté au démarrage
// - initFn : pointeur vers la fonction d'initialisation
// - readFn : pointeur vers la fonction de lecture

struct Sensor {
  const char *name;
  bool       *en;
  bool        present;
  bool      (*initFn)(struct Sensor *);
  void      (*readFn)(struct Sensor *, float *, float *, float *);
};

// Description du capteur BME280
Sensor sBME = {
  "BME",     // Nom
  &enBME,     // Capteur activable via configuration
  false,      // Non présent par défaut, sera mis à jour par initBME
  initBME,
  readBME
};

// Description du capteur de luminosité (LDR sur A0)
Sensor sLUM = {
  "LUM",     // Nom
  &enLUM,
  true,       // Toujours « présent » car simple entrée analogique
  initLUM,
  readLUM
};

// Tableau de pointeurs vers les capteurs à initialiser dans setup()
Sensor *sInit[] = {
  &sBME,
  &sLUM
};

/* ---------- Gestion de la LED de statut ---------- */
// baseColor : couleur de base (selon le mode)
// ledBeatMs : sert pour le clignotement d'erreur
// ledPhase  : phase actuelle du clignotement (A/B)

LedColor       baseColor  = LED_W;
unsigned long  ledBeatMs  = 0;
bool           ledPhase   = false;

// Met la LED à une couleur prédéfinie (voir enum LedColor)
void setLed(enum LedColor c) {
  switch (c) {
    case LED_G:
      led.setColorRGB(0, 0, 255, 0);       // Vert pur
      break;
    case LED_B:
      led.setColorRGB(0, 0, 0, 255);       // Bleu pur
      break;
    case LED_Y:
      led.setColorRGB(0, 255, 255, 0);     // Jaune (rouge + vert)
      break;
    case LED_O:
      led.setColorRGB(0, 255, 100, 0);     // Orange
      break;
    case LED_R:
      led.setColorRGB(0, 255, 0, 0);       // Rouge
      break;
    case LED_W:
      led.setColorRGB(0, 255, 255, 255);   // Blanc
      break;
    default:
      led.setColorRGB(0, 0, 0, 0);         // Éteint
      break;
  }
}

/* ---------- Gestion de la configuration (EEPROM) ---------- */
// Calcul d'un pseudo-CRC (somme + XOR) pour vérifier que la structure lue
// en EEPROM est cohérente et qu'elle n'a pas été corrompue.

uint16_t crc16(const Settings &s) {
  const uint8_t *p = (const uint8_t *)&s;
  uint16_t       sum = 0;

  // On ne prend pas en compte le champ crc lui-même
  for (size_t i = 0; i < sizeof(Settings) - 2; i++) {
    sum += p[i];
  }

  return sum ^ 0xA55A;   // Petit « sel » pour varier le résultat
}

// Remise à des valeurs de configuration raisonnables par défaut
void setDefaults(void) {
  cfg.logMin   = DEF_LOG_MIN;
  cfg.toutSec  = DEF_TOUT;
  cfg.maxBytes = DEF_MAXB;
  cfg.enMask   = 0;

  // On encode l'état des capteurs dans enMask
  if (enBME) {
    cfg.enMask |= 1;   // bit 0 → BME
  }
  if (enLUM) {
    cfg.enMask |= 2;   // bit 1 → LUM
  }

  cfg.crc = crc16(cfg);
}

// Sauvegarde la configuration actuelle dans l'EEPROM
void saveEEPROM(void) {
  // Met à jour le masque de capteurs à partir des bool globaux
  cfg.enMask = 0;

  if (enBME) {
    cfg.enMask |= 1;
  }
  if (enLUM) {
    cfg.enMask |= 2;
  }

  cfg.crc = crc16(cfg);
  EEPROM.put(0, cfg);   // Écriture à partir de l'adresse 0
}

// Lecture de la configuration depuis l'EEPROM
void loadEEPROM(void) {
  EEPROM.get(0, cfg);

  // Vérifie la validité via le CRC. Si invalide → valeurs par défaut.
  if (crc16(cfg) != cfg.crc) {
    setDefaults();
    saveEEPROM();
  }

  // Applique le masque aux bool de capteurs
  enBME = cfg.enMask & 1;
  enLUM = cfg.enMask & 2;
}

/* ---------- ISR (Interruptions des boutons, niveau actif bas) ---------- */
// Les boutons sont câblés en logique inversée (LOW = appuyé). On détecte
// un appui long (HOLD_MS) pour basculer d'un mode à l'autre.

void isrV(void) {
  int lv = digitalRead(PIN_BTN_V);

  if (lv == LOW) {
    // Début d'appui → on mémorise le temps
    tV = millis();
  } else {
    // Relâchement → on regarde si l'appui était suffisamment long
    if (tV && (millis() - tV >= HOLD_MS)) {
      if (modeCur == ECONOMIQUE) {
        // Depuis ÉCO → retour en STANDARD
        fStd = true;
      } else {
        // Sinon → passage en ÉCO
        fEco = true;
      }
    }
    tV = 0;
  }
}

void isrR(void) {
  int lr = digitalRead(PIN_BTN_R);

  if (lr == LOW) {
    tR = millis();
  } else {
    if (tR && (millis() - tR >= HOLD_MS)) {
      if (modeCur == MAINTENANCE) {
        // Si on est déjà en MAINTENANCE → quitter ce mode
        fQuitM = true;
      } else {
        // Sinon → entrer en MAINTENANCE
        fMaint = true;
      }
    }
    tR = 0;
  }
}

/* ---------- Gestion des fichiers de log sur carte SD ---------- */
// Ouvre un nouveau fichier de log (LOG000.CSV, LOG001.CSV, etc.).
// Si un ancien fichier était ouvert, il est d'abord fermé.

bool openNewLog(void) {
  if (logFile) {
    logFile.flush();
    logFile.close();
  }

  snprintf(logName, sizeof(logName), "LOG%03u.CSV", logIdx++);
  logFile = SD.open(logName, FILE_WRITE);

  if (!logFile) {
    // Échec d'ouverture → probablement carte pleine ou problème SD
    gErrors |= ERR_SD_FULL;

#if PRETTY_UI
    Serial.println(F("🟥📁 SD: ouverture fichier échouée (pleine ?"));
#else
    Serial.println(F("SD open fail"));
#endif
    return false;
  }

  gErrors &= ~ERR_SD_FULL;

  // Si le fichier vient d'être créé, on ajoute la ligne d'en-tête CSV
  if (logFile.size() == 0) {
    logFile.println(F("DateTime;TempC;Hum%;hPa;Lum"));
  }

#if PRETTY_UI
  Serial.print(F("📄 Nouveau log: "));
  Serial.println(logName);
#else
  Serial.print(F("LOG: "));
  Serial.println(logName);
#endif
  return true;
}

// Vérifie si le fichier courant a dépassé la taille maximale autorisée.
// Si oui, on bascule automatiquement sur un nouveau fichier.
void rotateIfNeed(void) {
  if (logFile && (uint32_t)logFile.size() >= cfg.maxBytes) {
    openNewLog();
  }
}

/* ---------- Initialisation et lecture des capteurs ---------- */
// Initialisation du BME280 (on tente d'abord l'adresse 0x76 puis 0x77)

bool initBME(struct Sensor *s) {
  bool ok = bme.begin(0x76);

  if (!ok) {
    ok = bme.begin(0x77);
  }

  s->present = ok;

#if PRETTY_UI
  Serial.println(
    ok ? F("✅ BME280 détecté") : F("❌ BME280 absent")
  );
#else
  Serial.println(
    ok ? F("BME ok") : F("BME abs")
  );
#endif

  return ok;
}

// Initialisation du capteur de luminosité (simple entrée analogique)
bool initLUM(struct Sensor *s) {
  pinMode(PIN_LUM, INPUT);
  s->present = true;  // On considère qu'il est toujours présent

#if PRETTY_UI
  Serial.println(F("✅ Capteur luminosité prêt"));
#else
  Serial.println(F("LUM ok"));
#endif

  return true;
}

// Lecture du BME280. Si le capteur n'est pas présent ou désactivé, on
// renvoie des NaN pour signaler que la valeur n'est pas valable.

void readBME(struct Sensor *s, float *t, float *h, float *p) {
  if (!s->present || !(*s->en)) {
    *t = *h = *p = NAN;
    return;
  }

  *t = bme.readTemperature();
  *h = bme.readHumidity();
  *p = bme.readPressure() / 100.0f;   // Conversion en hPa
}

// Lecture de la luminosité sur l'entrée analogique.
// On utilise seulement *p pour stocker la valeur de lumière (0..1023).

void readLUM(struct Sensor *s, float *t, float *h, float *p) {
  (void)t;  // Non utilisé
  (void)h;  // Non utilisé

  if (!(*s->en)) {
    *p = NAN;
    return;
  }

  *p = (float)analogRead(PIN_LUM);
}

/* ---------- Fonctions d'impression / formatage ---------- */
// Affiche la date/heure actuelle issue du RTC au format "YYYY-MM-DD HH:MM:SS".

void printDateTime(Print &out) {
  if (!haveRTC) {
    out.print(F("NA"));
    return;
  }

  DateTime n = rtc.now();
  char     b[20];

  snprintf(
    b,
    sizeof(b),
    "%04u-%02u-%02u %02u:%02u:%02u",
    n.year(), n.month(), n.day(),
    n.hour(), n.minute(), n.second()
  );

  out.print(b);
}

// Affiche un nombre flottant avec un nombre de décimales précisé.
// Si la valeur est NaN, affiche "NA".

void printNum(Print &out, float v, uint8_t prec) {
  if (isnan(v)) {
    out.print(F("NA"));
    return;
  }

  char buf[16];
  dtostrf(v, 0, prec, buf);
  out.print(buf);
}

// Affiche une paire label=valeur[ unité]
// Exemple : "🌡️ Temp=23.4 °C"

void labelVal(
  Print &out,
  const __FlashStringHelper *label,
  float v,
  uint8_t prec,
  const __FlashStringHelper *unit
) {
  out.print(label);
  out.print('=');
  printNum(out, v, prec);

  if (unit) {
    out.print(' ');
    out.print(unit);
  }
}

/* ---------- Acquisition et log d'une ligne de mesures ---------- */
// logOnce() réalise les actions suivantes :
//   1. Lire les capteurs (BME + LUM)
//   2. Vérifier les incohérences simples (valeurs hors plage physique)
//   3. Afficher les mesures sur le port série
//   4. Les enregistrer dans le fichier CSV si la SD est disponible

void logOnce(void) {
  float t = NAN;
  float h = NAN;
  float p = NAN;
  float l = NAN;

  // Lecture BME
  sBME.readFn(&sBME, &t, &h, &p);
  // Lecture luminosité (on n'utilise que le 4e paramètre)
  sLUM.readFn(&sLUM, NULL, NULL, &l);

  // Si on pense que le BME devrait être là mais qu'il n'est pas présent
  if (enBME && !sBME.present) {
    gErrors |= ERR_SENSOR;
  }

  // Vérification d'incohérences simples (plages physiques simplifiées)
  bool incoh = false;

  if (!isnan(t) && (t < -40.0 || t > 85.0)) {
    incoh = true;
  }
  if (!isnan(h) && (h < 0.0 || h > 100.0)) {
    incoh = true;
  }
  if (!isnan(p) && (p < 300.0 || p > 1100.0)) {
    incoh = true;
  }
  if (!isnan(l) && (l < 0.0 || l > 1023.0)) {
    incoh = true;
  }

  if (incoh) {
    gErrors |= ERR_INCOH;
  } else {
    gErrors &= ~ERR_INCOH;
  }

#if PRETTY_UI
  // Affichage « joli » avec emojis
  Serial.print(F("⏱ "));
  printDateTime(Serial);
  Serial.print(F(" | "));
  labelVal(Serial, F("🌡️ Temp"),  t, 1, F("°C"));
  Serial.print(F("  | "));
  labelVal(Serial, F("💧 Hum"),   h, 0, F("%"));
  Serial.print(F("  | "));
  labelVal(Serial, F("🧭 Press"), p, 1, F("hPa"));
  Serial.print(F("  | "));
  labelVal(Serial, F("💡 Lum"),   l, 0, F(""));
  Serial.println();
#else
  // Affichage plus compact (sans emojis)
  Serial.print(F("[MEAS] "));
  printDateTime(Serial);
  Serial.print(F(" | T="));
  printNum(Serial, t, 1);
  Serial.print(F("C"));
  Serial.print(F(" | H="));
  printNum(Serial, h, 0);
  Serial.print(F("%"));
  Serial.print(F(" | P="));
  printNum(Serial, p, 1);
  Serial.print(F("hPa"));
  Serial.print(F(" | L="));
  printNum(Serial, l, 0);
  Serial.println();
#endif

  // Enregistrement sur carte SD si disponible
  if (haveSD) {
    if (logFile) {
      printDateTime(logFile);
      logFile.print(';');
      printNum(logFile, t, 1);
      logFile.print(';');
      printNum(logFile, h, 0);
      logFile.print(';');
      printNum(logFile, p, 1);
      logFile.print(';');
      printNum(logFile, l, 0);
      logFile.println();
      logFile.flush();
      rotateIfNeed();
      gErrors &= ~ERR_SD_RW;
    } else {
      gErrors |= ERR_SD_RW;
    }
  } else {
    gErrors |= ERR_SD_RW;
  }
}

/* ---------- Gestion des modes ---------- */
// enterMode() modifie :
//   - le mode courant
//   - la couleur de base de la LED
//   - un message explicite sur le port série

void enterMode(enum Mode m) {
  modeCur = m;

  switch (m) {
    case STANDARD:
      baseColor = LED_G;   // Vert
#if PRETTY_UI
      Serial.println(F("🟢 Mode STANDARD — mesure toutes 10 s"));
#else
      Serial.println(F("[STD] 10s"));
#endif
      break;

    case ECONOMIQUE:
      baseColor = LED_B;   // Bleu
#if PRETTY_UI
      Serial.println(F("🔵 Mode ÉCONOMIQUE — mesure toutes 20 s"));
#else
      Serial.println(F("[ECO] 20s"));
#endif
      break;

    case MAINTENANCE:
      baseColor = LED_O;   // Orange
#if PRETTY_UI
      Serial.println(F("🟠 Mode MAINTENANCE — logging désactivé. Tapez READ / EJECT"));
#else
      Serial.println(F("[MAINT]"));
#endif
      break;

    case CONFIGURATION:
      baseColor = LED_Y;   // Jaune
#if PRETTY_UI
      Serial.println(F("🟡 Mode CONFIGURATION — tapez HELP pour l’aide."));
#else
      Serial.println(F("[CFG]"));
#endif
      break;
  }

  // Applique immédiatement la couleur de base
  setLed(baseColor);
}

// Fonctions qui retournent l'intervalle de log en fonction du mode.
// Ici elles se basent sur cfg.logMin, ce qui permet de changer facilement
// la période effective via la configuration.

unsigned long intStd(void) {
  // Intervalle en mode STANDARD (par défaut : 10 s)
  return (unsigned long)cfg.logMin * 1000UL;
}

unsigned long intEco(void) {
  // Intervalle en mode ÉCONOMIQUE (ici 2× plus long que STANDARD)
  return (unsigned long)cfg.logMin * 2000UL;
}

// tickStd() et tickEco() sont appelées en permanence dans loop() lorsque
// le mode correspondant est actif. Elles déclenchent un log à intervalle
// régulier sans utiliser delay() (sauf le petit delay(3) en fin de loop()).

void tickStd(void) {
  static unsigned long t = 0;
  unsigned long       n = millis();

  if (t == 0 || n - t >= intStd()) {
    t = n;
    logOnce();
  }
}

void tickEco(void) {
  static unsigned long t = 0;
  unsigned long       n = millis();

  if (t == 0 || n - t >= intEco()) {
    t = n;
    logOnce();
  }
}

// En MAINTENANCE et CONFIGURATION, le comportement est volontairement
// "silencieux" (pas de log périodique automatique).

void tickMaint(void) {
  /* silencieux */
}

void tickCfg(void) {
  /* silencieux aussi */
}

/* ---------- Aide (mode configuration) ---------- */
// showHelp() affiche la liste des commandes série disponibles pour régler
// la configuration ou interagir avec le système.

void showHelp(void) {
#if PRETTY_UI
  Serial.println(F("\n===== AIDE CONFIG ====="));
  Serial.println(F(" LOG_INTERVAL=<sec>      ex: LOG_INTERVAL=10"));
  Serial.println(F(" TIMEOUT=<s>             ex: TIMEOUT=3"));
  Serial.println(F(" FILE_MAX_SIZE=<octets>  ex: FILE_MAX_SIZE=4096"));
  Serial.println(F(" CAPTEUR=BME:on|off      ex: CAPTEUR=BME:on"));
  Serial.println(F(" CAPTEUR=LUM:on|off      ex: CAPTEUR=LUM:off"));
  Serial.println(F(" DATE=YYYY-MM-DD HH:MM:SS  ex: DATE=2025-10-28 14:20:00"));
  Serial.println(F(" READ   → mesure immédiate"));
  Serial.println(F(" EJECT  → sécuriser retrait SD"));
  Serial.println(F(" RESET  → paramètres par défaut"));
  Serial.println(F("=======================\n"));
#else
  Serial.println(
    F("CMDS: LOG_INTERVAL=<s>, TIMEOUT=<s>, FILE_MAX_SIZE=<o>, CAPTEUR=BME:on|off, CAPTEUR=LUM:on|off, DATE=YYYY-MM-DD HH:MM:SS, READ, EJECT, RESET")
  );
#endif
}

/* ---------- Gestion des commandes série ---------- */
// Les commandes sont tapées dans le Moniteur Série (9600 bauds) et
// terminées par un retour à la ligne. Elles sont mises dans un buffer
// puis analysées dans handleCmdLine().

char    cmdBuf[96];  // Buffer de la commande en cours
uint8_t cmdLen = 0;  // Longueur actuelle

// Appelée à chaque caractère reçu sur le port série
void handleCmdChar(char c) {
  if (c == '\r') {
    return;   // On ignore le CR
  }

  if (c == '\n') {
    // Fin de ligne → on traite la commande complète
    handleCmdLine();
    cmdLen   = 0;
    cmdBuf[0]= 0;
    return;
  }

  // Ajout du caractère au buffer si la taille le permet
  if (cmdLen < sizeof(cmdBuf) - 1) {
    cmdBuf[cmdLen++] = c;
    cmdBuf[cmdLen]   = 0;
  }
}

// Petite fonction utilitaire : teste si s commence par p
bool starts(const char *s, const char *p) {
  while (*p) {
    if (*s++ != *p++) {
      return false;
    }
  }
  return true;
}

// Analyse d'une ligne de commande complète
void handleCmdLine(void) {
  char *s = cmdBuf;

  // Skip des espaces de début
  while (*s == ' ') {
    s++;
  }

  if (*s == 0) {
    return;   // Ligne vide
  }

  // Commande : HELP
  if (strcmp(s, "HELP") == 0) {
    showHelp();
    return;
  }

  // Commande : VERSION
  if (strcmp(s, "VERSION") == 0) {
    Serial.println(F("WWW-Pretty 1.0"));
    return;
  }

  // Commande : LOG_INTERVAL=<sec>
  if (starts(s, "LOG_INTERVAL=")) {
    cfg.logMin = (uint16_t)atoi(s + 13);
    saveEEPROM();
    Serial.println(F("OK LOG_INTERVAL"));
    return;
  }

  // Commande : TIMEOUT=<s>
  if (starts(s, "TIMEOUT=")) {
    cfg.toutSec = (uint16_t)atoi(s + 8);
    saveEEPROM();
    Serial.println(F("OK TIMEOUT"));
    return;
  }

  // Commande : FILE_MAX_SIZE=<octets>
  if (starts(s, "FILE_MAX_SIZE=")) {
    cfg.maxBytes = (uint32_t)atol(s + 14);
    saveEEPROM();
    Serial.println(F("OK FILE_MAX_SIZE"));
    return;
  }

  // Commande : CAPTEUR=BME:on|off ou CAPTEUR=LUM:on|off
  if (starts(s, "CAPTEUR=")) {
    char w[4];
    char val[4];

    if (sscanf(s + 8, "%3[^:]:%3s", w, val) == 2) {
      bool on = (val[0] == 'o' || val[0] == 'O');   // on/off

      if (w[0] == 'B' || w[0] == 'b') {
        enBME = on;
      } else if (w[0] == 'L' || w[0] == 'l') {
        enLUM = on;
      }

      saveEEPROM();
      Serial.println(F("OK CAPTEUR"));
    }
    return;
  }

  // Commande : DATE=YYYY-MM-DD HH:MM:SS
  if (starts(s, "DATE=")) {
    if (!haveRTC) {
      Serial.println(F("ERR RTC"));
      return;
    }

    int y, mo, d, h, mi, se;

    if (sscanf(
          s + 5,
          "%4d-%2d-%2d %2d:%2d:%2d",
          &y,
          &mo,
          &d,
          &h,
          &mi,
          &se
        ) == 6) {
      rtc.adjust(DateTime(y, mo, d, h, mi, se));
      Serial.println(F("OK DATE"));
    } else {
      Serial.println(F("ERR DATE"));
    }
    return;
  }

  // Commande : READ → lancer une mesure immédiate
  if (strcmp(s, "READ") == 0) {
    logOnce();
    return;
  }

  // Commande : EJECT → fermer proprement le fichier pour pouvoir retirer la SD
  if (strcmp(s, "EJECT") == 0) {
    if (logFile) {
      logFile.flush();
      logFile.close();
    }
    Serial.println(F("OK EJECT"));
    return;
  }

  // Commande : RESET → revenir aux paramètres par défaut
  if (strcmp(s, "RESET") == 0) {
    setDefaults();
    saveEEPROM();
    Serial.println(F("OK RESET"));
    return;
  }

  // Si rien ne correspond : commande inconnue
  Serial.println(F("Commande inconnue. HELP"));
}

/* ---------- Gestion des alertes LED en fonction des erreurs ---------- */
// Priorité : SD_RW > SD_FULL > RTC > SENSOR > INCOH
// La LED alterne entre deux couleurs (colA ↔ colB) avec des durées
// différentes selon le type d'erreur pour rendre le diagnostic visuel.

void updateErrorLED() {
  if (gErrors == ERR_NONE) {
    // Aucune erreur → LED = couleur de base du mode
    static LedColor last = LED_OFF;

    if (last != baseColor) {
      setLed(baseColor);
      last = baseColor;
    }
    return;
  }

  uint8_t e =
    (gErrors & ERR_SD_RW)   ? ERR_SD_RW   :
    (gErrors & ERR_SD_FULL) ? ERR_SD_FULL :
    (gErrors & ERR_RTC)     ? ERR_RTC     :
    (gErrors & ERR_SENSOR)  ? ERR_SENSOR  :
                              ERR_INCOH;

  unsigned long now = millis();
  uint16_t      tS  = 220;  // temps court
  uint16_t      tL  = 600;  // temps long
  uint16_t      tE  = 360;  // temps "moyen"

  LedColor      colA  = LED_R;  // Couleur A (souvent rouge)
  LedColor      colB  = LED_W;  // Couleur B (varie selon l'erreur)
  uint16_t      halfA = tE;
  uint16_t      halfB = tE;

  if (e == ERR_RTC) {
    colB  = LED_B;           // Rouge ↔ Bleu
    halfA = halfB = tE;      // Durée égale
  } else if (e == ERR_SENSOR) {
    colB  = LED_G;           // Rouge ↔ Vert
    halfA = halfB = tE;
  } else if (e == ERR_INCOH) {
    colB  = LED_G;           // Rouge court / Vert long
    halfA = tS;
    halfB = tL;
  } else {
    colB  = LED_W;           // Rouge ↔ Blanc
    halfA = (e == ERR_SD_RW) ? tS : tE;
    halfB = (e == ERR_SD_RW) ? tL : tE;
  }

  static uint8_t  lastE = 0xFF;  // Dernière erreur affichée
  static uint16_t cur   = 0;     // Durée du demi-cycle actuel

  if (e != lastE) {
    // Nouvelle erreur → on réinitialise le pattern de clignotement
    lastE     = e;
    ledPhase  = false;
    ledBeatMs = now;
    cur       = halfA;
    setLed(colA);
    return;
  }

  if (now - ledBeatMs >= cur) {
    ledBeatMs = now;
    ledPhase  = !ledPhase;

    if (ledPhase) {
      setLed(colB);
      cur = halfB;
    } else {
      setLed(colA);
      cur = halfA;
    }
  }
}

/* ---------- Setup / Loop principal ---------- */
// lastCmdMs : date de la dernière commande reçue en mode CONFIGURATION
// CFG_BACK  : durée après laquelle on repasse automatiquement en STANDARD

unsigned long       lastCmdMs = 0;
const unsigned long CFG_BACK  = 30000UL;  // 30 s

void setup() {
  Serial.begin(9600);

  // Initialisation de la LED (blanc au démarrage)
  led.init();
  setLed(LED_W);

  // Configuration des boutons avec résistances de pull-up internes
  pinMode(PIN_BTN_V, INPUT_PULLUP);
  pinMode(PIN_BTN_R, INPUT_PULLUP);

  // Attachement des interruptions sur changement d'état
  attachInterrupt(
    digitalPinToInterrupt(PIN_BTN_V),
    isrV,
    CHANGE
  );
  attachInterrupt(
    digitalPinToInterrupt(PIN_BTN_R),
    isrR,
    CHANGE
  );

  // Charge la configuration depuis l'EEPROM
  loadEEPROM();

  // Initialise le bus I2C
  Wire.begin();

  // Initialisation de l'horloge RTC
  haveRTC = rtc.begin();
  if (!haveRTC) {
#if PRETTY_UI
    Serial.println(F("⚠️ RTC non détectée"));
#else
    Serial.println(F("RTC abs"));
#endif
    gErrors |= ERR_RTC;
  }

  // Initialisation de la carte SD
  if (SD.begin(PIN_SD_CS)) {
    haveSD = true;
    openNewLog();
  } else {
    haveSD = false;
#if PRETTY_UI
    Serial.println(F("⚠️ Carte SD absente"));
#else
    Serial.println(F("SD abs"));
#endif
    gErrors |= ERR_SD_RW;
  }

  // Initialisation des capteurs via le tableau sInit
  for (uint8_t i = 0; i < sizeof(sInit) / sizeof(sInit[0]); i++) {
    if (sInit[i]->initFn) {
      sInit[i]->initFn(sInit[i]);
    }
  }

  // Si le BME est censé être actif mais pas détecté → erreur capteur
  if (!sBME.present && enBME) {
    gErrors |= ERR_SENSOR;
  }

  // Si on démarre avec le bouton rouge déjà appuyé → mode CONFIGURATION
  if (digitalRead(PIN_BTN_R) == LOW) {
    enterMode(CONFIGURATION);
  } else {
    enterMode(STANDARD);
  }

#if PRETTY_UI
  Serial.println(F("✦ Aide: tapez HELP (Moniteur série 9600 bauds)"));
#else
  Serial.println(F("HELP pr cmds"));
#endif
}

void loop() {
  // Gestion des demandes de changement de mode (flags mis par les ISR)
  if (fEco) {
    fEco = false;
    enterMode(ECONOMIQUE);
  }

  if (fMaint) {
    fMaint  = false;
    modePrev= modeCur;
    enterMode(MAINTENANCE);
  }

  if (fStd) {
    fStd = false;
    enterMode(STANDARD);
  }

  if (fQuitM) {
    fQuitM = false;
    enterMode(modePrev);
  }

  // Lecture non bloquante du port série
  while (Serial.available()) {
    char c = (char)Serial.read();
    handleCmdChar(c);
    lastCmdMs = millis();
  }

  // Exécution de la tâche périodique selon le mode
  switch (modeCur) {
    case STANDARD:
      tickStd();
      break;

    case ECONOMIQUE:
      tickEco();
      break;

    case MAINTENANCE:
      tickMaint();
      break;

    case CONFIGURATION:
      tickCfg();
      break;
  }

  // En mode CONFIGURATION, si aucune commande n'est reçue pendant CFG_BACK,
  // on repasse automatiquement en mode STANDARD pour reprendre le logging.
  if (modeCur == CONFIGURATION && (millis() - lastCmdMs > CFG_BACK)) {
    enterMode(STANDARD);
  }

  // Mise à jour de la LED d'erreur (clignotement, etc.)
  updateErrorLED();

  // Petit délai pour éviter de surcharger la boucle
  delay(3);
}
