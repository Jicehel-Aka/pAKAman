idf_component_register(
    SRCS
        "app_main.cpp"
        # Core (adaptateurs au-dessus du composant gamebuino)
        "core/audio.cpp"
        "core/input.cpp"
        "core/graphics.cpp"
        "core/sprite.cpp"
        "core/sdcard.cpp"
        "core/i18n.cpp"
        "core/settings.cpp"
        # Assets
        "assets/assets.cpp"
        "assets/title_image.cpp"
        # Game (logique Pac-Man). fruits.cpp = balade des fruits (Ms. Pac-Man).
        # levels_preset.cpp reste hors build (code mort : 'Level' non defini).
        "game/config.cpp"
        "game/game.cpp"
        "game/maze.cpp"
        "game/pacman.cpp"
        "game/ghost.cpp"
        "game/fruits.cpp"
        "game/level.cpp"
        # UI
        "ui/highscores.cpp"
        "ui/title_screen.cpp"
        "ui/menu.cpp"
    INCLUDE_DIRS
        "."
        "core"
        "game"
        "assets"
        "ui"
    # gamebuino DOIT etre dans REQUIRES (pas INCLUDE_DIRS) : c'est le composant
    # materiel qui expose gb_core / gb_graphics / gb_audio / gb_ll_*.
    REQUIRES
        gamebuino
        nvs_flash
        fatfs
        esp_timer
        esp_psram
        spi_flash
        driver
        app_update
        esp_partition
)
