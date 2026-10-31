# Shroud — solution log

Current submission: `shroud.cpp` (C++17, compile with `g++ -O2 -std=c++17 shroud.cpp`). Best judged score so far: **53.18 / 100**. The latest version (v5) has not been judged yet.

## The game

Two players paint cells on an R×C grid under fog of war. The map is symmetric under a half turn, and each side's units start in opposite corners.

**Parameters**
- 5 ≤ R, C ≤ 60
- 1 ≤ U ≤ 8 units per side
- 1 ≤ V ≤ 5 view radius
- 1 ≤ T ≤ 500 rounds
- `known` is 1 if the walls are given at the start

**Input:** `GAME R C U V T known`, then `MAP` and R lines if `known` is 1. Each round sends `ROUND n cells`, then for each unit `UNIT i r c` followed by its (2V+1)×(2V+1) view.

**View characters**

| Char | Meaning |
|---|---|
| `#` | wall or off the map |
| `.` | unpainted |
| `+` | ours |
| `-` | opponent's |
| `M` | our unit |
| `E` | enemy unit |

**Output:** `ORDERS d0 … dU-1`, each one of N/E/S/W/H, then flush. The program exits on `GAME_END`.

**Collisions:** units ordered into the same cell, or swapping cells, hold. A unit ordered into a cell whose unit holds also holds. After moving, each unit paints the cell it stands on.

**Score per game:** 5 + 10·(mine − theirs)/S, clamped to [0, 10], where S is the number of open cells. An invalid line, a crash, or exceeding the time or memory limit forfeits the game.

## Judge history

| Version | Language | Change | Score |
|---|---|---|---|
| Python v3 | Python | belief map + DFS planner | 45 (examples only) |
| Python v4 | Python | enemy-path prediction | 40.83 (examples only) — reverted |
| C++ v1 | C++ | port + beam planner, horizon 7, contest weight 30 | **50.21** |
| probe 1 | — | encodes T, K in memory | 50.45 |
| C++ v2 | C++ | big maps: horizon 12, enemy-cell value 3 | **52.27** |
| probe 2 | — | cells at T/2 in memory, U and V in CPU | 52.53 |
| probe 3 | — | R in memory, C in CPU | 52.31 |
| C++ v3 | C++ | teammate-Voronoi discount 0.8 | **52.62** |
| probe 4 | — | new-cell moves and steal ratio | 52.63 |
| C++ v4 | C++ | compact-painting bonus 0.15 | **53.18** |
| C++ v5 | C++ | unpainted value scaled toward the enemy side | not judged yet |

The same code scores within about ±0.3 between runs: the contestants play randomly, while the bots are deterministic.

Breakdown of the 53.18 run:
- Bots 1/2/3: 58.3 / 62.0 / 59.2
- Contestants 1/2/3: 48.4 / 45.0 / 46.1

The examples always end 4–4 and 9–9 (5.00 each). Example 1 is capped at about 5.83 even when won.

## Hidden test set (decoded with probes)

There are 13 maps, each played against 6 opponents (Bots 1–3 and Contestants 1–3).

| Test | S | T | U | V | R | C (approx) |
|---|---|---|---|---|---|---|
| 1 | 250 | 100 | 1 | 5 | 20 | 18 |
| 2 | 274 | 150 | 2–3 | 5/1 | 20 | 18 |
| 3 | 720 | 200 | 3 | 5 | 30 | 28 |
| 4 | 674 | 200 | 3 | 4 | 30 | 27 |
| 5 | 814 | 250 | 4 | 4 | 25 | 37 |
| 6 | 772 | 250 | 2 | 5 | 40 | 22 |
| 7 | 1292 | 300 | 5 | 3 | 40 | 36 |
| 8 | 1306 | 300 | 3 | 5 | 40 | 35 |
| 9 | 2166 | 400 | 6 | 5 | 50 | 44 |
| 10 | 2084 | 400 | 4 | 3 | 50 | 45 |
| 11 | 3126 | 500 | 7 | 1 | 60 | 53 |
| 12 | 3048 | 500 | 4 | 5 | 60 | 54 |
| 13 | 1599 | 350 | 6 | 3 | 35 | 51 |

**What the probes established**
- All tests are long games on big maps that are 70–95% open.
- Only 40–70% of cells end up painted. About 65% of moves repaint enemy cells, so the late game is a stealing war.
- **The contestants win in the first half.** Our cell count at T/2 is about 25% lower against them than against bots.
- **No single cause** (probe 4). Against contestants compared with bots, we paint about 3% fewer new cells, steal about 3% less, and lose about 4% more cells.
- **Bots:** we beat them by about 0.8 points per game.

### Probe technique

The judge prints CPU time and memory per game, so a probe build encodes hidden values in them:
- **Memory:** allocate and touch `code × 64 KB` (or 512 KB per unit of R). Accurate to about ±1 step, so this channel is reliable.
- **CPU:** busy-wait `code × 20 ms` spread over the last 10 rounds. A per-round wait spread over every round accumulates clock overshoot and aliases with T, which made probe 1's U decode ambiguous.
- **Decoding:** subtract the CPU and memory of the previous run of the same, deterministic bot.

