# pAKAman — Documentation technique

Portage d'un Pac-Man custom sur console AKA (ESP32-S3), au-dessus du composant
matériel `gamebuino` (I2C + ADC + LCD + audio). Ce document décrit
l'architecture, les formats de données, le build/flash, les commandes, et
liste les correctifs appliqués lors de la revue de code.

---

## 1. Architecture générale

```
pAKAman/
├─ CMakeLists.txt              (projet ESP-IDF, cible esp32s3)
├─ components/gamebuino/       (composant matériel — bus I2C/ADC unique)
└─ main/
   ├─ app_main.cpp             (point d'entrée : boucle unique 33 FPS)
   ├─ core/                    (adaptateurs au-dessus du composant)
   ├─ ui/                      (menu, écran-titre, high-scores)
   ├─ game/                    (logique Pac-Man, indépendante du matériel)
   └─ assets/                  (sprites, police, musique PMF)
```

**Principe directeur** : `components/gamebuino/` est le seul propriétaire du
bus I2C et de l'ADC. Tout le reste (`main/core/`) est un adaptateur fin
au-dessus de ce composant ; `main/game/` ne connaît que des structures
génériques (`Keys`, `GameState`) et ne fait aucun appel matériel direct.

### 1.1 Boucle principale (`app_main.cpp`)

Une seule boucle `while(true)` à cadence fixe (`FRAME_US = 30000` µs, soit
**~33,33 FPS**), remplaçant l'ancienne architecture à deux tâches
(`task_game` / `task_input`). Une tâche séparée existe uniquement pour le
mixage audio (`core/audio.cpp`, cœur 0), car le FIFO I2S doit être alimenté
plus souvent que le rythme de la boucle jeu.

Séquence de démarrage (`app_main()`) :
1. `g_core.init()` + `gfx_init()` — matériel.
2. `g_core.joystick.calibrate_center()` — le stick doit être au repos.
3. `i18n::check_table_integrity()` — vérifie qu'aucune traduction ne manque.
4. `sd_init()` — vérifie que la SD est montée ET accepte l'écriture (voir §4).
5. `settings_load()` — langue/volume/musique depuis `SETTINGS.DAT`.
6. `audio_game_init()` — codec + voix + tâche de mixage.
7. `input_init()`, `assets_init()`, `highscores_init()`.
8. Boucle principale : lecture entrées → combo loader global → menu →
   machine à états du jeu (`TitleScreen` / `StartingLevel` / `Playing` /
   `Paused` / `PacmanDying` / `GameOver` / `Highscores`).

### 1.2 Entrées (`core/input.cpp`)

`input_poll()` est **l'unique appelant** de `g_core.pool()` (lecture I2C +
ADC). Tout le reste du code (jeu, menu, combo loader) consomme la structure
`Keys` déjà remplie — aucune double lecture matérielle par frame.

Le joystick est recalibré à `-1000..1000` par le composant, puis reconverti
vers l'échelle historique `JOYX_MID ± DEADZONE` attendue par `game/pacman.cpp`,
qui reste ainsi inchangé.

### 1.3 Graphisme (`core/graphics.cpp`)

Framebuffer unique + DMA (`lcd_refresh_completed()` attendu avant tout
`clear`/`draw` plein écran, pour éviter le clignotement). Pixels en **BGR565**
(même ordre que `lcd_color_rgb()` du composant — pas de conversion nécessaire
pour les assets).

La capture d'écran (`gfx_save_screenshot_bmp()`) génère un BMP 24 bits en
lisant `lcd_getpixel()` pixel par pixel. Un index en RAM (`s_next_shot_index`)
évite de rescanner tous les slots déjà utilisés à chaque capture (voir §6).

### 1.4 Audio (`core/audio.cpp`)

4 voix matérielles (`AUDIO_PLAYER_TRACK_COUNT` du composant) :
- **3 voix effets** (`SfxBus`, round-robin) — un son n'en coupe plus un autre.
- **1 voix musique** (`AudioPMF`, format PMF) via `gb_audio_track_pmf`.

Les effets sont **synthétisés** (tons/balayages), sans dépendance à des
fichiers WAV externes — le build ne nécessite aucun asset audio sur SD. Le
README documente comment rebrancher les WAV d'origine si besoin (voir §7).

