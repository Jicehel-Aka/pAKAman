/*
============================================================
  ghost.cpp — IA, mouvement et rendu des fantômes
------------------------------------------------------------
Ce module gère :
 - les modes Scatter / Chase / Frightened / Eaten
 - la ghost house (Inside / Leaving / Outside)
 - le pathfinding BFS pour les yeux
 - le mouvement case-based (comme Pac-Man)
 - le tunnel wrap
 - le rendu (corps + yeux directionnels)

La logique est strictement arcade-faithful.
============================================================
*/

#include "ghost.h"
#include "game/game.h"
#include "maze.h"
#include "config.h"
#include "assets/assets.h"
#include "core/graphics.h"
#include "core/sprite.h"

#include <queue>
#include <algorithm>
#include <random>
#include <cmath>

extern float g_camera_y;
extern int   debug;

// RNG pour frightened
static std::mt19937 ghost_rng(123456);

/*
============================================================
  Helpers direction
============================================================
*/
int Ghost::dirX(Dir d) {
    switch (d) {
        case Dir::Left:  return -1;
        case Dir::Right: return +1;
        default:         return 0;
    }
}

int Ghost::dirY(Dir d) {
    switch (d) {
        case Dir::Up:    return -1;
        case Dir::Down:  return +1;
        default:         return 0;
    }
}

bool Ghost::isCentered() const {
    return pixel_offset == 0;
}

/*
============================================================
  Demi-tour instantané
============================================================
*/
void Ghost::reverse_direction()
{
    switch (dir)
    {
        case Dir::Left:  dir = Dir::Right; break;
        case Dir::Right: dir = Dir::Left;  break;
        case Dir::Up:    dir = Dir::Down;  break;
        case Dir::Down:  dir = Dir::Up;    break;
        default: break;
    }
}

/*
============================================================
  Opposé de direction (helper interne)
============================================================
*/
static bool is_opposite(Ghost::Dir a, Ghost::Dir b)
{
    return (a == Ghost::Dir::Left  && b == Ghost::Dir::Right) ||
           (a == Ghost::Dir::Right && b == Ghost::Dir::Left)  ||
           (a == Ghost::Dir::Up    && b == Ghost::Dir::Down)  ||
           (a == Ghost::Dir::Down  && b == Ghost::Dir::Up);
}

/*
============================================================
  Walkability spéciale pour les yeux (mode Eaten)
============================================================
*/
bool is_walkable_for_eyes(TileType t)
{
    switch (t)
    {
        case TileType::Wall:
            return false;

        // Les yeux peuvent traverser la maison et la porte
        case TileType::GhostHouse:
        case TileType::GhostDoorClosed:
        case TileType::GhostDoorOpening:
        case TileType::GhostDoorOpen:
            return true;

        // Toutes les cases normales
        default:
            return true;
    }
}

