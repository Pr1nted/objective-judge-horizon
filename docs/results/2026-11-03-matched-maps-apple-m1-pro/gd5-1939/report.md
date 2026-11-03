# Objective Judge Horizon (OJH) report

Written by OJH 0.2.0 on 2026-11-03 13:25 from 14 result file(s) covering 6 game(s). Measured: turn speed for 6, frame rate for 4, network for 4 and footprint for 0.

## At a glance

Games in order of their OJH score. Firsts and lasts count the statistics below on which a game came best or worst of the games measured for it; a statistic only one game has is not counted. Went best and went worst name the statistic where the game stood furthest ahead or behind.

| Game | OJH score | Firsts | Lasts | Went best | Went worst |
|---|---|---|---|---|---|
| Open Doctrines | 3,835 | 15 | 0 | CPU time per player-turn: 0.0006 s, 1st of 6 | 1% low frame rate: 82.75 fps, 2nd of 3 |
| Unciv | 2,127 | 12 | 0 | While a turn resolves: 2,203 fps, 1st of 2 | Busiest second: 743 KiB, 3rd of 4 |
| Freeciv | 1,738 (provisional) | 0 | 5 | Peak memory: 70.1 MiB, 2nd of 6 | Data per turn, lowest: 171 KiB, 3rd of 3 |
| Greater Diplomacy 5 | 1,007 (provisional) | 2 | 11 | Late-game slowdown: 0.58x, 1st of 6 | While a turn resolves: 117.6 fps, 2nd of 2 |
| FreeCol (version 1.2.0) | 726 | 0 | 1 | Region-turns per minute: 211,584 region-turns/min, 3rd of 6 | Late-game slowdown: 11.75x, 6th of 6 |
| FreeOrion (version 0.5.1.2) | 310 | 0 | 12 | Late-game slowdown: 3.58x, 5th of 6 | CPU time per turn: 23.8 s, 6th of 6 |

![OJH score, best first](graphs/score.svg)

## What went best and what went worst

### For every statistic

| Statistic | Better when | Best | Worst | Best vs worst |
|---|---|---|---|---|
| Turns per minute | more | Open Doctrines, 2,915 turns/min | FreeOrion, 6.71 turns/min | 434.5x more |
| Player-turns per minute | more | Open Doctrines, 107,863 player-turns/min | FreeOrion, 234.9 player-turns/min | 459.2x more |
| Region-turns per minute | more | Open Doctrines, 2,641,185 region-turns/min | FreeOrion, 6,081 region-turns/min | 434.3x more |
| Turns per minute, slowest run | more | Open Doctrines, 2,838 turns/min | FreeOrion, 6.37 turns/min | 445.5x more |
| Median turn | less | Open Doctrines, 0.0206 s | FreeOrion, 8.447 s | 410.0x less |
| Slow turn (95th percentile) | less | Open Doctrines, 0.0251 s | FreeOrion, 17.3 s | 687.3x less |
| Slowest turn | less | Open Doctrines, 0.0533 s | FreeOrion, 21.5 s | 403.1x less |
| Late-game slowdown | less | Greater Diplomacy 5, 0.58x | FreeCol, 11.75x | 20.4x less |
| Start-up | less | Open Doctrines, 0.177 s | FreeOrion, 7.813 s | 44.1x less |
| Peak memory | less | Open Doctrines, 65.3 MiB | FreeOrion, 4.53 GiB | 71.0x less |
| Memory per player | less | Open Doctrines, 1.76 MiB | FreeOrion, 132 MiB | 75.1x less |
| CPU time per turn | less | Open Doctrines, 0.0205 s | FreeOrion, 23.8 s | 1163.2x less |
| CPU time per player-turn | less | Open Doctrines, 0.0006 s | FreeOrion, 0.680 s | 1229.6x less |
| Frame rate on the map | more | Unciv, 2,212 fps | Greater Diplomacy 5, 181.1 fps | 12.2x more |
| 1% low frame rate | more | Unciv, 261.3 fps | Greater Diplomacy 5, 4.73 fps | 55.2x more |
| Slow frame (99th percentile) | less | Unciv, 0.0032 s | Greater Diplomacy 5, 0.0349 s | 10.9x less |
| Main menu | more | Unciv, 2,309 fps | Greater Diplomacy 5, 173.7 fps | 13.3x more |
| Map, start of game | more | Unciv, 2,374 fps | Greater Diplomacy 5, 181.1 fps | 13.1x more |
| Map, zoomed out | more | Unciv, 2,212 fps | Greater Diplomacy 5, 129.6 fps | 17.1x more |
| Map, zoomed in | more | Unciv, 1,521 fps | Greater Diplomacy 5, 200.4 fps | 7.6x more |
| Map, scrolling | more | Unciv, 2,186 fps | Greater Diplomacy 5, 182.0 fps | 12.0x more |
| Heaviest panel | more | Unciv, 1,719 fps | Greater Diplomacy 5, 181.6 fps | 9.5x more |
| Map, late game | more | Unciv, 2,377 fps | Greater Diplomacy 5, 170.1 fps | 14.0x more |
| While a turn resolves | more | Unciv, 2,203 fps | Greater Diplomacy 5, 117.6 fps | 18.7x more |
| Data per turn, highest | less | Open Doctrines, 12.5 KiB | Freeciv, 225 KiB | 17.9x less |
| Data per turn, median | less | Open Doctrines, 10.8 KiB | Freeciv, 199 KiB | 18.4x less |
| Data per turn, lowest | less | Open Doctrines, 2.45 KiB | Freeciv, 171 KiB | 69.9x less |
| Highest data per turn per player | less | Unciv, 2.55 KiB | Freeciv, 6.42 KiB | 2.5x less |
| Busiest second | less | Greater Diplomacy 5, 0 B | Freeciv, 883 KiB | n/a |

