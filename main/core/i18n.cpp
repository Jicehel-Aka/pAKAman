/*
  core/i18n.cpp — Tables de traduction pAKAman. Textes SANS accent (police ASCII).
  L'ordre des colonnes suit l'enum Str de i18n.h.
*/
#include "i18n.h"
#include <cstdio>
#include <cstring>

namespace i18n {

// -----------------------------------------------------------------------
//  Table de correspondance Str -> nom de cle JSON.
//  Les 5 cles marquees "commune" existent aussi dans /sdcard/AKA/lang/ et
//  sont partagees mot pour mot avec les autres portages AKA (meme
//  convention que aka_runtime : MENU_TITLE, MENU_RESUME, MENU_CONTROLS,
//  MENU_LANGUAGE, MENU_RETURN_LOADER). Le reste est specifique a pAKAman.
// -----------------------------------------------------------------------
static const char* const KEY_NAMES[STR_COUNT] = {
    "TITLE_PRESS_A",        // STR_PRESS_A_START
    "MENU_TITLE",            // STR_MENU              (commune)
    "MENU_PLAY",             // STR_PLAY
    "MENU_RESUME",           // STR_RESUME             (commune)
    "MENU_MUSIC",            // STR_MUSIC
    "MENU_VOLUME",           // STR_VOLUME
    "MENU_LANGUAGE",         // STR_LANGUAGE           (commune)
    "MENU_SCORES",           // STR_SCORES
    "SCORES_TITLE",          // STR_HIGHSCORES
    "SCORES_EMPTY",          // STR_NO_SCORE
    "MENU_CONTROLS",         // STR_CONTROLS           (commune)
    "MENU_RECALIBRATE",      // STR_RECALIBRATE
    "MENU_SCREENSHOT",       // STR_SCREENSHOT
    "MENU_RETURN_LOADER",    // STR_RETURN_LOADER      (commune)
    "COMMON_BACK",           // STR_BACK
    "COMMON_ON",             // STR_ON
    "COMMON_OFF",            // STR_OFF
    "GAME_READY",            // STR_READY
    "GAME_OVER",             // STR_GAMEOVER
    "GAME_RETRY",            // STR_PRESS_A_RETRY
    "GAME_PAUSE",            // STR_PAUSE
    "GAME_RESUME_HINT",      // STR_PRESS_A_RESUME
    "GAME_QUIT",             // STR_QUIT
    "MENU_SHOT_SAVED",       // STR_SHOT_SAVED
    "LANG_NAME",             // STR_LANG_NAME
};

static const char* const LANG_CODES[LANG_COUNT] = { "fr", "en", "de", "es", "it" };

static const char* K[LANG_COUNT][STR_COUNT] = {
  // ---- FR ----
  {
    "Appuyez sur A pour jouer", "Menu", "Jouer", "Reprendre", "Musique",
    "Volume", "Langue", "Scores", "Meilleurs scores", "Aucun score",
    "Commandes", "Recalibrer le stick", "Capture d'ecran", "Retour au loader", "B : retour",
    "ON", "OFF", "READY!", "Game Over", "A : rejouer",
    "Pause", "A : reprendre", "Quitter la partie", "Capture enregistree", "Francais"
  },
  // ---- EN ----
  {
    "Press A to play", "Menu", "Play", "Resume", "Music",
    "Volume", "Language", "Scores", "High scores", "No score yet",
    "Controls", "Recalibrate stick", "Screenshot", "Back to loader", "B: back",
    "ON", "OFF", "READY!", "Game Over", "A: play again",
    "Pause", "A: resume", "Quit game", "Screenshot saved", "English"
  },
  // ---- DE ----
  {
    "A druecken zum Spielen", "Menue", "Spielen", "Weiter", "Musik",
    "Lautstaerke", "Sprache", "Punkte", "Bestenliste", "Kein Ergebnis",
    "Steuerung", "Stick kalibrieren", "Screenshot", "Zum Loader", "B: zurueck",
    "AN", "AUS", "READY!", "Game Over", "A: nochmal",
    "Pause", "A: weiter", "Spiel beenden", "Screenshot gespeichert", "Deutsch"
  },
  // ---- ES ----
  {
    "Pulsa A para jugar", "Menu", "Jugar", "Seguir", "Musica",
    "Volumen", "Idioma", "Puntos", "Mejores puntos", "Sin puntos",
    "Controles", "Calibrar mando", "Captura", "Volver al loader", "B: atras",
    "SI", "NO", "READY!", "Game Over", "A: reintentar",
    "Pausa", "A: seguir", "Salir del juego", "Captura guardada", "Espanol"
  },
  // ---- IT ----
  {
    "Premi A per giocare", "Menu", "Gioca", "Riprendi", "Musica",
    "Volume", "Lingua", "Punteggi", "Migliori punti", "Nessun punteggio",
    "Comandi", "Calibra stick", "Schermata", "Torna al loader", "B: indietro",
    "ON", "OFF", "READY!", "Game Over", "A: rigioca",
    "Pausa", "A: riprendi", "Esci dalla partita", "Schermata salvata", "Italiano"
  },
};

static Lang s_lang = FR;

// -----------------------------------------------------------------------
//  Chargement JSON depuis la SD (parseur minimal, plat {"CLE":"valeur"}) —
//  meme technique que aka_runtime : pas de dependance a une bibliotheque
//  JSON complete, juste assez pour ce format volontairement simple.
// -----------------------------------------------------------------------
#define LANG_MAX_ENTRIES 48
#define LANG_KEY_LEN     24
#define LANG_VAL_LEN     64
static char s_langKeys[LANG_MAX_ENTRIES][LANG_KEY_LEN];
static char s_langVals[LANG_MAX_ENTRIES][LANG_VAL_LEN];
static int  s_langCount = 0;

static void load_language_file_append(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("i18n: %s absent (ok, fallback table C++)\n", path); return; }

