/*
============================================================
  ui/intermission.cpp — Intermede narratif entre deux niveaux
------------------------------------------------------------
  Ecran bloquant (meme convention que menu_open()/highscores_submit()) :
  une courte scene animee en deux temps (poursuite, puis retournement),
  avec legende texte, jouee avec les sprites deja existants du jeu
  (aucun nouvel asset requis). Purement cosmetique.

  Skippable a tout moment (A ou B) pour ne pas frustrer un joueur pressé.
============================================================
*/
#include "intermission.h"
#include "core/graphics.h"
#include "core/sprite.h"      // gfx_drawSprite
#include "core/input.h"
#include "assets/assets.h"
#include "game/config.h"      // SCREEN_W / SCREEN_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gb_ll_common.h"     // EXPANDER_KEY_A / EXPANDER_KEY_B

namespace {

struct Chapter {
    const char* caption1;   // pendant la poursuite (fantome fuit Pac-Man)
    const char* caption2;   // pendant le retournement (Pac-Man repart en chasse)
    int         ghost_idx;  // 0=rouge 1=rose 2=bleu 3=orange
};

// Petite histoire recurrente (dans l'esprit, pas la lettre, des cutscenes
// de l'arcade original) : un fantome filou chipe une pastille doree et
// s'enfuit ; Pac-Man le pourchasse. Le texte varie legerement par chapitre
// pour eviter une repetition exacte d'une partie a l'autre ; les chapitres
// sont rejoues en boucle au-dela du 4eme.
const Chapter CHAPTERS[4] = {
    { "Un fantome file avec une pastille doree !",  "Pac-Man ne compte pas se laisser faire...", 0 },
    { "Le fantome jette un oeil derriere lui...",    "...et detale de plus belle !",               1 },
    { "Pac-Man a mange un fruit geant !",            "Cette fois, c'est LUI qui donne la chasse !", 2 },
    { "Les fantomes n'en reviennent pas...",         "Longue vie au roi du labyrinthe !",           3 },
};

const uint16_t* ghost_sprite(int idx, int frame) {
    switch (idx) {
        case 0: return frame ? ghost_red_1    : ghost_red_0;
        case 1: return frame ? ghost_pink_1   : ghost_pink_0;
        case 2: return frame ? ghost_blue_1   : ghost_blue_0;
        default: return frame ? ghost_orange_1 : ghost_orange_0;
    }
}

// True si le joueur demande a sauter la scene (A ou B, front d'appui).
bool skip_requested() {
    Keys k;
    input_poll(k);
    return (k.pressed & EXPANDER_KEY_A) || (k.pressed & EXPANDER_KEY_B);
}

// Fait defiler deux sprites (poursuivant derriere, poursuivi devant) de
// gauche a droite (ou l'inverse si reverse=true) sur une ligne fixe, avec
// une legende affichee en haut. Retourne false si le joueur a demande a
// sauter (pour interrompre la scene en cours).
bool run_chase_scene(const uint16_t* fleeing_frames[2],
                      const uint16_t* chasing_frames[2],
                      const char* caption,
                      bool reverse)
{
    const int y        = SCREEN_H / 2 - 7;
    const int gap       = 26;              // ecart entre les deux sprites
    const int total_run = SCREEN_W + 2 * gap + 28;   // traversee complete hors ecran
    const int step       = 3;              // vitesse de defilement (px/frame)

    for (int t = 0; t < total_run; t += step) {
        if (skip_requested()) return false;

        int lead_x = reverse ? (SCREEN_W - t) : (t - gap - 14);
        int back_x = reverse ? (lead_x + gap) : (lead_x - gap);

        int frame = (t / 6) % 2;

        gfx_clear(COLOR_BLACK);
        gfx_text_center(40, caption, COLOR_YELLOW);

        // Le poursuivant est dessine derriere (dans l'ordre d'affichage).
        gfx_drawSprite(back_x, y, chasing_frames[frame], 14, 14);
        gfx_drawSprite(lead_x, y, fleeing_frames[frame],  14, 14);

        gfx_flush();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return true;
}

} // namespace

void intermission_show(int completed_level)
{
    // Choix du chapitre : les 3 premiers sont uniques (apres niveaux 2, 5,
    // 9), au-dela on rejoue en boucle les 4 chapitres pour varier un peu
    // sans avoir a ecrire une histoire infinie.
    int idx = 0;
    if      (completed_level == 2) idx = 0;
    else if (completed_level == 5) idx = 1;
    else if (completed_level == 9) idx = 2;
    else                            idx = (completed_level / 4) % 4;

    const Chapter& ch = CHAPTERS[idx];

    const uint16_t* pacman_run[2]  = { pacman_left_0, pacman_left_1 };
    const uint16_t* pacman_chase[2]= { pacman_right_0, pacman_right_1 };
    const uint16_t* ghost_run[2]   = { ghost_sprite(ch.ghost_idx, 0), ghost_sprite(ch.ghost_idx, 1) };

    // Purge des fronts deja enfonces (evite un skip immediat si A/B etait
    // encore appuye en arrivant sur cet ecran).
    for (;;) { Keys k; input_poll(k); if (!k.A && !k.B) break; vTaskDelay(pdMS_TO_TICKS(30)); }

    // Scene 1 : le fantome fuit vers la droite, Pac-Man le poursuit.
    if (!run_chase_scene(ghost_run, pacman_chase, ch.caption1, /*reverse=*/false))
        return;   // scene sautee -> on sort directement, pas de scene 2

    vTaskDelay(pdMS_TO_TICKS(200));

    // Scene 2 : retournement -> le fantome fuit vers la gauche cette fois
    // (petit clin d'oeil a la 2eme cutscene de l'arcade original, ou
    // Pac-Man agrandi repart en chasse dans l'autre sens). Le fantome reste
    // devant (poursuivi), Pac-Man derriere (poursuivant) : seul le sens du
    // defilement s'inverse (reverse=true).
    if (!run_chase_scene(ghost_run, pacman_run, ch.caption2, /*reverse=*/true))
        return;

    vTaskDelay(pdMS_TO_TICKS(300));
}
