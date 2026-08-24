/*
  core/i18n.h — Localisation pAKAman (5 langues), sur le modele de mAKArena.
  La police du composant etant ASCII, les textes sont volontairement SANS accent.
  La persistance (langue + volume + musique) est sur SD, voir core/settings.
*/
#pragma once
#include <cstdint>

namespace i18n {

enum Lang : uint8_t { FR = 0, EN, DE, ES, IT, LANG_COUNT };

enum Str : uint16_t {
    STR_PRESS_A_START = 0,
    STR_MENU,
    STR_PLAY,
    STR_RESUME,
    STR_MUSIC,
    STR_VOLUME,
    STR_LANGUAGE,
    STR_SCORES,
    STR_HIGHSCORES,
    STR_NO_SCORE,
    STR_CONTROLS,
    STR_RECALIBRATE,
    STR_SCREENSHOT,
    STR_RETURN_LOADER,
    STR_BACK,
    STR_ON,
    STR_OFF,
    STR_READY,
    STR_GAMEOVER,
    STR_PRESS_A_RETRY,
    STR_PAUSE,
    STR_PRESS_A_RESUME,
    STR_QUIT,
    STR_SHOT_SAVED,
    STR_LANG_NAME,          // nom de la langue courante (Francais, English...)
    STR_COUNT
};

const char* T(Str s);

void  set_language(Lang l);
Lang  get_language();
void  next_language();      // cycle FR -> EN -> DE -> ES -> IT -> FR

// Charge/recharge les tables JSON pour la langue courante depuis la SD :
//   /sdcard/AKA/lang/<code>.json      (COMMUN a tous les jeux AKA)
//   /sdcard/PAKAMAN/lang/<code>.json  (SPECIFIQUE a pAKAman, prioritaire)
// Meme convention que les autres portages AKA (aka_runtime) : un fichier
// plat {"CLE": "texte"}, la cle specifique au jeu l'emporte en cas de
// doublon. Si la SD ou les fichiers sont absents, T() retombe sur la table
// C++ codee en dur ci-dessous (jamais de texte manquant/crash). Appelee
// automatiquement par set_language() ; a appeler une fois manuellement au
// demarrage (apres sd_init()) pour charger la langue par defaut.
void reload_sd_tables();

// Verifie que la table K[LANG_COUNT][STR_COUNT] ne contient aucune entree
// manquante (nullptr) : une ligne de langue plus courte que STR_COUNT est
// silencieusement completee a nullptr par le compilateur (aggregate init),
// ce qui plante T() au runtime SEULEMENT quand cette langue/entree precise
// est affichee. A appeler une fois au demarrage (app_main), avant toute
// langue non-FR ne soit jamais testee manuellement.
// Retourne true si la table est complete ; loggue chaque trou sinon.
bool check_table_integrity();

} // namespace i18n
