# Objective Judge Horizon (OJH)

A reproducible benchmark of open-source and source-available turn-based strategy
games, measured the same way on the same machine:

| Game | Version | License | Why it is here |
|---|---|---|---|
| Open Doctrines | tree under test | see its repository | the game this suite was written for |
| Greater Diplomacy 5 | git HEAD | GPL-3.0 | the closest match: grand strategy on a world map, real-time multiplayer |
| Freeciv | 3.2.6 | GPL-2.0 | the reference open-source 4X, with a client/server netcode |
| Unciv | 4.22.0 | MPL-2.0 | Civilization V remake, multiplayer by synced saves |
| FreeOrion | 0.5.1.2 | GPL-2.0 | turn-based space 4X, client/server with AI clients |
| FreeCol | 1.2.0 | GPL-2.0 | Colonization remake, server with AI players |

Nothing from any game is copied into this repository. Each game runs as its own
program, installed from its own release, and is driven from the outside.

## What is measured

Every number carries the machine it was measured on, the game version, the map,
the number of players and the settings. A number without those is not reported.

### TPM: turns per minute

How many complete game turns the game processes in one minute, with every
country or player controlled by the game's own AI and no human waiting.

- **Run**: a fixed map and player count, a fixed seed where the game has one, all
  players AI, no rendering where the game can run without it. The clock runs from
  the first turn starting to the last turn ending; loading is timed separately.
- **Reported**: TPM over the whole run, and per-turn time at the start, middle and
  end, because turns get slower as a game fills up.
- **Normalised**: TPM × players, and TPM × map regions (provinces, tiles or
  systems), so a game with a larger world is not punished for its size. Both the
  raw and the normalised figure are always shown together.

### FPS under load

Frames per second on screens that exist, in some form, in every one of these games:

| Scene | What is on screen |
|---|---|
| `menu` | main menu, idle |
| `map-start` | the world map at the start of a game, default zoom, idle |
| `map-out` | the same, zoomed all the way out |
| `map-in` | the same, zoomed all the way in |
| `map-pan` | the map scrolling continuously |
| `panel` | the heaviest information screen (economy, diplomacy or research list) |
| `map-late` | the world map in a late-game save, idle |
| `end-turn` | the map while a turn is being processed |

- **Reported**: average FPS, 1% low FPS, and frame time p50/p95/p99, measured over a
  fixed window after the scene has settled, with frame caps and vsync off where the
  game allows it (and marked where it does not).
- A scene a game does not have is shown as *n/a*, never as zero.

### NIPM: network information per minute

How much game information the netcode moves per minute of play, and how fast it
can move it, with the internet taken out of the measurement.

- **Internet-agnostic**: server and clients run on this machine and talk through a
  counting relay on the loopback interface (`src/netmeter.c`). The relay sees
  every byte in both directions; there is no latency, loss or bandwidth limit
  other than the machine itself.
- **Reported per game**: bytes per minute and reads per minute in each direction
  (client→server and server→client), per client, the busiest second, and the
  largest single read.
- **Hardware-aware**: every run records the machine profile and a fixed CPU
  reference score (`src/machine.c`), and the report gives NIPM both raw and per
  reference-score unit, so a faster machine does not look like a better netcode.
- Games without live netcode (Unciv synchronises save files) are reported for what
  they do, not as a per-minute stream.

### DPT: data per turn

How many bytes it takes to make one turn, in bytes, reported as the **lowest** and
the **highest** turn of the run, with the median between them.

- **Measured**: the game driver marks the end of every turn on the relay, and the
  bytes between two marks are that turn's data, client→server, server→client and
  both ways together. The turns that set the lowest and the highest are named.
- **Why both ceilings**: the lowest turn is what a quiet turn costs (the floor a
  connection must carry at all times); the highest is the worst turn (what a
  connection must survive, usually a war or a crowded late game).
- Bytes after the last mark belong to no turn and are reported separately, never
  folded into a turn.

