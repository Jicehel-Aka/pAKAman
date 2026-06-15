/*
============================================================
  game.cpp — Logique principale du jeu (boucle, états, HUD)
------------------------------------------------------------
  Gère :
   - Scatter / Chase schedule
   - Frightened / collision Pac-Man ↔ fantômes
   - Scores flottants
   - Portails (détection automatique)
   - Reset partiel (mort) et reset complet (niveau suivant)
   - game_update() : machine d'états principale
   - game_draw()   : rendu scène + HUD (via helper centralisé)
============================================================
*/

#include "game.h"
#include "level.h"
#include "maze.h"
#include "core/graphics.h"
#include "core/audio.h"
#include "core/input.h"
#include "core/sprite.h"
#include "assets/assets.h"
#include "assets/pacman_pmf.h"
#include "lib/audio_pmf.h"
#include "config.h"
#include <algorithm>
#include <string>

extern AudioPMF audioPMF;
extern int      debug;

float g_camera_y = 0.0f;

// ===========================================================================
//  SECTION 1 — SCHEDULE SCATTER / CHASE
// ===========================================================================

static void init_ghost_schedule(GameState& g)
{
    g.schedule.phase_count   = 4;
    g.schedule.phases[0]     = { GlobalGhostMode::Scatter, 7 * 60 };
    g.schedule.phases[1]     = { GlobalGhostMode::Chase,   20 * 60 };
    g.schedule.phases[2]     = { GlobalGhostMode::Scatter, 7 * 60 };
    g.schedule.phases[3]     = { GlobalGhostMode::Chase,   9999 * 60 };

    g.current_phase_index    = 0;
    g.phase_timer_ticks      = g.schedule.phases[0].duration_ticks;
    g.global_mode            = g.schedule.phases[0].mode;
}

static void update_modes(GameState& g)
{
    if (g.phase_timer_ticks <= 0) return;

    if (--g.phase_timer_ticks == 0)
    {
        if (g.current_phase_index + 1 < g.schedule.phase_count)
        {
            ++g.current_phase_index;
            const auto& phase    = g.schedule.phases[g.current_phase_index];
            g.global_mode        = phase.mode;
            g.phase_timer_ticks  = phase.duration_ticks;

            for (auto& ghost : g.ghosts)
                ghost.reverse_direction();
        }
    }
}

// ===========================================================================
//  SECTION 2 — FRIGHTENED
// ===========================================================================

void game_trigger_frightened(GameState& g)
{
    g.frightened_timer_ticks = g.frightened_duration_ticks;
    g.frightened_chain       = 0;

    for (auto& gh : g.ghosts)
        gh.on_start_frightened();
}

static void update_frightened(GameState& g)
{
    if (g.frightened_timer_ticks <= 0) return;

    if (--g.frightened_timer_ticks == 0)
    {
        for (auto& gh : g.ghosts)
            gh.on_end_frightened();
    }
}

// ===========================================================================
//  SECTION 3 — SCORES FLOTTANTS
// ===========================================================================

void update_floating_scores(GameState& g)
{
    for (auto& fs : g.floatingScores)
        --fs.timer;

    g.floatingScores.erase(
        std::remove_if(g.floatingScores.begin(), g.floatingScores.end(),
                       [](const FloatingScore& fs){ return fs.timer <= 0; }),
        g.floatingScores.end());
}

// ===========================================================================
//  SECTION 4 — COLLISION PAC-MAN / FANTÔMES
// ===========================================================================

bool ghost_can_kill(const Ghost& gh)
{
    return (gh.mode       != Ghost::Mode::Eaten     &&
            gh.mode       != Ghost::Mode::Frightened &&
            gh.houseState == Ghost::HouseState::Outside);
}

bool ghost_can_be_eaten(const Ghost& gh)
{
    return (gh.mode       == Ghost::Mode::Frightened &&
            gh.houseState == Ghost::HouseState::Outside);
}

void check_pacman_ghost_collision(GameState& g)
{
    const int px = g.pacman.x + PACMAN_SIZE / 2;
    const int py = g.pacman.y + PACMAN_SIZE / 2;

    for (auto& gh : g.ghosts)
    {
        const int dx = px - (gh.x + GHOST_SIZE / 2);
        const int dy = py - (gh.y + GHOST_SIZE / 2);

        if (dx*dx + dy*dy >= COLLISION_RADIUS * COLLISION_RADIUS)
            continue;

        if (ghost_can_be_eaten(gh))
        {
            gh.mode = Ghost::Mode::Eaten;
            gh.path.clear();

            g.score += g.ghostEatScore;

            g.floatingScores.push_back({ gh.x + GHOST_SIZE/2, gh.y,
                                         g.ghostEatScore, 60 });

            g.ghostEatScore *= 2;   // 200 → 400 → 800 → 1600
            audio_play_eatghost();
        }
        else if (ghost_can_kill(gh))
        {
            g.state              = GameState::State::PacmanDying;
            g.pacman_death_timer = 0;
            audioPMF.stop();
            audio_play_death();
            return;
        }
    }
}

