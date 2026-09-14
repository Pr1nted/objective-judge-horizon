# Putting your own game through OJH

OJH measures any turn-based game that can play itself: every player controlled by the
game's AI, no human waiting. You describe the game in a small JSON file, a *game spec*.
OJH needs no changes and no plugin. Your results get the same report and the same kind
of independent score as the games OJH ships with.

## In five steps

1. Give your game a way to play itself from the command line for a number of turns. A
   headless or server build is best, because drawing frames is not part of a turn.
2. Write a starter spec:

   ```bash
   ojh spec new my-game.json
   ```

3. Fill it in (the reference is below), then check what OJH will run:

   ```bash
   ojh spec check my-game.json
   ```

4. Measure:

   ```bash
   ojh tpm my-game.json --turns 200 --out results/my-game/my-game.json
   ```

5. Build the report and the scorecard:

   ```bash
   ojh report results/my-game
   ```

That writes `report.md`, `report.txt`, and `score-my-game.md`, `.txt` and `.svg`. To
compare against other games, put their result files in the same folder. The score is
the exception: it never depends on what else is in the folder.

Two working examples come with OJH. `examples/games/sample-protocol.json` and
`examples/games/sample-marker.json` drive a stand-in game built into OJH, and you can run
them straight away:

```bash
build/ojh tpm examples/games/sample-protocol.json --out results/sample/sample-protocol.json
```

## How OJH knows a turn has ended

Choose one.

### The protocol: your game prints OJH's lines

This is the most exact choice when you can change the game. Print these lines to stdout
or stderr, and flush after each one:

| Line | When |
|---|---|
| `OJH ready` | start-up is over and turn 1 is about to begin |
| `OJH turn N` | turn N has ended |
| `OJH turn N 0.184` | turn N has ended, and the game timed it itself at 0.184 seconds |
| `OJH players N` | how many players the game has (any time) |
| `OJH regions N kind` | how big the map is, e.g. `OJH regions 906 provinces` (any time) |

A turn's time is the gap since the previous `OJH ready` or `OJH turn` line. When a turn
line carries seconds, OJH uses the game's own figure instead, which keeps the time spent
printing or saving between turns out of the turn. A logger's prefix is fine:
`[12:00:01] INFO: OJH turn 3` is read. `OJH` must start the line or follow a space, tab,
`]` or `:`.

```json
"turns": {"from": "protocol"}
```

### A marker: text your game already prints

Use this for a game you cannot change. Pick text that appears exactly once per turn, at
the end of the turn. OJH times the gaps between those lines.

```json
"turns": {
  "from": "marker",
  "turn_ends": "End of turn",
  "game_starts": "Game started",
  "stream": "stderr",
  "players_after": "players: ",
  "regions_after": "map tiles: "
}
```

| Key | Meaning |
|---|---|
| `turn_ends` | required: a line containing this text ends a turn |
| `game_starts` | optional: the first line containing this ends start-up. Without it, the first `turn_ends` line starts the clock and is not counted as a turn |
| `stream` | `stdout`, `stderr` or `either` (the default) |
| `players_after`, `regions_after` | optional: the number right after this text, on any line, is the player count or map size. These also work with the protocol |

## The spec, key by key

| Key | Required | Meaning |
|---|---|---|
| `ojh_game_spec` | yes | `1`, the spec version |
| `id` | yes | lower-case letters, digits, `-` and `_`; names the result and scorecard files. The ids of the games OJH ships with are taken |
| `name` | yes | the game's name as reports show it |
| `version`, `license`, `homepage` | no | shown next to the name and on the scorecard |
| `notes` | no | anything for people reading the spec; OJH ignores it |
| `command` | yes | the program, then its arguments, as a list |
| `working_directory` | no | where the game runs; relative paths start at the spec's folder, which is also the default |
| `environment` | no | extra environment variables, e.g. `{"LC_ALL": "C"}` |
| `turns` | yes | how OJH knows a turn has ended (above) |
| `players`, `regions`, `region_kind` | no | fixed counts for a game that does not print them |
| `default_turns` | no | turns when `--turns` is not given (100) |
| `timeout_seconds` | no | OJH stops the game and everything it started after this long (3600) |

Unknown keys are refused, so a typo is caught instead of silently ignored.

### Placeholders

OJH fills these in wherever they appear in `command`, `environment` and
`working_directory`:

| Placeholder | Value |
|---|---|
| `{turns}` | `--turns`, or `default_turns` |
| `{seed}` | `--seed` (20260914 by default) |
| `{players}` | `--players` (8 by default). Using it tells the report that OJH chose the player count |
| `{work}` | a scratch folder for saves and logs |
| `{spec_dir}` | the folder the spec is in |
| `{ojh}` | the OJH executable itself |

A program written as `./MyGameServer` or `bin/MyGameServer` is found next to the spec. A
bare name such as `python3` or `java` is found on `PATH`.

## A game in Python, Java or an engine

`command` can start anything. A Python game might use:

```json
"command": ["python3", "{spec_dir}/tools/autoplay.py", "--turns", "{turns}", "--seed", "{seed}"]
```

A small driver script next to your game is a good home for the `OJH turn` lines when the
game itself should not print them. OJH's own drivers for Greater Diplomacy 5
(`drivers/gd5_tpm.py`) and Unciv (`drivers/unciv/UncivTpm.java`) are examples.

## Your score

Every result gets an OJH score built from that result file alone, against fixed reference
levels. Adding or removing other games never moves it, so you can publish your score
without publishing anyone else's. A score is *provisional* when fewer than 100 turns were
timed, when the run did not finish cleanly, or when the machine's CPU reference score is
missing. Measure at least 100 turns (200 is better) before you publish. The scorecard
lists every part, its weight, and what was measured, so a reader can check the
arithmetic. Score version 1 is built from turn speed; FPS, network and memory join as
parts when OJH measures them.

## Fair comparisons

- Measure every game on the same machine, plugged in, with nothing else running.
- Use the same number of turns. Turns slow down as a game fills up, so a shorter run
  looks faster.
- Give games a similar size (players, map) where they allow it. The report shows TPM ×
  players and TPM × regions to account for size, but not for rules.
- Say which version you measured. OJH writes `version` into the result.