/*
============================================================
  BFS pour les yeux (mode Eaten)
============================================================
*/
static bool bfs_to_target(const Maze& maze,
                          int sr, int sc,
                          int tr, int tc,
                          std::vector<std::pair<int,int>>& path)
{
    struct Node { int r, c; };
    static const int dr[4] = {-1, +1, 0, 0};
    static const int dc[4] = {0, 0, -1, +1};

    // visited/parent en STATIQUE (pas sur la pile) : ~3.8 Ko qui, sur la pile
    // reduite de la tache principale (3584 o), provoquaient un debordement ->
    // reboot loader quand on mangeait un fantome (mode Eaten -> BFS). Sans risque
    // ici : le jeu est mono-thread (seul app_main appelle game_update).
    static bool visited[MAZE_HEIGHT][MAZE_WIDTH];
    static Node parent[MAZE_HEIGHT][MAZE_WIDTH];
    for (int r = 0; r < MAZE_HEIGHT; ++r)
        for (int c = 0; c < MAZE_WIDTH; ++c)
            visited[r][c] = false;

    std::queue<Node> q;
    q.push({sr, sc});
    visited[sr][sc] = true;

    while (!q.empty())
    {
        Node n = q.front(); q.pop();

        if (n.r == tr && n.c == tc)
        {
            // Reconstruction du chemin
            path.clear();
            int r = tr, c = tc;

            while (!(r == sr && c == sc))
            {
                path.push_back({r, c});
                Node p = parent[r][c];
                r = p.r;
                c = p.c;
            }

            std::reverse(path.begin(), path.end());
            return true;
        }

        for (int i = 0; i < 4; i++)
        {
            int nr = n.r + dr[i];
            int nc = n.c + dc[i];

            if (nr < 0 || nr >= MAZE_HEIGHT ||
                nc < 0 || nc >= MAZE_WIDTH)
                continue;

            if (!is_walkable_for_eyes(maze.tiles[nr][nc]))
                continue;

            if (!visited[nr][nc])
            {
                visited[nr][nc] = true;
                parent[nr][nc] = n;
                q.push({nr, nc});
            }
        }
    }

    return false;
}

/*
============================================================
  Constructeur
============================================================
*/
Ghost::Ghost(int id_, int start_c, int start_r)
{
    id = id_;
    start_col = start_c;
    start_row = start_r;

    tile_c = start_c;
    tile_r = start_r;
    pixel_offset = 0;

    x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2;
    y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2;

    dir = Dir::Left;
    next_dir = Dir::Left;

    mode = Mode::Scatter;
    previous_mode = Mode::Scatter;

    houseState = HouseState::Inside;
    eaten_timer = 0;

    speed_normal     = GHOST_SPEED;
    speed_frightened = GHOST_SPEED_FRIGHTENED;
    speed_tunnel     = GHOST_SPEED_TUNNEL;
    speed_eyes       = GHOST_SPEED_EYES;
}

/*
============================================================
  Reset après mort de Pac-Man
============================================================
*/
void Ghost::reset_to_start()
{
    tile_r = start_row;
    tile_c = start_col;
    pixel_offset = 0;

    dir = Dir::Left;
    next_dir = Dir::Left;

    mode = Mode::Scatter;
    previous_mode = Mode::Scatter;

    houseState = HouseState::Inside;
    eaten_timer = 0;

    path.clear();
}

/*
============================================================
  Frightened : début / fin
============================================================
*/
void Ghost::on_start_frightened()
{
    if (mode == Mode::Eaten)
        return;

    previous_mode = mode;
    mode = Mode::Frightened;

    reverse_direction();
}

void Ghost::on_end_frightened()
{
    if (mode == Mode::Frightened)
        mode = previous_mode;
}

/*
============================================================
  Directions valides
============================================================
*/
std::vector<Ghost::Dir> Ghost::getValidDirections(const GameState& g,
                                                  int row, int col,
                                                  bool is_eyes) const
{
    std::vector<Dir> dirs;
    dirs.reserve(4);

    for (Dir d : {Dir::Up, Dir::Left, Dir::Down, Dir::Right})
    {
        int nr = row + dirY(d);
        int nc = col + dirX(d);

        if (nr < 0 || nr >= MAZE_HEIGHT ||
            nc < 0 || nc >= MAZE_WIDTH)
            continue;

        TileType t = g.maze.tiles[nr][nc];

        if (is_eyes)
        {
            if (t != TileType::Wall)
                dirs.push_back(d);
        }
        else
        {
            if (t == TileType::Wall ||
                t == TileType::GhostDoorClosed ||
                t == TileType::GhostDoorOpening)
                continue;

            if (houseState == HouseState::Outside &&
                (t == TileType::GhostHouse || t == TileType::GhostDoorOpen))
                continue;

            dirs.push_back(d);
        }
    }

    return dirs;
}

