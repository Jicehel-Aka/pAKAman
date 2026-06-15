#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct HighscoreEntry {
    char    name[9];
    int32_t score;
};

constexpr int MAX_SCORES = 6;

void highscores_init();
void highscores_refresh();        // relit la SD dans le cache (une fois à l'entrée de l'écran)
void highscores_submit(int32_t score);
void highscores_show();           // rendu depuis cache, PAS de gfx_flush()
