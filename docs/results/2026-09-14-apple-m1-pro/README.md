# Results: four strategy games on an Apple M1 Pro (2026-09-14)

**[Read the report](report.md)**. It ranks the games by OJH score, compares every statistic from best to worst and draws a graph for each. The other files here are what the report was built from.

| Game | OJH score | Score covers |
|---|---|---|
| Open Doctrines | 2,599 | 96% of the weight |
| Unciv 4.22.0 | 1,566 | 76% |
| Freeciv | 1,435 (provisional) | 80% |
| Greater Diplomacy 5 | 1,039 | 100% |

## Machine

MacBook Pro (MacBookPro18,1) with an Apple M1 Pro (8 performance and 2 efficiency cores, 16 GiB). It ran macOS 26.3 on mains power. Every game ran on this one machine, one at a time.

## Files

- `report.md` / `report.txt` are the full report. `graphs/` holds one SVG per statistic.
- `score-<game>.md` / `.txt` / `.svg` are each game's scorecard: every part, its weight, its reference level and its points.
- `<game>-tpm.json`, `-footprint.json`, `-net.json` and `-fps.json` are the raw results. `ojh report docs/results/2026-09-14-apple-m1-pro` rebuilds the report from them.

Local folders in the results are shortened to `<open-doctrines>`, `<greater-diplomacy-5>` and `<ojh>`.

## Read these before quoting a number

- **Who wrote it.** OJH, its score weights and every game's driver were written by the developer of Open Doctrines. Open Doctrines has small `OJH` hooks for printing turn times, its network payload and frame-rate scenes. Every driver is in `drivers/` and every hook is described in each result's `how` field. Corrections from any game's developers are welcome.
- **The worlds differ.** Each game runs its own map:
  - Open Doctrines: 37 players and 827 provinces.
  - Greater Diplomacy 5: 35 nations and 906 regions (the 1939 scenario).
  - Freeciv and Unciv: 8 players each.

  Turns per minute is shown, but the score uses player-turns and region-turns per minute.
- **Open Doctrines' network bytes are reported, not captured.** Joining an Open Doctrines match needs a signed-in account, so no client sat on the wire. The game reports the size of the turn delta its host broadcasts, times the number of clients. Turn delivery is not measured for it.
- **Unciv has no frame rate.** Its desktop window stalled in both attempts: one scene drew 1 fps and a rerun drew nothing in 15 minutes. So those runs are not used.
- **Freeciv has no frame rate either.** Its client redraws only when the map changes, so there is no steady frame rate to time. That missing run is the only reason its score is marked provisional.
- **Greater Diplomacy 5's frame rate is software drawing.** pygame ran with SDL's dummy video driver, which is its own drawing cost with no display in the way. It is not what a GPU-backed window would show.
- **Reruns.** The first full run had four failures, all caused by OJH's own drivers and hooks, not by the games. They were fixed and rerun:
  - Open Doctrines' load time and frame rate, because the save was passed by path.
  - Open Doctrines' late-game map scene, because the scene timer started before the late-game turns ran.
  - Greater Diplomacy 5's network match. The host driver crashed once its nation was eliminated. After that fix, the driver had seated all three human players on the three alphabetically-first nations, and all were eliminated by turn 18. GD5 then waits forever, because there is no human left to end the turn. The driver now seats players on the largest nations.
