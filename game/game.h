#pragma once
/*
============================================================
  game.h — État global du jeu et prototypes
------------------------------------------------------------
  - GameState : structure centrale (labyrinthe, pacman,
    fantômes, score, timers…)
  - try_portal_wrap : template inline pour tunnels
  - Prototypes des fonctions de jeu
============================================================
*/

#include <vector>
#include "pacman.h"
#include "ghost.h"
#include "maze.h"
#include "config.h"

// ---------------------------------------------------------------------------
//  MODE GLOBAL DES FANTÔMES (ARCADE-FAITHFUL)
// ---------------------------------------------------------------------------
enum class GlobalGhostMode : uint8_t {
    Scatter,
    Chase
};

// ---------------------------------------------------------------------------
//  STRUCTURES UTILITAIRES
// ---------------------------------------------------------------------------

struct GhostModePhase {
    GlobalGhostMode mode          = GlobalGhostMode::Scatter;
    int             duration_ticks = 0;   // durée en frames (base 60 FPS)
};

struct GhostModeSchedule {
    GhostModePhase phases[8];
    int            phase_count = 0;
};

struct FloatingScore {
    int x     = 0;
    int y     = 0;
    int value = 0;
    int timer = 0;   // durée d'affichage en frames
};

// ---------------------------------------------------------------------------
//  GAMESTATE
// ---------------------------------------------------------------------------
struct GameState {

    // -----------------------------------------------------------------------
    //  ÉTATS MACHINE
    // -----------------------------------------------------------------------
    enum class State : uint8_t {
        TitleScreen,
        StartingLevel,
        Playing,
        PacmanDying,
        LevelComplete,
        Paused,
        Options,
        OptionsMenu,
        Highscores,
        GameOver
    };

    // -----------------------------------------------------------------------
    //  PORTAILS (TUNNELS)
    // -----------------------------------------------------------------------
    struct PortalPair {
        int  T0_r = -1, T0_c = -1;   // tuile "entrée" côté 0
        int  E0_r = -1, E0_c = -1;   // tuile juste avant (ext)
        int  T1_r = -1, T1_c = -1;   // tuile "entrée" côté 1
        int  E1_r = -1, E1_c = -1;
        bool exists = false;
    };

    PortalPair portalH;   // portails horizontaux (bords gauche / droite)
    PortalPair portalV;   // portails verticaux   (bords haut / bas)

    // -----------------------------------------------------------------------
    //  GÉNÉRAL
    // -----------------------------------------------------------------------
    State state      = State::TitleScreen;
    int   levelIndex = 0;
    int   level      = 1;

    // Écran "READY!"
    bool ready_waiting_for_input = false;
    int  ready_timer             = 0;

    // Écran "GAME OVER"
    bool gameover_waiting_for_input = false;
    int  gameover_timer             = 0;

    // -----------------------------------------------------------------------
    //  LABYRINTHE / ENTITÉS
    // -----------------------------------------------------------------------
    Maze                    maze;
    Pacman                  pacman;
    int                     pacman_start_r = 0;
    int                     pacman_start_c = 0;

    std::vector<Ghost>        ghosts;
    std::vector<FloatingScore> floatingScores;

    // -----------------------------------------------------------------------
    //  GHOST HOUSE (PORTE)
    // -----------------------------------------------------------------------
    enum class DoorState : uint8_t { Closed, Opening, Open };
    DoorState ghostDoorState       = DoorState::Closed;
    int       ghostDoorTimer_ticks = 0;

    // -----------------------------------------------------------------------
    //  TIMERS / SCORE / VIES
    // -----------------------------------------------------------------------
    int ghostReleaseInterval_ticks = GHOST_RELEASE_INTERVAL_TICKS;
    int elapsed_ticks              = 0;
    int score                      = 0;
    int lives                      = 3;
    int ghostEatScore              = GHOST_SCORE;   // doublé à chaque kill en chaîne
    int pacman_death_timer         = 0;

    // -----------------------------------------------------------------------
    //  SÉQUENCE SCATTER / CHASE
    // -----------------------------------------------------------------------
    GhostModeSchedule schedule;
    int               current_phase_index = 0;
    int               phase_timer_ticks   = 0;
    GlobalGhostMode   global_mode         = GlobalGhostMode::Scatter;

    // -----------------------------------------------------------------------
    //  FRIGHTENED
    // -----------------------------------------------------------------------
    int frightened_timer_ticks       = 0;
    int frightened_duration_ticks    = FRIGHTENED_DURATION_TICKS;
    int frightened_blink_start_ticks = FRIGHTENED_BLINK_START_TICKS;
    int frightened_chain             = 0;
};

// ---------------------------------------------------------------------------
//  TUNNEL WRAP — template pour Pacman et Ghost
// ---------------------------------------------------------------------------
template<typename Actor>
inline bool try_portal_wrap(const GameState& g,
                            const GameState::PortalPair& P,
                            Actor& a)
{
    if (!P.exists || !a.isCentered())
        return false;

    auto apply = [&](int dest_r, int dest_c, typename Actor::Dir new_dir)
    {
        a.tile_r       = dest_r;
        a.tile_c       = dest_c;
        a.pixel_offset = 0;
        a.dir          = new_dir;
        a.prev_tile_r  = dest_r;
        a.prev_tile_c  = dest_c;
    };

    // Côté 0 → 1
    bool from_E0 = (a.prev_tile_r == P.E0_r && a.prev_tile_c == P.E0_c &&
                    a.tile_r      == P.T0_r && a.tile_c      == P.T0_c);
    bool thru_T0 = (a.prev_tile_r == P.T0_r && a.prev_tile_c == P.T0_c &&
                    !(a.tile_r == P.E0_r && a.tile_c == P.E0_c));

    if (from_E0 || thru_T0) { apply(P.T1_r, P.T1_c, Actor::Dir::Right); return true; }

    // Côté 1 → 0
    bool from_E1 = (a.prev_tile_r == P.E1_r && a.prev_tile_c == P.E1_c &&
                    a.tile_r      == P.T1_r && a.tile_c      == P.T1_c);
    bool thru_T1 = (a.prev_tile_r == P.T1_r && a.prev_tile_c == P.T1_c &&
                    !(a.tile_r == P.E1_r && a.tile_c == P.E1_c));

    if (from_E1 || thru_T1) { apply(P.T0_r, P.T0_c, Actor::Dir::Left); return true; }

    return false;
}

// ---------------------------------------------------------------------------
//  PROTOTYPES
// ---------------------------------------------------------------------------
void game_init(GameState& g);
void game_update(GameState& g);
void game_draw(const GameState& g);

bool game_is_over(const GameState& g);
bool ghost_can_kill(const Ghost& gh);
bool ghost_can_be_eaten(const Ghost& gh);

void check_pacman_ghost_collision(GameState& g);
void game_trigger_frightened(GameState& g);
void update_floating_scores(GameState& g);
void detect_portals(GameState& g);