    char buf[2048];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    int added = 0;
    const char* p = buf;
    while (*p && s_langCount < LANG_MAX_ENTRIES) {
        while (*p && *p != '"') ++p;
        if (!*p) break;
        ++p;
        const char* keyStart = p;
        while (*p && *p != '"') ++p;
        if (!*p) break;
        size_t keyLen = (size_t)(p - keyStart);
        ++p;
        while (*p && *p != ':') ++p;
        if (!*p) break;
        ++p;
        while (*p && *p != '"') ++p;
        if (!*p) break;
        ++p;
        const char* valStart = p;
        while (*p && *p != '"') {
            if (*p == '\\' && *(p + 1)) ++p;   // ignore les echappements simples
            ++p;
        }
        if (!*p) break;
        size_t valLen = (size_t)(p - valStart);
        ++p;

        if (keyLen > 0 && keyLen < LANG_KEY_LEN && valLen < LANG_VAL_LEN) {
            memcpy(s_langKeys[s_langCount], keyStart, keyLen);
            s_langKeys[s_langCount][keyLen] = '\0';
            memcpy(s_langVals[s_langCount], valStart, valLen);
            s_langVals[s_langCount][valLen] = '\0';
            ++s_langCount; ++added;
        }
    }
    printf("i18n: %s -> %d entrees chargees\n", path, added);
}

static const char* lookup_sd(const char* key) {
    // Ordre inverse : le fichier charge en dernier (specifique au jeu)
    // l'emporte sur le commun en cas de cle en double.
    for (int i = s_langCount - 1; i >= 0; --i)
        if (strcmp(s_langKeys[i], key) == 0) return s_langVals[i];
    return nullptr;
}

void reload_sd_tables() {
    s_langCount = 0;
    const char* code = LANG_CODES[s_lang];
    char pathCommon[48], pathGame[48];
    snprintf(pathCommon, sizeof pathCommon, "/sdcard/AKA/lang/%s.json", code);
    snprintf(pathGame,   sizeof pathGame,   "/sdcard/PAKAMAN/lang/%s.json", code);
    load_language_file_append(pathCommon);
    load_language_file_append(pathGame);
}

const char* T(Str s) {
    if (s >= STR_COUNT) return "?";
    const char* v = lookup_sd(KEY_NAMES[s]);
    if (v) return v;
    return K[s_lang][s];   // filet de securite : jamais de texte manquant
}

void set_language(Lang l) {
    if (l < LANG_COUNT) { s_lang = l; reload_sd_tables(); }
}
Lang get_language()  { return s_lang; }
void next_language() { s_lang = (Lang)((s_lang + 1) % LANG_COUNT); reload_sd_tables(); }

bool check_table_integrity() {
    static const char* LANG_NAMES[LANG_COUNT] = {"FR", "EN", "DE", "ES", "IT"};
    bool ok = true;
    for (int l = 0; l < LANG_COUNT; ++l) {
        for (int s = 0; s < STR_COUNT; ++s) {
            if (K[l][s] == nullptr) {
                printf("i18n: ERREUR - entree manquante langue=%s index=%d "
                       "(verifier la table K[][] dans i18n.cpp)\n", LANG_NAMES[l], s);
                ok = false;
            }
        }
    }
    printf("i18n: integrite table -> %s\n", ok ? "OK" : "ECHEC (voir erreurs ci-dessus)");
    return ok;
}

} // namespace i18n
