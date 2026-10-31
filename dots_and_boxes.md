# Dots and Boxes — bot development notes

Interactive judge problem (IEEEXtreme style). These notes cover everything from the chat: the problem, how the bot works, how it was tested, the judge results, and what was tried and dropped.

**Files**
- [dots_boxes.c](dots_boxes.c) — **the version to submit** (C, ~600 lines). Compile with `gcc -O2 dots_boxes.c`.
- [dots_boxes.cpp](dots_boxes.cpp) — the earlier C++ version, kept for reference. It is weaker and still has a milder form of the memo bug (see "Bugs found").
- [dots_boxes_tools/](dots_boxes_tools/) — local judge simulator, test opponents and comparison scripts.

---

## 1. Problem summary

- Board of R×C boxes (1 ≤ R, C ≤ 10), drawn as (R+1)×(C+1) dots. The game starts from a position with some lines already drawn. Boxes that start with all four sides drawn are out of play. No box starts with exactly 3 sides drawn.
- Players alternately draw one line. Completing a box scores it and the same player moves again, including after the last line of a chain.
- **Input:** `GAME R C first`, then 2R+1 rows of the position (`+` dots, `-`/`|` drawn lines, `.` undrawn, `#` complete box, `o` box in play). Then `YOUR_TURN`, `OPPONENT r1 c1 r2 c2`, and finally `GAME_END points`.
- **Output:** one `LINE r1 c1 r2 c2` per `YOUR_TURN`, flushed.
- **Scoring per game:** `5 + 10·(mine − theirs)/N`, clamped to [0, 10], where N is the number of boxes in play. Winning by N/2 or more earns the full 10. Breaking a rule (illegal line, crash, time or memory limit) gives 0.
- **Limits:** **2000 ms per game** (CPU), **256 MB** memory.
- Opponents: three judge bots (Bot 1–3) and three other contestants' submissions.

---

## 2. How the bot works

**State.** Edges are indexed horizontal-then-vertical. Each edge stores its 1–2 adjacent boxes, each box stores its 4 edges, and a side count is kept per box. Positions are Zobrist-hashed.

**Search.** Negamax alpha-beta with principal-variation (null-window) search, iterative deepening and a transposition table (2^21 entries, 32 MB). The value is the net future boxes for the player to move, which depends only on the edge set, so the table key is just the edge hash. A capturing move adds its boxes and keeps the turn; any other move negates the value and passes the turn. Search depth only decreases on turn-passing moves.

