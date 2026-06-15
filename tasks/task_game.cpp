/*
============================================================
  task_game.cpp — Tâche principale 40 FPS
------------------------------------------------------------
  Menu Options (6 items, index 0-6) :
    0-3  : audio
    4    : Highscores
    5    : Quitter partie (OptionsMenu) / Lanceur reboot (Options)
    6    : Retour
============================================================
*/

#include "task_game.h"
#include "game/game.h"
#include "core/graphics.h"
#include "core/input.h"
#include "lib/LCD.h"
#include "ui/title_screen.h"
#include "ui/menu.h"
#include "ui/highscores.h"
#include "game/level.h"
#include "assets/assets.h"
#include "core/audio.h"
#include "lib/audio_sfx.h"
#include "lib/audio_pmf.h"
#include "core/persist.h"
#include "ui/options.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern AudioPMF audioPMF;

static GameState g;
static int  options_index        = 0;
static bool highscores_from_game = false;
static int  input_cooldown       = 0;   // anti-rebond lors des transitions d'état

// ---------------------------------------------------------------------------
//  Helper : lancer le reboot vers le lanceur OTA
// ---------------------------------------------------------------------------
static void launch_bootloader()
{
    const esp_partition_t* loader = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (loader) {
        esp_ota_set_boot_partition(loader);
        esp_restart();
    }
}

// ===========================================================================
//  TITLE SCREEN
// ===========================================================================
static void state_title_screen(const Keys& k)
{
    title_screen_show();

    if (input_cooldown > 0) return;   // ignore les inputs pendant le délai anti-rebond

    if (k.A) {
        audio_sfx_validate();
        game_init(g);
        g.state = GameState::State::StartingLevel;
        gfx_clear(COLOR_BLACK);
        return;
    }
    if (k.MENU) {
        audio_sfx_click();
        options_index = 0;
        g.state = GameState::State::Options;
    }
}

// ===========================================================================
//  JEU
// ===========================================================================
static void state_starting_level(const Keys& k)
{
    game_update(g);
    game_draw(g);
}

static void state_playing(const Keys& k)
{
    game_update(g);
    game_draw(g);

    if (g.state != GameState::State::Playing)
        return;   // game_update a changé l'état (ex: GameOver, LevelComplete)

    if (k.RUN) {
        g.state = GameState::State::Paused;
        return;
    }
    if (k.MENU) {
        audio_sfx_click();
        options_index = 0;
        g.state = GameState::State::OptionsMenu;
    }
}

static void state_pacman_dying(const Keys& k)
{
    game_update(g);
    game_draw(g);
}

// ===========================================================================
//  PAUSE
// ===========================================================================
static void state_paused(const Keys& k)
{
    game_draw(g);
    gfx_text(100, 100, "- PAUSE -",               COLOR_YELLOW);
    gfx_text(60,  125, "A = Reprendre",            COLOR_WHITE);
    gfx_text(60,  145, "MENU = Options/Quitter",   COLOR_WHITE);

    if (k.A) { audio_sfx_validate(); g.state = GameState::State::Playing; return; }
    if (k.MENU) { audio_sfx_click(); options_index = 0; g.state = GameState::State::OptionsMenu; }
}

// ===========================================================================
//  GAME OVER → saisie initiales → Highscores
// ===========================================================================
static void state_gameover(const Keys& k)
{
    // Phase 1 : laisser finir le son (~90 frames)
    if (g.gameover_timer > 0) {
        --g.gameover_timer;
        game_draw(g);
        return;
    }
    // Phase 2 : saisie une seule fois
    if (g.gameover_waiting_for_input) {
        g.gameover_waiting_for_input = false;
        highscores_submit(g.score);     // bloquant : saisie + écriture SD + cache OK
        // Le cache est déjà à jour après submit, on va directement sur Highscores
        highscores_from_game = false;
        g.state = GameState::State::Highscores;
    }
}