### For every game

- **Open Doctrines**: went best at CPU time per player-turn: 0.0006 s, 1st of 6; CPU time per turn: 0.0205 s, 1st of 6; Data per turn, lowest: 2.45 KiB, 1st of 3. Went worst at nothing (no last places).
- **Unciv**: went best at While a turn resolves: 2,203 fps, 1st of 2; Map, late game: 2,377 fps, 1st of 3; 1% low frame rate: 261.3 fps, 1st of 3. Went worst at nothing (no last places).
- **Freeciv**: went best at nothing (no first places). Went worst at Data per turn, lowest: 171 KiB, 3rd of 3; Data per turn, median: 199 KiB, 3rd of 3; Data per turn, highest: 225 KiB, 3rd of 3.
- **Greater Diplomacy 5**: went best at Late-game slowdown: 0.58x, 1st of 6; Busiest second: 0 B, 1st of 4. Went worst at While a turn resolves: 117.6 fps, 2nd of 2; 1% low frame rate: 4.73 fps, 3rd of 3; Map, zoomed out: 129.6 fps, 3rd of 3.
- **FreeCol (version 1.2.0)**: went best at nothing (no first places). Went worst at Late-game slowdown: 11.75x, 6th of 6.
- **FreeOrion (version 0.5.1.2)**: went best at nothing (no first places). Went worst at CPU time per turn: 23.8 s, 6th of 6; Region-turns per minute: 6,081 region-turns/min, 6th of 6; CPU time per player-turn: 0.680 s, 6th of 6.

## Machine

| Part | This machine |
|---|---|
| OS | macOS 26.3 (25D125) |
| Model | MacBookPro18,1 |
| CPU | Apple M1 Pro |
| Logical CPUs | 10 (8 performance, 2 efficiency) |
| Memory | 16.0 GiB |
| GPU | Apple M1 Pro (16 cores) |
| Display | 3456 x 2234 Retina |
| Power | battery |
| CPU reference score | 1,739 rounds/s on one core, 13,063 on all cores |

The reference score is OJH's own fixed CPU workload (src/machine.c), timed on this machine. Divide a result by it to compare across hardware.

## OJH scores

Each game's score is its own: it is built from that game's result files alone, against fixed reference levels, so no game's score depends on which other games are in this report. Each scorecard next to this report shows every part, and has a badge (.svg) to go with it.

| Game | OJH score | Coverage | Status | Scorecard |
|---|---|---|---|---|
| Open Doctrines | 3,835 | 96% | final | score-opendoctrines.md |
| Unciv | 2,127 | 96% | final | score-unciv.md |
| Freeciv | 1,738 | 80% | provisional | score-freeciv.md |
| Greater Diplomacy 5 | 1,007 | 87% | provisional | score-gd5.md |
| FreeCol (version 1.2.0) | 726 | 67% | final | score-freecol.md |
| FreeOrion (version 0.5.1.2) | 310 | 67% | final | score-freeorion.md |

A provisional score comes from a run too short or incomplete to stand behind; its scorecard says why. Measure again before publishing it.

