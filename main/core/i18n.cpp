/*
  core/i18n.cpp — Tables de traduction pAKAman. Textes SANS accent (police ASCII).
  L'ordre des colonnes suit l'enum Str de i18n.h.
*/
#include "i18n.h"

namespace i18n {

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

const char* T(Str s) {
    if (s >= STR_COUNT) return "?";
    return K[s_lang][s];
}

void set_language(Lang l) { if (l < LANG_COUNT) s_lang = l; }
Lang get_language()       { return s_lang; }
void next_language()      { s_lang = (Lang)((s_lang + 1) % LANG_COUNT); }

} // namespace i18n
