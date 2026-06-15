#pragma once
/*
============================================================
  persist.h — Persistance : highscores & réglages audio
------------------------------------------------------------
  HighScore est la structure commune utilisée par persist.cpp
  ET par ui/highscores.cpp (plus besoin de HighscoreEntry
  séparé).
============================================================
*/

#include <vector>
#include <cstdint>

// ---------------------------------------------------------------------------
//  Structure commune pour les scores
//  (remplace l'ancienne HighscoreEntry dans highscores.h)
// ---------------------------------------------------------------------------
struct HighScore {
    char    name[9];   // 8 caractères + '\0'
    int32_t score;
};

// ---------------------------------------------------------------------------
//  Highscores (SD card)
// ---------------------------------------------------------------------------
void persist_load(std::vector<HighScore>& scores);
void persist_save(const std::vector<HighScore>& scores);

// ---------------------------------------------------------------------------
//  Réglages audio (SD card)
// ---------------------------------------------------------------------------
bool audio_settings_load();
bool audio_settings_save();
