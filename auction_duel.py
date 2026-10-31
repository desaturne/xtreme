import sys, random, math, time

class Bot:
    BUDGET = 0.5      # seconds of thinking per game (soft)
    def start(s, K, M, cash, B, rule, vals):
        s.K, s.M, s.cash0, s.cash, s.B, s.rule, s.vals = K, M, cash, cash, B, rule, vals
        s.bf = B / 100.0
        s.sv = sorted(vals); s.mean = sum(vals) / K
        s.opp_cash = cash
        s.seen = [0] * K; s.my_cnt = [0] * K; s.op_cnt = [0] * K
        s.op_bids = [[] for _ in range(K)]
        s.obs = []   # (g, opp_cash, op_cnt[g], seen[g], remaining tuple, bid)
        s.cur = 0
        s.t0 = time.time()
        s.rng = random.Random(12345)
        s.total_rounds = K * M

    # ---------- opponent values (rank match) ----------
    def est_w(s):
        K = s.K
        obs = sorted((sum(b) / len(b), g) for g, b in enumerate(s.op_bids) if b)
        w = [s.mean] * K
        n = len(obs)
        if n == 0:
            return w
        # assign observed goods to evenly spread ranks; unobserved get mean of unused
        used = set()
        for r, (_, g) in enumerate(obs):
            idx = max(0, min(K - 1, int(round((r + 0.5) / n * K - 0.5))))
            w[g] = (s.sv[idx] * n + s.mean) / (n + 1)
        return w

    # ---------- opponent model ----------
    def base_feats(s, w, g, oc, own, se, rem):
        tb = w[g]
        if own == se:
            r = s.M - se
            tb = w[g] + w[g] * s.bf * s.M / r
        remv = sum(w[i] * rem[i] for i in range(s.K))
        p = oc * w[g] / remv if remv > 0 else oc
        return (w[g], tb, p, tb + s.mean)

    def fit_model(s, w):
        if s.rule == 'second':
            default = (1, 1.0, 0.35)
        else:
            default = (1, 0.6, 0.45)
        if len(s.obs) < 2:
            return default
        best = None
        for fam in range(4):
            ratios = []
            rows = []
            for (g, oc, own, se, rem, b) in s.obs:
                base = s.base_feats(w, g, oc, own, se, rem)[fam]
                base = min(base, oc) if fam != 2 else base
                if base > 0:
                    rows.append((base, b, oc))
                    if b < oc:
                        ratios.append(b / base)
            if len(rows) < 2 or not ratios:
                continue
            ratios.sort()
            a = ratios[len(ratios) // 2]
            err = 0.0
            for base, b, oc in rows:
                pred = min(oc, a * base)
                err += abs(math.log((b + 1.0) / (pred + 1.0)))
            err /= len(rows)
            if best is None or err < best[3]:
                best = (fam, a, err)
                best = (fam, a, max(0.08, err * 1.25), err)
        if best is None:
            return default
        return best[:3]

    def perm_matrix(s, w, model):
        fam, a, sig = model
        K = s.K
        C = [[0.0] * K for _ in range(K)]
        for (g, oc, own, se, rem, b) in s.obs:
            remv_o = sum(w[i] * rem[i] for i in range(K)) - w[g] * rem[g]
            for j in range(K):
                v = s.sv[j]
                if fam == 0:
                    base = v
                elif fam == 1:
                    base = v + (v * s.bf * s.M / (s.M - se) if own == se else 0.0)
                elif fam == 2:
                    remv = remv_o + v * rem[g]
                    base = oc * v / remv if remv > 0 else oc
                else:
                    base = v + (v * s.bf * s.M / (s.M - se) if own == se else 0.0) + s.mean
                pred = min(oc, a * base)
                C[g][j] -= abs(math.log((b + 1.0) / (pred + 1.0))) / sig
        return C

    def sample_w(s, C, rng):
        K = s.K
        goods = list(range(K)); rng.shuffle(goods)
        unused = list(range(K))
        w = [0.0] * K
        for g in goods:
            row = C[g]
            mx = max(row[j] for j in unused)
            ws = [math.exp(row[j] - mx) for j in unused]
            r = rng.random() * sum(ws)
            k = 0
            while k < len(ws) - 1 and r > ws[k]:
                r -= ws[k]; k += 1
            j = unused.pop(k)
            w[g] = s.sv[j]
        return w

    # ---------- my heuristic ----------
    def eff(s, v, own, se):
        if own != se:
            return float(v)
        rem = s.M - se
        bonus = v * s.bf * s.M
        return v + bonus if rem <= 1 else v + bonus / rem * 0.6

    def heur(s, g, cash, mycnt, opcnt, seen, rem, w, pred):
        ve = s.eff(s.vals[g], mycnt[g], seen[g])
        we = s.eff(w[g], opcnt[g], seen[g])
        t = (ve + we) / 2.0
        remv = 0.0
        for i in range(s.K):
            remv += s.vals[i] * rem[i]
        cap = max(cash * 2.5 * ve / (remv + ve * 1.5 * s.bf), cash * s.vals[g] / remv) if remv > 0 else cash
        if s.rule == 'first':
            bid = pred + 1 if pred + 1 <= t else min(t * 0.5, ve * 0.5)
        else:
            bid = t
        b = min(cash, bid, cap)
        return int(b) if b > 0 else 0

    # ---------- rollout ----------
    def rollout(s, g0, mybid, w, model, rng, order):
        fam, a, sig = model
        K, M, bf, rule = s.K, s.M, s.bf, s.rule
        cash = s.cash; oc = s.opp_cash
        mycnt = s.my_cnt[:]; opcnt = s.op_cnt[:]; seen = s.seen[:]
        rem = [M - x for x in seen]
        vals = s.vals
        first = True
        seq = [g0] + order
        for g in seq:
            # opponent bid
            own = opcnt[g]; se = seen[g]
            if fam == 0:
                base = w[g]
            elif fam == 1:
                base = w[g] + (w[g] * bf * M / (M - se) if own == se else 0.0)
            elif fam == 2:
                remv = 0.0
                for i in range(K):
                    remv += w[i] * rem[i]
                base = oc * w[g] / remv if remv > 0 else oc
            else:
                base = w[g] + (w[g] * bf * M / (M - se) if own == se else 0.0) + s.mean
            mean = a * base
            ob = mean * math.exp(rng.gauss(0, sig))
            if ob > oc: ob = oc
            ob = int(ob)
            if first:
                x = mybid; first = False
            else:
                x = s.heur(g, cash, mycnt, opcnt, seen, rem, w, min(mean, oc))
            if x > ob or (x == ob and rng.random() < 0.5):
                p = ob if rule == 'second' else x
                cash -= p; mycnt[g] += 1
            else:
                p = x if rule == 'second' else ob
                oc -= p; opcnt[g] += 1
            seen[g] += 1; rem[g] -= 1
        d = cash - oc
        for i in range(K):
            d += vals[i] * mycnt[i] * (1 + bf if mycnt[i] == M else 1)
            d -= w[i] * opcnt[i] * (1 + bf if opcnt[i] == M else 1)
        return d

    def bid(s, n, g):
        s.cur = g
        w = s.est_w()
        model = s.fit_model(w)
        rem = [s.M - x for x in s.seen]
        fam, a, sig = model
        base = s.base_feats(w, g, s.opp_cash, s.op_cnt[g], s.seen[g], rem)[fam]
        pred = min(s.opp_cash, a * base)
        hb = s.heur(g, s.cash, s.my_cnt, s.op_cnt, s.seen, rem, w, pred)
        # candidates
        c = set([0, hb, s.cash])
        for f in (0.8, 0.9, 0.97, 1.0, 1.03, 1.1, 1.25):
            c.add(int(pred * f)); c.add(int(pred * f) + 1)
        for f in (0.5, 0.75, 1.25, 1.5):
            c.add(int(hb * f))
        cands = sorted(x for x in c if 0 <= x <= s.cash)
        # time budget
        rounds_left = s.total_rounds - n + 1
        elapsed = time.time() - s.t0
        tb = max(0.002, (s.BUDGET - elapsed) / max(1, rounds_left))
        rest = []
        for i in range(s.K):
            rest += [i] * rem[i]
        rest.remove(g)
        if not rest and s.rule == 'second' and False:
            pass
        scores = {x: 0.0 for x in cands}
        cnt = 0
        dl = time.time() + tb
        C = s.perm_matrix(w, model)
        while True:
            ws = s.sample_w(C, s.rng)
            order = rest[:]
            s.rng.shuffle(order)
            seed = s.rng.random()
            for x in cands:
                scores[x] += s.rollout(g, x, ws, model, random.Random(seed), order)
            cnt += 1
            if cnt >= 400 or (cnt >= 2 and time.time() > dl):
                break
        best = max(cands, key=lambda x: (scores[x], -abs(x - hb)))
        return int(best)

    def result(s, won, ob, price):
        g = s.cur
        rem = tuple(s.M - x for x in s.seen)
        s.obs.append((g, s.opp_cash, s.op_cnt[g], s.seen[g], rem, ob))
        s.seen[g] += 1
        s.op_bids[g].append(ob)
        if won:
            s.my_cnt[g] += 1; s.cash -= price
        else:
            s.op_cnt[g] += 1; s.opp_cash -= price
        if s.cash < 0: s.cash = 0
        if s.opp_cash < 0: s.opp_cash = 0


def main():
    inp = sys.stdin
    bot = Bot()
    K = M = cash = B = 0
    rule = "second"
    while True:
        line = inp.readline()
        if not line:
            break
        t = line.split()
        if not t:
            continue
        c = t[0]
        if c == "GAME":
            K, M, cash, B, rule = int(t[1]), int(t[2]), int(t[3]), int(t[4]), t[5]
        elif c == "VALUES":
            bot.start(K, M, cash, B, rule, [int(x) for x in t[1:1 + K]])
        elif c == "ROUND":
            try:
                b = bot.bid(int(t[1]), int(t[2]) - 1)
            except Exception:
                b = 0
            b = max(0, min(int(b), bot.cash))
            sys.stdout.write("BID %d\n" % b)
            sys.stdout.flush()
        elif c == "RESULT":
            bot.result(int(t[1]), int(t[2]), int(t[3]))
        elif c == "GAME_END":
            break


main()
