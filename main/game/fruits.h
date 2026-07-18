#pragma once
/*
============================================================
  fruits.h — Fruit "baladeur" facon Ms. Pac-Man
------------------------------------------------------------
  Le fruit ne reste plus fixe : il ENTRE par un tunnel, se
  PROMENE dans le labyrinthe (choix aleatoire aux intersections,
  wrap par les tunnels), puis disparait apres un delai. Pac-Man
  peut le manger pour un bonus (score selon le niveau).

  Modele de deplacement identique a Pac-Man / fantomes :
    tile_r / tile_c : position en tuiles
    pixel_offset    : progression 0..TILE_SIZE le long de dir
    x / y           : position pixel derivee (rendu)

  Compatible avec try_portal_wrap<Actor> (game.h) : expose
  tile_r/tile_c/prev_tile_* /pixel_offset/dir/isCentered() et un
  Dir::Left / Dir::Right.
============================================================
*/
#include <cstdint>
#include "config.h"

struct GameState;   // forward (evite d'inclure game.h ici)

struct Fruit {
    enum class Dir : uint8_t { None, Left, Right, Up, Down };

    bool active          = false;   // fruit visible/en balade ?
    int  spawned_count   = 0;       // combien de fruits deja apparus ce niveau (0..2)
    int  initial_pellets = 0;       // gommes au debut du niveau (seuil d'apparition)

    // Position logique (case-based)
    int tile_r = 0, tile_c = 0;
    int prev_tile_r = 0, prev_tile_c = 0;
    int pixel_offset = 0;           // 0..TILE_SIZE
    Dir dir = Dir::None;

    // Position pixel (coin haut-gauche du sprite 14x14) pour le rendu
    int x = 0, y = 0;

    int life_ticks = 0;             // duree de vie restante (frames)
    int score      = 0;             // bonus rapporte
    const uint16_t* sprite = nullptr;

    bool isCentered() const { return pixel_offset == 0; }
};

// Debut de niveau : desactive + reamorce les 2 apparitions.
void fruit_reset(GameState& g);
// Chaque frame en jeu : apparition, balade, collision avec Pac-Man.
void fruit_update(GameState& g);
// Rendu (si actif).
void fruit_draw(const GameState& g);

// Sprite du fruit associe a un niveau (pour la rangee de fruits du HUD).
const uint16_t* fruit_sprite_for_level(int level);
