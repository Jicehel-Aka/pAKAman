# pAKAman — Écarts avec le Pac-Man d'arcade original (1980)

Constats basés sur la lecture de `game/ghost.cpp`, `game/game.cpp`,
`game/fruits.cpp`, `game/pacman.cpp` et `game/maze.cpp`. Ce que le jeu fait
déjà correctement n'est pas re-détaillé ici (voir §1.5 de la documentation
technique) — cette liste ne couvre que ce qui diffère de l'arcade original,
classé par impact sur la fidélité du gameplay.

## Déjà fidèle (pour référence, non traité ci-dessous)
- Ciblage individuel des 4 fantômes (Blinky/Pinky/Inky/Clyde).
- Score fantômes en chaîne 200→400→800→1600.
- Table des fruits bonus par palier (cerise → clé).
- Vie bonus à 10 000 points.
- Tunnels avec ralentissement dédié (`GHOST_SPEED_TUNNEL`).
- Mode Frightened avec clignotement en fin de durée.

---

## P1 — Impact fort sur le ressenti de jeu

**1. Schedule Scatter/Chase fixe, indépendant du niveau**
`game.cpp:init_ghost_schedule()` applique toujours les 4 mêmes phases
(7 s Scatter / 20 s Chase / 7 s Scatter / Chase infini), quel que soit le
niveau. Dans l'arcade original, le nombre de phases et leurs durées varient
significativement :
- Niveau 1 : 7-20-7-20-5-20-5-∞ (8 phases).
- Niveaux 2-4 : 7-20-7-20-5-1033-1/60-∞ (la 6ᵉ phase Chase dure ~17 min avant
  un dernier Scatter d'1 frame, puis Chase permanent).
- Niveau 5+ : 5-20-5-20-5-1037-1/60-∞.

C'est ce qui donne la sensation de fantômes de plus en plus agressifs et
de moins en moins prévisibles à mesure qu'on progresse. Actuellement, le
niveau 1 et le niveau 15 ont exactement le même rythme d'alternance.
→ Passer `init_ghost_schedule(GameState& g, int level)` et sélectionner la
table de phases appropriée (au moins 3 profils : niveau 1 / niveaux 2-4 /
niveau 5+, qui couvrent déjà l'essentiel de la sensation recherchée).

**2. Sortie des fantômes basée sur un minuteur, pas sur un compteur de points**
`ghost.cpp` calcule `releaseTime_ticks = elapsed_ticks + ghostReleaseInterval_ticks`
: c'est un pur minuteur. L'arcade original utilise un **compteur de points
mangés** (dot counter) par fantôme, avec un compteur global de secours en
cas d'inactivité prolongée (le fameux comportement "Pac-Man affamé qui ne
mange aucune pastille fait sortir les fantômes plus vite"). Cette différence
est ce qui rend certaines stratégies de speedrun/évitement de l'original
impossibles à reproduire ici, et change légèrement le rythme des sorties si
le joueur laisse traîner des pastilles dans un coin.
→ Ajouter un compteur de pastilles mangées par niveau, avec seuils de
sortie par fantôme (Pinky immédiat, Inky à 30, Clyde à 60 typiquement pour
le niveau 1) plutôt qu'un pur minuteur ; garder le minuteur actuel comme
filet de sécurité (c'est d'ailleurs ce que fait l'arcade original aussi,
via un minuteur "frustration").

**3. Absence du mode Cruise Elroy (accélération de Blinky)**
Dans l'original, quand il reste peu de pastilles sur le niveau (seuils
dépendant du niveau, typiquement 20 puis 10 pastilles restantes), Blinky
devient "Elroy" : il accélère (vitesse Chase normale → +5 % puis +10 %) et
son mode redevient Chase en continu même pendant les phases Scatter
programmées. C'est un des éléments les plus caractéristiques de la tension
de fin de niveau dans le jeu original — sans lui, la fin de niveau reste
au même rythme que le début.
→ Ajouter un compteur de pastilles restantes par niveau, deux seuils
configurables (`ELROY1_DOTS_LEFT`, `ELROY2_DOTS_LEFT`, décroissants par
niveau comme dans l'original), et une vitesse/mode dédiés pour Blinky une
fois ces seuils franchis.

## P2 — Impact modéré, surtout visuel/narratif

**4. Pas d'intermèdes animés entre niveaux**
L'arcade original insère 3 courtes animations (Pac-Man/Blinky en poursuite,
Blinky déchiré, Blinky reconstitué) après les niveaux 2, 5 et 9 (puis tous
les 4 niveaux à partir du 9ᵉ passage). Aucune trace de ce type d'écran dans
`game/` ou `ui/` — le jeu enchaîne directement au niveau suivant.
→ Optionnel selon le temps disponible : un intermède simplifié (2-3
sprites en défilement horizontal) après les niveaux clés serait suffisant
pour retrouver la sensation, sans nécessiter l'animation complète.

**5. Pas de mode démo/attract à l'écran-titre**
`ui/title_screen.cpp` (15 lignes) affiche un écran statique. L'arcade
original fait défiler un mini-gameplay simulé (fantômes qui se déplacent
seuls) en boucle sur l'écran-titre pour attirer les joueurs en salle
d'arcade — moins pertinent sur une console portable personnelle, mais reste
un manque si l'objectif est la fidélité totale.

**6. Pas de "cornering" (pré-virage anticipé)**
Dans l'arcade original, Pac-Man et les fantômes commencent à tourner
légèrement avant d'atteindre le centre exact d'une intersection si la
touche de direction est déjà enfoncée, ce qui rend les virages plus fluides
et permet certaines techniques de jeu avancées (couper des virages pour
distancer un fantôme). Aucune trace de cette logique dans `pacman.cpp`
(mouvement fait au tick/case, avec tolérance `CENTER_EPS`/`SNAP_EPS` mais
pas d'anticipation de virage). Impact surtout sensible pour les joueurs
expérimentés de l'original ; les nouveaux joueurs ne remarqueront pas
l'absence.

## P3 — Fidélité fine, à envisager seulement si le reste est traité

**7. Palier de réduction de difficulté par niveau non calibré sur des données d'origine**
`game.cpp:apply_level_difficulty()` réduit `frightened_duration_ticks` de
45 ticks/niveau et `ghostReleaseInterval_ticks` de 20 ticks/niveau, avec des
planchers (90 et 45 ticks). Ce sont des valeurs de tuning maison
raisonnables mais qui ne correspondent à aucune table publiée de l'arcade
original (qui a ses propres paliers, notamment une durée frightened qui
tombe à **0 seconde** dès le niveau 19 — les pastilles de super-pouvoir ne
font plus fuir les fantômes du tout, seulement inverser leur direction).
→ Non prioritaire : à traiter seulement si l'objectif est une réplique
stricte des tables de difficulté originales niveau par niveau.

**8. Vitesses de déplacement en pixels/frame fixes, pas en pourcentage de la vitesse de base**
`config.h` fixe des vitesses absolues (`PACMAN_SPEED = 3`, `GHOST_SPEED = 2`,
etc.) alors que l'original exprime toutes les vitesses en **pourcentage
d'une vitesse de référence**, ce qui permet un calibrage fin par niveau
(ex. Pac-Man va à 90 % niveau 1, 100 % ensuite, les fantômes normaux à
75 % niveau 1 puis 85 %...). Le système actuel est plus simple à maintenir
mais moins granulaire.

---

## Recommandation d'ordre de traitement

Si vous voulez un gain de fidélité maximal pour un effort raisonnable,
traiter dans cet ordre : **1 → 3 → 2** (schedule variable + Elroy changent
immédiatement le ressenti de "fin de niveau qui monte en tension", pour un
coût de code modéré), puis évaluer si les points P2/P3 valent l'effort
selon votre public cible (un tutoriel pédagogique n'a probablement pas
besoin d'aller jusqu'au cornering ou aux intermèdes animés).
