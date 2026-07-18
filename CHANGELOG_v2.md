# pAKAman — Corrections + fruit baladeur (mise à jour)

## Bugs corrigés (signalés sur la version portée)

1. **« Appuyez sur A » décalé à droite (accueil)** — la police du composant fait
   **8 px** de large, pas 6. `FONT_CHAR_WIDTH` corrigé → tout le texte centré
   (titre, menu, scores, pause, game over) est désormais bien centré.

2. **Clignotement en jeu** — le LCD n'a qu'**un seul framebuffer** avec DMA.
   `gfx_flush()` lançait le DMA sans attendre sa fin, et le `gfx_clear` du frame
   suivant écrasait l'image *pendant* son transfert. Corrigé : `gfx_clear()` (et
   le blit plein écran du titre) **attendent `lcd_refresh_completed()`** avant
   d'écrire, comme le fait le composant lui-même.

3. **Retour au loader en mangeant un fantôme clignotant** — ce n'était pas le
   combo, mais un **crash** (reboot → loader). Le BFS des yeux (mode « Eaten »,
   déclenché en mangeant un fantôme) allouait ~3,8 Ko **sur la pile**, or la
   tâche principale n'avait que **3584 o**. Corrigé sur deux fronts :
   `visited`/`parent` du BFS passés en **statique** (hors pile) + pile principale
   portée à **8192 o** (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`).

4. **Son/animation de mort tronqués** — `audio_wav_is_playing()` ne couvrait que
   le jingle de début, donc la séquence de mort reprenait aussitôt. Désormais le
   son de mort (long balayage descendant ~1,7 s) **occupe l'audio ~2,1 s**, ce qui
   laisse jouer **toute l'animation** de disparition (12 frames) avant de reprendre.

## Améliorations validées

1. **Vie bonus à 10 000 pts** — accordée une seule fois, avec petit jingle.
2. **HUD enrichi** — ajout de **« LVL n »** en haut, et d'une **rangée de fruits**
   des 7 derniers niveaux en bas à droite (comme la borne).
3. **Montée de difficulté par niveau** — rythme global un peu **ralenti** (≈33 FPS
   au lieu de 40, jugé trop rapide). La difficulté monte via `apply_level_difficulty()` :
   **durée « frightened » ↓** et **sorties de fantômes plus rapides** à chaque niveau.
   (La vitesse des fantômes reste à 2 px/frame : les seules valeurs « propres »
   possibles sont 1/2/4/8 et passer à 4 les rendrait plus rapides que Pac-Man,
   donc injouable — la difficulté passe par les autres leviers.)
4. **Nettoyage** — `assets_init()` n'est plus appelé qu'**une seule fois**
   (attribut `constructor` retiré, appel explicite conservé dans `app_main`).

## Réglages faciles à ajuster

- Rythme global : `FRAME_US` dans `app_main.cpp` (30000 = 33 FPS ; 25000 = 40 FPS).
- Difficulté : `apply_level_difficulty()` dans `game/game.cpp`.
- Son/animation de mort : durée dans `audio_play_death()` (`core/audio.cpp`).
- Fruit : `FRUIT_SPEED` / `FRUIT_LIFE_TICKS` en tête de `game/fruits.cpp`.
