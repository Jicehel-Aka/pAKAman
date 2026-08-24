/*
  ui/menu.cpp — Menu moderne pAKAman (modal, i18n), inspire de mAKArena/AKAsseBricks.
  Navigation HAUT/BAS, A = valider, GAUCHE/DROITE = regler (volume, langue, musique),
  B = retour. Ouvert par un appui MENU.
*/
#include "menu.h"
#include "core/graphics.h"
#include "core/input.h"
#include "core/audio.h"
#include "core/i18n.h"
#include "core/settings.h"
#include "highscores.h"
#include "gb_core.h"
#include "gb_ll_common.h"
#include "config.h"
#include <cstdio>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern gb_core g_core;   // pour le recalibrage du stick
extern int     volume;   // volume global (0..255)

using i18n::T;
using namespace i18n;

// Entrees du menu (l'ordre = l'ordre a l'ecran).
enum Item {
    IT_PLAY = 0,   // Jouer / Reprendre
    IT_MUSIC,      // musique ON/OFF (A ou gauche/droite)
    IT_VOLUME,     // reglable gauche/droite
    IT_LANGUAGE,   // cycle gauche/droite
    IT_SCORES,     // ouvre l'ecran des scores
    IT_CONTROLS,   // ouvre l'aide commandes
    IT_RECALIBRATE,// recalibre le joystick
    IT_LOADER,     // retour au loader
    IT_COUNT
};

// Geometrie de la boite du menu systeme — reprise a l'identique des autres
// portages AKA (aka_runtime) pour une apparence coherente d'un jeu a
// l'autre : boite 240x200 centree sur l'ecran 320x240.
static const int BOX_X = 40, BOX_Y = 20, BOX_W = 240, BOX_H = 200;

static void menu_frame(const char* title) {
    gfx_fill_rect(BOX_X, BOX_Y, BOX_W, BOX_H, COLOR_DARKBLUE);
    gfx_draw_rect(BOX_X, BOX_Y, BOX_W, BOX_H, COLOR_WHITE);
    gfx_text(BOX_X + 12, BOX_Y + 8, title, COLOR_YELLOW);
}

static void return_to_loader() {
    const esp_partition_t* loader = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (loader) { esp_ota_set_boot_partition(loader); esp_restart(); }
}