### 1.5 Logique de jeu (`game/`)

Fidèle à Pac-Man dans ses grands principes :
- 4 fantômes avec ciblage individuel (`ghost.cpp:getChaseTarget`) :
  Blinky (poursuite directe), Pinky (case 4 devant Pac-Man), Inky (miroir
  via Blinky), Clyde (poursuite/fuite selon distance).
- Score fantômes en chaîne : 200 → 400 → 800 → 1600 (`GHOST_SCORE`, doublé à
  chaque fantôme mangé pendant un même frightened).
- Fruits bonus par palier de niveau (cerise 100 → clé 5000), fidèles à
  l'échelle Ms. Pac-Man/Pac-Man.
- Vie bonus à 10 000 points (une seule fois par partie).
- Schedule Scatter/Chase (7 s / 20 s / 7 s / Chase permanent).

Voir §8 pour ce qui diffère encore du jeu d'arcade original.

---

## 2. Boucle de rendu et cadence

**Point d'attention critique** : toutes les durées de jeu exprimées "en
ticks" (frightened, sortie des fantômes, vie des fruits, etc.) sont
désormais dérivées d'une seule constante `GAME_FPS` (`game/config.h`), elle
même alignée sur `FRAME_US` de `app_main.cpp`, via la fonction
`SEC_TO_TICKS(secondes)`.

**Si vous changez `FRAME_US`, `GAME_FPS` doit être mis à jour en conséquence**
(actuellement `1000000.0f / 30000.0f` pour rester synchrone). C'est le seul
endroit à toucher : plus aucune constante de `config.h`/`game.cpp`/`fruits.cpp`
n'encode un framerate en dur.

---

## 3. Formats de données persistées (SD)

Tous les fichiers vivent dans `/sdcard/PAKAMAN/`.

### 3.1 `SETTINGS.DAT` — réglages (langue, volume, musique)

```c
struct SettingsBlob {
    uint8_t magic;      // 0xAC
    uint8_t language;    // index dans i18n::Lang
    uint8_t volume;       // 0..255
    uint8_t music_on;      // 0/1
};
```
4 octets. `settings_load()` ignore le fichier (garde les défauts) si le
magic ne correspond pas ou si la taille lue diffère.

### 3.2 `SCORES.DAT` — meilleurs scores

```c
struct HighscoreFileHeader {   // ajouté lors de la revue de code
    uint8_t magic;      // 0xAC (même famille que SETTINGS_MAGIC)
    uint8_t version;    // 1
};
struct HighscoreEntry {
    char    name[9];    // 8 caractères + '\0'
    int32_t score;
};
```
Le fichier commence désormais par l'en-tête, suivi de N `HighscoreEntry`
(N = `MAX_SCORES` = 6 maximum, triés par score décroissant). Un fichier sans
en-tête valide (ancien format, ou fichier étranger) est ignoré plutôt que
lu comme des données incohérentes — le prochain score soumis réécrit un
fichier propre.

**Migration** : les fichiers `SCORES.DAT` générés par une version antérieure
au correctif (sans en-tête) seront traités comme absents/invalides à la
première lecture ; les scores existants seront donc perdus une fois, ce qui
est acceptable pour ce type de format ludique. Documentez ce point si des
testeurs ont déjà accumulé des scores.

### 3.3 `SHOTxxxx.BMP` — captures d'écran

BMP 24 bits standard, 320×240, non compressé. Numérotation automatique
`SHOT0000.BMP`, `SHOT0001.BMP`, ... L'index de reprise est mis en cache en
RAM (`s_next_shot_index`) pour éviter un rescan complet à chaque capture ;
il est réinitialisé à chaque redémarrage (un seul scan complet au premier
appel après boot).

---

## 4. Robustesse SD

`sd_init()` **ne remonte jamais** la carte (déjà fait par
`g_core.init()` → `gb_ll_sd_init()` ; un second montage casse l'accès
fichiers). Depuis le correctif, il vérifie réellement l'accès : présence de
`/sdcard` (`stat`) + écriture/suppression d'un fichier témoin
(`.pakaman_sdtest`). Si l'un des deux échoue, `app_main()` affiche un
avertissement à l'écran ("Carte SD absente ou illisible") pendant 1,8 s
avant de continuer sans persistance — plutôt qu'un échec totalement
silencieux.

