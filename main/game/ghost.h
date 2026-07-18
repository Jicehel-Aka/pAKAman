#pragma once
/*
============================================================
  ghost.h — Gestion des fantômes (IA, mouvement, rendu)
------------------------------------------------------------
Ce module gère :
 - la position case-based des fantômes
 - les directions et vitesses selon le mode
 - les états de la ghost house (Inside / Leaving / Outside)
 - les modes Scatter / Chase / Frightened / Eaten
 - le pathfinding (BFS) pour les yeux
 - l’API update() / draw()

Les fantômes utilisent la même logique case-based que Pac-Man :
 - tile_r / tile_c : position dans la grille
 - pixel_offset    : progression dans la tuile
 - x / y           : position pixel dérivée pour le rendu
============================================================
*/

#include "config.h"
#include "maze.h"
#include <vector>

struct GameState;

// Walkability spéciale pour les yeux (mode Eaten)
bool is_walkable_for_eyes(TileType t);

struct Ghost
{
    /*
    ------------------------------------------------------------
      Modes de comportement
    ------------------------------------------------------------
    */
    enum class Mode {
        Scatter,     // fuite vers un coin
        Chase,       // poursuite
        Frightened,  // bleu (aléatoire)
        Eaten        // yeux (retour maison)
    };

    /*
    ------------------------------------------------------------
      Offsets des yeux (pour le rendu)
    ------------------------------------------------------------
    */
    struct EyeOffset {
        int dx;
        int dy;
    };

    /*
    ------------------------------------------------------------
      États de la ghost house
    ------------------------------------------------------------
    */
    enum class HouseState {
        Inside,     // dans la maison
        Leaving,    // en train de sortir
        Outside,    // dans le labyrinthe
        Returning   // (optionnel) retour maison
    };

    /*
    ------------------------------------------------------------
      Directions logiques
    ------------------------------------------------------------
    */
    enum class Dir {
        None,
        Left,
        Right,
        Up,
        Down
    };

    /*
    ------------------------------------------------------------
      Position logique (case-based)
    ------------------------------------------------------------
    */
    int tile_r = 0;
    int tile_c = 0;

    // Tuile précédente (utile pour collisions, debug, IA)
    int prev_tile_r = 0;
    int prev_tile_c = 0;

    // Progression dans la tuile (0..TILE_SIZE)
    int pixel_offset = 0;

    /*
    ------------------------------------------------------------
    // Constructeur par défaut d'une fantôme "neutre"
    ------------------------------------------------------------
    */	
	Ghost()
		: tile_r(0),
		  tile_c(0),
		  prev_tile_r(0),
		  prev_tile_c(0),
		  pixel_offset(0),

		  x(0),
		  y(0),

		  dir(Dir::Left),
		  next_dir(Dir::Left),

		  mode(Mode::Scatter),
		  previous_mode(Mode::Scatter),

		  houseState(HouseState::Inside),
		  eaten_timer(0),

		  speed_normal(0),
		  speed_frightened(0),
		  speed_tunnel(0),
		  speed_eyes(0),

		  id(-1),

		  animTick(0),

		  start_row(0),
		  start_col(0),

		  releaseTime_ticks(0)
	{
		// Rien de spécial : fantôme neutre non initialisé
	}


	 /*
    ------------------------------------------------------------
      Position pixel (pour le rendu uniquement)
    ------------------------------------------------------------
    */
    int x = 0;
    int y = 0;

    /*
    ------------------------------------------------------------
      Direction actuelle + direction anticipée
    ------------------------------------------------------------
    */
    Dir dir      = Dir::Left;
    Dir next_dir = Dir::Left;

    /*
    ------------------------------------------------------------
      Mode de comportement
    ------------------------------------------------------------
    */
    Mode mode           = Mode::Scatter;
    Mode previous_mode  = Mode::Scatter;

    /*
    ------------------------------------------------------------
      Gestion de la ghost house
    ------------------------------------------------------------
    */
    HouseState houseState = HouseState::Inside;
    int eaten_timer       = 0;   // délai après retour maison

    /*
    ------------------------------------------------------------
      Pathfinding (mode Eaten)
    ------------------------------------------------------------
    */
    std::vector<std::pair<int,int>> path;

    /*
    ------------------------------------------------------------
      Vitesses selon le mode
    ------------------------------------------------------------
    */
    int speed_normal     = 0;
    int speed_frightened = 0;
    int speed_tunnel     = 0;
    int speed_eyes       = 0;

    /*
    ------------------------------------------------------------
      Identité du fantôme (0=blinky, 1=pinky, 2=inky, 3=clyde)
    ------------------------------------------------------------
    */
    int id = 0;

    /*
    ------------------------------------------------------------
      Animation
    ------------------------------------------------------------
    */
    int animTick = 0;

    /*
    ------------------------------------------------------------
      Position de spawn (pour reset)
    ------------------------------------------------------------
    */
    int start_row = 0;
    int start_col = 0;

    /*
    ------------------------------------------------------------
      Timer de release (sortie de la maison)
    ------------------------------------------------------------
    */
    int releaseTime_ticks = 0;

    /*
    ------------------------------------------------------------
      Constructeur
    ------------------------------------------------------------
    */
    Ghost(int id_, int start_c, int start_r);

    /*
    ------------------------------------------------------------
      API principale
    ------------------------------------------------------------
    */
    void update(GameState& g);
	void draw(const GameState& g) const;
    void reset_to_start();

    /*
    ------------------------------------------------------------
      Gestion du mode Frightened
    ------------------------------------------------------------
    */
    void on_start_frightened();
    void on_end_frightened();

    /*
    ------------------------------------------------------------
      Helpers direction
    ------------------------------------------------------------
    */
    static int dirX(Dir d);
    static int dirY(Dir d);
    bool isCentered() const;

    /*
    ------------------------------------------------------------
      IA : directions valides
    ------------------------------------------------------------
    */
    std::vector<Dir> getValidDirections(const GameState& g,
                                        int row, int col,
                                        bool is_eyes) const;

    /*
    ------------------------------------------------------------
      IA : choix de direction
    ------------------------------------------------------------
    */
    Dir chooseDirectionTowardsTarget(const GameState& g,
                                     int row, int col,
                                     int tr, int tc,
                                     bool is_eyes) const;

    Dir chooseRandomDir(const GameState& g,
                        int row, int col,
                        bool is_eyes) const;

    Dir chooseDirectionInsideHouse(const GameState& g,
                                   int row, int col);

    /*
    ------------------------------------------------------------
      IA : cibles Scatter / Chase
    ------------------------------------------------------------
    */
    void getScatterTarget(const GameState& g, int& tr, int& tc) const;
    void getChaseTarget(const GameState& g, int& tr, int& tc) const;

    /*
    ------------------------------------------------------------
      Demi-tour instantané
    ------------------------------------------------------------
    */
    void reverse_direction();
};
