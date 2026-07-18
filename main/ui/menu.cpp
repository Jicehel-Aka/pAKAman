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

static void return_to_loader() {
    const esp_partition_t* loader = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (loader) { esp_ota_set_boot_partition(loader); esp_restart(); }
}

static void wait_release_B() {
    for (;;) { Keys k; input_poll(k); if (!k.B) break; vTaskDelay(pdMS_TO_TICKS(40)); }
}

static void show_controls() {
    gfx_clear(color_black);
    gfx_text(110, 10, T(STR_CONTROLS), color_yellow);
    gfx_text(20, 45,  "Stick / D-Pad : deplacer", color_white);
    gfx_text(20, 68,  "A : valider / demarrer",   color_white);
    gfx_text(20, 91,  "RUN : pause",              color_white);
    gfx_text(20, 114, "MENU : ce menu",           color_white);
    gfx_text(20, 137, "MENU long : capture",      color_white);
    gfx_text(20, 160, "RUN+MENU : loader",        color_white);
    gfx_text(20, 195, T(STR_BACK), color_yellow);
    gfx_flush();
    for (;;) { Keys k; input_poll(k); if (k.pressed & EXPANDER_KEY_B) break; vTaskDelay(pdMS_TO_TICKS(40)); }
}

static void draw_menu(int sel, bool in_game) {
    gfx_clear(color_black);
    gfx_text(130, 8, T(STR_MENU), color_yellow);

    char buf[40];
    const int x = 40, y0 = 40, dy = 20;

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
        uint16_t col = (i == sel) ? color_yellow : color_white;
        if (i == sel) gfx_text(x - 16, y0 + i * dy, ">", color_yellow);
        gfx_text(x, y0 + i * dy, label, col);
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
                                     gfx_clear(color_black);
                                     gfx_text(60, 110, T(STR_RECALIBRATE), color_yellow);
                                     gfx_flush(); vTaskDelay(pdMS_TO_TICKS(600)); break;
                case IT_LOADER:      return_to_loader(); break;
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