---

## 5. Contrôles

| Action                              | Commande                          |
|--------------------------------------|-----------------------------------|
| Déplacer Pac-Man                     | Stick / D-Pad                    |
| Valider / démarrer                   | A                                 |
| Pause / reprendre                    | RUN                               |
| Ouvrir le menu                       | MENU (appui court)               |
| Capture d'écran                      | MENU maintenu ≥ 500 ms           |
| Retour au loader OTA                 | RUN + MENU maintenus 500 ms, **avec confirmation A/B** |
| Dans le menu : naviguer               | HAUT / BAS                       |
| Dans le menu : régler (volume/langue/musique) | GAUCHE / DROITE          |
| Dans le menu : valider / fermer       | A / B                             |
| Saisie du pseudo (high-score)         | GAUCHE/DROITE = lettre, A = valider lettre, C = effacer, B = terminer |

---

## 6. Correctifs appliqués lors de la revue de code

Résumé (voir diffs dans le zip livré) :

| # | Problème | Fichier(s) | Correctif |
|---|----------|-----------|-----------|
| 1 | Constantes de tick calculées sur 3 framerates différents (60/40/33 FPS) → durées de jeu réelles fausses de 20 à 80 % | `config.h`, `game.cpp`, `fruits.cpp` | `GAME_FPS`/`SEC_TO_TICKS()` centralisés |
| 2 | Aucune confirmation avant retour loader (perte de partie possible) | `app_main.cpp`, `menu.cpp/h` | Écran de confirmation A/B obligatoire |
| 3 | `sd_init()` retournait toujours `true` sans vérifier | `sdcard.cpp` | Test d'écriture réel + avertissement écran |
| 4 | Recherche de nom de capture en O(n), rescane depuis 0 à chaque fois | `graphics.cpp` | Cache RAM du prochain index libre |
| 5 | `fwrite()` jamais vérifié (settings/scores/captures) | `settings.cpp`, `highscores.cpp`, `graphics.cpp` | Vérification + log d'erreur explicite |
| 6 | Table i18n : entrée manquante = `nullptr` silencieux | `i18n.cpp/h`, `app_main.cpp` | `check_table_integrity()` au boot |
| 7 | `SCORES.DAT` sans magic/version (contrairement à `SETTINGS.DAT`) | `highscores.cpp/h` | En-tête magic+version, migration douce |
| 8 | `levels_preset.cpp` : fichier mort livré, ne compile pas isolément | `game/levels_preset.cpp` | *Non corrigé — recommandé : supprimer ou finaliser* |
| 9 | `components/gamebuino` mis à jour (21/08) : `gb_core::init()` et `gb_ll_sd_init()` retournent désormais un code d'erreur documenté (`gb_err.h`) au lieu de `void`/`bool` implicite | `app_main.cpp`, `core/sdcard.cpp/h` | `g_core.init()` vérifié (écran d'erreur + halte si échec matériel critique) ; `sd_init()` simplifié pour lire l'état réel via `gb_ll_sd_is_mounted()` au lieu d'un test d'écriture manuel |

### Note sur la mise à jour du composant `gamebuino` (21/08)