/*
============================================================
  Choix direction vers une cible (Scatter / Chase / Eyes)
============================================================
*/
Ghost::Dir Ghost::chooseDirectionTowardsTarget(const GameState& g,
                                               int row, int col,
                                               int tr, int tc,
                                               bool is_eyes) const
{
    auto valid = getValidDirections(g, row, col, is_eyes);
    if (valid.empty())
        return dir;

    // Évite le demi-tour si possible
    std::vector<Dir> filtered;
    for (Dir d : valid)
        if (!is_opposite(d, dir))
            filtered.push_back(d);

    if (!filtered.empty())
        valid = filtered;

    Dir best = valid.front();
    int bestDist = 999999;

    for (Dir d : valid)
    {
        int nr = row + dirY(d);
        int nc = col + dirX(d);
        int dist = std::abs(nr - tr) + std::abs(nc - tc);

        if (dist < bestDist)
        {
            best = d;
            bestDist = dist;
        }
    }

    return best;
}

/*
============================================================
  Direction aléatoire (Frightened)
============================================================
*/
Ghost::Dir Ghost::chooseRandomDir(const GameState& g,
                                  int row, int col,
                                  bool is_eyes) const
{
    auto valid = getValidDirections(g, row, col, is_eyes);
    if (valid.empty())
        return dir;

    std::vector<Dir> filtered;
    for (Dir d : valid)
        if (!is_opposite(d, dir))
            filtered.push_back(d);

    if (!filtered.empty())
        valid = filtered;

    std::uniform_int_distribution<int> dist(0, valid.size() - 1);
    return valid[dist(ghost_rng)];
}

/*
============================================================
  Cibles Scatter / Chase
============================================================
*/
void Ghost::getScatterTarget(const GameState& g, int& tr, int& tc) const
{
    switch (id)
    {
        case 0: tr = 0;              tc = MAZE_WIDTH - 1; break; // Blinky
        case 1: tr = 0;              tc = 0;              break; // Pinky
        case 2: tr = MAZE_HEIGHT - 1; tc = MAZE_WIDTH - 1; break; // Inky
        case 3: tr = MAZE_HEIGHT - 1; tc = 0;              break; // Clyde
        default: tr = 0; tc = MAZE_WIDTH - 1; break;
    }
}

void Ghost::getChaseTarget(const GameState& g, int& tr, int& tc) const
{
    int pr = g.pacman.tileRow();
    int pc = g.pacman.tileCol();

    switch (id)
    {
        case 0: // Blinky
            tr = pr; tc = pc;
            break;

        case 1: // Pinky
            tr = pr + 4 * dirY((Dir)g.pacman.dir);
            tc = pc + 4 * dirX((Dir)g.pacman.dir);
            break;

        case 2: // Inky
        {
            const Ghost* blinky = nullptr;
            for (const auto& gh : g.ghosts)
                if (gh.id == 0) blinky = &gh;

            int aheadR = pr + 2 * dirY((Dir)g.pacman.dir);
            int aheadC = pc + 2 * dirX((Dir)g.pacman.dir);

            if (blinky)
            {
                int br = blinky->tile_r;
                int bc = blinky->tile_c;
                int vr = aheadR - br;
                int vc = aheadC - bc;
                tr = br + 2 * vr;
                tc = bc + 2 * vc;
            }
            else
            {
                tr = aheadR;
                tc = aheadC;
            }
            break;
        }

        case 3: // Clyde
        {
            int dist = std::abs(tile_r - pr) + std::abs(tile_c - pc);
            if (dist >= 8)
            {
                tr = pr;
                tc = pc;
            }
            else
            {
                getScatterTarget(g, tr, tc);
            }
            break;
        }

        default:
            tr = pr;
            tc = pc;
            break;
    }
}