bool confirm_return_to_loader() {
    // Vide les fronts en cours (evite qu'un A/B deja enfonce ne soit relu).
    for (;;) { Keys k; input_poll(k); if (!k.A && !k.B) break; vTaskDelay(pdMS_TO_TICKS(40)); }

    for (;;) {
        Keys k;
        input_poll(k);

        gfx_clear(COLOR_BLACK);
        menu_frame(T(STR_RETURN_LOADER));
        gfx_text(BOX_X + 12, BOX_Y + 40, "A: confirmer", COLOR_WHITE);
        gfx_text(BOX_X + 12, BOX_Y + 60, "B: annuler",   COLOR_WHITE);
        gfx_flush();

        if (k.pressed & EXPANDER_KEY_A) return true;
        if (k.pressed & EXPANDER_KEY_B) return false;
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

static void wait_release_B() {
    for (;;) { Keys k; input_poll(k); if (!k.B) break; vTaskDelay(pdMS_TO_TICKS(40)); }
}

static void show_controls() {
    gfx_clear(COLOR_BLACK);
    menu_frame(T(STR_CONTROLS));
    int y = BOX_Y + 34;
    gfx_text(BOX_X + 12, y, "Stick / D-Pad : deplacer", COLOR_WHITE); y += 16;
    gfx_text(BOX_X + 12, y, "A : valider / demarrer",   COLOR_WHITE); y += 16;
    gfx_text(BOX_X + 12, y, "RUN : pause",              COLOR_WHITE); y += 16;
    gfx_text(BOX_X + 12, y, "MENU : ce menu",           COLOR_WHITE); y += 16;
    gfx_text(BOX_X + 12, y, "MENU long : capture",      COLOR_WHITE); y += 16;
    gfx_text(BOX_X + 12, y, "RUN+MENU : loader",        COLOR_WHITE); y += 24;
    gfx_text(BOX_X + 12, y, T(STR_BACK), COLOR_YELLOW);
    gfx_flush();
    for (;;) { Keys k; input_poll(k); if (k.pressed & EXPANDER_KEY_B) break; vTaskDelay(pdMS_TO_TICKS(40)); }
}

static void draw_menu(int sel, bool in_game) {
    gfx_clear(COLOR_BLACK);
    menu_frame(T(STR_MENU));

    char buf[40];
    const int x = BOX_X + 24, y0 = BOX_Y + 34, dy = 16;

    for (int i = 0; i < IT_COUNT; ++i) {
        const char* label = "";
        switch (i) {
            case IT_PLAY:        label = T(in_game ? STR_RESUME : STR_PLAY); break;
            case IT_MUSIC:
                snprintf(buf, sizeof buf, "%s: %s", T(STR_MUSIC),
                         T(g_audio_settings.music_enabled ? STR_ON : STR_OFF));
                label = buf; break;
            case IT_VOLUME:
                snprintf(buf, sizeof buf, "%s: %d", T(STR_VOLUME), volume);
                label = buf; break;
            case IT_LANGUAGE:
                snprintf(buf, sizeof buf, "%s: %s", T(STR_LANGUAGE), T(STR_LANG_NAME));
                label = buf; break;
            case IT_SCORES:      label = T(STR_SCORES); break;
            case IT_CONTROLS:    label = T(STR_CONTROLS); break;
            case IT_RECALIBRATE: label = T(STR_RECALIBRATE); break;
            case IT_LOADER:      label = T(STR_RETURN_LOADER); break;
        }
        int y = y0 + i * dy;
        uint16_t col = (i == sel) ? COLOR_YELLOW : COLOR_WHITE;
        if (i == sel) gfx_text(BOX_X + 12, y, ">", COLOR_YELLOW);
        gfx_text(x, y, label, col);
    }
    gfx_flush();
}

MenuAction menu_open(bool in_game) {
    int sel = 0;
    wait_release_B();   // evite que le B/MENU d'ouverture ne soit relu aussitot

    for (;;) {
        Keys k;
        input_poll(k);

        if (k.pressed & EXPANDER_KEY_DOWN) { sel = (sel + 1) % IT_COUNT; snd_keypress.play_tone(660,35,0.5f); }
        if (k.pressed & EXPANDER_KEY_UP)   { sel = (sel + IT_COUNT - 1) % IT_COUNT; snd_keypress.play_tone(660,35,0.5f); }

        // Reglages gauche/droite sur les entrees concernees
        if (sel == IT_VOLUME) {
            if (k.pressed & EXPANDER_KEY_LEFT)  { if (volume > 0)   volume -= 16; audio_set_volume(volume); }
            if (k.pressed & EXPANDER_KEY_RIGHT) { if (volume < 255) volume += 16; audio_set_volume(volume); }
        } else if (sel == IT_LANGUAGE) {
            if (k.pressed & (EXPANDER_KEY_LEFT | EXPANDER_KEY_RIGHT)) {
                next_language(); settings_save(); snd_keypress.play_tone(880,45,0.5f);
            }
        } else if (sel == IT_MUSIC) {
            if (k.pressed & (EXPANDER_KEY_LEFT | EXPANDER_KEY_RIGHT)) {
                g_audio_settings.music_enabled = !g_audio_settings.music_enabled;
                if (!g_audio_settings.music_enabled) audioPMF.stop();
                settings_save(); snd_keypress.play_tone(700,45,0.5f);
            }
        }

        // Validation
        if (k.pressed & EXPANDER_KEY_A) {
            switch (sel) {
                case IT_PLAY:        settings_save(); return in_game ? MenuAction::Resume : MenuAction::StartGame;
                case IT_MUSIC:       g_audio_settings.music_enabled = !g_audio_settings.music_enabled;
                                     if (!g_audio_settings.music_enabled) audioPMF.stop();
                                     settings_save(); snd_keypress.play_tone(700,45,0.5f); break;
                case IT_SCORES:      highscores_show();
                                     for (;;){ Keys kk; input_poll(kk); if (kk.pressed & EXPANDER_KEY_B) break; vTaskDelay(pdMS_TO_TICKS(40)); }
                                     break;
                case IT_CONTROLS:    show_controls(); break;
                case IT_RECALIBRATE: g_core.joystick.calibrate_center();
                                     gfx_clear(COLOR_BLACK);
                                     menu_frame(T(STR_MENU));
                                     gfx_text(BOX_X + 12, BOX_Y + 40, T(STR_RECALIBRATE), COLOR_YELLOW);
                                     gfx_flush(); vTaskDelay(pdMS_TO_TICKS(600)); break;
                case IT_LOADER:      if (confirm_return_to_loader()) return_to_loader();
                                     break;
                default: break;
            }
        }

        // B : fermer le menu (reprendre si en jeu, sinon retour titre)
        if (k.pressed & EXPANDER_KEY_B) {
            settings_save();
            return in_game ? MenuAction::Resume : MenuAction::ReturnTitle;
        }

        draw_menu(sel, in_game);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}