## Turn speed

How fast each game plays its turns with every player run by its own AI. Start-up and world generation are timed separately and are not part of the turns. Player-turns and region-turns multiply turn speed by the size of the game, so a larger game is not punished for its size.

| Game | Turns per minute | Player-turns per minute | Region-turns per minute | Turns per minute, slowest run | Median turn | Slow turn (95th percentile) | Slowest turn | Late-game slowdown | Start-up | Players | Map regions |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Open Doctrines | 2,915 turns/min (1st) | 107,863 player-turns/min (1st) | 2,641,185 region-turns/min (1st) | 2,838 turns/min (1st) | 0.0206 s (1st) | 0.0251 s (1st) | 0.0533 s (1st) | 0.76x (2nd) | 0.177 s (1st) | 37 | 906 |
| Unciv | 242.6 turns/min (2nd) | 8,491 player-turns/min (2nd) | 305,925 region-turns/min (2nd) | 199.7 turns/min (2nd) | 0.245 s (2nd) | 0.379 s (2nd) | 0.452 s (2nd) | 1.99x (3rd) | 0.787 s (2nd) | 35 | 1,261 |
| Greater Diplomacy 5 | 85.47 turns/min (3rd) | 2,992 player-turns/min (3rd) | 77,436 region-turns/min (5th) | 84.01 turns/min (3rd) | 0.576 s (3rd) | 1.311 s (3rd) | 1.581 s (3rd) | 0.58x (1st) | 2.669 s (5th) | 35 | 906 |
| FreeCol | 52.90 turns/min (4th) | 846.3 player-turns/min (5th) | 211,584 region-turns/min (3rd) | 51.19 turns/min (5th) | 0.966 s (4th) | 3.153 s (5th) | 4.043 s (4th) | 11.75x (6th) | 1.046 s (4th) | 16 | 4,000 |
| Freeciv | 52.83 turns/min (5th) | 1,849 player-turns/min (4th) | 152,572 region-turns/min (4th) | 52.76 turns/min (4th) | 1.101 s (5th) | 2.216 s (4th) | 4.290 s (5th) | 2.15x (4th) | 0.903 s (3rd) | 35 | 2,888 |
| FreeOrion | 6.71 turns/min (6th) | 234.9 player-turns/min (6th) | 6,081 region-turns/min (6th) | 6.37 turns/min (6th) | 8.447 s (6th) | 17.3 s (6th) | 21.5 s (6th) | 3.58x (5th) | 7.813 s (6th) | 35 | 906 |

Places are in brackets: 1st is the best of the games measured for that statistic. A statistic that is not ranked (players, map size, cores in use, information per minute) is context: more of it is neither better nor worse on its own.

### Graphs

![Turns per minute, best first](graphs/tpm.svg)

![Player-turns per minute, best first](graphs/tpm_x_players.svg)

![Region-turns per minute, best first](graphs/tpm_x_regions.svg)

![Turns per minute, slowest run, best first](graphs/tpm_slowest_run.svg)

![Median turn, best first](graphs/median_turn.svg)

![Slow turn (95th percentile), best first](graphs/p95_turn.svg)

![Slowest turn, best first](graphs/slowest_turn.svg)

![Late-game slowdown, best first](graphs/late_slowdown.svg)

![Start-up, best first](graphs/start_up.svg)

![Players, largest first](graphs/players.svg)

![Map regions, largest first](graphs/regions.svg)

![Turn time through the run](graphs/turn-times.svg)

### What each statistic means

- **Turns per minute**: complete turns played in a minute with every player run by the game's AI. More is better.
- **Player-turns per minute**: turns per minute times players, so a game with more players is not punished. More is better.
- **Region-turns per minute**: turns per minute times map regions, so a bigger map is not punished. More is better.
- **Turns per minute, slowest run**: the slowest of the repeated runs: the figure a game reaches every time. More is better.
- **Median turn**: the typical time one turn takes. Less is better.
- **Slow turn (95th percentile)**: only one turn in twenty takes longer than this. Less is better.
- **Slowest turn**: the longest a player waited for one turn in the run. Less is better.
- **Late-game slowdown**: median late turn time divided by median early turn time: 1 means turns never slowed down. Less is better.
- **Start-up**: from launching the game to its first turn starting, world generation or loading included. Less is better.
- **Players**: how many players the measured game had. Not ranked.
- **Map regions**: provinces or tiles on the measured map. Not ranked.

