# Greater Diplomacy 5: OJH score

**500 points**, OJH score version 2, provisional

![OJH score 500](score-gd5.svg)

This score is built from this game's own result files and nothing else. Every part is measured against a fixed reference level, not against other games, so adding, removing or re-running another game never changes it.

## Why it is provisional

- the Turn speed run did not finish cleanly, so its figures may be partial
- the Network run did not finish cleanly, so its figures may be partial
- scored on 87% of the weight; not reported: Data per turn, Turn delivery

## Parts

| Part | Measured by | Weight | Measured | On the reference CPU | Worth 1,000 points | Points |
|---|---|---|---|---|---|---|
| Turn throughput | ojh tpm | 22% | 225.1 player-turns/min | 129.0 player-turns/min | 2,000 player-turns/min | 90 |
| World throughput | ojh tpm | 13% | 7,916 region-turns/min | 4,536 region-turns/min | 200,000 region-turns/min | 32 |
| Late-game pace | ojh tpm | 8% | 1.07 | 1.00 (capped) | 0.50 | 1,585 |
| Steadiness | ojh tpm | 5% | 0.62 | same | 0.50 | 1,162 |
| Start-up | ojh tpm | 5% | 17.3 s | 30.2 s | 10.0 s | 413 |
| CPU per player-turn | ojh tpm | 6% | 0.262 s | 0.456 s | 0.0100 s | 31 |
| Memory | ojh tpm | 8% | 4.33 GiB | same | 2.00 GiB | 547 |
| Frame rate | ojh fps | 12% | 87.85 fps | same | 60 fps | 1,301 |
| Smoothness | ojh fps | 8% | 0.990 fps | same | 30 fps | 47 |
| Data per turn | ojh net | 9% | not measured | n/a | 256 KiB | n/a |
| Turn delivery | ojh net | 4% | not measured | n/a | 0.100 s | n/a |

### What each part measures

- **Turn throughput**: turns per minute times players: how many player-turns the game resolves in a minute.
- **World throughput**: turns per minute times map regions: how much map the game resolves in a minute.
- **Late-game pace**: median early turn time divided by median late turn time: 1 means turns never slow down.
- **Steadiness**: median turn time divided by the 95th percentile: 1 means no turn is slower than usual.
- **Start-up**: seconds from launching the game to its first turn starting. Less is better.
- **CPU per player-turn**: processor time one AI player's turn costs, summed over every core. Less is better.
- **Memory**: the most memory the game held while its turns ran. Less is better.
- **Frame rate**: median of the average frame rate over the map scenes.
- **Smoothness**: the frame rate of the slowest 1% of frames in the game's worst scene.
- **Data per turn**: the most bytes one turn took over the network, both ways. Less is better.
- **Turn delivery**: median time from a turn ending to its last byte reaching the clients. Less is better.

## The runs

- **Turn speed**: Greater Diplomacy 5's own ojh_benchmark.py: every turn through turn_manager, AI preparation, resolution and the map refresh, every nation AI, model diplomacy skipped (213 turns)
- **Frame rate**: Greater Diplomacy 5's own ojh_benchmark.py fps mode: the real game window brought to the front, no frame cap, every frame of each scene timed for 5.00 s after settling, on /Users/vladyavdoshenko/CLionProjects/bench-competitors/gd5-ojh-pr/base_maps/OJH_OD_1939; pygame draws in software; the late-game map comes after 20 turns
- **Network**: GD5's own real-time server (RealtimeServer and MapRealtimeDriver) hosting the 1939 scenario, with 2 real RealtimeClient guests joining through OJH's loopback relay and submitting every turn; turns end when the server opens the next one
- **Turn speed settings**: 250 turns asked for, 213 timed, seed 20260914, players set by the game
- **Machine**: Apple M1 Pro, macOS 26.3 (25D125)
- **CPU reference score**: 1,745 rounds/s on one core; CPU-bound figures were put on OJH's reference CPU (1,000 rounds/s) with a factor of 0.573

## How the score is built

- **Points**: each part scores 1000 × log2(1 + value ÷ reference level): the reference level is worth 1,000 points, three times it 2,000 and seven times it 3,000, and nothing scores below zero. Where less is better, the ratio is turned around.
- **Hardware**: turn speed, start-up, CPU time and turn delivery are put on OJH's reference CPU with the machine's single-core reference score (src/machine.c), so a faster computer does not make a faster game. A game that uses more cores keeps that advantage. Frame rate, memory and data are used as measured.
- **Total**: the weighted mean of the parts the results have. Coverage is how much of the weight that was, here 87%; a part a game was not measured for is left out, never counted as zero.
- **Version**: score version 2. Its parts, weights and reference levels are fixed in src/score.c; any change makes a new version, and scores of different versions are not compared.

