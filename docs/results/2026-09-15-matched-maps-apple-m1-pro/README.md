# Results: matched maps and player counts, Apple M1 Pro (2026-09-15)

FreeOrion and FreeCol were added on 2026-09-15, turn speed only.

The [first results](../2026-09-14-apple-m1-pro/README.md) ran every game on its own default world, with 8 to 37 players on very different maps. This round puts the games on the same worlds and the same player counts, in two runs:

- **[GD5's 1939](gd5-1939/report.md):** Greater Diplomacy 5's own `scenarios/historical/1939`, converted for Open Doctrines, with Freeciv and Unciv at 35 players.
- **[Open Doctrines' 1939](od-1939/report.md):** Open Doctrines' own `1939.odmap`, converted for Greater Diplomacy 5, with Freeciv and Unciv at 63 players.

It also fixes how Greater Diplomacy 5 is measured. Its turns and frames are now timed by GD5's own benchmark hook ([GD5 pull request #46](https://github.com/GitGetGot415/Greater-Diplomacy-5/pull/46)), not by OJH guessing from outside.

## GD5's 1939

906 provinces in both Greater Diplomacy 5 and Open Doctrines (756 land, 150 sea in GD5).

| Game | OJH score | Turns per minute | Players | Map | Data per turn (median) | Map frame rate (1% low) |
|---|---|---|---|---|---|---|
| Open Doctrines | **3,653** | **2,373** | 37 | 906 provinces | **10.7 KiB** | 657 fps (218) |
| Unciv 4.22.0 | 2,084 | 300 | 35 | 1,261 tiles (medium) | 79.9 KiB | **1,389 fps (255)** |
| Freeciv 3.2.6 | 1,741 (provisional) | 52.4 | 35 | 2,888 tiles (size 3) | 199 KiB | not measurable |
| Greater Diplomacy 5 | 893 | 74.0 | 35 | 906 provinces | 11.5 MiB | 104 fps (5.2) |
| FreeCol 1.2.0 | 679 (turn speed only) | 48.7 | 16 | 4,000 tiles (40 × 100) | not measured | not measured |
| FreeOrion 0.5.1.2 | 295 (turn speed only) | 6.4 | 35 | 906 star systems | not measured | not measured |

## Open Doctrines' 1939

1,298 land provinces in both games. GD5's copy also has 917 sea provinces Dragoman generated, because GD5 can only draw and sail on painted provinces.

| Game | OJH score | Turns per minute | Players | Map | Data per turn (median) | Map frame rate (1% low) |
|---|---|---|---|---|---|---|
| Open Doctrines | **2,656** | **615** | 63 | 1,298 provinces | **35.7 KiB** | 621 fps (150) |
| Unciv 4.22.0 | 1,994 | 143 | 63 | 2,791 tiles (large) | 131 KiB | **1,570 fps (249)** |
| Freeciv 3.2.6 | 1,598 (provisional) | 27.9 | 63 | 3,872 tiles (size 4) | 445 KiB | not measurable |
| FreeCol 1.2.0 | 666 (turn speed only) | 40.2 | 16 | 5,760 tiles (48 × 120) | not measured | not measured |
| Greater Diplomacy 5 | 507 | 4.1 | 63 | 2,215 provinces | 23.4 MiB | 102 fps (6.8) |
| FreeOrion 0.5.1.2 | not published | | 63 | 1,298 star systems | | |

**FreeOrion's run on this map is not published.** The Mac's lid was closed during it, and the machine slept for 66 minutes, during turns 110 to 112. Those three turns account for 67 of the run's 180 minutes, so its turns per minute measures the sleep. It will be rerun.

**This run is a stress test for Greater Diplomacy 5, not its normal case.** Open Doctrines' map keeps its 8192 × 4096 picture, about 25 times the pixels of GD5's own 1939 (1881 × 698). GD5 redraws its map layers from that picture every turn, so its turn time here mostly measures the picture size. Read GD5's numbers in the first run as its real performance.

## Machine and settings

- MacBook Pro (MacBookPro18,1): Apple M1 Pro (8 performance and 2 efficiency cores), 16 GiB, macOS 26.3, mains power, one game at a time.
- 250 turns, every player AI.
  - Turn speed is the median of 3 runs on GD5's 1939 and 1 run on Open Doctrines' 1939.
  - Network runs are 20 turns with 2 clients.
  - Each frame-rate scene is timed for 5 s after settling, with the late-game map after 20 turns.
- Install and save footprint were not rerun. See the first results.
- `tools/run-matched.sh` reproduces the whole round. `ojh report <folder>` rebuilds each report from its result files.

## How each game was put on the same world

- **Open Doctrines on GD5's 1939:**
  - `tools/matched_maps.py` converts the scenario with [Dragoman](https://github.com/Pr1nted/dragoman), and Open Doctrines plays it through its `OD_EVAL_MAP` hook.
  - GD5 starts its nations with no treasury, and Open Doctrines needs one or every nation goes bankrupt on turn 1. The script gives each nation 2.5 per province it owns, the median in Open Doctrines' own 1939. Two nations with armies but no provinces still go bankrupt and disband.
  - Open Doctrines counts 37 countries on this map where GD5 counts 35 nations.
- **Greater Diplomacy 5 on Open Doctrines' 1939:** converted with Dragoman into GD5's `base_maps/`, where it finds GD5's tech tree. Open Doctrines' non-aggression pacts and guarantees have no GD5 equivalent.
- **Freeciv and Unciv** cannot load either map, so they are sized to match:
  - **Freeciv:** `aifill` equals the player count. Map `size` 3 or 4 (thousands of tiles) gives about 870 or 1,160 land tiles at its default 30% land. Above 50 players it needs `nationset all`.
  - **Unciv:** its Gods & Kings ruleset has 34 major civilizations, so players above that are city-states (1 on GD5's 1939, 29 on Open Doctrines').
- Freeciv and Unciv count tiles, not provinces, so their region counts are not the same unit.
- **FreeOrion** has no map either game can use. It runs a galaxy with the same number of star systems as the other maps have provinces, and as many AI empires as players.
  - `drivers/freeorion/tpm.sh` starts FreeOrion's own `freeoriond` in hostless mode on a quick-start galaxy with every empire played by FreeOrion's AI clients, and times the gaps between the server's "Turn number incremented" log lines.
- **FreeCol** allows at most 8 European and 8 native nations, so it runs at its maximum of 16 players on its classic rules.
  - The map is 40 × 100 tiles (FreeCol's default) on GD5's 1939 and 48 × 120 on Open Doctrines' 1939.
  - FreeCol's headless mode needs its desktop interface and crashes, so `drivers/freecol/FreeColTpm.java` starts FreeCol's own server the way FreeCol's `main` does: seed, data folder, message bundle, European count, rules and map size. It turns on FreeCol's only-AI-turns debug mode and times turns as the game's turn number advances.
  - The driver fails the run if FreeCol's AI logs any error. Without the message bundle, native AI turns ended early with an error and turns looked several times faster.

## Read these before quoting a number

- **Who wrote it.** OJH, its score weights and the Freeciv, Unciv, FreeOrion, FreeCol and network drivers were written by the developer of Open Doctrines. Greater Diplomacy 5 is measured through a hook proposed to its developer in the PR above; until it is merged, OJH runs it from that branch.
- **Open Doctrines' network bytes are reported, not captured.** Joining an Open Doctrines match needs a signed-in account, so no client sits on the wire. The game reports the turn delta its host broadcasts, times the number of clients. Unciv's are its synced save after each turn. Freeciv's and Greater Diplomacy 5's are bytes on the wire through OJH's relay.
- **Freeciv's turn delivery (about 0.1 ms) is an artifact, not a result.**
  - Delivery is timed from a turn mark to the end of the first burst of data after it. Freeciv writes its turn log line after it has already sent most of the turn, so the burst after the mark is a small trailing packet.
  - It earns Freeciv its only "first" on delivery and counts for 4% of its score.
- **Frame rates are each game's own renderer at its own resolution.**
  - Open Doctrines and Unciv draw with OpenGL at 1600 × 900.
  - Greater Diplomacy 5 draws in software (pygame) at its fixed 1280 × 720.
  - Greater Diplomacy 5's 1% low comes from the frames drawn while a turn resolves on its background thread. Open Doctrines resolves a turn inside one frame behind a loading screen, so it has no such scene.
- **macOS slows covered windows.** A covered or unfocused window gets about one frame a second. GD5's and Unciv's frame-rate modes bring their window to the front first.
- **Freeciv's scores are provisional** only because Freeciv has no frame rate to measure. Its client redraws only when the map changes.
- **FreeOrion's and FreeCol's scores cover turn speed only** (67% of the weight). Network and frame rate would need their game clients driven, which OJH does not do yet, so their scores leave out the parts where the other games gain or lose points.
- **FreeCol's three runs on GD5's 1939 spread from 39.8 to 57.2 turns per minute** with the same seed. FreeCol's AI games do not play out identically, so the median is shown. On Open Doctrines' 1939 it ran once.
- **FreeOrion is measured as it ships, debug logging included.** The server and each AI client write tens of megabytes of logs a minute, and `--log-level` did not change that.