Le nouveau composant corrige, côté bibliothèque, exactement le problème
identifié au point 3 (`sd_init()` non vérifié) : `gb_ll_sd_init()` retourne
maintenant un vrai code d'erreur (`GB_OK` / `GB_ERR_IO` / `GB_ERR_BUSY`), et
`gb_ll_sd_is_mounted()` expose l'état réel du montage. Le correctif
applicatif a été simplifié en conséquence (`sdcard.cpp` interroge
directement la bibliothèque plutôt que refaire un test d'écriture maison).

Autre point important repéré dans le diff : `gb_buttons::pool()` appelait
auparavant `gb_ll_expander_power_off()` **à chaque appui sur RUN**, sans
condition — or pAKAman utilise RUN pour la pause. Le nouveau composant
gate ce comportement derrière `set_run_power_off(bool)`, **désactivé par
défaut** ; pAKAman ne l'active pas, donc RUN reste bien dédié à la pause.
Aucune action requise côté `main/`, mais bon à savoir si vous étendez le
projet et voyez un jour RUN couper l'appareil de façon inattendue.

Enfin, `gb_core::init()` retourne maintenant un code d'erreur : en cas
d'échec critique (I2C/ADC/expander/ampli audio), la fonction retourne
**avant** d'initialiser le LCD — `app_main()` vérifie donc ce retour et
affiche un écran d'erreur en best-effort avant de s'arrêter proprement,
plutôt que de laisser le jeu démarrer sur un bus matériel cassé.

---

## 7. Build / Flash

Cible : **ESP32-S3** (`CONFIG_IDF_TARGET=esp32s3` dans `sdkconfig`).

```bash
# Depuis un environnement ESP-IDF déjà initialisé (idf.py exporté) :
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

> La version exacte d'ESP-IDF testée par l'auteur n'est pas indiquée dans le
> dépôt — à documenter dans le README (`idf.py --version` au moment du
> dernier build validé), pour éviter des divergences de comportement entre
> contributeurs sur des versions différentes du SDK.

Composants requis (`main/CMakeLists.txt`) : `gamebuino`, `nvs_flash`,
`fatfs`, `esp_timer`, `esp_psram`, `spi_flash`, `driver`, `app_update`,
`esp_partition`.

### Restaurer les sons WAV d'origine (optionnel)

Voir le README du dépôt : deux options via le composant `gamebuino`
(`gb_audio_track_wav::play_wav()` depuis la SD, ou assets WAV embarqués en
ROM via `convert_wav_to_header`). Non câblé actuellement (effets synthétisés
par défaut pour garantir un build sans assets externes).

---

## 8. Écarts encore présents par rapport au Pac-Man d'arcade original

Voir le document séparé `AMELIORATIONS_FIDELITE_ORIGINAL.md` pour la liste
priorisée et détaillée. **Mise à jour** : les points P1-1 (schedule
scatter/chase par niveau), P1-2 (sortie des fantômes sur compteur de
pastilles), P1-3 (Cruise Elroy) et l'intermède narratif (P2-4) ont été
implémentés — voir §10 ci-dessous.

## 10. Améliorations de fidélité implémentées (après revue de code)

| Amélioration | Fichiers | Détail |
|---|---|---|
| **Bug latent corrigé** | `game.cpp` (`update_modes`) | `ghost.mode` n'était plus jamais resynchronisé sur `g.global_mode` une fois le fantôme sorti de sa maison : seul le demi-tour était appliqué aux changements de phase suivants, la cible de poursuite/fuite restait figée. Corrigé — sans ce correctif, l'amélioration suivante n'aurait eu aucun effet visible. |
| **Schedule Scatter/Chase par niveau** | `game.cpp` (`init_ghost_schedule`) | 3 profils (niveau 1 / niveaux 2-4 / niveau 5+), durées alignées sur l'arcade original (7-20-7-20-5-20-5-∞, puis 7-20-7-20-5-1033-1tick-∞, puis 5-20-5-20-5-1037-1tick-∞). |
| **Cruise Elroy** | `game.h` (`ghost_elroy_stage`), `game.cpp` (`apply_level_difficulty`), `ghost.cpp` | Blinky (id 0) accélère (+8% puis +16%) et ignore les phases Scatter dès que les pastilles restantes passent sous des seuils proportionnels au total du niveau (`elroy1_dots_left`/`elroy2_dots_left`). |
| **Sortie des fantômes sur compteur de pastilles** | `ghost.h` (`dot_release_threshold`), `game.cpp`, `ghost.cpp` | En plus du minuteur existant (conservé comme filet de sécurité), Inky et Clyde ont désormais un seuil de pastilles mangées (30/60 au niveau 1, décroissant), Blinky/Pinky sortent immédiatement. |
| **Intermèdes narratifs entre niveaux** | `ui/intermission.h/.cpp`, `game.h/.cpp`, `app_main.cpp` | Nouvel écran bloquant (même convention que `menu_open()`/`highscores_submit()`) déclenché après les niveaux 2, 5, 9 puis tous les 4 niveaux (`level_has_intermission()`). Petite histoire récurrente en 4 chapitres (poursuite/retournement), jouée avec les sprites déjà existants — aucun nouvel asset requis. Skippable à tout moment (A ou B). |

**Note d'implémentation sur l'intermède** : le nouvel état
`GameState::State::LevelComplete` (déclaré mais jamais utilisé auparavant)
sert maintenant de point de passage : `game.cpp` y bascule au lieu d'appeler
directement `reset_level_full()`, et `app_main.cpp` affiche l'intermède puis
appelle la nouvelle fonction exportée `game_advance_to_next_level()` qui fait
ce que faisait l'ancien appel direct. Le niveau qui vient d'être terminé est
mémorisé dans `g.last_completed_level` pour que l'écran sache quel chapitre
jouer.

---

## 9. Attribution tierce

`components/gamebuino/gb_lib/pmf_player/pmf_player.cpp` est sous licence
BSD (Copyright Profoundic Technologies, Inc.), distincte de l'Apache-2.0 du
dépôt racine. Un fichier `NOTICE` ou `THIRD_PARTY_LICENSES.md` listant ce
composant est recommandé pour la conformité de licence.

## 11. Menu système et langue — alignés sur le style commun AKA

Suite à l'analyse d'un autre portage AKA (Kong II, composant `aka_runtime`),
le menu et la gestion de langue de pAKAman ont été alignés sur ce style
partagé plutôt que de rester sur l'implémentation "maison" d'origine.

**Menu (`ui/menu.cpp`)** — nouvelle fonction `menu_frame()` : boîte encadrée
240×200 centrée sur l'écran 320×240 (fond bleu foncé `COLOR_DARKBLUE`,
bordure blanche, titre en jaune), identique en géométrie et en apparence à
celle utilisée par Kong II. Ajout de deux primitives graphiques génériques
dans `core/graphics.h/.cpp` pour la rendre possible : `gfx_fill_rect()` et
`gfx_draw_rect()`. Le contenu du menu (Jouer/Reprendre, Musique, Volume,
Langue, Scores, Commandes, Recalibrage, Retour loader) est inchangé — seul
l'habillage visuel a changé.

**Langue (`core/i18n.cpp/h`)** — bascule d'une table C++ figée à la
compilation vers un chargement JSON depuis la carte SD, même mécanisme que
`aka_runtime` :
- `/sdcard/AKA/lang/<code>.json` — fichier **commun à tous les jeux AKA**
  (copié tel quel depuis Kong II) : `MENU_TITLE`, `MENU_RESUME`,
  `MENU_CONTROLS`, `MENU_LANGUAGE`, `MENU_RETURN_LOADER` (+ `MENU_CREDITS`
  et les clés `CREDITS_*`, non utilisées par pAKAman mais présentes dans
  le fichier partagé).
- `/sdcard/PAKAMAN/lang/<code>.json` — fichier **spécifique à pAKAman**,
  prioritaire en cas de clé en double : toutes les autres chaînes du jeu
  (`MENU_PLAY`, `MENU_MUSIC`, `GAME_OVER`, etc.).
- L'API `T(Str s)` reste inchangée pour tous les appelants existants
  (aucune modification requise ailleurs dans le code) : elle cherche
  d'abord dans les tables JSON chargées depuis la SD, et **retombe sur
  l'ancienne table C++ codée en dur** si la SD ou le fichier est absent —
  aucune régression possible même sans carte SD.
- **Changement visible** : le titre du menu affiche désormais "MENU" (non
  traduit) dans toutes les langues, y compris en allemand où il affichait
  auparavant "Menue" — c'est la valeur exacte du fichier commun partagé
  avec les autres jeux AKA, adoptée intentionnellement pour la cohérence
  inter-jeux plutôt que conservée telle quelle.
- Les 5 fichiers `sdcard_files/PAKAMAN/lang/*.json` livrés avec le projet
  contiennent une traduction complète (aucune dépendance au repli C++ en
  usage normal) ; ils doivent être copiés sur la carte SD en même temps
  que `sdcard_files/AKA/lang/`.
