#pragma once
#include <stdint.h>
#include "game/config.h"  // pour L_p, H_p, GHOST_SIZE si besoin

/*
============================================================
  assets.h — Déclarations des sprites du jeu (BGR565)
------------------------------------------------------------
Tous les sprites sont en BGR565 (uint16_t).

Ce header déclare tous les tableaux définis dans assets.cpp :
 - tiles du labyrinthe (murs, gommes, tunnels, porte fantômes…)
 - Pac-Man (4 directions × 3 frames)
 - fantômes (4 couleurs × 2 frames)
 - yeux des fantômes
 - sprites frightened (bleu / blanc)
 - animation de mort de Pac-Man
 - fruits (placeholders)
============================================================
*/

/*
============================================================
  TILES LABYRINTHE (16×16)
============================================================
*/
extern uint16_t tile_wall[16*16];
extern uint16_t tile_pacgum[16*16];
extern uint16_t tile_powerdot[16*16];

extern uint16_t tile_tunnel_wall[16*16];
extern uint16_t tile_tunnel_entry_left[16*16];
extern uint16_t tile_tunnel_entry_right[16*16];
extern uint16_t tile_tunnel_entry_up[16*16];
extern uint16_t tile_tunnel_entry_down[16*16];
extern uint16_t tile_tunnel_entry_neutral[16*16];

extern uint16_t tile_ghost_door_closed[16*16];
extern uint16_t tile_ghost_door_opening[16*16]; 
extern uint16_t tile_ghost_door_open[16*16];


/*
============================================================
  PAC-MAN (L_p × H_p)
  4 directions × 3 frames
============================================================
*/
extern uint16_t pacman_right_0[14*14];
extern uint16_t pacman_right_1[14*14];
extern uint16_t pacman_right_2[14*14];

extern uint16_t pacman_left_0[14*14];
extern uint16_t pacman_left_1[14*14];
extern uint16_t pacman_left_2[14*14];

extern uint16_t pacman_up_0[14*14];
extern uint16_t pacman_up_1[14*14];
extern uint16_t pacman_up_2[14*14];

extern uint16_t pacman_down_0[14*14];
extern uint16_t pacman_down_1[14*14];
extern uint16_t pacman_down_2[14*14];


/*
============================================================
  ANIMATION DE MORT DE PAC-MAN (14×14 × 12 frames)
============================================================
*/
extern const uint16_t pacman_death_0[14*14];
extern const uint16_t pacman_death_1[14*14];
extern const uint16_t pacman_death_2[14*14];
extern const uint16_t pacman_death_3[14*14];
extern const uint16_t pacman_death_4[14*14];
extern const uint16_t pacman_death_5[14*14];
extern const uint16_t pacman_death_6[14*14];
extern const uint16_t pacman_death_7[14*14];
extern const uint16_t pacman_death_8[14*14];
extern const uint16_t pacman_death_9[14*14];
extern const uint16_t pacman_death_10[14*14];
extern const uint16_t pacman_death_11[14*14];

// tableau d’animation utilisé par game_draw()
extern const uint16_t* pacman_death_anim[12];

/*
============================================================
  FANTÔMES (L_p × H_p)
  4 couleurs × 2 frames
============================================================
*/
extern uint16_t ghost_red_0[14*14];
extern uint16_t ghost_red_1[14*14];

extern uint16_t ghost_pink_0[14*14];
extern uint16_t ghost_pink_1[14*14];

extern uint16_t ghost_blue_0[14*14];
extern uint16_t ghost_blue_1[14*14];

extern uint16_t ghost_orange_0[14*14];
extern uint16_t ghost_orange_1[14*14];

/*
============================================================
  YEUX DES FANTÔMES (10×5)
============================================================
*/
extern uint16_t ghost_eyes_left[10 * 5];
extern uint16_t ghost_eyes_right[10 * 5];
extern uint16_t ghost_eyes_up[10 * 5];
extern uint16_t ghost_eyes_down[10 * 5];

/*
============================================================
  GHOSTS BLANCS (clignotement en fin de mode Frightened)
============================================================
*/
extern uint16_t ghost_white_0[14*14];
extern uint16_t ghost_white_1[14*14];

/*
============================================================
  MODE FRIGHTENED (BLEU / BLANC)
============================================================
*/
extern uint16_t ghost_scared_0[14*14];
extern uint16_t ghost_scared_1[14*14];

/*
============================================================
  FRUITS (14×14) — PLACEHOLDERS
============================================================
*/
extern uint16_t fruit_cherry[14*14];
extern uint16_t fruit_strawberry[14*14];
extern uint16_t fruit_orange[14*14];
extern uint16_t fruit_apple[14*14];
extern uint16_t fruit_melon[14*14];
extern uint16_t fruit_galaxian[14*14];
extern uint16_t fruit_bell[14*14];
extern uint16_t fruit_key[14*14];

/*
============================================================
  INITIALISATION DES ASSETS
------------------------------------------------------------
assets_init() :
 - remplit les tiles dynamiques (mur, pacgum, powerdot)
 - ne modifie pas les sprites définis en dur
============================================================
*/
void assets_init();
