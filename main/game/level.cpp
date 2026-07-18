#include "level.h"
#include "game.h"
#include "maze.h"
#include "pacman.h"
#include "ghost.h"

void level_init(GameState &g)
{
    g.ghosts.clear();

    maze_from_ascii(maze_B_ascii, g.maze);

    // Pac-Man
    int prow = g.maze.pac_spawn_row;
    int pcol = g.maze.pac_spawn_col;
    int px = pcol * TILE_SIZE;
    int py = prow * TILE_SIZE;
    g.pacman.x = px + PACMAN_OFFSET;
    g.pacman.y = py + PACMAN_OFFSET;

    // Localise la maison des fantômes
    int grow = g.maze.ghost_center_row;
    int gcol = g.maze.ghost_center_col;

    // Fantômes initialisés dans la maison
    auto addGhost = [&](int id, int col, int row)
    {
        g.ghosts.push_back(Ghost(id, col, row));
        g.ghosts.back().x = col * TILE_SIZE + GHOST_OFFSET;
        g.ghosts.back().y = row * TILE_SIZE + GHOST_OFFSET;
        g.ghosts.back().mode = Ghost::Mode::Scatter;
        g.ghosts.back().houseState = Ghost::HouseState::Inside;
    };

    addGhost(0, gcol, grow);
    addGhost(1, gcol - 1, grow);
    addGhost(2, gcol + 1, grow);
    addGhost(3, gcol, grow + 1);
}
