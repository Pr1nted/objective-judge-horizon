# Unciv: OJH score

**2,099 points**, OJH score version 2

![OJH score 2,099](score-unciv.svg)

This score is built from this game's own result files and nothing else. Every part is measured against a fixed reference level, not against other games, so adding, removing or re-running another game never changes it.

## Notes

- scored on 96% of the weight; not reported: Turn delivery

## Parts

| Part | Measured by | Weight | Measured | On the reference CPU | Worth 1,000 points | Points |
|---|---|---|---|---|---|---|
| Turn throughput | ojh tpm | 22% | 8,274 player-turns/min | 4,614 player-turns/min | 2,000 player-turns/min | 1,725 |
| World throughput | ojh tpm | 13% | 366,537 region-turns/min | 204,392 region-turns/min | 200,000 region-turns/min | 1,016 |
| Late-game pace | ojh tpm | 8% | 0.33 | same | 0.50 | 740 |
| Steadiness | ojh tpm | 5% | 0.49 | same | 0.50 | 978 |
| Start-up | ojh tpm | 5% | 0.893 s | 1.601 s | 10.0 s | 2,857 |
| CPU per player-turn | ojh tpm | 6% | 0.0136 s | 0.0245 s | 0.0100 s | 495 |
| Memory | ojh tpm | 8% | 1.58 GiB | same | 2.00 GiB | 1,177 |
| Frame rate | ojh fps | 12% | 3,215 fps | same | 60 fps | 5,770 |
| Smoothness | ojh fps | 8% | 429.8 fps | same | 30 fps | 3,938 |
| Data per turn | ojh net | 9% | 165 KiB | same | 256 KiB | 1,350 |
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

- **Turn speed**: OJH's Unciv driver times each GameInfo.nextTurn call (250 turns)
- **Frame rate**: OJH's Unciv driver opens Unciv's own desktop window (LWJGL3, vsync off, no frame cap), starts a game and times every rendered frame of each scene for 5.00 s after a warm-up; the late-game map comes after 20 turns
- **Network**: Unciv has no live connection: its multiplayer uploads the whole game after a turn and the other player downloads it. OJH's Unciv driver plays 20 turns with 63 civilizations and reports the size of that compressed game after each one, counted once up and once down; no relay, so turn delivery is n/a
- **Turn speed settings**: 250 turns asked for, 250 timed, seed 20260914, 63 players chosen by OJH
- **Machine**: Apple M1 Pro, macOS 26.3 (25D125)
- **CPU reference score**: 1,793 rounds/s on one core; CPU-bound figures were put on OJH's reference CPU (1,000 rounds/s) with a factor of 0.558

## How the score is built

- **Points**: each part scores 1000 × log2(1 + value ÷ reference level): the reference level is worth 1,000 points, three times it 2,000 and seven times it 3,000, and nothing scores below zero. Where less is better, the ratio is turned around.
- **Hardware**: turn speed, start-up, CPU time and turn delivery are put on OJH's reference CPU with the machine's single-core reference score (src/machine.c), so a faster computer does not make a faster game. A game that uses more cores keeps that advantage. Frame rate, memory and data are used as measured.
- **Total**: the weighted mean of the parts the results have. Coverage is how much of the weight that was, here 96%; a part a game was not measured for is left out, never counted as zero.
- **Version**: score version 2. Its parts, weights and reference levels are fixed in src/score.c; any change makes a new version, and scores of different versions are not compared.