// ===========================================================================
//  OPTIONS — depuis le title (state == Options)
//  4 = Highscores   5 = Lanceur reboot   6 = Retour
// ===========================================================================
static void state_options(const Keys& k)
{
    gfx_clear(COLOR_BLACK);
    draw_options_menu(g.state, options_index);
    handle_audio_options_navigation(k, options_index, g.state);

    if (!k.A) return;

    if (options_index == 4) {                   // Highscores
        audio_sfx_validate();
        audio_settings_save();
        highscores_refresh();
        highscores_from_game = false;
        g.state = GameState::State::Highscores;
    }
    else if (options_index == 5) {              // Lanceur
        audio_sfx_validate();
        audio_settings_save();
        launch_bootloader();
    }
    else if (options_index == 6) {              // Retour
        audio_sfx_cancel();
        audio_settings_save();
        g.state = GameState::State::TitleScreen;
    }
}

// ===========================================================================
//  OPTIONS — depuis le jeu (state == OptionsMenu)
//  4 = Highscores   5 = Quitter → TitleScreen   6 = Retour (reprendre)
// ===========================================================================
static void state_options_menu(const Keys& k)
{
    gfx_clear(COLOR_BLACK);
    draw_options_menu(g.state, options_index);
    handle_audio_options_navigation(k, options_index, g.state);

    // B = reprendre directement
    if (k.B) {
        audio_sfx_validate();
        audio_settings_save();
        g.state = GameState::State::Playing;
        return;
    }

    if (!k.A) return;

    if (options_index == 4) {                   // Highscores
        audio_sfx_validate();
        audio_settings_save();
        highscores_refresh();
        highscores_from_game = true;
        g.state = GameState::State::Highscores;
    }
    else if (options_index == 5) {              // Quitter → TitleScreen
        audio_sfx_cancel();
        audio_settings_save();
        audioPMF.stop();
        input_cooldown = 20;   // ~500 ms anti-rebond
        g.state = GameState::State::TitleScreen;
    }
    else if (options_index == 6) {              // Retour = reprendre
        audio_sfx_validate();
        audio_settings_save();
        g.state = GameState::State::Playing;
    }
}

// ===========================================================================
//  HIGHSCORES — rendu depuis cache
// ===========================================================================
static void state_highscores(const Keys& k)
{
    highscores_show();   // cache, pas de fopen, pas de gfx_flush

    if (k.B) {
        audio_sfx_cancel();
        if (highscores_from_game) {
            highscores_from_game = false;
            options_index = 0;
            g.state = GameState::State::OptionsMenu;
        } else {
            g.state = GameState::State::TitleScreen;
        }
    }
}

// ===========================================================================
//  BOUCLE PRINCIPALE — 40 FPS
// ===========================================================================
void task_game(void* param)
{
    game_init(g);

    const int64_t FRAME_US = 25000;
    int64_t last = esp_timer_get_time();

    while (true)
    {
        int64_t now = esp_timer_get_time();
        if (now - last >= FRAME_US)
        {
            last = now;
            const Keys& k = g_keys;

            if (input_cooldown > 0) --input_cooldown;

            switch (g.state)
            {
                case GameState::State::TitleScreen:   state_title_screen(k);   break;
                case GameState::State::StartingLevel: state_starting_level(k); break;
                case GameState::State::Playing:       state_playing(k);        break;
                case GameState::State::PacmanDying:   state_pacman_dying(k);   break;
                case GameState::State::Paused:        state_paused(k);         break;
                case GameState::State::GameOver:      state_gameover(k);       break;
                case GameState::State::Options:       state_options(k);        break;
                case GameState::State::OptionsMenu:   state_options_menu(k);   break;
                case GameState::State::Highscores:    state_highscores(k);     break;
                default: g.state = GameState::State::TitleScreen; break;
            }
            gfx_flush();
        }
        else {
            vTaskDelay(1);
        }
    }
}
