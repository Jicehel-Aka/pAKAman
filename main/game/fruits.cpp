/*
============================================================
  fruits.cpp — Fruit "baladeur" (Ms. Pac-Man style)
------------------------------------------------------------
  - Apparait 2 fois par niveau (apres 70 puis 170 gommes mangees).
  - Entre par un tunnel, se promene (aleatoire aux intersections,
    wrap par les tunnels), disparait apres ~10 s.
  - Mange par Pac-Man -> bonus (score selon le niveau) + score flottant.
  Reutilise le meme modele de deplacement que Pac-Man / fantomes.
============================================================
*/
#include "fruits.h"
#include "game.h"            // GameState, try_portal_wrap, FloatingScore
#include "maze.h"
#include "assets/assets.h"
#include "core/sprite.h"     // gfx_drawSprite
#include "core/audio.h"      // audio_play_fruit
#include <cstdlib>           // rand

extern float g_camera_y;     // defini dans game.cpp

// Vitesse (px/frame) : doit diviser TILE_SIZE (16). 2 -> balade tranquille.
static constexpr int FRUIT_SPEED       = 2;
static constexpr int FRUIT_LIFE_TICKS  = 10 * 40;   // ~10 s a 40 FPS
static constexpr int FRUIT_SPRITE      = 14;        // taille du sprite fruit

// Seuils d'apparition (gommes mangees) — fideles a l'arcade.
static constexpr int FRUIT1_EATEN = 70;
static constexpr int FRUIT2_EATEN = 170;

static int dirX(Fruit::Dir d){ return d==Fruit::Dir::Left ? -1 : d==Fruit::Dir::Right ? 1 : 0; }
static int dirY(Fruit::Dir d){ return d==Fruit::Dir::Up   ? -1 : d==Fruit::Dir::Down  ? 1 : 0; }
static Fruit::Dir opposite(Fruit::Dir d){
    switch (d){
        case Fruit::Dir::Left:  return Fruit::Dir::Right;
        case Fruit::Dir::Right: return Fruit::Dir::Left;
        case Fruit::Dir::Up:    return Fruit::Dir::Down;
        case Fruit::Dir::Down:  return Fruit::Dir::Up;
        default:                return Fruit::Dir::None;
    }
}

// Le fruit circule dans les corridors : ni mur, ni maison/porte des fantomes.
static bool fruit_can_enter(TileType t){
    return !(t == TileType::Wall ||
             t == TileType::GhostHouse ||
             t == TileType::GhostDoorClosed ||
             t == TileType::GhostDoorOpening ||
             t == TileType::GhostDoorOpen);
}
static bool tile_ok(const GameState& g, int r, int c){
    if (r < 0 || r >= MAZE_HEIGHT || c < 0 || c >= MAZE_WIDTH) return false;
    return fruit_can_enter(g.maze.tiles[r][c]);
}

static void valid_dirs(const GameState& g, int r, int c, Fruit::Dir out[4], int& n){
    n = 0;
    const Fruit::Dir all[4] = { Fruit::Dir::Up, Fruit::Dir::Left, Fruit::Dir::Down, Fruit::Dir::Right };
    for (Fruit::Dir d : all)
        if (tile_ok(g, r + dirY(d), c + dirX(d))) out[n++] = d;
}

// Direction : aleatoire parmi les valides, en evitant le demi-tour sauf
// cul-de-sac. -> corridors suivis tout droit, virages aleatoires aux carrefours.
static Fruit::Dir choose_dir(const GameState& g, const Fruit& fr){
    Fruit::Dir v[4]; int n; valid_dirs(g, fr.tile_r, fr.tile_c, v, n);
    if (n == 0) return Fruit::Dir::None;
    Fruit::Dir rev = opposite(fr.dir);
    Fruit::Dir cand[4]; int m = 0;
    for (int i = 0; i < n; ++i) if (v[i] != rev) cand[m++] = v[i];
    if (m == 0) { cand[0] = rev; m = 1; }            // cul-de-sac : demi-tour autorise
    return cand[rand() % m];
}

// Sprite + score selon le niveau (arcade-faithful).
static void pick_fruit_for_level(int level, const uint16_t*& sprite, int& score){
    if      (level == 1)  { sprite = fruit_cherry;     score = 100;  }
    else if (level == 2)  { sprite = fruit_strawberry; score = 300;  }
    else if (level <= 4)  { sprite = fruit_orange;     score = 500;  }
    else if (level <= 6)  { sprite = fruit_apple;      score = 700;  }
    else if (level <= 8)  { sprite = fruit_melon;      score = 1000; }
    else if (level <= 10) { sprite = fruit_galaxian;   score = 2000; }
    else if (level <= 12) { sprite = fruit_bell;       score = 3000; }
    else                  { sprite = fruit_key;        score = 5000; }
}

