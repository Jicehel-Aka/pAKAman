#include "maze.h"
#include "assets/assets.h"
#include "core/sprite.h"
#include "game/config.h"
#include "core/graphics.h"

extern float g_camera_y;

/*
============================================================
  ASCII DU LABYRINTHE B (NIVEAU 1)
============================================================
*/
const char* maze_B_ascii[MAZE_HEIGHT] = {
    "####################",
    "#o.........#......o#",
    "#.##.#####.#.##.##.#",
    "#.#......#.#.#...#.#",
    "#.#.##.#.#.#.#.#.#.#",
    "#.#.#..#...#.#.#.#.#",
    "#.#.#.##.###.#.#.#.#",
    "#...#............#.#",
    "###.#.####D###.###.#",
    "#...#.#HHHHHH#.#...#",
    "###.#.#HHHHHH#.#.###",
    "#...#.#HHHHHH#.#...#",
    "#.###.########.###.#",
    "#.......#...#......#",
    "#.#.#.#.#.#.####.#.#",
    "#.#.#.#.#.#.#....#.#",
    "#.#.###.#.#...##.#.#",
    "#.#...#.#.#.#....#.#",
    "#.#.###.#.#.######.#",
    "TE.......P........ET",
    "####################",
};

/*
============================================================
  setGhostDoor : met à jour TOUTES les tuiles porte
============================================================
*/
void Maze::setGhostDoor(TileType newState)
{
    for (int r = 0; r < MAZE_HEIGHT; r++)
        for (int c = 0; c < MAZE_WIDTH; c++)
            if (isGhostDoorTile(tiles[r][c]))
                tiles[r][c] = newState;
}

/*
============================================================
  ASCII → MAZE
============================================================
*/
void maze_from_ascii(const char* ascii[MAZE_HEIGHT], Maze& maze)
{
    maze.pellet_count        = 0;
    maze.power_pellet_count  = 0;
    maze.tunnel_entry_count  = 0;

    int house_sum_r = 0;
    int house_sum_c = 0;
    int house_count = 0;

    for (int r = 0; r < MAZE_HEIGHT; r++)
    {
        const char* line = ascii[r];

        for (int c = 0; c < MAZE_WIDTH; c++)
        {
            char ch = line[c];
            TileType t = TileType::Empty;

            switch (ch)
            {
                case '#': t = TileType::Wall; break;
                case '.': t = TileType::Pellet; maze.pellet_count++; break;
                case 'o': t = TileType::PowerPellet; maze.power_pellet_count++; break;

                case 'H':
                    t = TileType::GhostHouse;
                    house_sum_r += r;
                    house_sum_c += c;
                    if (house_count < 4) {
                        maze.ghost_spawn_row[house_count] = r;
                        maze.ghost_spawn_col[house_count] = c;
                    }
                    house_count++;
                    break;

                case 'D':
                    t = TileType::GhostDoorClosed;
                    maze.ghost_door_row = r;
                    maze.ghost_door_col = c;
                    break;

                case 'T':
                    t = TileType::Tunnel;
                    break;

                case 'E':
                    t = TileType::TunnelEntry;
                    if (maze.tunnel_entry_count < 2) {
                        maze.tunnel_entry_row[maze.tunnel_entry_count] = r;
                        maze.tunnel_entry_col[maze.tunnel_entry_count] = c;
                        maze.tunnel_entry_count++;
                    }
                    break;

                case 'P':
                    t = TileType::PacSpawn;
                    maze.pac_spawn_row = r;
                    maze.pac_spawn_col = c;
                    break;

                case 'F':
                    t = TileType::FruitSpawn;
                    maze.fruit_row = r;
                    maze.fruit_col = c;
                    break;

                default:
                    t = TileType::Empty;
                    break;
            }

            maze.tiles[r][c] = t;
        }
    }

    // Centre de la maison fantôme (moyenne des H)
    if (house_count > 0) {
        maze.ghost_center_row = house_sum_r / house_count;
        maze.ghost_center_col = house_sum_c / house_count;
    }
}

/*
============================================================
  RENDU DU LABYRINTHE
============================================================
*/
void Maze::draw() const
{
    for (int r = 0; r < MAZE_HEIGHT; r++)
    {
        for (int c = 0; c < MAZE_WIDTH; c++)
        {
            TileType t = tiles[r][c];

            int px = c * TILE_SIZE;
            int py = r * TILE_SIZE;

            int sx = px;
            int sy = py - (int)g_camera_y;

            if (sy < -TILE_SIZE || sy >= SCREEN_H)
                continue;

            switch (t)
            {
                case TileType::Wall:
                    draw_sprite16(sx, sy, tile_wall);
                    break;

                case TileType::Pellet:
                    draw_sprite16(sx, sy, tile_pacgum);
                    break;

                case TileType::PowerPellet:
                    draw_sprite16(sx, sy, tile_powerdot);
                    break;

                case TileType::Tunnel:
                    draw_sprite16(sx, sy, tile_tunnel_wall);
                    break;

                case TileType::TunnelEntry:
                {
                    // Détection directionnelle robuste
                    bool left  = (c > 0              && tiles[r][c-1] == TileType::Tunnel);
                    bool right = (c < MAZE_WIDTH-1   && tiles[r][c+1] == TileType::Tunnel);
                    bool up    = (r > 0              && tiles[r-1][c] == TileType::Tunnel);
                    bool down  = (r < MAZE_HEIGHT-1  && tiles[r+1][c] == TileType::Tunnel);

                    if (left)       draw_sprite16(sx, sy, tile_tunnel_entry_left);
                    else if (right) draw_sprite16(sx, sy, tile_tunnel_entry_right);
                    else if (up)    draw_sprite16(sx, sy, tile_tunnel_entry_up);
                    else if (down)  draw_sprite16(sx, sy, tile_tunnel_entry_down);
                    else            draw_sprite16(sx, sy, tile_tunnel_entry_neutral);
                    break;
                }

                case TileType::GhostDoorClosed:
                    draw_sprite16(sx, sy, tile_ghost_door_closed);
                    break;

                case TileType::GhostDoorOpening:
                    draw_sprite16(sx, sy, tile_ghost_door_opening);
                    break;

                case TileType::GhostDoorOpen:
                    draw_sprite16(sx, sy, tile_ghost_door_open);
                    break;

/*                case TileType::GhostHouse:
                    // Optionnel : motif discret
                    draw_sprite16(sx, sy, tile_ghost_house);
                    break;
*/
                default:
                    break;
            }
        }
    }
}
