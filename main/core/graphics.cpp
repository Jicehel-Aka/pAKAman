/*
  core/graphics.cpp — Implementation de l'adaptateur graphique sur gb_graphics.
  gb_graphics pour texte/formes, lcd_putpixel pour les sprites, et capture BMP
  via lcd_getpixel. Meme approche que la couche core d'AKAsseBricks/mAKArena.
*/
#include "graphics.h"
#include "config.h"          // SCREEN_W / SCREEN_H
#include "gb_graphics.h"
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

static gb_graphics g_gfx;
uint16_t current_text_color = 0xFFFF;

void gfx_init() {
    // L'init materiel de l'ecran est faite par gb_core::init() dans app_main.
    // Ici on ne fait que regler retro-eclairage et cadence d'affichage.
    g_gfx.set_backlight_percent(80);
    g_gfx.set_refresh_rate(60);
}

void gfx_clear(uint16_t color) {
    // Buffer unique + DMA : on attend la fin du DMA precedent AVANT d'effacer le
    // framebuffer, sinon on ecrase une image en cours de transfert -> clignotement.
    while (!lcd_refresh_completed()) { }
    lcd_clear(color);
}
void gfx_flush()               { lcd_refresh(); }

void gfx_putpixel16(int x, int y, uint16_t color) {
    // Clipping ecran (les routines de sprite peuvent deborder d'un pixel).
    if ((unsigned)x >= (unsigned)SCREEN_W || (unsigned)y >= (unsigned)SCREEN_H) return;
    lcd_putpixel((uint16_t)x, (uint16_t)y, color);
}

void gfx_set_text_color(uint16_t color) { current_text_color = color; }

void gfx_text(int x, int y, const char* txt, uint16_t color) {
    current_text_color = color;
    g_gfx.setColor(color);
    g_gfx.move_cursor((uint16_t)x, (uint16_t)y);
    g_gfx.print_str(txt);
}

void lcd_draw_text(uint16_t x, uint16_t y, const char* pc) {
    g_gfx.setColor(current_text_color);
    g_gfx.move_cursor(x, y);
    g_gfx.print_str(pc);
}

static const int FONT_CHAR_WIDTH = 8;   // police du SDK : avance de 8 px (cursor_x += 8)
int  gfx_char_width(char)                  { return FONT_CHAR_WIDTH; }
int  gfx_text_width(const char* text)      { return text ? (int)strlen(text) * FONT_CHAR_WIDTH : 0; }
void gfx_text_center(int y, const char* text, uint16_t color) {
    gfx_text((SCREEN_W - gfx_text_width(text)) / 2, y, text, color);
}

void lcd_draw_bitmap(const uint16_t* pixels, int w, int h, int dx, int dy) {
    // Ecriture plein ecran : comme gfx_clear, on attend la fin du DMA precedent
    // pour ne pas ecraser une image en cours de transfert (anti-clignotement).
    if (dx == 0 && dy == 0 && w >= SCREEN_W && h >= SCREEN_H)
        while (!lcd_refresh_completed()) { }
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            gfx_putpixel16(dx + i, dy + j, pixels[j * w + i]);
}

void lcd_draw_partial_bitmap(const uint16_t* pixels,
                             int sheetW, int sheetH,
                             int sx, int sy,
                             int spriteW, int spriteH,
                             int dx, int dy) {
    (void)sheetH;
    for (int y = 0; y < spriteH; ++y)
        for (int x = 0; x < spriteW; ++x)
            gfx_putpixel16(dx + x, dy + y, pixels[(sy + y) * sheetW + (sx + x)]);
}

// --- Capture d'ecran BMP 24 bits -> /sdcard/PAKAMAN/SHOTxxxx.BMP ---
bool gfx_save_screenshot_bmp(char* out_path, int out_path_size) {
    static const char* kDir = "/sdcard/PAKAMAN";   // meme dossier que scores/reglages
    mkdir(kDir, 0777);   // ok si deja present

    char path[64];
    int shot_num = -1;
    for (int i = 0; i < 10000; ++i) {
        snprintf(path, sizeof(path), "%s/SHOT%04d.BMP", kDir, i);
        FILE* test = fopen(path, "rb");
        if (!test) { shot_num = i; break; }
        fclose(test);
    }
    if (shot_num < 0) return false;

    FILE* f = fopen(path, "wb");
    if (!f) return false;

    const int W = SCREEN_W, H = SCREEN_H;
    const int row_bytes   = W * 3;
    const int row_padding = (4 - (row_bytes % 4)) % 4;
    const int row_stride  = row_bytes + row_padding;
    const uint32_t data_size = (uint32_t)row_stride * H;
    const uint32_t file_size = 14 + 40 + data_size;

    uint8_t header[54] = {0};
    header[0]='B'; header[1]='M';
    header[2]=(uint8_t)file_size;       header[3]=(uint8_t)(file_size>>8);
    header[4]=(uint8_t)(file_size>>16); header[5]=(uint8_t)(file_size>>24);
    header[10]=54;
    header[14]=40;
    header[18]=(uint8_t)W;  header[19]=(uint8_t)(W>>8);
    header[22]=(uint8_t)H;  header[23]=(uint8_t)(H>>8);
    header[26]=1; header[28]=24;
    header[34]=(uint8_t)data_size;       header[35]=(uint8_t)(data_size>>8);
    header[36]=(uint8_t)(data_size>>16); header[37]=(uint8_t)(data_size>>24);
    fwrite(header, 1, 54, f);

    uint8_t row[SCREEN_W * 3 + 3] = {0};
    for (int y = H - 1; y >= 0; --y) {          // BMP : lignes bas -> haut
        for (int x = 0; x < W; ++x) {
            // Les pixels sont en BGR565 (rouge sur les bits de poids faible).
            gb_pixel v = lcd_getpixel((uint16_t)x, (uint16_t)y);
            uint8_t r5 =  v        & 0x1F;
            uint8_t g6 = (v >> 5)  & 0x3F;
            uint8_t b5 = (v >> 11) & 0x1F;
            row[x*3+0] = (uint8_t)((b5 * 255) / 31);   // BMP = B,G,R
            row[x*3+1] = (uint8_t)((g6 * 255) / 63);
            row[x*3+2] = (uint8_t)((r5 * 255) / 31);
        }
        for (int p = 0; p < row_padding; ++p) row[row_bytes + p] = 0;
        fwrite(row, 1, row_stride, f);
    }
    fclose(f);

    if (out_path && out_path_size > 0) {
        strncpy(out_path, path, out_path_size - 1);
        out_path[out_path_size - 1] = '\0';
    }
    return true;
}