/*
============================================================
  Choix direction dans la maison
============================================================
*/
Ghost::Dir Ghost::chooseDirectionInsideHouse(const GameState& g,
                                             int row, int col)
{
    std::vector<Dir> valid;

    auto try_add = [&](Dir d)
    {
        int nr = row + dirY(d);
        int nc = col + dirX(d);

        if (g.maze.tiles[nr][nc] == TileType::GhostHouse)
            valid.push_back(d);
    };

    try_add(Dir::Up);
    try_add(Dir::Down);
    try_add(Dir::Left);
    try_add(Dir::Right);

    if (valid.empty())
        return dir;

    return valid[rand() % valid.size()];
}

/*
============================================================
  UPDATE principal
============================================================
*/
void Ghost::update(GameState& g)
{
    bool is_eyes = (mode == Mode::Eaten);
    int row = tile_r;
    int col = tile_c;

    // --------------------------------------------------------
    // Fail-safe : si on n'est plus dans la maison → Outside
    // --------------------------------------------------------
    TileType t = g.maze.tiles[row][col];

    if (t != TileType::GhostHouse &&
        t != TileType::GhostDoorClosed &&
        t != TileType::GhostDoorOpening &&
        t != TileType::GhostDoorOpen)
    {
        if (houseState != HouseState::Outside)
            houseState = HouseState::Outside;
    }

    // --------------------------------------------------------
    // Vitesses selon mode
    // --------------------------------------------------------
    int speed = speed_normal;
    if (mode == Mode::Frightened) speed = speed_frightened;
    if (mode == Mode::Eaten)      speed = speed_eyes;

    if (g.maze.tiles[row][col] == TileType::Tunnel)
        speed = speed_tunnel;

    /*
    ============================================================
      1) Mode Eaten → retour maison (pathfinding)
    ============================================================
    */
    if (mode == Mode::Eaten)
    {
        int tr = g.maze.ghost_center_row;
        int tc = g.maze.ghost_center_col;

        // Arrivé au centre → reset
        if (tile_r == tr && tile_c == tc)
        {
            mode = previous_mode;
            houseState = HouseState::Inside;
            eaten_timer = 30;

            releaseTime_ticks = g.elapsed_ticks + g.ghostReleaseInterval_ticks;

            pixel_offset = 0;
            dir = Dir::Up;

            x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2;
            y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2;
            return;
        }

        // Avancement
        pixel_offset += speed;

        if (pixel_offset >= TILE_SIZE)
        {
            pixel_offset = 0;

            prev_tile_r = tile_r;
            prev_tile_c = tile_c;

            if (path.empty())
                bfs_to_target(g.maze, tile_r, tile_c, tr, tc, path);

            if (!path.empty())
            {
                auto [nr, nc] = path.front();
                path.erase(path.begin());

                if (nr < tile_r)      dir = Dir::Up;
                else if (nr > tile_r) dir = Dir::Down;
                else if (nc < tile_c) dir = Dir::Left;
                else if (nc > tile_c) dir = Dir::Right;

                tile_r = nr;
                tile_c = nc;
            }

            // Tunnel wrap
            if (try_portal_wrap(g, g.portalH, *this) ||
                try_portal_wrap(g, g.portalV, *this))
            {
                x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2;
                y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2;
                return;
            }
        }

        // Position pixel
        x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2 + dirX(dir) * pixel_offset;
        y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2 + dirY(dir) * pixel_offset;
        return;
    }

    /*
    ============================================================
      2) Inside : déplacement dans la maison
    ============================================================
    */
    if (houseState == HouseState::Inside)
    {
        bool door_open   = (g.ghostDoorState == GameState::DoorState::Open);
        bool can_release = (g.elapsed_ticks >= releaseTime_ticks);

        // A) Porte fermée ou pas mon tour → mouvement interne
        if (!door_open || (door_open && !can_release))
        {
            if (isCentered())
                dir = chooseDirectionInsideHouse(g, row, col);

            pixel_offset += speed;

            if (pixel_offset >= TILE_SIZE)
            {
                int nr = row + dirY(dir);
                int nc = col + dirX(dir);

                if (g.maze.tiles[nr][nc] == TileType::GhostHouse)
                {
                    tile_r = nr;
                    tile_c = nc;
                }

                pixel_offset = 0;
            }

            x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2 + dirX(dir) * pixel_offset;
            y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2 + dirY(dir) * pixel_offset;
            return;
        }

        // B) Porte ouverte ET c'est mon tour → sortir
        int up_r = tile_r - 1;
        int up_c = tile_c;

        if (isCentered() &&
            up_r >= 0 &&
            g.maze.tiles[up_r][up_c] == TileType::GhostDoorOpen)
        {
            houseState   = HouseState::Leaving;
            dir          = Dir::Up;
            pixel_offset = 0;

            x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2;
            y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2;
            return;
        }

        // Sinon : BFS vers la case juste sous la porte
        if (isCentered())
        {
            int target_r = g.maze.ghost_door_row + 1;
            int target_c = g.maze.ghost_door_col;

            if (path.empty())
                bfs_to_target(g.maze, row, col, target_r, target_c, path);

            if (!path.empty())
            {
                auto [nr, nc] = path.front();
                path.erase(path.begin());

                if (nr < row)      dir = Dir::Up;
                else if (nr > row) dir = Dir::Down;
                else if (nc < col) dir = Dir::Left;
                else if (nc > col) dir = Dir::Right;
            }
        }

        pixel_offset += speed;
        if (pixel_offset >= TILE_SIZE)
        {
            tile_r += dirY(dir);
            tile_c += dirX(dir);
            pixel_offset = 0;
        }

        x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2 + dirX(dir) * pixel_offset;
        y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2 + dirY(dir) * pixel_offset;
        return;
    }

    /*
    ============================================================
      3) Leaving → Outside
    ============================================================
    */
    if (houseState == HouseState::Leaving)
    {
        // Porte refermée → fantôme figé
        if (g.ghostDoorState != GameState::DoorState::Open)
        {
            pixel_offset = 0;
            x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2;
            y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2;
            return;
        }

        // Avancer vers le haut
        pixel_offset += speed;
        if (pixel_offset >= TILE_SIZE)
        {
            tile_r += dirY(dir);
            tile_c += dirX(dir);
            pixel_offset = 0;
        }

        x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2 + dirX(dir) * pixel_offset;
        y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2 + dirY(dir) * pixel_offset;

        // Dès qu’on quitte la porte → Outside
        TileType t2 = g.maze.tiles[tile_r][tile_c];
        if (t2 != TileType::GhostDoorOpen)
        {
            houseState = HouseState::Outside;

            if (mode != Mode::Eaten && mode != Mode::Frightened)
            {
                mode = (g.global_mode == GlobalGhostMode::Scatter)
                       ? Mode::Scatter
                       : Mode::Chase;
            }
        }

        return;
    }

    /*
    ============================================================
      4) Outside : choix de direction (Scatter / Chase / Frightened)
    ============================================================
    */
    if (isCentered())
    {
        if (mode == Mode::Frightened)
        {
            dir = chooseRandomDir(g, row, col, is_eyes);
        }
        else if (mode == Mode::Scatter)
        {
            int tr, tc;
            getScatterTarget(g, tr, tc);
            dir = chooseDirectionTowardsTarget(g, row, col, tr, tc, is_eyes);
        }
        else if (mode == Mode::Chase)
        {
            int tr, tc;
            getChaseTarget(g, tr, tc);
            dir = chooseDirectionTowardsTarget(g, row, col, tr, tc, is_eyes);
        }
    }

    /*
    ============================================================
      5) Mouvement case-based + tunnel wrap
    ============================================================
    */
    pixel_offset += speed;

    if (pixel_offset >= TILE_SIZE)
    {
        prev_tile_r = tile_r;
        prev_tile_c = tile_c;

        tile_r += dirY(dir);
        tile_c += dirX(dir);
        pixel_offset = 0;

        // Tunnel wrap
        if (try_portal_wrap(g, g.portalH, *this) ||
            try_portal_wrap(g, g.portalV, *this))
        {
            x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2;
            y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE)/2;
            return;
        }
    }

    // Position pixel
    x = tile_c * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2 + dirX(dir) * pixel_offset;
    y = tile_r * TILE_SIZE + (TILE_SIZE - GHOST_SIZE) / 2 + dirY(dir) * pixel_offset;
}