### How each game was timed

- **Open Doctrines**: Open Doctrines' own OJH turn lines (OD_OJH=1): processTurn timed turn by turn inside the headless eval, 250 turns. Asked for 250 turns, seed 20260914, players as the game's own scenario or world sets them.
- **Unciv**: OJH's Unciv driver times each GameInfo.nextTurn call (250 turns). Asked for 250 turns, seed 20260914, 35 players, chosen by OJH.
- **Greater Diplomacy 5**: Greater Diplomacy 5's own ojh_benchmark.py: every turn through turn_manager, AI preparation, resolution and the map refresh, every nation AI, model diplomacy skipped (250 turns). Asked for 250 turns, seed 20260914, players as the game's own scenario or world sets them.
- **FreeCol**: FreeCol's own "OJH turn" lines, the gaps between them (250 turns timed of 250 turn lines). Asked for 250 turns, seed 20260914, players as the game's own scenario or world sets them.
- **Freeciv**: gaps between Freeciv's per-turn "End/start-turn server/ai activities" log lines (249 turns timed of 250 markers). Asked for 250 turns, seed 20260914, 35 players, chosen by OJH.
- **FreeOrion**: FreeOrion's own "OJH turn" lines, the gaps between them (250 turns timed of 250 turn lines). Asked for 250 turns, seed 20260914, 35 players, chosen by OJH.

### Before comparing these numbers

- **Run length**: The runs timed different numbers of turns (249 to 250). Turns get slower as a game goes on, so a shorter run reads faster: compare runs of the same length.
- **Game size**: Player counts differ (16 to 37), and so do map sizes. The player-turn and region-turn figures account for size, not for rules or how much thinking each AI does.
- **Scope**: Every number comes from one machine under the fairness rules in README.md. None of it is a claim about other hardware.

## CPU and memory

What the game cost the machine while it played those turns, sampled ten times a second over the game and every process it started.

| Game | Peak memory | Memory per player | CPU time per turn | CPU time per player-turn | Cores in use |
|---|---|---|---|---|---|
| Open Doctrines | 65.3 MiB (1st) | 1.76 MiB (1st) | 0.0205 s (1st) | 0.0006 s (1st) | 1 cores |
| Freeciv | 70.1 MiB (2nd) | 2.00 MiB (2nd) | 1.124 s (4th) | 0.0321 s (4th) | 1 cores |
| Unciv | 796 MiB (3rd) | 22.7 MiB (3rd) | 0.615 s (2nd) | 0.0176 s (2nd) | 2.10 cores |
| FreeCol | 962 MiB (4th) | 60.1 MiB (5th) | 1.319 s (5th) | 0.0825 s (5th) | 1.05 cores |
| Greater Diplomacy 5 | 1.27 GiB (5th) | 37.3 MiB (4th) | 0.696 s (3rd) | 0.0199 s (3rd) | 1 cores |
| FreeOrion | 4.53 GiB (6th) | 132 MiB (6th) | 23.8 s (6th) | 0.680 s (6th) | 1.05 cores |

Places are in brackets: 1st is the best of the games measured for that statistic. A statistic that is not ranked (players, map size, cores in use, information per minute) is context: more of it is neither better nor worse on its own.

### Graphs

![Peak memory, best first](graphs/peak_memory.svg)

![Memory per player, best first](graphs/memory_per_player.svg)

![CPU time per turn, best first](graphs/cpu_per_turn.svg)

![CPU time per player-turn, best first](graphs/cpu_per_player_turn.svg)

![Cores in use, largest first](graphs/cores_used.svg)

### What each statistic means

- **Peak memory**: the most memory the game and every process it started held at once. Less is better.
- **Memory per player**: peak memory divided by players. Less is better.
- **CPU time per turn**: processor time spent per turn, summed over every core. Less is better.
- **CPU time per player-turn**: CPU time per turn divided by players: what one AI player's turn costs. Less is better.
- **Cores in use**: median number of cores busy while turns ran; more is neither better nor worse on its own. Not ranked.

## Frame rate

Frames per second in the same scenes in every game, with frame caps and vsync off where the game allows it. A scene a game does not have is n/a.

