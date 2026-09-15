# Results: matched maps and player counts, Apple M1 Pro (2026-09-15)

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

## Open Doctrines' 1939

1,298 land provinces in both games. GD5's copy also has 917 sea provinces Dragoman generated, because GD5 can only draw and sail on painted provinces.

| Game | OJH score | Turns per minute | Players | Map | Data per turn (median) | Map frame rate (1% low) |
|---|---|---|---|---|---|---|
| Open Doctrines | **2,656** | **615** | 63 | 1,298 provinces | **35.7 KiB** | 621 fps (150) |
| Unciv 4.22.0 | 1,994 | 143 | 63 | 2,791 tiles (large) | 131 KiB | **1,570 fps (249)** |
| Freeciv 3.2.6 | 1,598 (provisional) | 27.9 | 63 | 3,872 tiles (size 4) | 445 KiB | not measurable |
| Greater Diplomacy 5 | 507 | 4.1 | 63 | 2,215 provinces | 23.4 MiB | 102 fps (6.8) |

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

## Read these before quoting a number

- **Who wrote it.** OJH, its score weights and the Freeciv, Unciv and network drivers were written by the developer of Open Doctrines. Greater Diplomacy 5 is measured through a hook proposed to its developer in the PR above; until it is merged, OJH runs it from that branch.
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
