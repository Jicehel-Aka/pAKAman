/*
  core/graphics.h — Adaptateur graphique pAKAman au-dessus du composant
  gamebuino. L'API historique du jeu est conservee (gfx_*, gfx_putpixel16,
  lcd_draw_bitmap, palette COLOR_*), mais tout passe desormais par
  gb_graphics / gb_ll_lcd. Le rendu se fait dans le framebuffer du composant :
  gfx_clear/gfx_flush/gfx_putpixel16 -> lcd_clear/lcd_refresh/lcd_putpixel.

  NOTE PIXEL : les sprites de pAKAman sont en BGR565 (rouge sur les bits de
  poids faible). C'est exactement l'ordre produit par lcd_color_rgb() du
  composant : les assets s'affichent donc sans conversion.
*/
#pragma once
#include <stdint.h>
#include "gb_ll_lcd.h"   // lcd_clear/refresh/putpixel/getpixel + enum gamebuino_color

// ------------------------------------------------------------
// API PUBLIQUE — utilisee par tout le moteur de jeu
// ------------------------------------------------------------
void gfx_init();
void gfx_clear(uint16_t color);
void gfx_flush();

// Pixel unique : primitive de base des routines de sprite (core/sprite.cpp).
void gfx_putpixel16(int x, int y, uint16_t color);

void gfx_set_text_color(uint16_t color);
void gfx_text(int x, int y, const char* txt, uint16_t color);
int  gfx_text_width(const char* text);
void gfx_text_center(int y, const char* text, uint16_t color);
int  gfx_char_width(char c);

// Blit d'image complete (opaque) — utilise par l'ecran-titre.
void lcd_draw_bitmap(const uint16_t* pixels, int w, int h, int dx, int dy);
void lcd_draw_partial_bitmap(const uint16_t* pixels,
                             int sheetW, int sheetH,
                             int sx, int sy,
                             int spriteW, int spriteH,
                             int dx, int dy);
void lcd_draw_text(uint16_t x, uint16_t y, const char* pc);

// Capture d'ecran BMP 24 bits -> /sdcard/PAKAMAN/SHOTxxxx.BMP.
bool gfx_save_screenshot_bmp(char* out_path = nullptr, int out_path_size = 0);

// Couleur de texte active (utilisee par lcd_draw_text)
extern uint16_t current_text_color;

// ------------------------------------------------------------
// Palette de couleurs standard (BGR565) — conservee telle quelle
// pour rester compatible avec tout le code de rendu du jeu.
// ------------------------------------------------------------
#define COLOR_BLACK      0x0000
#define COLOR_WHITE      0xFFFF
#define COLOR_RED        0x001F
#define COLOR_GREEN      0x03E0
#define COLOR_BLUE       0xF800
#define COLOR_GRAY       0x84D5
#define COLOR_DARKGRAY   0x426A
#define COLOR_PURPLE     0x4012
#define COLOR_PINK       0x8239
#define COLOR_ORANGE     0x155F
#define COLOR_BROWN      0x4479
#define COLOR_BEIGE      0x96BF
#define COLOR_YELLOW     0x073E
#define COLOR_LIGHTGREEN 0x07E0
#define COLOR_DARKBLUE   0x8200
#define COLOR_LIGHTBLUE  0xFDCF
#define COLOR_SILVER     0xBDF7
#define COLOR_GOLD       0x159C