| Game | Frame rate on the map | 1% low frame rate | Slow frame (99th percentile) | Main menu | Map, start of game | Map, zoomed out | Map, zoomed in | Map, scrolling | Heaviest panel | Map, late game | While a turn resolves |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Unciv | 2,212 fps (1st) | 261.3 fps (1st) | 0.0032 s (1st) | 2,309 fps (1st) | 2,374 fps (1st) | 2,212 fps (1st) | 1,521 fps (1st) | 2,186 fps (1st) | 1,719 fps (1st) | 2,377 fps (1st) | 2,203 fps (1st) |
| Open Doctrines | 1,420 fps (2nd) | 82.75 fps (2nd) | 0.0061 s (2nd) | 1,488 fps (2nd) | 1,650 fps (2nd) | 1,477 fps (2nd) | 1,130 fps (2nd) | 1,420 fps (2nd) | 1,072 fps (2nd) | 581.7 fps (2nd) | n/a |
| Greater Diplomacy 5 | 181.1 fps (3rd) | 4.73 fps (3rd) | 0.0349 s (3rd) | 173.7 fps (3rd) | 181.1 fps (3rd) | 129.6 fps (3rd) | 200.4 fps (3rd) | 182.0 fps (3rd) | 181.6 fps (3rd) | 170.1 fps (3rd) | 117.6 fps (2nd) |
| Freeciv | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a |

Places are in brackets: 1st is the best of the games measured for that statistic. A statistic that is not ranked (players, map size, cores in use, information per minute) is context: more of it is neither better nor worse on its own.

### Graphs

![Frame rate on the map, best first](graphs/fps.svg)

![1% low frame rate, best first](graphs/fps_low.svg)

![Slow frame (99th percentile), best first](graphs/frame_p99.svg)

![Main menu, best first](graphs/fps_menu.svg)

![Map, start of game, best first](graphs/fps_map_start.svg)

![Map, zoomed out, best first](graphs/fps_map_out.svg)

![Map, zoomed in, best first](graphs/fps_map_in.svg)

![Map, scrolling, best first](graphs/fps_map_pan.svg)

![Heaviest panel, best first](graphs/fps_panel.svg)

![Map, late game, best first](graphs/fps_map_late.svg)

![While a turn resolves, best first](graphs/fps_end_turn.svg)

### What each statistic means

- **Frame rate on the map**: median of the average frame rate over the map scenes. More is better.
- **1% low frame rate**: the frame rate of the slowest 1% of frames in the game's worst scene: what stutter feels like. More is better.
- **Slow frame (99th percentile)**: in the game's slowest scene, only one frame in a hundred takes longer than this. Less is better.
- **Main menu**: average frame rate on the main menu, idle. More is better.
- **Map, start of game**: the world map at the start of a game, default zoom, idle. More is better.
- **Map, zoomed out**: the world map zoomed all the way out. More is better.
- **Map, zoomed in**: the world map zoomed all the way in. More is better.
- **Map, scrolling**: the world map scrolling continuously. More is better.
- **Heaviest panel**: the heaviest information screen: economy, diplomacy or research. More is better.
- **Map, late game**: the world map after many turns have been played. More is better.
- **While a turn resolves**: the map while the game is processing a turn. More is better.

### How each game was measured

- **Unciv**: OJH's Unciv driver opens Unciv's own desktop window (LWJGL3, vsync off, no frame cap), starts a game and times every rendered frame of each scene for 5.00 s after a warm-up; the late-game map comes after 20 turns
- **Open Doctrines**: Open Doctrines' own --ojh-fps mode: its real window with vsync and the frame cap off, every frame timed for 5.00 s per scene after two seconds of settling, on the /Users/vladyavdoshenko/CLionProjects/objective-judge-horizon/results/matched-20261103/maps/gd5-1939.odmap world; the late-game map comes after 20 turns
- **Greater Diplomacy 5**: Greater Diplomacy 5's own ojh_benchmark.py fps mode: the real game window brought to the front, no frame cap, every frame of each scene timed for 5.00 s after settling, on scenarios/historical/1939; pygame draws in software; the late-game map comes after 20 turns
- **Freeciv**: not measured: Freeciv's client redraws only when something on the map changes, so it has no steady frame rate to time

## Network

The game's netcode, measured on this machine through OJH's counting relay, so the internet is not in the numbers: bytes per turn, information per minute and how quickly a finished turn reaches the clients.