/*
============================================================
  DRAW — corps + yeux directionnels
============================================================
*/
void Ghost::draw(const GameState& g) const
{
    const uint16_t* body_anim[2] = { nullptr, nullptr };
    const uint16_t* eyes = nullptr;

    bool is_frightened = (mode == Mode::Frightened);
    bool is_eyes       = (mode == Mode::Eaten);

    /*
    ------------------------------------------------------------
      1) Sélection du corps
    ------------------------------------------------------------
    */
    if (is_eyes)
    {
        // Corps invisible → yeux seuls
        body_anim[0] = nullptr;
        body_anim[1] = nullptr;
    }
    else if (is_frightened)
    {
        bool blink = false;
        int t = g.frightened_timer_ticks;

        if (t <= g.frightened_blink_start_ticks)
            blink = ((t / 8) % 2 == 0);

        if (blink)
        {
            body_anim[0] = ghost_white_0;
            body_anim[1] = ghost_white_1;
        }
        else
        {
            body_anim[0] = ghost_scared_0;
            body_anim[1] = ghost_scared_1;
        }
    }
    else
    {
        switch (id)
        {
            case 0: body_anim[0] = ghost_red_0;    body_anim[1] = ghost_red_1;    break;
            case 1: body_anim[0] = ghost_blue_0;   body_anim[1] = ghost_blue_1;   break;
            case 2: body_anim[0] = ghost_pink_0;   body_anim[1] = ghost_pink_1;   break;
            case 3: body_anim[0] = ghost_orange_0; body_anim[1] = ghost_orange_1; break;
        }
    }

    /*
    ------------------------------------------------------------
      2) Sélection des yeux directionnels
    ------------------------------------------------------------
    */
    switch (dir)
    {
        case Dir::Left:  eyes = ghost_eyes_left;  break;
        case Dir::Right: eyes = ghost_eyes_right; break;
        case Dir::Up:    eyes = ghost_eyes_up;    break;
        case Dir::Down:  eyes = ghost_eyes_down;  break;
        default:         eyes = ghost_eyes_left;  break;
    }

    static const EyeOffset eyeOffsets[4] = {
        {2, 4}, // Left
        {4, 4}, // Right
        {3, 2}, // Up
        {3, 6}  // Down
    };

    int frame = (animTick / 8) % 2;
    int sx = x;
    int sy = y - (int)g_camera_y;

    /*
    ------------------------------------------------------------
      3) Dessin du corps
    ------------------------------------------------------------
    */
    if (body_anim[0])
        gfx_drawSprite(sx, sy, body_anim[frame], GHOST_SIZE, GHOST_SIZE);

    /*
    ------------------------------------------------------------
      4) Dessin des yeux
    ------------------------------------------------------------
    */
    if (!is_frightened && !is_eyes)
    {
        int idx = 0;
        switch (dir)
        {
            case Dir::Left:  idx = 0; break;
            case Dir::Right: idx = 1; break;
            case Dir::Up:    idx = 2; break;
            case Dir::Down:  idx = 3; break;
            default:         idx = 0; break;
        }

        const EyeOffset& off = eyeOffsets[idx];
        gfx_drawSprite(sx + off.dx, sy + off.dy, eyes, 10, 5);
    }
    else if (is_eyes)
    {
        gfx_drawSprite(sx + 3, sy + 4, eyes, 10, 5);
    }
}