// ===========================================================================
//  SECTION 5 — PORTAILS (DÉTECTION AUTOMATIQUE)
// ===========================================================================

void detect_portals(GameState& g)
{
    const Maze& m = g.maze;
    g.portalH = {};
    g.portalV = {};

    // --- Horizontal (bords gauche / droite) ---
    for (int r = 0; r < MAZE_HEIGHT; r++)
    {
        if (m.tiles[r][0] == TileType::Tunnel) {
            g.portalH.T0_r = r; g.portalH.T0_c = 0;
            g.portalH.E0_r = r; g.portalH.E0_c = 1;
        }
        if (m.tiles[r][MAZE_WIDTH-1] == TileType::Tunnel) {
            g.portalH.T1_r = r; g.portalH.T1_c = MAZE_WIDTH-1;
            g.portalH.E1_r = r; g.portalH.E1_c = MAZE_WIDTH-2;
        }
    }

    if ((g.portalH.T0_r != -1) != (g.portalH.T1_r != -1))
        printf("ERREUR MAP: portail horizontal incohérent\n");
    else if (g.portalH.T0_r != -1)
        g.portalH.exists = true;

    // --- Vertical (bords haut / bas) ---
    for (int c = 0; c < MAZE_WIDTH; c++)
    {
        if (m.tiles[0][c] == TileType::Tunnel) {
            g.portalV.T0_r = 0; g.portalV.T0_c = c;
            g.portalV.E0_r = 1; g.portalV.E0_c = c;
        }
        if (m.tiles[MAZE_HEIGHT-1][c] == TileType::Tunnel) {
            g.portalV.T1_r = MAZE_HEIGHT-1; g.portalV.T1_c = c;
            g.portalV.E1_r = MAZE_HEIGHT-2; g.portalV.E1_c = c;
        }
    }

    if ((g.portalV.T0_r != -1) != (g.portalV.T1_r != -1))
        printf("ERREUR MAP: portail vertical incohérent\n");
    else if (g.portalV.T0_r != -1)
        g.portalV.exists = true;
}

// ===========================================================================
//  SECTION 6 — CAMÉRA VERTICALE
// ===========================================================================

static void update_camera(const GameState& g)
{
    const int maze_h_px = MAZE_HEIGHT * TILE_SIZE;
    const int max_scroll = (maze_h_px > SCREEN_H) ? (maze_h_px - SCREEN_H) : 0;

    int target = g.pacman.y + PACMAN_SIZE / 2 - SCREEN_H / 2;
    if (target < 0)           target = 0;
    if (target > max_scroll)  target = max_scroll;

    g_camera_y = static_cast<float>(target);
}

// ===========================================================================
//  SECTION 7 — GHOST HOUSE (PORTE)
// ===========================================================================

static void update_ghost_door(GameState& g)
{
    switch (g.ghostDoorState)
    {
        case GameState::DoorState::Closed:
            if (g.elapsed_ticks >= 2 * 60) {
                g.ghostDoorState      = GameState::DoorState::Opening;
                g.maze.setGhostDoor(TileType::GhostDoorOpening);
                g.ghostDoorTimer_ticks = g.elapsed_ticks + 30;
            }
            break;

        case GameState::DoorState::Opening:
            if (g.elapsed_ticks >= g.ghostDoorTimer_ticks) {
                g.ghostDoorState = GameState::DoorState::Open;
                g.maze.setGhostDoor(TileType::GhostDoorOpen);
            }
            break;

        case GameState::DoorState::Open:
        {
            bool all_out = true;
            for (const auto& gh : g.ghosts)
                if (gh.houseState != Ghost::HouseState::Outside &&
                    gh.mode       != Ghost::Mode::Eaten)
                { all_out = false; break; }

            if (all_out) {
                g.ghostDoorState = GameState::DoorState::Closed;
                g.maze.setGhostDoor(TileType::GhostDoorClosed);
            }
            break;
        }
    }
}

// ===========================================================================
//  SECTION 8 — RESET
// ===========================================================================

