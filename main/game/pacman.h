#pragma once

/*
============================================================
  pacman.h — Gestion de Pac-Man (logique & rendu)
------------------------------------------------------------
Ce module gère :
 - la position de Pac-Man en coordonnées tuiles
 - la direction actuelle et la direction demandée
 - l'avancement avec pixel_offset (case-based smooth)
 - l’animation (3 frames par direction)
 - l’API update() / draw()

Pac-Man est entièrement case-based :
 - tile_r / tile_c : position dans la grille
 - pixel_offset    : progression dans la tuile (0..TILE_SIZE)
 - x / y           : dérivé pour le rendu uniquement

La logique de tunnels (portails) et de collecte (pellets,
power pellets) est gérée dans pacman.cpp via GameState.
============================================================
*/

#include <cstdint>
#include "game/config.h"

struct GameState;

// Chaîne de debug optionnelle (utilisée dans pacman.cpp)
extern char debugText[64];

class Pacman {
public:

    /*
    ------------------------------------------------------------
      Directions logiques
    ------------------------------------------------------------
    */
    enum class Dir : uint8_t {
        None  = 0,
        Left  = 1,
        Right = 2,
        Up    = 3,
        Down  = 4
    };

    /*
    ------------------------------------------------------------
      Position logique (case-based)
    ------------------------------------------------------------
    */
    int tile_r      = 0;  // ligne de tuile actuelle
    int tile_c      = 0;  // colonne de tuile actuelle
    int prev_tile_r = 0;  // tuile précédente (ligne)
    int prev_tile_c = 0;  // tuile précédente (colonne)

    /*
    ------------------------------------------------------------
      Progression dans la tuile
    ------------------------------------------------------------
    pixel_offset :
      - exprimé en pixels (0..TILE_SIZE)
      - signé pour gérer les directions "négatives"
        (Up / Left si besoin).
    */
    int pixel_offset = 0;

    /*
    ------------------------------------------------------------
      Position pixel (pour le rendu uniquement)
    ------------------------------------------------------------
    x, y sont dérivés de :
      - tile_r, tile_c
      - pixel_offset
      - direction
    et servent à dessiner Pac-Man à l’écran.
    */
    int x = 0;
    int y = 0;

    /*
    ------------------------------------------------------------
      Direction actuelle + direction demandée
    ------------------------------------------------------------
    dir      : direction réelle de déplacement
    next_dir : direction demandée par le joueur (input)
               persistante pour permettre les virages anticipés.
    */
    Dir dir      = Dir::None;
    Dir next_dir = Dir::None;

    /*
    ------------------------------------------------------------
      Animation
    ------------------------------------------------------------
    animTick est incrémenté à chaque frame et sert à choisir
    la frame de bouche (3 frames).
    */
    int animTick = 0;

    /*
    ------------------------------------------------------------
      Helpers case-based
    ------------------------------------------------------------
    */
    inline bool isCentered() const {
        return (pixel_offset == 0);
    }

    inline int tileRow() const { return tile_r; }
    inline int tileCol() const { return tile_c; }

    /*
    ------------------------------------------------------------
      Constructeur
    ------------------------------------------------------------
    start_col / start_row : position de départ en tuiles.
    Pac-Man est centré dans sa tuile en pixels.
    ------------------------------------------------------------
    */
    Pacman(int start_col = 0, int start_row = 0);

    /*
    ------------------------------------------------------------
      API principale
    ------------------------------------------------------------
      update(GameState& g) :
        - lit les inputs
        - met à jour direction / offset / tuile
        - gère tunnels, pellets, power pellets, frightened

      draw(const GameState& g) :
        - choisit la bonne frame (direction + animTick)
        - dessine Pac-Man à l’écran
    ------------------------------------------------------------
    */
    void update(GameState& g);
    void draw(const GameState& g) const;

    /*
    ------------------------------------------------------------
      Helpers direction (composantes unitaires)
    ------------------------------------------------------------
    */
    static int dirX(Dir d) {
        switch (d) {
            case Dir::Left:  return -1;
            case Dir::Right: return +1;
            default:         return 0;
        }
    }

    static int dirY(Dir d) {
        switch (d) {
            case Dir::Up:    return -1;
            case Dir::Down:  return +1;
            default:         return 0;
        }
    }
};

