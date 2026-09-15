# Open Doctrines: OJH score

**3,653 points**, OJH score version 2

![OJH score 3,653](score-opendoctrines.svg)

This score is built from this game's own result files and nothing else. Every part is measured against a fixed reference level, not against other games, so adding, removing or re-running another game never changes it.

## Notes

- scored on 96% of the weight; not reported: Turn delivery

## Parts

| Part | Measured by | Weight | Measured | On the reference CPU | Worth 1,000 points | Points |
|---|---|---|---|---|---|---|
| Turn throughput | ojh tpm | 22% | 87,795 player-turns/min | 51,103 player-turns/min | 2,000 player-turns/min | 4,731 |
| World throughput | ojh tpm | 13% | 2,149,791 region-turns/min | 1,251,334 region-turns/min | 200,000 region-turns/min | 2,859 |
| Late-game pace | ojh tpm | 8% | 1.07 | 1.00 (capped) | 0.50 | 1,585 |
| Steadiness | ojh tpm | 5% | 0.88 | same | 0.50 | 1,464 |
| Start-up | ojh tpm | 5% | 0.188 s | 0.323 s | 10.0 s | 4,998 |
| CPU per player-turn | ojh tpm | 6% | 0.0007 s | 0.0012 s | 0.0100 s | 3,256 |
| Memory | ojh tpm | 8% | 83.7 MiB | same | 2.00 GiB | 4,670 |
| Frame rate | ojh fps | 12% | 657.0 fps | same | 60 fps | 3,579 |
| Smoothness | ojh fps | 8% | 218.1 fps | same | 30 fps | 3,048 |
| Data per turn | ojh net | 9% | 12.1 KiB | same | 256 KiB | 4,473 |
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

- **Turn speed**: Open Doctrines' own OJH turn lines (OD_OJH=1): processTurn timed turn by turn inside the headless eval, 250 turns
- **Frame rate**: Open Doctrines' own --ojh-fps mode: its real window with vsync and the frame cap off, every frame timed for 5.00 s per scene after two seconds of settling, on the <ojh>/results/matched-20260915/maps/gd5-1939.odmap world; the late-game map comes after 20 turns
- **Network**: Open Doctrines' headless eval with OD_OJH_NET: after every turn it packs the same turn delta its multiplayer host broadcasts (Game::mpResolveTurn, SaveManager::packTurn) and reports its size times 2 clients. Joining needs a signed-in account, so no client is on the wire: the bytes are the host's real payload, turn delivery is n/a, and the few bytes of each client's orders are not counted
- **Turn speed settings**: 250 turns asked for, 250 timed, seed 20260914, players set by the game
- **Machine**: Apple M1 Pro, macOS 26.3 (25D125)
- **CPU reference score**: 1,718 rounds/s on one core; CPU-bound figures were put on OJH's reference CPU (1,000 rounds/s) with a factor of 0.582

## How the score is built

- **Points**: each part scores 1000 × log2(1 + value ÷ reference level): the reference level is worth 1,000 points, three times it 2,000 and seven times it 3,000, and nothing scores below zero. Where less is better, the ratio is turned around.
- **Hardware**: turn speed, start-up, CPU time and turn delivery are put on OJH's reference CPU with the machine's single-core reference score (src/machine.c), so a faster computer does not make a faster game. A game that uses more cores keeps that advantage. Frame rate, memory and data are used as measured.
- **Total**: the weighted mean of the parts the results have. Coverage is how much of the weight that was, here 96%; a part a game was not measured for is left out, never counted as zero.
- **Version**: score version 2. Its parts, weights and reference levels are fixed in src/score.c; any change makes a new version, and scores of different versions are not compared.

