/*
  ui/title_screen.cpp — Ecran-titre. Blit de l'image plein ecran (BGR565) puis
  invite a jouer (texte traduit). Porte sur le nouveau core/graphics + i18n.
*/
#include "title_screen.h"
#include "assets/title_image.h"
#include "core/graphics.h"
#include "core/i18n.h"
#include "game/config.h"

void title_screen_show() {
    lcd_draw_bitmap(title_image_pixels, SCREEN_W, SCREEN_H, 0, 0);
    gfx_text_center(196, i18n::T(i18n::STR_PRESS_A_START), COLOR_WHITE);
    gfx_flush();
}
