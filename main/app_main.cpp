/**
 * @file app_main.cpp
 * @brief Point d'entree pAKAman pour la console AKA (ESP32-S3).
 *
 * Architecture (alignee sur AKAsseBricks / mAKArena) :
 *   - Materiel via le composant standard "gamebuino" (gb_core proprietaire I2C+ADC).
 *   - Couche core (input / graphics / audio / i18n / settings) au-dessus du composant.
 *   - Boucle unique (plus de taches jeu/input separees) : machine a etats + features
 *     globales.
 *
 * Features globales :
 *   - Retour au loader OTA : RUN + MENU maintenus 500 ms (a tout moment).
 *   - Capture d'ecran BMP : MENU maintenu >= 500 ms  -> /sdcard/PAKAMAN/SHOTxxxx.BMP.
 *   - Menu moderne (i18n, volume, musique, langue, scores, commandes, recalibrage,
 *     loader) : appui court sur MENU.
 *   - Calibration du joystick au demarrage (stick au repos).
 */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "gb_common.h"
#include "gb_core.h"
#include "gb_ll_common.h"      

// Core
#include "core/sdcard.h"
#include "core/input.h"
#include "core/graphics.h"
#include "core/audio.h"
#include "core/i18n.h"
#include "core/settings.h"

// UI
#include "ui/menu.h"
#include "ui/title_screen.h"
#include "ui/highscores.h"

// Jeu
#include "game/config.h"
#include "game/game.h"
#include "assets/assets.h"

// --- Globals attendus par le reste du code ---
int     volume = 200;   // volume general (0..255) ; ecrase par settings_load()
gb_core g_core;         // instance unique du materiel (proprietaire I2C+ADC)

