#pragma once
#include <stdint.h>

/*
===============================================================================
  sprite.h — API de dessin de sprites (backend-agnostique)
-------------------------------------------------------------------------------
Ce module fournit toutes les primitives de dessin de sprites utilisées par
le moteur de jeu. Il NE DOIT PAS appeler directement lcd_putpixel().

Toutes les fonctions utilisent l’API façade gfx_*(), qui redirige vers :
    - gfx_fb_*()     (framebuffer + DMA)
    - gfx_direct_*() (LCD direct)

Ainsi, le moteur reste totalement indépendant du pipeline graphique réel.
===============================================================================
*/


// ============================================================================
//  SPRITES 16×16 (format classique Pac-Man)
// ============================================================================

/**
 * @brief Dessine un sprite 16×16 opaque (RGB565).
 *
 * @param x Position X en pixels
 * @param y Position Y en pixels
 * @param pixels Tableau 16×16 de couleurs RGB565
 */
void draw_sprite16(int x, int y, const uint16_t* pixels);

/**
 * @brief Dessine un sprite 16×16 avec couleur de transparence.
 *
 * @param x Position X
 * @param y Position Y
 * @param pixels Tableau 16×16
 * @param transparentColor Couleur à ignorer
 */
void draw_sprite16_transparent(int x, int y,
                               const uint16_t* pixels,
                               uint16_t transparentColor);


// ============================================================================
//  SPRITES DANS UNE GRILLE (tilemap)
// ============================================================================

/**
 * @brief Dessine un sprite dans une grille (tilemap), opaque.
 *
 * @param gridX Position X en cases
 * @param gridY Position Y en cases
 * @param tileSizeX Largeur d’une case
 * @param tileSizeY Hauteur d’une case
 * @param spriteW Largeur du sprite
 * @param spriteH Hauteur du sprite
 * @param offsetX Décalage X dans la case
 * @param offsetY Décalage Y dans la case
 * @param pixels Données du sprite
 */
void draw_sprite_grid(int gridX, int gridY,
                      int tileSizeX, int tileSizeY,
                      int spriteW, int spriteH,
                      int offsetX, int offsetY,
                      const uint16_t* pixels);

/**
 * @brief Variante avec transparence.
 */
void draw_sprite_grid_transparent(int gridX, int gridY,
                                  int tileSizeX, int tileSizeY,
                                  int spriteW, int spriteH,
                                  int offsetX, int offsetY,
                                  const uint16_t* pixels,
                                  uint16_t transparentColor);


// ============================================================================
//  SPRITE GÉNÉRIQUE (w × h)
// ============================================================================

/**
 * @brief Dessine un sprite générique (w × h) avec transparence.
 *
 * @param x Position X
 * @param y Position Y
 * @param sprite Tableau w×h de couleurs
 * @param w Largeur
 * @param h Hauteur
 * @param transparentColor Couleur ignorée (par défaut : 0x0000)
 */
void gfx_drawSprite(int x, int y,
                    const uint16_t* sprite,
                    int w, int h,
                    uint16_t transparentColor = 0x0000);


// ============================================================================
//  STRUCTURE D’ANIMATION SIMPLE
// ============================================================================

/**
 * @brief Structure d’animation générique basée sur un tableau de frames.
 *
 * Usage :
 *      const uint16_t* pacFrames[3] = { pac0, pac1, pac2 };
 *      SpriteAnim anim(pacFrames, 3, 8); // 8 ticks par frame
 *
 *      anim.update();
 *      const uint16_t* frame = anim.getFrame();
 */
struct SpriteAnim {
    const uint16_t** frames;   ///< Tableau de pointeurs vers les frames
    int frameCount;            ///< Nombre total de frames
    int tick;                  ///< Compteur interne
    int speed;                 ///< Nombre de ticks par frame
    int currentFrame;          ///< Frame courante

    SpriteAnim(const uint16_t** f, int count, int spd)
        : frames(f), frameCount(count), tick(0), speed(spd), currentFrame(0) {}

    /// Avance l’animation selon la vitesse définie
    void update() {
        tick++;
        if (tick >= speed) {
            tick = 0;
            currentFrame = (currentFrame + 1) % frameCount;
        }
    }

    /// Retourne la frame courante
    const uint16_t* getFrame() const {
        return frames[currentFrame];
    }
};
