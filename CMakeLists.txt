# ============================================================
#  Sélection du backend graphique
# ============================================================
option(USE_FRAMEBUFFER "Use framebuffer backend" ON)

if(USE_FRAMEBUFFER)
    message(STATUS "Using FRAMEBUFFER backend")
    set(GFX_BACKEND_SRCS core/gfx_fb.cpp)
else()
    message(STATUS "Using DIRECT LCD backend")
    set(GFX_BACKEND_SRCS core/gfx_direct.cpp)
endif()


# ============================================================
#  Assets
# ============================================================
set(ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)

file(GLOB_RECURSE GAME_ASSETS
    ${ASSETS_DIR}/*
    ${ASSETS_DIR}/tiles/*
    ${ASSETS_DIR}/sounds/*
)


# ============================================================
#  Composant principal
# ============================================================
idf_component_register(
    SRCS
        app_main.cpp

        # Lib / matériel
        lib/LCD.cpp
        lib/expander.cpp
        lib/graphics_basic.cpp
        lib/sdcard.cpp
        lib/audio_sfx.cpp
        lib/audio_player.cpp
        lib/audio_pmf.cpp
        lib/pmf_player.cpp
        lib/pmf_player_esp32s3.cpp
        lib/audio_track_sfx.cpp
        lib/audio_sfx_cache.cpp

        # Core
        core/audio.cpp
        core/input.cpp
        core/graphics.cpp
        core/sprite.cpp
        core/sprite_atlas.cpp
        core/persist.cpp

        # Backend graphique sélectionné
        ${GFX_BACKEND_SRCS}

        # UI
        ui/title_screen.cpp
        ui/highscores.cpp
        ui/menu.cpp
	ui/options.cpp

        # Game
        assets/assets.cpp
        game/config.cpp
        game/game.cpp
        game/maze.cpp
        game/pacman.cpp
        game/ghost.cpp
        game/level.cpp

        # Tasks
        tasks/task_game.cpp
        tasks/task_audio.cpp
        tasks/task_input.cpp

        # Assets
        ${GAME_ASSETS}

    INCLUDE_DIRS
        .
        core
        lib
        ui
        game
        tasks
)


# ============================================================
#  Passe USE_FRAMEBUFFER au code C++
# ============================================================
if(USE_FRAMEBUFFER)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE USE_FRAMEBUFFER=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE USE_FRAMEBUFFER=0)
endif()