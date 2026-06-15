#include "options.h"
#include "core/graphics.h"
#include "core/audio.h"
#include "lib/audio_sfx.h"
#include "game/config.h"

extern AudioSettings g_audio_settings;
int nav_cooldown = 0;

// Nombre d'items selon le contexte
// OptionsMenu (jeu)  : 7 items (0-6)  — 0-3 audio, 4 highscores, 5 quitter, 6 retour
// Options (titre)    : 7 items (0-6)  — 0-3 audio, 4 highscores, 5 lanceur,  6 retour
static int max_index(GameState::State state)
{
    return 6;
}

void handle_audio_options_navigation(const Keys& k, int& index, GameState::State state)
{
    if (nav_cooldown > 0) { nav_cooldown--; return; }

    if (k.up) {
        if (--index < 0) index = max_index(state);
        audio_sfx_click();
        nav_cooldown = 8;
    }
    if (k.down) {
        if (++index > max_index(state)) index = 0;
        audio_sfx_click();
        nav_cooldown = 8;
    }
    if (k.left) {
        audio_sfx_click();
        if (index == 0) g_audio_settings.music_enabled = !g_audio_settings.music_enabled;
        if (index == 1 && g_audio_settings.music_volume  > 0)   g_audio_settings.music_volume   -= 8;
        if (index == 2 && g_audio_settings.sfx_volume    > 0)   g_audio_settings.sfx_volume     -= 8;
        if (index == 3 && g_audio_settings.master_volume > 0)   g_audio_settings.master_volume  -= 8;
        nav_cooldown = 8;
    }
    if (k.right) {
        audio_sfx_click();
        if (index == 0) g_audio_settings.music_enabled = !g_audio_settings.music_enabled;
        if (index == 1 && g_audio_settings.music_volume  < 255) g_audio_settings.music_volume   += 8;
        if (index == 2 && g_audio_settings.sfx_volume    < 255) g_audio_settings.sfx_volume     += 8;
        if (index == 3 && g_audio_settings.master_volume < 255) g_audio_settings.master_volume  += 8;
        nav_cooldown = 8;
    }
}

void draw_options_menu(GameState::State state, int index)
{
    auto item = [&](int y, const char* text, bool selected) {
        gfx_text(20, y, text, selected ? COLOR_YELLOW : COLOR_WHITE);
    };

    gfx_text(20, 10, "OPTIONS", COLOR_WHITE);

    char buf[64];
    item(40,  g_audio_settings.music_enabled ? "Music: ON" : "Music: OFF", index == 0);

    snprintf(buf, sizeof(buf), "Music Volume: %d",  g_audio_settings.music_volume);
    item(65,  buf, index == 1);

    snprintf(buf, sizeof(buf), "SFX Volume: %d",    g_audio_settings.sfx_volume);
    item(90,  buf, index == 2);

    snprintf(buf, sizeof(buf), "Master Volume: %d", g_audio_settings.master_volume);
    item(115, buf, index == 3);

    // item 4 : Highscores (dans les deux menus)
    item(145, "Highscores", index == 4);

    // item 5 : contextuel
    if (state == GameState::State::OptionsMenu)
        item(170, "Quitter la partie", index == 5);
    else
        item(170, "Lanceur (reboot)", index == 5);

    // item 6 : Retour
    item(195, "Retour", index == 6);
}