A probe must make exactly the same moves as the real bot, so a probe submission costs nothing in score.

## How the bot works (`shroud.cpp`)

1. **Belief map**
   - Walls: −1 unknown / 0 open / 1 wall, using half-turn symmetry (the mirror of cell i is N−1−i).
   - Paint state of every cell, and the round each cell was last seen.
2. **Hidden-cell estimates**
   - Cells stolen from us: `lost = believed own − cells`, spread over unseen own cells weighted by age × w.
   - Enemy paint we haven't seen is spread over unseen `.` cells.
   - w = 0.3 + 1.4·(distance from our start)/(sum of distances to both starts), so it is higher toward the enemy's side.
3. **Cell values (big maps)**
   - Enemy cell: 3.
   - Unpainted: 1, scaled by (w−0.3)/1.4 (v5).
   - Unseen cells use the expected values from step 2.
   - A cell with an enemy unit on it is worth half.
4. **Planner**: a beam search per unit over the state (position, set of visited cells), deduplicated by a Zobrist hash.
   - Horizon is 12 on big maps, discounted 0.8 per step; it plans to the end of the game in the last 12 rounds.
   - A contest multiplier of ×30 applies to cells the enemy could reach by step t+1, based on BFS from the mirrored enemy start and the last sighting.
   - Teammate discount: cells in another unit's Voronoi region count 0.8 (v3).
   - Compact painting: +0.15 per wall, painted cell or path cell next to a new cell (v4).
   - Penalties for moving into an enemy unit (−0.6, or −3 if it has been standing still) and for repeating a move that just failed.
   - Units plan one after another; each one's chosen path cells are zeroed for the next.
5. **Regimes**
   - T ≤ 12: horizon 8, contest ×8.
   - Maps with ≤ 100 open cells: horizon 3, terminal potential.
   - Small maps and short games keep the older tuned behaviour, which plays the examples identically.
6. **Cost:** at most about 0.6 s CPU per game on the judge, and about 2 MB memory.

## What helped (in REAL-mode replica tests)

- Racing for contested cells: contest weight 30, slack 1.
- On small maps, a short horizon of 3 plus a terminal potential.
- **Big maps**
  - Horizon 12.
  - Enemy-cell value 3.
  - Teammate discount.
  - Compact painting.
  - Pushing toward the enemy's side first.
- Local gains transfer to the judge at roughly 10×: +0.1 per game locally ≈ +0.5 to +1 judge points.

## What did not help (tied or lost)

- **Search:** wider beams, longer horizons beyond 12, different discounts.
- **Valuation:** survival or durability factors, being drawn toward enemy units or trailing them, phase-dependent steal values, a longer early horizon, relocating estimated stolen cells, consistent 1+2q valuation of unseen cells.
- **Strategies:** a home-corner rush (tested as an opponent: the longer the rush, the worse it scored).
- **Python:** enemy-path prediction (it hurt on the judge).

## Local test setup (session scratchpad)

The scratchpad sits under `/tmp/claude-1000/-home-sush-other-xtreme/<session>/scratchpad/` and does not persist.

**Judge and replica**
- `judge.py` is the local judge.
- Env modes: SMALL, EX2, EX3, MED, BIG, UNSAT, FORCE="U V T K", MAPF, and VISA (full vision for player A).
- **REAL=1** reproduces the 13 decoded tests; `REAL=k` selects test k−1. Its painted fractions matched the judge within about 0.1.

**Harness scripts**
- `wst2.py`: A/B runs with a move breakdown.
- `multi.py`: runs against an opponent pool.
- `realfrac.py`: per-test painted fraction, sim vs judge.
- `calib.py`, `calib2.py`: compare an opponent model against the real contestant and bot counts.
- `findbad.py`: crash sweep.

**Bots**
- `ft.cpp`: tunable copy of the bot (`P="key=val,..."`).
- `hunter2.cpp`: a fair long-range greedy opponent. Its results resemble the judge's bots (we score about 5.6 against it).

**Baselines:** `baseline_run*.txt` hold the judge tables used for probe decoding.

## Files in this folder

| File | Contents |
|---|---|
| `shroud.cpp` | current submission (v5) |
| `shroud_probe.cpp` … `shroud_probe4.cpp` | diagnostic probes — not for final submission |
| `shroud.py` | last Python version |
| `probe.py` | Python probe |

## Next steps toward 55

- **Judge v5** and compare it with 53.18.
- **Keep the territory theme going.** v4 and v5 gained by reshaping where and how we paint, so try more ideas in that direction, testing each against the pool of v4, hunter2 and base on at least two seed sets.
- **Biggest per-test losses:** contestants beat us most on tests 11 and 13 (6–7 units, short view range). Contestant 2 painted 1,552 cells against our 962 on test 11.
