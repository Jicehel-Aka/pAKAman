#pragma once
#include <stdint.h>
#include "level.h"
#include "game/config.h"
#include "core/graphics.h"

// Dimensions du labyrinthe (ASCII)
static const int MAZE_WIDTH  = LEVEL_COLS;  // doit être 20
static const int MAZE_HEIGHT = LEVEL_ROWS;  // doit être 21

/*
============================================================
  TYPES DE CASES DU LABYRINTHE
============================================================
*/
enum class TileType : uint8_t {
    Empty,
    Wall,
    Pellet,
    PowerPellet,

    GhostHouse,         // intérieur de la maison des fantômes

    GhostDoorClosed,
    GhostDoorOpening,
    GhostDoorOpen,

    Tunnel,             // case adjacente au tunnel
    TunnelEntry,        // portail (wrap)

    PacSpawn,
    FruitSpawn
};

/*
============================================================
  STRUCTURE MAZE (LOGIQUE PURE)
============================================================
*/
struct Maze {

    TileType tiles[MAZE_HEIGHT][MAZE_WIDTH];

    // Comptage
    int pellet_count       = 0;
    int power_pellet_count = 0;

    // Points spéciaux
    int pac_spawn_row = 0;
    int pac_spawn_col = 0;

    int ghost_spawn_row[4] = {0,0,0,0};
    int ghost_spawn_col[4] = {0,0,0,0};

    int fruit_row = -1;
    int fruit_col = -1;

    // Porte fantôme
    int ghost_door_row = -1;
    int ghost_door_col = -1;

    // Centre de la maison fantôme
    int ghost_center_row = -1;
    int ghost_center_col = -1;

    // Tunnels (2 entrées max)
    int tunnel_entry_count = 0;
    int tunnel_entry_row[2];
    int tunnel_entry_col[2];

    /*
    --------------------------------------------------------
      Rendu (séparé de la logique)
    --------------------------------------------------------
    */
    void draw() const;

    /*
    --------------------------------------------------------
      Mise à jour de la porte fantôme
      (met à jour toutes les tuiles GhostDoor)
    --------------------------------------------------------
    */
    void setGhostDoor(TileType newState);
};

/*
============================================================
  CHARGEMENT ASCII → MAZE
============================================================
*/
void maze_from_ascii(const char* ascii[MAZE_HEIGHT], Maze& maze);

/*
============================================================
  NIVEAU 1 : ASCII
============================================================
*/
extern const char* maze_B_ascii[MAZE_HEIGHT];

/*
============================================================
  HELPERS
============================================================
*/
inline bool isGhostHouseTile(TileType t)
{
    return (t == TileType::GhostHouse);
}

inline bool isGhostDoorTile(TileType t)
{
    return (t == TileType::GhostDoorClosed ||
            t == TileType::GhostDoorOpening ||
            t == TileType::GhostDoorOpen);
}

inline bool isGhostRestrictedForPacman(TileType t)
{
    return isGhostHouseTile(t) || isGhostDoorTile(t);
}
