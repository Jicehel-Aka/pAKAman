	#pragma once
	struct GameState;     // forward only (pas d’include de game.h ici)
	constexpr int LEVEL_ROWS = 21;
	constexpr int LEVEL_COLS = 20;

	void level_init(GameState& g);

	extern const int level_grid[LEVEL_ROWS][LEVEL_COLS];