// ---------------------------------------------------------------------------
//  Retour au loader : RUN + MENU maintenus 500 ms.
//  On REUTILISE l'etat deja lu par input_poll() : pas de second lecteur I2C.
// ---------------------------------------------------------------------------
inline void checkReturnToLoader(bool run_held, bool menu_held) {
    static uint32_t combo_start = 0;
    uint32_t now = esp_timer_get_time() / 1000;   // en ms

    if (run_held && menu_held) {
        if (combo_start == 0) {
            combo_start = now;                     // début du maintien
        } else if (now - combo_start >= 500) {     // tenu 500 ms
            const esp_partition_t* loader = esp_partition_find_first(
                ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
            if (loader) {
                esp_ota_set_boot_partition(loader);
                esp_restart();
            }
        }
    } else {
        combo_start = 0;                           // réarmement
    }
}


// Petit son de confirmation au lancement d'une partie.
static void play_start_jingle() {
    g_sfx.play_tone(523.0f, 80, 0.6f);
    g_sfx.play_tone(659.0f, 80, 0.6f);
    g_sfx.play_tone(784.0f, 120, 0.6f);
}

// True si une partie est en cours (pour "Reprendre" dans le menu).
static bool is_in_game(const GameState& g) {
    using S = GameState::State;
    return g.state == S::Playing || g.state == S::Paused ||
           g.state == S::StartingLevel || g.state == S::PacmanDying;
}

extern "C" void app_main(void) {
    // 1) Materiel : tout passe par le composant standard.
    g_core.init();
    gfx_init();

    // 2) Calibration joystick : g_core.init() a deja lu l'ADC, on fige le centre
    //    maintenant que le stick est au repos (sinon get_x()/get_y() decentres).
    g_core.joystick.calibrate_center();

    // 3) SD + reglages persistants (langue + volume + musique) AVANT audio.
    bool sd_ok = sd_init();
    printf("app_main: sd_init() -> %s\n", sd_ok ? "true" : "false");
    settings_load();

    // 4) Audio (codec + voix SFX + tache de mixage) puis volume enregistre.
    audio_game_init();
    audio_set_volume(volume);

    // 5) Core jeu.
    input_init();
    assets_init();
    highscores_init();

    srand((unsigned)esp_timer_get_time());   // power-ups/fruits non deterministes

    // GameState en stockage STATIQUE (comme l'ancien task_game) : il contient
    // le labyrinthe (~450 o) + entites ; on evite ainsi toute pression sur la
    // pile (reduite) de la tache app_main.
    static GameState g;
    game_init(g);                 // initialise maze/pacman/ghosts ; met state=TitleScreen
    g.state = GameState::State::TitleScreen;

    // --- Suivi de l'appui MENU : court = menu, long = capture ---
    uint32_t menu_press_start = 0;
    bool     menu_shot_done   = false;

    const int64_t FRAME_US = 30000;   // ~33 FPS : rythme un peu plus calme qu'avant
                                      // (etait 40 FPS, juge trop rapide). La montee
                                      // de difficulte se fait via frightened + sorties
                                      // fantomes (apply_level_difficulty), pas la vitesse.
    int64_t last = esp_timer_get_time();

    while (true) {
        int64_t now = esp_timer_get_time();
        if (now - last < FRAME_US) { vTaskDelay(1); continue; }
        last = now;

		Keys k;
		input_poll(k);

		// lecture unique des boutons expander
		uint32_t s = g_core.buttons.state();
		bool run_held  = s & EXPANDER_KEY_RUN;     // RUN = HOME
		bool menu_held = s & EXPANDER_KEY_MENU;

		// (a) Combo loader global.
		checkReturnToLoader(run_held, menu_held);

        // (b) MENU seul : court -> menu moderne ; long (>=500 ms) -> capture.
        if (k.MENU && !k.RUN) {
            uint32_t ms = esp_timer_get_time() / 1000;
            if (!menu_press_start) { menu_press_start = ms; menu_shot_done = false; }
            else if (!menu_shot_done && ms - menu_press_start >= 500) {
                menu_shot_done = true;
                char shot[64];
                if (gfx_save_screenshot_bmp(shot, sizeof shot)) {
                    printf("Screenshot: %s\n", shot);
                    gfx_text_center(115, i18n::T(i18n::STR_SHOT_SAVED), COLOR_YELLOW);
                    gfx_flush();
                    vTaskDelay(pdMS_TO_TICKS(900));
                }
            }
        } else {
            if (menu_press_start && !menu_shot_done) {
                // Appui court relache -> menu modal.
                MenuAction act = menu_open(is_in_game(g));
                if (act == MenuAction::StartGame) {
                    game_init(g);
                    g.state = GameState::State::StartingLevel;
                    play_start_jingle();
                    gfx_clear(COLOR_BLACK); gfx_flush();
                } else if (act == MenuAction::ReturnTitle) {
                    audioPMF.stop();
                    g.state = GameState::State::TitleScreen;
                }
                // Resume : on ne touche pas a l'etat courant.
            }
            menu_press_start = 0;
            menu_shot_done   = false;
        }

        // (c) Machine a etats du jeu.
        switch (g.state) {

        case GameState::State::TitleScreen:
            title_screen_show();
            if (k.pressed & EXPANDER_KEY_A) {
                play_start_jingle();
                game_init(g);
                g.state = GameState::State::StartingLevel;
                gfx_clear(COLOR_BLACK);
            }
            break;

        case GameState::State::StartingLevel:
        case GameState::State::PacmanDying:
            game_update(g);
            game_draw(g);
            break;

        case GameState::State::Playing:
            game_update(g);
            game_draw(g);
            if (g.state == GameState::State::Playing && (k.pressed & EXPANDER_KEY_RUN))
                g.state = GameState::State::Paused;
            break;

        case GameState::State::Paused:
            game_draw(g);
            gfx_text_center(100, i18n::T(i18n::STR_PAUSE), COLOR_YELLOW);
            gfx_text_center(125, i18n::T(i18n::STR_PRESS_A_RESUME), COLOR_WHITE);
            if (k.pressed & EXPANDER_KEY_A) g.state = GameState::State::Playing;
            break;

        case GameState::State::GameOver:
            if (g.gameover_timer > 0) {
                --g.gameover_timer;
                game_draw(g);
            } else if (g.gameover_waiting_for_input) {
                g.gameover_waiting_for_input = false;
                audioPMF.stop();
                highscores_submit(g.score);     // bloquant : saisie + ecriture SD
                g.state = GameState::State::Highscores;
            }
            break;

        case GameState::State::Highscores:
            highscores_show();
            if (k.pressed & EXPANDER_KEY_B)
                g.state = GameState::State::TitleScreen;
            break;

        default:
            g.state = GameState::State::TitleScreen;
            break;
        }

        gfx_flush();   // presente la frame (game_draw n'appelle pas lcd_refresh)
    }
}