// Helper partagé : ré-initialise tous les fantômes à leur spawn
static void reset_ghosts(GameState& g)
{
    for (auto& gh : g.ghosts)
    {
        gh.start_row      = g.maze.ghost_spawn_row[gh.id];
        gh.start_col      = g.maze.ghost_spawn_col[gh.id];
        gh.reset_to_start();

        gh.houseState     = Ghost::HouseState::Inside;
        gh.mode           = Ghost::Mode::Scatter;
        gh.previous_mode  = Ghost::Mode::Scatter;
        gh.eaten_timer    = 0;
        gh.path.clear();

        gh.pixel_offset   = 0;
        gh.x              = gh.tile_c * TILE_SIZE + GHOST_OFFSET;
        gh.y              = gh.tile_r * TILE_SIZE + GHOST_OFFSET;
    }
}

// Reset partiel après mort (labyrinthte conservé)
static void reset_level(GameState& g)
{
    g.pacman.tile_r      = g.pacman_start_r;
    g.pacman.tile_c      = g.pacman_start_c;
    g.pacman.pixel_offset = 0;
    g.pacman.dir         = Pacman::Dir::Left;
    g.pacman.next_dir    = Pacman::Dir::Left;

    reset_ghosts(g);
    init_ghost_schedule(g);

    g.ghostDoorState          = GameState::DoorState::Closed;
    g.elapsed_ticks           = 0;
    g.ready_waiting_for_input = true;
    g.ready_timer             = 30;

    audioPMF.stop();
}

// Reset complet pour le niveau suivant
static void reset_level_full(GameState& g)
{
    level_init(g);
    detect_portals(g);

    g.pacman           = Pacman(g.maze.pac_spawn_col, g.maze.pac_spawn_row);
    g.pacman_start_r   = g.maze.pac_spawn_row;
    g.pacman_start_c   = g.maze.pac_spawn_col;

    reset_ghosts(g);
    init_ghost_schedule(g);

    int t = FIRST_GHOST_RELEASE_TICKS;
    for (auto& gh : g.ghosts) {
        gh.releaseTime_ticks = t;
        t += g.ghostReleaseInterval_ticks;
    }

    g.frightened_timer_ticks = 0;
    g.frightened_chain       = 0;
    g.ghostDoorState         = GameState::DoorState::Closed;
    g.ghostDoorTimer_ticks   = 0;
    g.elapsed_ticks          = 0;
    g.ghostEatScore          = GHOST_SCORE;

    audio_play_begin();
    audioPMF.stop();
    g.state = GameState::State::StartingLevel;
}

// ===========================================================================
//  SECTION 9 — INITIALISATION GLOBALE
// ===========================================================================

void game_init(GameState& g)
{
    g.score = 0;
    g.lives = 3;

    level_init(g);
    detect_portals(g);

    g.pacman           = Pacman(g.maze.pac_spawn_col, g.maze.pac_spawn_row);
    g.pacman_start_r   = g.maze.pac_spawn_row;
    g.pacman_start_c   = g.maze.pac_spawn_col;

    g.ghosts.clear();
    g.ghosts.resize(NUM_GHOSTS);
    for (int i = 0; i < NUM_GHOSTS; i++)
    {
        g.ghosts[i] = Ghost(i,
                            g.maze.ghost_spawn_col[i],
                            g.maze.ghost_spawn_row[i]);
        g.ghosts[i].houseState = Ghost::HouseState::Inside;
    }

    init_ghost_schedule(g);

    int t = FIRST_GHOST_RELEASE_TICKS;
    for (auto& gh : g.ghosts) {
        gh.releaseTime_ticks = t;
        t += g.ghostReleaseInterval_ticks;
    }

    g.frightened_timer_ticks = 0;
    g.frightened_chain       = 0;
    g.ghostDoorState         = GameState::DoorState::Closed;
    g.ghostDoorTimer_ticks   = 0;
    g.elapsed_ticks          = 0;
    g.ghostEatScore          = GHOST_SCORE;

    g.state = GameState::State::TitleScreen;

    audioPMF.stop();
    audioPMF.init(pacman_pmf);
    audio_play_begin();
}

bool game_is_over(const GameState& g)
{
    return (g.state == GameState::State::GameOver);
}

// ===========================================================================
//  SECTION 10 — MISE À JOUR PAR ÉTAT
// ===========================================================================

static void update_state_starting_level(GameState& g)
{
    if (audio_wav_is_playing()) {
        update_floating_scores(g);
        update_camera(g);
        return;
    }

    if (!audioPMF.isPlaying() && g_audio_settings.music_enabled) {
        audioPMF.init(pacman_pmf);
        audioPMF.start(GB_AUDIO_SAMPLE_RATE);
    }

    if (g.ready_timer > 0) {
        --g.ready_timer;
        update_floating_scores(g);
        update_camera(g);
        return;
    }

    if (g.ready_waiting_for_input) {
        update_floating_scores(g);
        update_camera(g);
        if (g_keys.A) {
            g.ready_waiting_for_input = false;
            g.state = GameState::State::Playing;
        }
        return;
    }

    g.pacman.pixel_offset = 0;
    for (auto& gh : g.ghosts) gh.pixel_offset = 0;

    g.state = GameState::State::Playing;
    update_camera(g);
}