**Move reductions (Berlekamp / Barker–Korf):**
- If a capturable box can be taken without giving the opponent a new capturable box, that capture is the only move considered.
- Otherwise, at the end of a chain the only choices are: capture, or leave the last 2 boxes (last 4 of a loop) for the opponent so they must open the next chain ("double-dealing").
- Safe phase (moves that create no 3-sided box exist): safe moves plus sacrifices of short chains (length ≤ 2). Long-chain and loop sacrifices are added only when ≤ 32 lines remain.
- No safe moves: one opening move per chain (both ends if a chain touches a junction; the middle line for 2-chains, so the opponent can't double-deal) and one per loop.

**Endgame (exact).** When no safe moves remain and the board is pure chains and loops, the value is computed exactly:
- opening a chain of length L ≤ 2: `−L − f(rest)`
- opening a chain of length L ≥ 3: `min(−L − f(rest), 4 − L + f(rest))`
- opening a loop of length L: `min(−L − f(rest), 8 − L + f(rest))`

f is maximised over which component to open, with a memo keyed by a Zobrist hash of the multiset. The exact calculation is used up to 4000 sub-states. Above that, a few natural opening orders are tried (short chains first, then loops and long chains in different orders).

**Midgame evaluation.** At a search leaf in the safe phase, 2 fixed random-order playouts of safe moves are run until none remain. The resulting chains are valued with the exact calculation, and the sign depends on the parity of the playout (who is forced to open first). This estimate is noisy and is the main weakness left (see §6).

**Time management (current).** Budget 1.55 s per game. Each move gets `remaining / (safe moves/2 + 3)`, capped at 0.6 s and at 40% of the remaining time. Forced moves return immediately, and search stops early once the value is proven exact.

**Key parameters** (command-line overrides, in order: `TOTAL PERMOVE NPLAY FULLSAC_N HLIM CRITMUL CRITN`):

| Parameter | Value | Meaning |
|---|---|---|
| TOTAL | 1.55 | think budget per game (s) |
| PERMOVE | 0.6 | per-move cap (s) |
| NPLAY | 2 | playouts per midgame estimate |
| FULLSAC_N | 32 | allow every sacrifice when ≤ this many lines remain |
| HLIM | 4000 | exact endgame state limit |
| TTBITS | 21 | transposition table size (2^21) |

Peak memory is about 43 MB.

---

## 3. Testing setup ([dots_boxes_tools/](dots_boxes_tools/))

- `judge.py A B games seed` — plays two programs on random boards. Board sizes are 1–10 (half the games 5–10), starting line densities are random, first move alternates. It prints per-game scores and timing, and logs moves slower than 0.3 s to stderr as `SLOW`.
- `opp.cpp` — baseline opponents: `./opp 0` random, `./opp 1` greedy (takes every box, random safe move, opens the smallest chain).
- `perfect.cpp` — brute-force perfect player, usable up to about 27 undrawn lines.
- `cmp.py` — bot vs perfect, compared with perfect vs perfect on the same small positions.
- `small.py seed n BOT ORACLE` — the same comparison on 14–25-box boards against a near-perfect reference (`./bot 120 8 2 200`, i.e. 8 s per move with every sacrifice considered).
- `fixed.py BOT` — three known positions where an early version lost boxes.
- `tiny.py` — robustness check on tiny and dense boards.
- `par2.sh "A" "B" jobs offset` — runs 10 games per job in parallel and averages.
- `tee.sh` — wraps a bot and logs its input, for replaying a game.

Self-play noise: about ±0.3 points over 200 games, so differences under ~0.3 are not meaningful. Self-play also can't reveal blind spots the opponent copy shares.

---

## 4. Results

**Local testing (final C version):**

| Check | Result |
|---|---|
| vs version that scored 40.58 on the judge | 5.73 avg over 200 games (5.0 = even) |
| vs greedy bot | 8.16 |
| vs random bot | 9.79 |
| small boards vs perfect player (≤ 22 lines) | identical to perfect, 160 positions |
| 14–25-box boards vs near-perfect reference | −0.32 points/game |
| worst game time under 20-way parallel load | 1.56 s |
| crashes / illegal moves | none |

**Real judge — the submission before the latest changes** (old time budget, peak 878 ms CPU, 32 MB): **40.58 / 100**.

| Opponent | Score |
|---|---|
| Bot 1 | 42.83 |
| Bot 2 | 51.07 |
| Bot 3 | 43.43 |
| Contestant 1 | 38.87 |
| Contestant 2 | 34.06 |
| Contestant 3 | 33.23 |

Notes from that report:
- There were 13 tests per opponent: tests 1–7 are small (8–24 boxes), tests 8–13 are 34–99 boxes.
- The heaviest losses are on the 51–99-box boards against the contestants (for example 13–38 and 24–70). The side that wins the fight over who opens the first long chain takes most of the board.
- Small-board results vary by opponent, so the bot was not playing those positions perfectly.

The latest version (time-budget and allocation changes) had **not yet been submitted** when these notes were written. The goal is at least 50%.

---

## 5. History: bugs found and changes made

1. **First C++ bot.** It hung on an empty 10×10 board: the exact endgame calculation blew up with many distinct chain lengths. Fixed by capping exact solving at 4000 sub-states, with the opening-order approximation above that.
2. **Small positions lost 1–2 boxes** because long-chain sacrifices were never considered while safe moves remained. Allowing them everywhere made big boards weaker, so they are only allowed when ≤ 32 lines remain.
3. **Timing.** The clock check was too infrequent; it now runs every 16 nodes.
4. **Memo cap regression (C++).** Capping the memo at 200k entries stopped memoization once full, and the calculation went exponential. Fixed in the C++ version by clearing the memo instead.
5. **Port to C** (Zig was not installed). No heap allocation in search, cheaper short-chain detection, integer-keyed memo, midgame estimates cached in the transposition table. About 1.4× faster; beat the C++ version 5.2–5.4.
6. **Memo eviction storm (C).** In the shared memo table, entries evicted each other mid-calculation: one 2,880-state calculation made 50 million calls, and single moves took up to 3 s. Fixed with a per-calculation linear-probing table that never evicts, plus a shared table only for final results.
7. **After the judge report:** budget raised for the 2 s limit (1.55 s total, 0.6 s per move), transposition table doubled, and time split by safe moves left instead of undrawn lines. Previously small boards used only about 0.4 s of the budget.

---

## 6. Tried and dropped (measured)

| Idea | Result | Why dropped |
|---|---|---|
| Long-chain sacrifices everywhere in the safe phase | 4.3–4.7 vs baseline | extra branching, noisy evaluation overrates them |
| Long sacrifices only when few safe moves / enough depth | 4.59–4.71 | same |
| Mark positions "inexact" when long sacrifices are skipped | 4.56 | correct in principle, but it stopped reusing those values across depths and cost too much search |
| Stop iterative deepening early to save time | 4.89 | later moves are capped anyway, so the saved time is wasted |
| MCTS (random-playout tree search) for the early safe phase | 4.77–4.94 | no better than alpha-beta in any setting |
| 1, 3, 4 or 6 playouts per estimate | ≤ 5.0 | 2 is best |
| More playouts ≈ smoother estimate, 4× time (old) | 4× time beat 1× by 5.65 | time helps, but is limited by the 2 s rule |

---

## 7. Where the remaining strength is

- **Big boards (51–99 boxes):** the midgame estimate (2 random playouts, sign by parity) is too noisy to steer the long-chain fight early. That's where the contestants win. A better estimate of the eventual number of long chains is the most promising direction. Under the long chain rule, the player to move becomes the controller when (remaining lines − remaining boxes + eventual long chains) is odd.
- **Small boards:** about 0.3 points per game is still lost against a near-perfect reference. Faster exact search would recover it.
- **Opponent modelling:** an opponent that never leaves the last boxes of a chain behind could be exploited. The estimated gain is a few tenths against greedy-type bots; not implemented.
- **Next step:** submit [dots_boxes.c](dots_boxes.c) and compare the new per-test judge results with the table in §4.
