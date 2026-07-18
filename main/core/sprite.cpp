// ============================================================================
//  sprite.cpp — Routines de dessin de sprites (backend-agnostiques)
// ============================================================================
//
//  Ce module NE DOIT PAS appeler directement lcd_putpixel().
//  Il utilise uniquement l’API façade gfx_*(), qui redirige vers :
//      - gfx_fb_*()   (framebuffer + DMA)
//      - gfx_direct_*() (LCD direct)
//
//  Ainsi, tout le moteur reste indépendant du pipeline graphique réel.
// ============================================================================

#include "sprite.h"
#include "core/graphics.h"
#include "game/config.h"

// ---------------------------------------------------------------------------
//  Sprite 16×16 opaque
// ---------------------------------------------------------------------------
void draw_sprite16(int x, int y, const uint16_t* pixels) {
    for (int j = 0; j < 16; ++j) {
        for (int i = 0; i < 16; ++i) {
            uint16_t c = pixels[j * 16 + i];
            gfx_putpixel16(x + i, y + j, c);
        }
    }
}

// ---------------------------------------------------------------------------
//  Sprite 16×16 avec transparence
// ---------------------------------------------------------------------------
void draw_sprite16_transparent(int x, int y,
                               const uint16_t* pixels,
                               uint16_t transparentColor)
{
    for (int j = 0; j < 16; ++j) {
        for (int i = 0; i < 16; ++i) {
            uint16_t c = pixels[j * 16 + i];
            if (c != transparentColor)
                gfx_putpixel16(x + i, y + j, c);
        }
    }
}

// ---------------------------------------------------------------------------
//  Sprite générique (w × h) avec transparence
// ---------------------------------------------------------------------------
void gfx_drawSprite(int x, int y,
                    const uint16_t* sprite,
                    int w, int h,
                    uint16_t transparentColor)
{
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= SCREEN_H) continue;

        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            if (px < 0 || px >= SCREEN_W) continue;

            uint16_t color = sprite[dy * w + dx];
            if (color != transparentColor)
                gfx_putpixel16(px, py, color);
        }
    }
}

// ---------------------------------------------------------------------------
//  Sprite dans une grille (tilemap) — opaque
// ---------------------------------------------------------------------------
void draw_sprite_grid(int gridX, int gridY,
                      int tileSizeX, int tileSizeY,
                      int spriteW, int spriteH,
                      int offsetX, int offsetY,
                      const uint16_t* pixels)
{
    int px = gridX * tileSizeX + offsetX;
    int py = gridY * tileSizeY + offsetY;

    for (int j = 0; j < spriteH; ++j)
        for (int i = 0; i < spriteW; ++i)
            gfx_putpixel16(px + i, py + j, pixels[j * spriteW + i]);
}

// ---------------------------------------------------------------------------
//  Sprite dans une grille (tilemap) — transparent
// ---------------------------------------------------------------------------
void draw_sprite_grid_transparent(int gridX, int gridY,
                                  int tileSizeX, int tileSizeY,
                                  int spriteW, int spriteH,
                                  int offsetX, int offsetY,
                                  const uint16_t* pixels,
                                  uint16_t transparentColor)
{
    int px = gridX * tileSizeX + offsetX;
    int py = gridY * tileSizeY + offsetY;

    for (int j = 0; j < spriteH; ++j)
        for (int i = 0; i < spriteW; ++i) {
            uint16_t c = pixels[j * spriteW + i];
            if (c != transparentColor)
                gfx_putpixel16(px + i, py + j, c);
        }
}
