# Results: matched maps and player counts, Apple M1 Pro (2026-11-03)

The [September round](../2026-09-15-matched-maps-apple-m1-pro/README.md) on newer versions of every game, and the first round where what each game's turn figure *includes* was measured rather than assumed. That measurement is the most useful thing here, and it is in [What a turn includes](#what-a-turn-includes) below. Read it before quoting a ratio.

- **[GD5's 1939](gd5-1939/report.md):** Greater Diplomacy 5's own `scenarios/historical/1939`, converted for Open Doctrines, with Freeciv and Unciv at 35 players.
- **[Open Doctrines' 1939](od-1939/report.md):** Open Doctrines' own `1939.odmap`, converted for Greater Diplomacy 5, with Freeciv and Unciv at 63 players.

## GD5's 1939

906 provinces in both Greater Diplomacy 5 and Open Doctrines.

| Game | OJH score | Turns per minute | Players | Map | Data per turn (median) | Map frame rate (1% low) |
|---|---|---|---|---|---|---|
| Open Doctrines | **3,835** | **2,915** | 37 | 906 provinces | **10.8 KiB** | 1,420 fps (82.8) |
| Unciv 4.22.16 | 2,127 | 243 | 35 | 1,261 tiles (medium) | 74.0 KiB | **2,212 fps (261)** |
| Freeciv 3.2.6 | 1,738 (provisional) | 52.8 | 35 | 2,888 tiles (size 3) | 199 KiB | not measurable |
| Greater Diplomacy 5 | 1,007 (provisional) | 85.5 | 35 | 906 provinces | not measured | 181 fps (4.7) |
| FreeCol 1.2.0 | 726 (turn speed only) | 52.9 | 16 | 4,000 tiles (40 × 100) | not measured | not measured |
| FreeOrion 0.5.1.2 | 310 (turn speed only) | 6.71 | 35 | 906 star systems | not measured | not measured |

## Open Doctrines' 1939

1,298 land provinces in both games. GD5's copy also has sea provinces Dragoman generated, because GD5 can only draw and sail on painted provinces.

| Game | OJH score | Turns per minute | Players | Map | Data per turn (median) | Map frame rate (1% low) |
|---|---|---|---|---|---|---|
| Open Doctrines | **2,903** | **669** | 63 | 1,298 provinces | **35.5 KiB** | 967 fps (171) |
| Unciv 4.22.16 | 2,099 | 131 | 63 | 2,791 tiles (large) | 129 KiB | **3,215 fps (430)** |
| Freeciv 3.2.6 | 1,563 (provisional) | 29.5 | 63 | 3,872 tiles (size 4) | 445 KiB | not measurable |
| FreeCol 1.2.0 | 644 (turn speed only) | 37.5 | 16 | 5,760 tiles (48 × 120) | not measured | not measured |
| Greater Diplomacy 5 | 500 (provisional) | 3.57 | 63 | 2,215 provinces | not measured | 87.8 fps (1.0) |
| FreeOrion 0.5.1.2 | 247 (provisional) | 3.13 | 63 | 1,298 star systems | not measured | not measured |

**Three of the six runs on this map did not finish.** Greater Diplomacy 5 stopped at turn 213 of 250 and FreeOrion at turn 229, both on the one-hour limit; Freeciv has no frame rate to measure. Their turns per minute are what they managed in the time, which is a fair reading of how long they take, but their scores are computed from a partial run. **This map is a stress test for Greater Diplomacy 5, not its normal case** — see the September round for why, and read its numbers in the first table as its real performance.

## What a turn includes

Every game here is timed through its own turn resolution, and none of them draws the map inside the figure. That sounds like one rule, but it was never checked, and in September it was not true: Greater Diplomacy 5's hook timed its seven map refresh passes inside the turn, and nothing else's did.

Measured, rather than argued about:

| | measured | effect |
|---|---|---|
| GD5's map refresh, as a share of its turn | 9.5% (12 turns) | its turns per minute read about 10% low |
| Open Doctrines' map repaint, on top of its turn | 2.69 ms on 21.2 ms, **12.7%** | its turns per minute read about 13% high |

So the two games' columns were tilted in opposite directions, by about a tenth each, and the net was roughly a fifth of the ratio between them: 34.1× as published, 26.6× with both corrected. A fix is written and **not yet offered to Greater Diplomacy 5**: the refresh still runs, because the next turn must start from the state a player would see, but it is timed separately and reported on its own line. Until it is merged, the hook in GD5 (added in [pull request #46](https://github.com/GitGetGot415/Greater-Diplomacy-5/pull/46)) still counts the refresh, and the numbers in the tables above were measured with it, on `f53efce6`.

### What Open Doctrines' figure leaves out

Open Doctrines is timed inside its headless eval, which skips work a played turn does. The same build playing the same map with a renderer present (`--simulate`) takes **34.0 ms** a turn against the eval's **20.3 ms**, a gap of 13.7 ms. Timing each skipped block directly, over 250 turns:

| | ms per turn | share of the gap |
|---|---|---|
| the save write (`SaveManager::appendTurn`) | 8.468 | 61.8% |
| the map repaint and its loading screen | 4.184 | 30.5% |
| province pixel index | 0.025 | 0.2% |
| turn order log | 0.014 | 0.1% |
| achievement snapshot | 0.001 | 0.0% |
| audio pump and loading frames (16 calls) | 0.001 | 0.0% |
| **unaccounted** | 1.007 | 7.4% |

It is a save and a repaint, and nothing else worth naming. Neither is unique to Open Doctrines — no game here has its per-turn save counted either, and Unciv's synced save is its network figure rather than its turn figure — so excluding them is consistent. It is stated because "2,915 turns a minute" is not what a player's turn costs, and the gap is a factor of 1.7, not a rounding error.

## What did not replicate

September's round is not comparable with this one as absolutes — different versions, a machine four quarters of drift away — but two things were checked across the two maps within this run.

- **Open Doctrines' turns per minute on the larger map, 669, is 11% above the 604 it managed in October.** Its figure on the smaller map is 41% above October's. The second number does not replicate the first, so the 41% is machine conditions, not a speed gain.
- **A frame-rate reading on the smaller map did not survive the larger one.** Open Doctrines' late-game map scene read 82.75 fps at the 1% low on GD5's 1939 — the worst of its seven scenes by a factor of two, against a 582 fps average. On the larger, busier map the same scene is its *fastest*: 1,112 fps average, 303 at the 1% low. Each scene is one unrepeated five-second sample, so the first reading was a bad window, not a late-game defect. It is recorded here because it looked exactly like a finding.

## Machine and settings

- MacBook Pro (MacBookPro18,1): Apple M1 Pro (8 performance and 2 efficiency cores), 16 GiB, macOS 26.3, mains power, one game at a time.
- 250 turns, every player AI, seed 20260914. One run per game per map.
- Each frame-rate scene is timed for 5 s after settling, with the late-game map after 20 turns. Network runs are 20 turns with 2 clients.
- Commits: Open Doctrines `97f79a7`, Greater Diplomacy 5 `f53efce`, OJH `4f8b965`. Freeciv and Unciv from their releases above, FreeCol 1.2.0, FreeOrion 0.5.1.2.
- `tools/run-matched.sh` reproduces the round. `ojh report <folder>` rebuilds each report from its result files.
- Install and save footprint were not rerun. See the [first results](../2026-09-14-apple-m1-pro/README.md).

## Read these before quoting a number

- **Who wrote it.** OJH, its score weights, and the Freeciv, Unciv, FreeOrion, FreeCol and network drivers were written by the developer of Open Doctrines, and Open Doctrines comes first in both tables. Greater Diplomacy 5 is measured through a hook offered to its developer; the other four are driven from outside. Weigh the tables accordingly, and prefer the per-statistic tables in each report to the single score.
- **Greater Diplomacy 5's network leg did not run on either map** — its server never reported ready — so it has no data-per-turn figure this round and keeps a "first" on busiest second for having sent nothing. It had figures in September; this is a measurement failure, not a result.
- **Scores are not comparable across rounds or between the two tables.** A score counts firsts and lasts among the games measured for each statistic, so the field changes it.
- **FreeCol runs at 16 players** — its maximum — where the others run at 35 or 63. Its row is a different experiment under the same heading.
- **Region counts are not one unit.** Provinces, tiles and star systems are counted side by side: 906 to 5,760 across one table. Region-turns per minute in the reports normalises for size; turns per minute does not.
- **Frame rates are each game's own renderer at its own resolution**, and frame-rate absolutes on a loaded machine are worth less than the ratios within one run. Open Doctrines and Unciv draw with OpenGL at 1600 × 900; Greater Diplomacy 5 draws in software (pygame) at 1280 × 720.
- **Freeciv's scores are provisional** only because its client redraws when the map changes and so has no steady frame rate. That will not be fixed by better instrumentation.
- **FreeOrion is measured as it ships, debug logging included.** It and its AI clients write tens of megabytes of logs a minute.
