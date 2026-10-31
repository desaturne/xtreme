# Auction Duel: how the bot was built and tuned

The submission is [auction_duel.cpp](auction_duel.cpp), written in C++17. It's an interactive bot that plays one side of a two-player sealed-bid auction against the judge's bots and other contestants.

**Judge score history:** 32.22 (examples only, first Python bot) → 49.92 → 51.36 → **52.88**. The current file has not been judged yet; in local self-play it beats the 52.88 version by +0.28 points per game. The target is 55 or more.

---

## 1. The problem

- There are **K** kinds of goods (2–8). Each is auctioned **M** times (1–8), with at most 60 rounds. The judge shuffles the order and neither player knows it.
- Both players start with the same **cash** (at most 10000). Cash left at the end counts in full.
- Each player has a private value for each good. The two players' values are **permutations of the same list** of distinct numbers (1–1000). You know your own values and the list, but not which good the opponent values at what.
- Each round both players bid at the same time, a whole number from 0 to their cash. The higher bid wins and ties go to a coin flip.
  - **Second-price rule:** the winner pays the loser's bid.
  - **First-price rule:** the winner pays its own bid.
  - After each round, both players see each other's bid.
- **Set bonus:** a player that wins all M copies of a good gets B% extra value on them (B is 0–200).
- **Final total** = value of goods won (including bonuses) + remaining cash.
- **Points** = 5 + 10·(your total − opponent's total)/S, clipped to the range 0–10. Here S = M·Σv, the total value of the goods before bonuses.
- Breaking a rule forfeits the game (0 points for you, 10 for the opponent). Rules: bid more than your cash, print an invalid line, crash, or go over the time or memory limit.

### Protocol (one line per message)

```
GAME K M cash B rule          # rule = first | second
VALUES v1 .. vK
ROUND n good                  -> answer "BID x", then flush
RESULT won oppBid price       # won=1 if you took it; 2nd token = OPPONENT'S BID; 3rd = price paid
GAME_END points               -> exit
```

This format was confirmed against the problem's example: `RESULT 0 20 19` means the opponent bid 20, won, and paid our 19.

---

## 2. What the judge's games look like

These facts were worked out from the judge's result tables. They matter more than anything else here.

- **Opponents:** Bots 1–3 plus Contestants 1–3, 13 games each, on the same 13 tests. Every opponent is adaptive and roughly as strong as this bot. Almost every game ends near 5 points.
- **Scale:** working back from the points, S is about 400–2600 per test.
- **Cash is tight:** both players' final totals add up to only about 1.2–1.3 × S, so most of the cash gets spent. Cash is roughly 0.15–0.6 × S.
- **Small values:** for that S, values are often under 100, so rounding bids to whole numbers matters.
- **Contestants change:** other contestants resubmit, so their columns can move by a few points between runs without any change on our side.
- **Simple test bots mislead:** opponents such as "bid your true value" let the bot score 9+ locally but only about 5 on the judge. Use self-play against the previous binary instead.

---

## 3. Version history and judge results

| Version | Main change | Judge score |
|---|---|---|
| Python v1 | Bid halfway between own value and the opponent's guessed value; opponent values guessed from the order of their bids | Examples only: 32.22 |
| Python v2–v4 | Simulate the rest of the game for each candidate bid; fit how the opponent bids; sample the opponent's value assignment | Not submitted |
| C++ port | Same strategy, many more simulations; exact search over opponent value assignments; simulations scored in clipped points | **49.92** |
| CPU-clock fix | Time budget counted in CPU time instead of wall time; 2× faster simulation loop; assumed noise in opponent bids ≥ 0.3 | **51.36** |
| Per-rule cash limits | Cash limit per bid 1.0 (first-price) / 2.0 (second-price); default opponent bid guess 0.7 | **52.88** |
| **Current file** | Separate settings per pricing rule; extra candidates near the target bid; default opponent bid guess 0.5 (second-price) and cash-scaled (first-price) | Not yet judged; +0.28 per game in self-play |

### Results breakdown for the 52.88 run

| Opponent | Score |
|---|---|
| Bot 1 | 51.83 |
| Bot 2 | 54.86 |
| Bot 3 | 56.97 |
| Contestant 1 | 58.95 |
| Contestant 2 | 51.74 |
| Contestant 3 | **42.94** (weakest matchup; loses by a small margin almost every game) |

Test 10 against Contestant 1 scored 0 points twice (241 vs 733, 259 vs 755), then 10 points in the 52.88 run. That contestant's play seems to be very high-variance. The cause was never confirmed.

---

## 4. How the current bot works

For every `ROUND`, `choose_bid` does the following:

1. **Fits an opponent model** (`fit_joint`). It tries 5 bidding styles, each multiplied by a scale `a` (48 values on a log scale from 0.12 to 4.8):
   - 0: bids its value.
   - 1: bids its value plus a share of the set bonus.
   - 2: spreads its cash over the goods still to come (`oc · w / remaining value`).
   - 3: like style 1, plus the average value (pads its bids).
   - 4: like style 1, plus *our* value for the good (a spiteful bidder that knows our values).

   For each style and scale, an exact search over all value assignments (256 subsets for K=8) finds the assignment of the opponent's values that best explains their bids, measured as |log| error. The best combination wins. The assumed noise in opponent bids is `max(P[10], err·P[7])`. Before two bids have been observed, a default model is used: `P[13]`/`P[4]` for second-price, `P[14]`/`P[5]` for first-price.
2. **Weights every possible opponent value assignment** (`perm_matrix`) by how well it explains their bids. Each simulation draws an assignment from these weights (`sample_w`).
3. **Builds about 40 candidate bids:** 0, all-in, the fallback rule's bid `hb`, multiples of the predicted opponent bid (each ±1), multiples of `hb`, fractions of cash, and multiples of the target bid `t`.
4. **Simulates the rest of the game** (`rollout`) for every candidate, sharing the same random draws across candidates (shuffled order of remaining goods, opponent noise, tie coins) so they're compared fairly:
   - The opponent bids according to the fitted model with noise.
   - Our own future bids follow the fallback rule `heur_r`: the target t = ve·(1−P[0]) + we·P[0], where ve and we include a share of any reachable set bonus. In second-price games it bids t. In first-price games it bids the predicted opponent bid + 1 if that's ≤ t, and otherwise a low bid. Every bid is limited by a cash cap.
   - Each simulated game is scored in **clipped points** (5 + 10·d/S, clipped to 0–10), not raw lead. This was worth +0.5 to +0.7 against strong opponents.
5. **Picks the candidate with the best total score.** Ties go to the candidate closest to `hb`.

**Time:** `BUDGET = 0.6` s of **CPU time** per game, split evenly across the remaining rounds, with a hard stop at 1.25 × BUDGET. The judge reports about 600 ms per game.

**Safety guards:** clamp K to 8 or less; skip rounds for a good that has already had all its copies; replace NaN or negative opponent bids with 0; catch any exception and bid 0; clamp every bid to the range 0–cash.

### Settings (`PF` = first-price, `PS` = second-price; both currently equal)

| Index | Meaning | Value |
|---|---|---|
| 0 | Spite weight on the opponent's value when computing the target t | 0.5 |
| 1 | Cash cap multiplier, first-price | 1.0 |
| 2 | Share of a not-yet-reachable set bonus counted in the value | 0.6 |
| 3 | First-price fallback bid fraction | 0.5 |
| 4 | Default opponent bid scale `a`, second-price | **0.5** |
| 5 | Default opponent bid scale `a`, first-price | **0.5** |
| 6 | Default noise in opponent bids | 0.35 |
| 7 | Noise = fit error × this | 0.8 |
| 8, 9 | Experimental fallback tweaks | 0 (off) |
| 10 | Minimum noise in opponent bids | 0.3 |
| 11 | 1 = score simulations in clipped points | 1 |
| 12 | Cash cap multiplier, second-price | 2.0 |
| 13 | Default opponent bidding style, second-price | 1 (value + bonus) |
| 14 | Default opponent bidding style, first-price | 2 (cash-scaled) |

**Test-only environment overrides** (the judge never sets them): `AD_BUDGET`, `AD_P`, `AD_PF`, `AD_PS`, `AD_NFAM`, `AD_TC`, `AD_JOINT`, `AD_HALVE`, `AD_ORACLE`, `AD_DBG`.

---

## 5. Findings: what worked and what didn't

**Worked:**
- **Clipped-points objective** in the simulations: +0.5 to +0.7 against strong opponents.
- **CPU-time clock:** the old wall clock counted time spent waiting for the opponent, which starved the bot of thinking time. +0.45 in self-play, and the biggest judge gain (+1.4).
- **Faster simulation loop:** about 2× (xorshift generator, shared random draws, running sums of remaining value).
- **Minimum noise of 0.3:** +0.2. It makes the bot robust when its opponent model is wrong.
- **Per-rule cash caps** (first-price 1.0, second-price 2.0): +0.22 overall, and +0.43 on second-price games.
- **Lower default opponent bid guess** (second-price 0.5, first-price cash-scaled 0.5): +0.2 to +0.3 in judge-like games.
- **Extra candidates near the target bid:** +0.07.
- **Exact search over opponent value assignments** instead of guessing them from bid order: a small but consistent gain.

**Didn't work or not worth it:**
- **Dropping weak candidates early** (successive halving): worse, because noise threw away good candidates.
- **More time** once the clock was fair: 0.3 → 0.6 s gained nothing.
- **Making our simulated future bids use the opponent model:** no gain.
- **Knowing the opponent's true values** (a test-only cheat): only +0.45. Guessing their values isn't the bottleneck.
- **About 30 single-setting tweaks** (spite weight, bonus share, noise multiplier, first-price fallback, …): all within ±0.08, which is noise.
- **Opponents that bid to make us overpay** (trap bots): not catastrophic (6.4–7.4), so they don't explain the test 10 zero.

**Gotchas:**
- Python `round(0.5)` is 0, but C++ `llround(0.5)` is 1. Use `nearbyint` to match Python.
- Occasional "bot died" failures during local testing were SIGKILLs from the test machine under load (a process was killed at round 1). They weren't bot bugs: a sanitizer build ran thousands of games clean, and a debug build that prints a stack trace on any crash never printed one.

---

## 6. Local test setup

The test scripts are in the session scratchpad, `/tmp/claude-1000/-home-sush-other-xtreme/<session>/scratchpad/sim/`. **That folder is temporary**, so copy it somewhere permanent if you want to keep it.

| File | Purpose |
|---|---|
| `sim.py` | Game engine plus test opponents (truth, truthB, shade8, spite, paced, paced2, rand, trap, trap2, allin, v1, v4). `rand_game` gives wide random games; `rand_game_real` gives judge-like games (enable with `SB_GEN=real`). |
| `sbench.py N` | **Main tool.** Self-play of `../ad` (takes `AD_*` overrides) against `../ad_base` (clean environment, plus `BASE_ENV`). Both seats per deal, 20 games in parallel. Prints the average with its standard error, split by first and second price. |
| `pbench.py N opps` | `../ad` against the listed simple opponents, in parallel. |
| `sweep*.sh` | Change one setting at a time and run `sbench` for each. |
| `crash.py`, `hunt.py`, `rep.py` | Hunt for crashes (use with the `ad_asan` and `ad_dbg` builds). |
| `timing.py` | CPU time on the largest game (K=8, M=7). |

Typical loop:

```
g++ -O2 -std=c++17 -o ad auction_duel.cpp; cp ad ad_base
SB_GEN=real AD_P=<15 comma-separated values> python3 sbench.py 2000
```

A change is worth keeping if it improves by about 2 standard errors (about +0.07 at N=2000). On the judge, +0.4 per game in self-play has turned into roughly +1.5 on the 100-point scale.

---

## 7. Next steps

1. **Submit the current file** and check whether the jump is as big as the self-play gain suggests.
2. **Contestant 3 (42.94):** build a stronger, different-style self-play opponent, for example one that bids exactly halfway between the two values and guesses our values from our bids, and tune against it.
3. **Get the parameters of test 10** (K, M, cash, B, rule) if the platform shows them, to rebuild the Contestant 1 blow-up locally.
4. **Ideas not yet tried:**
   - A more careful first-price bid rule for our simulated future bids (bid shading against the predicted spread of opponent bids).
   - An opponent model in which bids react to our previous bids.
   - Exact search for the last 1–3 rounds.
