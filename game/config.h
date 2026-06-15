#pragma once

/*
============================================================
  config.h — Constantes globales du jeu (Pacman / Gamebuino AKA)
------------------------------------------------------------
  Toutes les constantes du projet centralisées ici.
  Utilisation de constexpr partout (pas de #define sauf debug).
============================================================
*/

// ---------------------------------------------------------------------------
//  ÉCRAN
// ---------------------------------------------------------------------------
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

// ---------------------------------------------------------------------------
//  GRILLE / LABYRINTHE
// ---------------------------------------------------------------------------
constexpr int TILE_SIZE  = 16;
constexpr int MAZE_COLS  = SCREEN_W / TILE_SIZE;   // 20 colonnes
constexpr int MAZE_ROWS  = SCREEN_H / TILE_SIZE;   // 15 lignes

constexpr int GRID_X0    = 0;   // origine X de la grille (pixels)
constexpr int GRID_Y0    = 0;   // origine Y de la grille (pixels)
constexpr int CENTER_EPS = 2;   // tolérance d'alignement au centre (px)
constexpr int SNAP_EPS   = 3;   // distance sous laquelle on "snap" au centre

// ---------------------------------------------------------------------------
//  PAC-MAN
// ---------------------------------------------------------------------------
constexpr int PACMAN_SIZE   = 14;
constexpr int PACMAN_SPEED  = 3;   // pixels / frame
constexpr int PACMAN_OFFSET = (TILE_SIZE - PACMAN_SIZE) / 2;

// Joystick — JOYX_MID et seuils sont déjà définis dans lib/common.h

// ---------------------------------------------------------------------------
//  FANTÔMES
// ---------------------------------------------------------------------------
constexpr int GHOST_SIZE   = 14;
constexpr int GHOST_OFFSET = (TILE_SIZE - GHOST_SIZE) / 2;
constexpr int NUM_GHOSTS   = 4;   // Blinky, Pinky, Inky, Clyde

// Vitesses (pixels / frame)
constexpr int GHOST_SPEED            = 2;   // normal (Scatter / Chase)
constexpr int GHOST_SPEED_TUNNEL     = 1;   // dans les tunnels (~40 % arcade)
constexpr int GHOST_SPEED_FRIGHTENED = 1;   // mode Frightened (~50 % arcade)
constexpr int GHOST_SPEED_EYES       = 4;   // yeux, retour maison

// Timings sortie de la ghost house
constexpr int GHOST_RELEASE_INTERVAL_TICKS = 3 * 60;   // 3 s
constexpr int FIRST_GHOST_RELEASE_TICKS    = 5 * 60;   // 5 s

// Mode Frightened
constexpr int FRIGHTENED_DURATION_TICKS    = 360;   // 6 s
constexpr int FRIGHTENED_BLINK_START_TICKS = 120;   // clignotement sur les 2 dernières s

// ---------------------------------------------------------------------------
//  SCORES
// ---------------------------------------------------------------------------
constexpr int DOT_SCORE       = 10;
constexpr int POWERDOT_SCORE  = 50;
constexpr int GHOST_SCORE     = 200;   // doublé à chaque chaîne (200, 400…)
constexpr int FRUIT_SCORE     = 500;

// ---------------------------------------------------------------------------
//  COLLISION
// ---------------------------------------------------------------------------
constexpr int COLLISION_RADIUS = 12;   // rayon carré pour détection Pac-Man / fantôme

// ---------------------------------------------------------------------------
//  MODE DEBUG
// ---------------------------------------------------------------------------
extern int debug;   // 0 = off, 1 = on

#define DBG(code) do { if (debug) { code; } } while(0)