### OJH score: one number per game

Every game gets its own score, built from its own result file and nothing else. Each
part is measured against a fixed reference level written into `src/score.c`, never
against the other games, so adding, removing or re-running a game cannot move anyone
else's score.

| Part (score version 1) | Weight | Worth 1,000 points |
|---|---|---|
| Turn throughput: TPM × players | 40% | 2,000 player-turns/min |
| World throughput: TPM × map regions | 25% | 200,000 region-turns/min |
| Late-game pace: early ÷ late median turn time | 15% | 0.5 |
| Steadiness: median ÷ 95th-percentile turn time | 10% | 0.5 |
| Start-up: seconds to the first turn | 10% | 10 s |

- **Points**: each part scores 1000 × log2(1 + value ÷ reference). The reference is
  worth 1,000, three times it 2,000, seven times it 3,000, and nothing scores below zero.
- **Hardware**: speeds are first put on OJH's reference CPU with the machine's
  single-core reference score, so a faster computer does not make a faster game.
- **Total**: the weighted mean of the parts the game reports. A part a game does not
  report is left out and named on the scorecard, never counted as zero.
- **Provisional**: fewer than 100 turns timed, a run that did not finish cleanly, or no
  CPU reference score.
- **Versions**: any change to a part, weight or reference level makes a new score
  version, and scores of different versions are not compared. FPS, NIPM, DPT and memory
  join as parts when they are measured.

Each scorecard (`score-<game>.md`, `.txt`) shows every part, with a `.svg` badge.

### Everything else a turn-based strategy player feels

Cold start to main menu, loading a new game, save and load time, save file size,
peak memory, CPU use while idle on the map and during a turn, install size, map
scale (regions and players), and AI turn time per player.

## Your own game

Any developer can put their game through OJH without changing OJH. Describe how to start
the game's AI-only mode, and how OJH tells when a turn ends, in a JSON game spec. Either
the game prints `OJH ready` and `OJH turn N`, or OJH watches for a line the game already
prints once per turn.

```bash
ojh spec new my-game.json
```

```bash
ojh tpm my-game.json --turns 200 --out results/my-game/my-game.json
```

The full guide is [docs/adding-your-game.md](docs/adding-your-game.md), and there are
working examples in `examples/games/`.

## Fairness rules

1. Same machine, same power settings, same display, nothing else running.
2. Every game at its latest stable release, installed from its official source.
3. Each scene and run repeated, and the median reported with its spread.
4. Settings that change the result (frame caps, vsync, AI difficulty, map size)
   are written into the report next to the number.
5. Where games are not directly comparable, the report says so in words.

## Building

OJH is C11 with no dependencies beyond the operating system, and builds on Linux,
macOS and Windows (MSVC or MinGW). Platform code lives in `src/platform.c`; the rest
is plain C. The games it drives run on whatever they need: GD5 on Python, Unciv and
FreeCol on Java.

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Release
```

```bash
ctest --test-dir build -C Release --output-on-failure
```

The Windows build can also be cross-compiled with MinGW-w64, and its tests run
under Wine:

```bash
cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
```

CI builds and tests all three on every push (`.github/workflows/ci.yml`).

## Running

```bash
build/ojh machine
```

prints this machine's profile and CPU reference score.

```bash
build/ojh tpm freeciv --turns 200 --out results/run/freeciv.json
```

measures one game's turns per minute (`opendoctrines`, `gd5`, `freeciv`, `unciv`, or a
game spec file), and

```bash
build/ojh report results/run
```

writes `report.md`, `report.txt` and every game's scorecard from the result files in
that folder. `build/ojh score results/run/freeciv.json` scores a single result on its
own.

```bash
build/ojh relay 37015 127.0.0.1 27015 600 4
```

relays a game server on port 27015 through port 37015 for ten minutes with four
clients, and prints NIPM and DPT. Game runs write `results/<date>-<machine>/results.json`
(every raw sample), `report.md` and `report.txt`.
