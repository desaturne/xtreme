import sys


def main():
    inp = sys.stdin
    out = sys.stdout

    K = M = cash = B = 0
    rule = "second"
    vals = []
    all_vals = []
    opp_cash = 0
    seen = []        # copies of each good auctioned so far
    my_cnt = []      # copies I won
    op_cnt = []      # copies opponent won
    op_bids = []     # opponent bids observed per good
    cur_good = -1
    my_last_bid = 0

    def est_opp_values():
        # Rank-match opponent's average bids to the known value list.
        sv = sorted(all_vals)
        obs = [(sum(op_bids[g]) / len(op_bids[g]), g) for g in range(K) if op_bids[g]]
        w = [sum(all_vals) / K] * K
        n = len(obs)
        if n:
            obs.sort()
            for r, (_, g) in enumerate(obs):
                idx = int(round((r + 0.5) / n * K - 0.5))
                idx = max(0, min(K - 1, idx))
                w[g] = (sv[idx] * n + (sum(all_vals) / K) * 1.0) / (n + 1.0)
        return w

    def eff(v, own, s):
        # value of this copy including (part of) the set bonus, if still reachable
        if own != s:
            return float(v)
        remaining = M - s
        bonus = v * B / 100.0 * M
        if remaining <= 1:
            return v + bonus
        return v + bonus / remaining * 0.6

    while True:
        line = inp.readline()
        if not line:
            break
        t = line.split()
        if not t:
            continue
        cmd = t[0]
        if cmd == "GAME":
            K, M, cash, B = int(t[1]), int(t[2]), int(t[3]), int(t[4])
            rule = t[5]
            opp_cash = cash
            seen = [0] * K
            my_cnt = [0] * K
            op_cnt = [0] * K
            op_bids = [[] for _ in range(K)]
        elif cmd == "VALUES":
            vals = [int(x) for x in t[1:1 + K]]
            all_vals = vals[:]
        elif cmd == "ROUND":
            g = int(t[2]) - 1
            cur_good = g
            w = est_opp_values()
            v_eff = eff(vals[g], my_cnt[g], seen[g])
            w_eff = eff(w[g], op_cnt[g], seen[g])
            target = (v_eff + w_eff) / 2.0

            # budget pacing over my remaining value
            rem = 0.0
            for i in range(K):
                rem += vals[i] * (M - seen[i])
            if rem > 0:
                cap = cash * 2.5 * v_eff / (rem + v_eff * 1.5 * B / 100.0)
                cap = max(cap, cash * vals[g] / rem)
            else:
                cap = cash

            if rule == "first":
                if op_bids[g]:
                    pred = max(op_bids[g][-1], sum(op_bids[g]) / len(op_bids[g]))
                else:
                    pred = target * 0.7
                pred = min(pred, opp_cash)
                if pred + 1 <= target:
                    bid = pred + 1
                else:
                    bid = min(target * 0.5, v_eff * 0.5)
            else:
                bid = target

            bid = min(bid, cap)
            bid = int(max(0, min(cash, bid)))
            my_last_bid = bid
            out.write("BID %d\n" % bid)
            out.flush()
        elif cmd == "RESULT":
            won = int(t[1])
            ob = int(t[2])
            price = int(t[3])
            g = cur_good
            if 0 <= g < K:
                seen[g] += 1
                op_bids[g].append(ob)
                if won == 1:
                    my_cnt[g] += 1
                    cash -= price
                else:
                    op_cnt[g] += 1
                    opp_cash -= price
            if cash < 0:
                cash = 0
            if opp_cash < 0:
                opp_cash = 0
        elif cmd == "GAME_END":
            break


main()