static void update_state_pacman_dying(GameState& g)
{
    ++g.pacman_death_timer;

    if (audio_wav_is_playing()) {
        update_floating_scores(g);
        return;
    }

    if (--g.lives <= 0) {
        g.gameover_timer             = 90;   // ~2.2s à 40 FPS — attend la fin du son
        g.gameover_waiting_for_input = true;
        g.state = GameState::State::GameOver;
        return;
    }

    reset_level(g);
    g.state = GameState::State::StartingLevel;
}

static void update_state_playing(GameState& g)
{
    g.pacman.update(g);

    for (auto& gh : g.ghosts)
        gh.update(g);

    check_pacman_ghost_collision(g);
    update_modes(g);
    update_camera(g);
    update_floating_scores(g);

    if (g.maze.pellet_count == 0) {
        ++g.level;
        reset_level_full(g);
    }
}

// GameOver : aucune logique ici.
// La transition vers highscores_submit puis TitleScreen
// est entièrement gérée par task_game.cpp → state_gameover().
static void update_state_gameover(GameState& /*g*/)
{
    // intentionnellement vide
}

// ---------------------------------------------------------------------------
//  game_update — boucle principale
// ---------------------------------------------------------------------------
void game_update(GameState& g)
{
    ++g.elapsed_ticks;

    update_frightened(g);
    update_ghost_door(g);

    switch (g.state)
    {
        case GameState::State::StartingLevel: update_state_starting_level(g); break;
        case GameState::State::PacmanDying:   update_state_pacman_dying(g);   break;
        case GameState::State::Playing:       update_state_playing(g);        break;
        case GameState::State::GameOver:      update_state_gameover(g);       break;

        // États gérés par task_game.cpp
        case GameState::State::TitleScreen:
        case GameState::State::LevelComplete:
        case GameState::State::Paused:
        case GameState::State::Options:
        case GameState::State::OptionsMenu:
        case GameState::State::Highscores:
        default:
            break;
    }
}

// ===========================================================================
//  SECTION 11 — RENDU
// ===========================================================================

// Helper : dessine les scores flottants
static void draw_floating_scores(const GameState& g)
{
    char sbuf[16];
    for (const auto& fs : g.floatingScores)
    {
        snprintf(sbuf, sizeof(sbuf), "%d", fs.value);
        gfx_text(fs.x, fs.y - (int)g_camera_y, sbuf, COLOR_YELLOW);
    }
}

// Helper : dessine le HUD (score + vies) — partagé par tous les états
static void draw_hud(const GameState& g)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE: %d", g.score);
    gfx_text(4,   4, buf, COLOR_WHITE);

    snprintf(buf, sizeof(buf), "LIVES: %d", g.lives);
    gfx_text(180, 4, buf, COLOR_YELLOW);
}

void game_draw(const GameState& g)
{
    gfx_clear(COLOR_BLACK);
    g.maze.draw();

    switch (g.state)
    {
        // -------------------------------------------------------------------
        case GameState::State::StartingLevel:
        {
            g.pacman.draw(g);
            gfx_text(150, 120, "READY!", COLOR_YELLOW);
            draw_floating_scores(g);
            draw_hud(g);
            return;
        }

        // -------------------------------------------------------------------
        case GameState::State::PacmanDying:
        {
            int frame = g.pacman_death_timer / 6;
            if (frame < 0)  frame = 0;
            if (frame > 11) frame = 11;

            gfx_drawSprite(g.pacman.x + 1,
                           g.pacman.y + 1 - (int)g_camera_y,
                           pacman_death_anim[frame], 14, 14);

            draw_floating_scores(g);
            draw_hud(g);
            return;
        }

        // -------------------------------------------------------------------
        case GameState::State::GameOver:
        {
            gfx_text(100, 120, "GAME OVER", COLOR_RED);
            gfx_text(110, 140, "PRESS A",   COLOR_WHITE);
            draw_hud(g);
            return;
        }

        // -------------------------------------------------------------------
        case GameState::State::Playing:
        default:
            break;
    }

    // État "Playing" (et tout autre état actif)
    g.pacman.draw(g);
    for (const auto& ghost : g.ghosts)
        ghost.draw(g);

    draw_floating_scores(g);
    draw_hud(g);
}
