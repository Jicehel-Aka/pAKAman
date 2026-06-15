/*
============================================================
  highscores.cpp
------------------------------------------------------------
  - highscores_submit() : saisie + save (une seule fois, bloquant)
  - highscores_show()   : rendu depuis cache (appelé chaque frame)
  - highscores_refresh(): recharge le cache depuis la SD
============================================================
*/

#include "highscores.h"
#include "core/graphics.h"
#include "core/input.h"
#include "lib/LCD.h"
#include "lib/graphics_basic.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/audio.h"
#include <cinttypes>
#include <cerrno>
#include <sys/stat.h>   // mkdir

#define HIGHSCORE_FILE "/sdcard/PAKAMAN/SCORES.DAT"
#define PAKAMAN_DIR    "/sdcard/PAKAMAN"

static std::vector<HighscoreEntry> s_cache;

// Garantit l'existence du répertoire avant toute écriture
static void ensure_dir()
{
    struct stat st;
    int r = stat(PAKAMAN_DIR, &st);
    printf("[HS] stat('%s')=%d (0=existe)\n", PAKAMAN_DIR, r);
    if (r != 0) {
        int m = mkdir(PAKAMAN_DIR, 0755);
        printf("[HS] mkdir=%d\n", m);
    }
}

// ---------------------------------------------------------------------------
//  Init : crée le fichier si absent + charge le cache
// ---------------------------------------------------------------------------
void highscores_init()
{
    ensure_dir();   // crée /sdcard/PAKAMAN/ si absent
    FILE* f = fopen(HIGHSCORE_FILE, "rb");
    if (!f) {
        f = fopen(HIGHSCORE_FILE, "wb");
        if (f) fclose(f);
    } else {
        fclose(f);
    }
    highscores_refresh();
}

// ---------------------------------------------------------------------------
//  Recharge le cache depuis la SD (appeler UNE FOIS à l'entrée de l'écran)
// ---------------------------------------------------------------------------
void highscores_refresh()
{
    s_cache.clear();
    FILE* f = fopen(HIGHSCORE_FILE, "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    size_t count = (size > 0) ? (size_t)(size / sizeof(HighscoreEntry)) : 0;
    s_cache.resize(count);
    if (count > 0)
        fread(s_cache.data(), sizeof(HighscoreEntry), count, f);
    fclose(f);
}

// ---------------------------------------------------------------------------
//  Saisie du pseudo
// ---------------------------------------------------------------------------
static std::string input_name()
{
    const std::string alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    std::string name;
    int  idx       = 0;
    bool b_pressed = false;

    while (true) {
        Keys k;
        input_poll(k);

        if (k.left)  { if (--idx < 0)                   idx = (int)alpha.size()-1; }
        if (k.right) { if (++idx >= (int)alpha.size())   idx = 0; }
        if (k.A && (int)name.size() < 8) { name.push_back(alpha[idx]); idx = 0; }
        if (k.C && !name.empty())          name.pop_back();
        if (k.B && !name.empty())          b_pressed = true;
        if (!k.B && b_pressed)             break;

        gfx_clear(COLOR_BLACK);
        gfx_text(20,  80, "Entrez votre pseudo :", COLOR_YELLOW);
        std::string display = "  " + name + alpha[idx];
        gfx_text(20, 105, display.c_str(),         COLOR_WHITE);
        gfx_text(20, 140, "A=lettre  C=effacer  B=OK", COLOR_WHITE);
        gfx_flush();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return name;
}

// ---------------------------------------------------------------------------
//  Soumettre un score : saisie + écriture SD + mise à jour cache
// ---------------------------------------------------------------------------
void highscores_submit(int32_t score)
{
    printf("[HS] submit score=%d\n", (int)score);

    std::string name = input_name();
    printf("[HS] name='%s' len=%d\n", name.c_str(), (int)name.size());
    if (name.empty()) { printf("[HS] name vide, abandon\n"); return; }

    HighscoreEntry entry{};
    memset(entry.name, 0, sizeof(entry.name));
    strncpy(entry.name, name.c_str(), sizeof(entry.name)-1);
    entry.score = score;
    s_cache.push_back(entry);
    printf("[HS] cache=%d entrees\n", (int)s_cache.size());

    std::sort(s_cache.begin(), s_cache.end(),
              [](const HighscoreEntry& a, const HighscoreEntry& b){
                  return a.score > b.score; });

    if ((int)s_cache.size() > MAX_SCORES)
        s_cache.resize(MAX_SCORES);

    // Écriture sur SD
    ensure_dir();
    printf("[HS] fopen wb '%s'\n", HIGHSCORE_FILE);
    FILE* f = fopen(HIGHSCORE_FILE, "wb");
    if (!f) {
        printf("[HS] ERREUR fopen echoue (errno=%d)\n", errno);
        return;
    }
    size_t written = fwrite(s_cache.data(), sizeof(HighscoreEntry), s_cache.size(), f);
    printf("[HS] fwrite: %d/%d entrees, sizeof=%d\n",
           (int)written, (int)s_cache.size(), (int)sizeof(HighscoreEntry));
    int rc = fclose(f);
    printf("[HS] fclose=%d  => fichier OK\n", rc);
}

// ---------------------------------------------------------------------------
//  Affichage (utilise le cache — PAS de fopen ici)
// ---------------------------------------------------------------------------
void highscores_show()
{
    gfx_clear(COLOR_BLACK);
    gfx_text(80, 10, "=== Highscores ===", COLOR_YELLOW);

    if (s_cache.empty()) {
        gfx_text(20,  80, "Aucun score enregistre.", COLOR_WHITE);
        gfx_text(20, 100, "Terminez une partie !",   COLOR_WHITE);
    } else {
        int y = 50, rank = 1;
        for (const auto& e : s_cache) {
            if (rank > MAX_SCORES) break;
            if (e.name[0] == '\0') { ++rank; continue; }
            char buf[64];
            snprintf(buf, sizeof(buf), "%2d. %-8s %6" PRId32, rank, e.name, e.score);
            gfx_text(20, y, buf, COLOR_WHITE);
            y += 20;
            ++rank;
        }
    }
    gfx_text(20, 210, "B = retour", COLOR_YELLOW);
    // PAS de gfx_flush() ici — la boucle principale le fait
}