| Game | Data per turn, highest | Data per turn, median | Data per turn, lowest | Highest data per turn per player | Turn delivery time | Network information per minute | Information per minute per client | Busiest second |
|---|---|---|---|---|---|---|---|---|
| Open Doctrines | 12.5 KiB (1st) | 10.8 KiB (1st) | 2.45 KiB (1st) | n/a | n/a | 24.0 MiB/min | 12.0 MiB/min | 198 KiB (2nd) |
| Unciv | 89.4 KiB (2nd) | 74.0 KiB (2nd) | 44.4 KiB (2nd) | 2.55 KiB (1st) | n/a | 43.5 MiB/min | 21.8 MiB/min | 743 KiB (3rd) |
| Freeciv | 225 KiB (3rd) | 199 KiB (3rd) | 171 KiB (3rd) | 6.42 KiB (2nd) | 0.0001 s | 32.3 MiB/min | 16.1 MiB/min | 883 KiB (4th) |
| Greater Diplomacy 5 | n/a | n/a | n/a | n/a | n/a | 0 B/min | n/a | 0 B (1st) |

Places are in brackets: 1st is the best of the games measured for that statistic. A statistic that is not ranked (players, map size, cores in use, information per minute) is context: more of it is neither better nor worse on its own.

### Graphs

![Data per turn, highest, best first](graphs/dpt_highest.svg)

![Data per turn, median, best first](graphs/dpt_median.svg)

![Data per turn, lowest, best first](graphs/dpt_lowest.svg)

![Highest data per turn per player, best first](graphs/dpt_per_player.svg)

![Turn delivery time, best first](graphs/delivery.svg)

![Network information per minute, largest first](graphs/nipm.svg)

![Information per minute per client, largest first](graphs/nipm_per_client.svg)

![Busiest second, best first](graphs/busiest_second.svg)

![Data per turn, lowest to highest](graphs/data-per-turn.svg)

### What each statistic means

- **Data per turn, highest**: the most bytes one turn took, both ways: what a connection must survive. Less is better.
- **Data per turn, median**: the bytes a typical turn takes, both ways. Less is better.
- **Data per turn, lowest**: the fewest bytes one turn took, both ways: the floor a connection always carries. Less is better.
- **Highest data per turn per player**: the highest data per turn divided by players in the game. Less is better.
- **Turn delivery time**: median time from a turn ending to its last byte reaching the clients, with no internet in the way. Less is better.
- **Network information per minute**: bytes moved per minute of play, both ways; more is not better or worse on its own. Not ranked.
- **Information per minute per client**: network information per minute divided by clients. Not ranked.
- **Busiest second**: the most bytes moved in any one second: the burst a connection must absorb. Less is better.

### How each game was measured

- **Open Doctrines**: Open Doctrines' headless eval with OD_OJH_NET: after every turn it packs the same turn delta its multiplayer host broadcasts (Game::mpResolveTurn, SaveManager::packTurn) and reports its size times 2 clients. Joining needs a signed-in account, so no client is on the wire: the bytes are the host's real payload, turn delivery is n/a, and the few bytes of each client's orders are not counted
- **Unciv**: Unciv has no live connection: its multiplayer uploads the whole game after a turn and the other player downloads it. OJH's Unciv driver plays 20 turns with 35 civilizations and reports the size of that compressed game after each one, counted once up and once down; no relay, so turn delivery is n/a
- **Freeciv**: freeciv-server with 35 AI players and 2 freeciv-gtk4 clients on this machine, each attached as a global observer, every byte through OJH's loopback relay; a turn ends at the server's End/start-turn log line
- **Greater Diplomacy 5**: GD5's own real-time server (RealtimeServer and MapRealtimeDriver) hosting the 1939 scenario, with 2 real RealtimeClient guests joining through OJH's loopback relay and submitting every turn; turns end when the server opens the next one

## Footprint

What a player downloads and keeps: install size, and the size and speed of a save.

Not measured for any game yet. Run `ojh footprint <game>` for each game, then build the report again.

## Not measured yet

These measurements have no result file in this folder, so their statistics are n/a above and their score parts are left out.

- **Freeciv**: Footprint (`ojh footprint freeciv`)
- **FreeCol**: Frame rate (`ojh fps freecol`), Network (`ojh net freecol`), Footprint (`ojh footprint freecol`)
- **FreeOrion**: Frame rate (`ojh fps freeorion`), Network (`ojh net freeorion`), Footprint (`ojh footprint freeorion`)
- **Greater Diplomacy 5**: Footprint (`ojh footprint gd5`)
- **Open Doctrines**: Footprint (`ojh footprint opendoctrines`)
- **Unciv**: Footprint (`ojh footprint unciv`)

