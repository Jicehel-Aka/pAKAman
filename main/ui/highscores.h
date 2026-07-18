#pragma once
#include <cstdint>

struct HighscoreEntry {
    char    name[9];   // 8 caracteres + '\0'
    int32_t score;
};

constexpr int MAX_SCORES = 6;

void highscores_init();
void highscores_refresh();        // relit la SD dans le cache (a l'entree de l'ecran)
void highscores_submit(int32_t score);
void highscores_show();           // rendu depuis le cache (auto-flush)
