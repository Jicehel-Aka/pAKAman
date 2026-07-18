# pAKAman 

Portage de pAKAman sur la même architecture que **AKAsseBricks** :
composant matériel ESP-IDF `components/gamebuino/` + couche `main/core/` propre
+ boucle unique dans `app_main.cpp`. Toute la logique de jeu Pac-Man est
conservée ; seules les couches matériel / entrées / audio / UI ont été
modernisées.

---

## Ce qui a changé

### Architecture
- **Composant `components/gamebuino/`** repris tel quel d'AKAsseBricks
  (`gb_ll` bas niveau + `gb_lib` : `gb_core`, `gb_graphics`, `gb_audio_*`).
  Il est désormais l'**unique propriétaire du bus I2C + ADC**.
- **Fin du multitâche jeu/input** : plus de `task_game` / `task_input`.
  `app_main.cpp` est une **boucle unique** à 40 FPS (cadence historique de
  pAKAman conservée : `FRAME_US = 25000`). Seule reste une tâche dédiée au
  **mixage audio** (cœur 0, seul appelant de `player.pool()`).
- Ancien dossier `main/lib/` (LCD, expander, ADC, audio maison, PMF…) **supprimé** :
  tout passe par le composant.
- `main/tasks/`, `core/gfx_fb`, `core/gfx_direct`, `core/graphics.cpp` (ancien),
  `core/audio.cpp` (ancien), `core/persist.*`, `ui/options.*`,
  `game/level_editor.*` **retirés** (remplacés ou inutilisés).

### Calibration joystick
- `g_core.joystick.calibrate_center()` au démarrage (stick au repos), et une
  entrée **« Recalibrer le stick »** dans le menu.
- `input_poll()` reconvertit `get_x()/get_y()` (−1000..1000) vers l'échelle
  `JOYX_MID` : **`pacman.cpp` est inchangé** (mêmes comparaisons `JOYX_MID ± DEADZONE`).
- **Lecture matérielle unique par frame** : `Pacman::update()` réutilise
  désormais `g_keys` (état déjà lu par `app_main`) au lieu de rappeler
  `input_poll()` → plus de double lecture I2C/ADC dans la frame.

### Menus modernes (i18n, modal)
- Nouveau `ui/menu.cpp` (ouvert par **appui court sur MENU**) : Jouer/Reprendre,
  **Musique ON/OFF**, Volume, **Langue (FR/EN/DE/ES/IT)**, Scores, Commandes,
  Recalibrer, Retour au loader. Navigation HAUT/BAS, A = valider,
  GAUCHE/DROITE = régler, B = retour.
- Réglages **persistants sur SD** (`/sdcard/PAKAMAN/SETTINGS.DAT`, via
  `core/settings`) : langue + volume + musique.

### Capture d'écran
- **MENU maintenu ≥ 500 ms** → capture BMP 24 bits dans
  `/sdcard/PAKAMAN/SHOTxxxx.BMP` (numérotation automatique). Un appui **court**
  ouvre le menu ; on distingue au relâchement.

### Retour au loader
- **RUN + MENU maintenus 500 ms** (à tout moment) → bascule sur la partition
  OTA du loader et redémarre. Également disponible comme entrée de menu.

### Audio
- Adaptateur `core/audio` sur le player du composant (4 voix) :
  **3 voix d'effets** (tons, round-robin : un son n'en coupe plus un autre) +
  **1 voix musique PMF** (`pacman_pmf`, via `gb_audio_track_pmf`).
- Les appels du jeu sont **inchangés** : `audio_play_pacgomme/power/eatghost/
  death/begin()` et `audioPMF.init/start/stop/isPlaying()` fonctionnent via un
  shim `AudioPMF` bâti sur le composant.
- **Effets synthétisés** (waka, sweep, arpège, descente de mort, jingle de
  début) : plus aucune dépendance à des fichiers WAV sur la carte SD → build
  garanti sans assets externes. Voir « Restaurer les WAV » plus bas.

---

## Arborescence

```
pAKAman/
├─ CMakeLists.txt
├─ sdkconfig                      (aligné sur le composant gamebuino)
├─ components/gamebuino/          (composant matériel, repris d'AKAsseBricks)
└─ main/
   ├─ CMakeLists.txt
   ├─ app_main.cpp                (boucle unique : machine à états + loader + capture + menu)
   ├─ core/
   │  ├─ input.{h,cpp}            (propriétaire I2C/ADC ; Keys ; g_keys)
   │  ├─ graphics.{h,cpp}         (gfx_* + gfx_putpixel16 + capture BMP ; palette BGR565)
   │  ├─ sprite.{h,cpp}           (routines sprite, via gfx_putpixel16 — conservé)
   │  ├─ audio.{h,cpp}            (SfxBus tons + AudioPMF + audio_play_*)
   │  ├─ sdcard.{h,cpp}
   │  ├─ i18n.{h,cpp}             (5 langues)
   │  └─ settings.{h,cpp}         (langue+volume+musique sur SD)
   ├─ ui/
   │  ├─ menu.{h,cpp}             (menu moderne)
   │  ├─ title_screen.{h,cpp}
   │  └─ highscores.{h,cpp}       (/sdcard/PAKAMAN/SCORES.DAT)
   ├─ game/                       (logique Pac-Man — conservée)
   └─ assets/                     (sprites BGR565, pacman_pmf, image titre)

```

## Restaurer les sons WAV d'origine (optionnel)

Les effets sont synthétisés pour garantir le build. Pour rejouer les WAV
d'origine, deux options via le composant :
- **Depuis la SD** : `gb_audio_track_wav::play_wav("/sdcard/PAKAMAN/PACGOMME.WAV")`
  (format 44 k / mono / 16 bits). Nécessite une piste dédiée → réduire `SfxBus`
  à 2 voix pour rester dans les 4 slots.
- **Embarqués en ROM** : convertir les WAV en header (`convert_wav_to_header`) et
  utiliser `play_wav(const uint8_t*)`.
Je peux te câbler l'une ou l'autre si tu préfères les WAV aux tons.