static void fruit_spawn(GameState& g){
    Fruit& fr = g.fruit;
    const Maze& m = g.maze;

    // Point d'entree : un tunnel au hasard (sinon repli sur le spawn de Pac-Man).
    int r, c;
    if (m.tunnel_entry_count > 0) {
        int idx = rand() % m.tunnel_entry_count;
        r = m.tunnel_entry_row[idx];
        c = m.tunnel_entry_col[idx];
    } else {
        r = m.pac_spawn_row; c = m.pac_spawn_col;
    }

    fr.active       = true;
    fr.tile_r = r;  fr.tile_c = c;
    fr.prev_tile_r = r; fr.prev_tile_c = c;
    fr.pixel_offset = 0;
    fr.life_ticks   = FRUIT_LIFE_TICKS;
    pick_fruit_for_level(g.level, fr.sprite, fr.score);

    // Direction initiale : une direction valide (vers l'interieur du labyrinthe).
    fr.dir = Fruit::Dir::None;
    fr.dir = choose_dir(g, fr);
    if (fr.dir == Fruit::Dir::None) fr.dir = Fruit::Dir::Up;   // garde-fou

    fr.x = fr.tile_c * TILE_SIZE + (TILE_SIZE - FRUIT_SPRITE) / 2;
    fr.y = fr.tile_r * TILE_SIZE + (TILE_SIZE - FRUIT_SPRITE) / 2;

    ++fr.spawned_count;
}

void fruit_reset(GameState& g){
    g.fruit = Fruit{};                          // tout a zero (active=false)
    g.fruit.initial_pellets = g.maze.pellet_count;
}

void fruit_update(GameState& g){
    Fruit& fr = g.fruit;
    const int eaten = fr.initial_pellets - g.maze.pellet_count;

    // --- Apparition (2 fois par niveau) ---
    if (!fr.active) {
        if      (fr.spawned_count == 0 && eaten >= FRUIT1_EATEN) fruit_spawn(g);
        else if (fr.spawned_count == 1 && eaten >= FRUIT2_EATEN) fruit_spawn(g);
        if (!fr.active) return;
    }

    // --- Duree de vie ---
    if (--fr.life_ticks <= 0) { fr.active = false; return; }

    // --- Wrap tunnels (memes portails que Pac-Man / fantomes) ---
    try_portal_wrap(g, g.portalH, fr);
    try_portal_wrap(g, g.portalV, fr);

    // --- Choix de direction au centre d'une tuile ---
    if (fr.isCentered()) {
        fr.prev_tile_r = fr.tile_r;
        fr.prev_tile_c = fr.tile_c;
        Fruit::Dir nd = choose_dir(g, fr);
        if (nd != Fruit::Dir::None) fr.dir = nd;
    }

    // --- Avance ---
    fr.pixel_offset += FRUIT_SPEED;
    if (fr.pixel_offset >= TILE_SIZE) {
        fr.tile_r += dirY(fr.dir);
        fr.tile_c += dirX(fr.dir);
        fr.pixel_offset = 0;
    }
    fr.x = fr.tile_c * TILE_SIZE + (TILE_SIZE - FRUIT_SPRITE) / 2 + dirX(fr.dir) * fr.pixel_offset;
    fr.y = fr.tile_r * TILE_SIZE + (TILE_SIZE - FRUIT_SPRITE) / 2 + dirY(fr.dir) * fr.pixel_offset;

    // --- Collision avec Pac-Man ---
    const int fcx = fr.x + FRUIT_SPRITE/2, fcy = fr.y + FRUIT_SPRITE/2;
    const int pcx = g.pacman.x + PACMAN_SIZE/2, pcy = g.pacman.y + PACMAN_SIZE/2;
    const int dx = fcx - pcx, dy = fcy - pcy;
    if (dx*dx + dy*dy < 12*12) {
        g.score += fr.score;
        g.floatingScores.push_back({ fr.x, fr.y, fr.score, 90 });
        fr.active = false;
        audio_play_fruit();
    }
}

void fruit_draw(const GameState& g){
    const Fruit& fr = g.fruit;
    if (!fr.active || !fr.sprite) return;
    // Meme convention que Pac-Man/fantomes : transparence noir (0x0000).
    gfx_drawSprite(fr.x, fr.y - (int)g_camera_y, fr.sprite, FRUIT_SPRITE, FRUIT_SPRITE);
}

const uint16_t* fruit_sprite_for_level(int level){
    const uint16_t* s = nullptr; int score = 0;
    if (level < 1) level = 1;
    pick_fruit_for_level(level, s, score);
    return s;
}
