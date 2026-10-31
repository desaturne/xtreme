import sys
import re
import math
import random
import numpy as np

CARD_RE = re.compile(r'^([PWL])(\d+)$')
CATS = ('P', 'W', 'L')
AX = {'P': 0, 'W': 1, 'L': 2}

# Models of how the opponent picks its guesses. Their guesses are evidence about the
# solution; each model gives a likelihood over all solution triples.
#   U: every guessed card is one it doesn't know yet (my unshown cards + solution)
#   F: tests one unknown card, fills the other slots with its own cards (or known solution)
#   R: random cards (no information)
#   H*: each slot independently is a test (prob q) or a filler from its own hand
# Each (except R) comes in two flavours for categories the opponent already knows:
#   plain: it keeps putting the solution there;  'o': it puts one of its own cards there
BASE = {'U': 0.4, 'F': 0.5}
HQ = {}
MODELS = tuple(m + f for m in BASE for f in ('', 'o')) + ('R',)
PRIOR = dict([(m + f, BASE[m] / 2) for m in BASE for f in ('', 'o')] + [('R', 0.1)])
UTYPE = ('U', 'Uo')
FTYPE = ('F', 'Fo')
EPS = 0.03        # off-model noise
DELTA = 0.0005    # F: chance a filler slot holds the already known solution card
NSIM = 300
FILL = 'random'   # fillers: random unshown own cards
ORDER = 'det'     # test order: 'det' most likely card first, or 'rand'
LOSE = 5.0        # value assumed when the opponent will win a later race (lower = gamble sooner)
LEAK = 0.3        # chance opponent deduced a category from my own NONE result
RHO = 0.0005        # filler in a category the opponent knows: chance it is the solution
KMINE = 0.2       # ... chance it is one of my (shown) cards
THETA = 0.25      # smart filler: NONE chance above which fresh fillers are used
PACC = 0.85       # accuse as soon as the most likely solution is at least this probable


def out(s):
    sys.stdout.write(s + '\n')
    sys.stdout.flush()


def sh(v, ax):
    s = [1, 1, 1]
    s[ax] = -1
    return v.reshape(s)


def outer(a, b, c):
    return sh(a, 0) * sh(b, 1) * sh(c, 2)


class Bot:
    def __init__(self, sizes, turns, hand):
        self.turns = turns
        self.n = [sizes[c] for c in CATS]
        self.mine = set(hand)
        self.opp = set()
        self.shown_to_opp = set()
        self.opp_none = [set() for _ in CATS]   # indices the opponent guessed when I said NONE
        self.leak = [False, False, False]
        self.logL = np.zeros(self.n)              # evidence from my own guesses
        for x in hand:
            a, i = self.idx(x)
            self._mask(a, i, keep=False)
        self.logF = {m: np.zeros(self.n) for m in MODELS}   # evidence from opponent guesses
        self.fixed_filler = {}
        self.exposed = set()     # my cards the opponent saw in my guesses when it showed a card
        self.t = 0
        self.last_guess = None
        seed = 9173 + 31 * sum(self.n) + turns + len(hand)
        self.rng = random.Random(seed)
        self.nrng = np.random.default_rng(seed)
        self._post = None

    @staticmethod
    def idx(x):
        return AX[x[0]], int(x[1:]) - 1

    def card(self, a, i):
        return '%s%d' % (CATS[a], i + 1)

    def _mask(self, a, i, keep):
        v = np.zeros(self.n[a]) if not keep else np.full(self.n[a], -np.inf)
        if keep:
            v[i] = 0.0
        else:
            v[i] = -np.inf
        self.logL = self.logL + sh(v, a)

    def unshown(self, a):
        return sum(1 for x in self.mine if self.idx(x)[0] == a and x not in self.shown_to_opp)

    # ---------- posterior ----------
    def posterior(self):
        if self._post is not None:
            return self._post
        lps = {m: self.logL + self.logF[m] + math.log(PRIOR[m]) for m in MODELS}
        mx = max(float(v.max()) for v in lps.values())
        parts = {m: np.exp(lps[m] - mx) for m in MODELS}
        tot = sum(float(v.sum()) for v in parts.values())
        J = sum(parts.values()) / tot
        self.model_w = {m: float(parts[m].sum()) / tot for m in MODELS}
        poss = [np.isfinite(self.logL).any(axis=tuple(b for b in range(3) if b != a)) for a in range(3)]
        marg = [J.sum(axis=tuple(b for b in range(3) if b != a)) for a in range(3)]
        self._post = (J, marg, poss)
        return self._post

    # ---------- opponent ----------
    def record_opp_guess(self, cards):
        if len(cards) != 3 or sorted(self.idx(x)[0] for x in cards) != [0, 1, 2]:
            return
        cards = sorted(cards, key=lambda x: self.idx(x)[0])
        res = self.threat_vectors()
        for flav in ('', 'o'):
            inU, fill, kf, unif = [], [], [], []
            for a, x in enumerate(cards):
                _, gi = self.idx(x)
                n = self.n[a]
                u = self.unshown(a) + 1
                h = (n - 1) // 2
                eq = np.zeros(n)
                eq[gi] = 1.0
                if x in self.mine:
                    iu = np.full(n, 0.0 if x in self.shown_to_opp else 1.0 / u)
                    ih = np.zeros(n)
                    k = np.full(n, KMINE / h)
                else:
                    iu = eq / u
                    ih = (1.0 - eq) / h
                    k = RHO * eq + (1 - RHO) * ih
                # 'o': in a category the opponent already knows, the slot is an own-card filler
                r = res[a] if flav else np.zeros(n)
                inU.append((1 - r) * iu)
                fill.append((1 - r) * ((1 - DELTA) * ih + DELTA * eq) + r * k)
                kf.append(r * k)
                unif.append(1.0 / n)
            floor = EPS * unif[0] * unif[1] * unif[2]
            pu = [(1 - EPS) * (inU[a] + kf[a]) + EPS * unif[a] for a in range(3)]
            self.logF['U' + flav] = self.logF['U' + flav] + np.log(outer(*pu))
            pf = (outer(inU[0], fill[1], fill[2]) + outer(fill[0], inU[1], fill[2])
                  + outer(fill[0], fill[1], inU[2])) / 3.0
            self.logF['F' + flav] = self.logF['F' + flav] + np.log((1 - EPS) * pf + floor)
            for m, q in HQ.items():
                ph = [(1 - EPS) * (q * inU[a] + (1 - q) * fill[a] + q * kf[a]) + EPS * unif[a]
                      for a in range(3)]
                self.logF[m + flav] = self.logF[m + flav] + np.log(outer(*ph))
        # R: constant, contributes nothing relative between triples but matters across models
        self.logF['R'] = self.logF['R'] + math.log(unif[0] * unif[1] * unif[2])
        self._post = None

    def threat_vectors(self):
        """res[a][i]: prob opponent already knows category a if solution index is i."""
        res = []
        for a in range(3):
            if self.unshown(a) == 0:
                v = np.ones(self.n[a])
            else:
                v = np.zeros(self.n[a])
                for i in self.opp_none[a]:
                    v[i] = 1.0
                if self.leak[a]:
                    v = np.maximum(v, LEAK)
            res.append(v)
        return res

    # ---------- my move ----------
    def continue_value(self, p):
        J, marg, poss = self.posterior()
        res = self.threat_vectors()
        flat = J.ravel()
        flat = flat / flat.sum()
        picks = self.nrng.choice(flat.size, size=NSIM, p=flat)
        tri = np.unravel_index(picks, J.shape)
        # my remaining tests: rank of true card in my test order, capped at k-1
        M = np.zeros(NSIM, dtype=int)
        for a in range(3):
            k = int(poss[a].sum())
            if k <= 1:
                continue
            order = np.argsort(-np.where(poss[a], marg[a], -1.0), kind='stable')
            rank = np.empty(self.n[a], dtype=int)
            rank[order] = np.arange(1, self.n[a] + 1)
            M += np.minimum(rank[tri[a]], k - 1)
        w = self.model_w
        mt = self.nrng.choice(3, size=NSIM, p=[sum(w[m] for m in UTYPE), sum(w[m] for m in FTYPE), w['R']])
        N = np.zeros(NSIM, dtype=int)
        for a in range(3):
            u = self.unshown(a) + 1
            if u <= 1:
                continue
            known = self.nrng.random(NSIM) < res[a][tri[a]]
            single = np.minimum(self.nrng.integers(1, u + 1, NSIM), u - 1)
            need = np.where(mt == 0, u - 1, np.where(mt == 1, single, 2 * (u - 1)))
            N += np.where(known, 0, need)
        my_ok = self.t + 2 * M <= self.turns
        op_ok = self.t + 1 + 2 * N <= self.turns
        gamble = 3 + 7 * p
        val = np.where(my_ok & ((M <= N) | ~op_ok), 10.0,
                       np.where(op_ok, np.where(N == 0, 0.0, LOSE), gamble))
        return float(val.mean())

    def filler(self, a, pnone=0.0):
        """Filler slot: a card the opponent already knows I hold (reveals nothing).
        If none yet, always reuse the same own card so at most one card leaks."""
        mine = sorted(x for x in self.mine if self.idx(x)[0] == a)
        if FILL == 'decoy':
            # a solution card I already know: the opponent can't hold it (test stays
            # single), and an opponent reading my fillers as my hand gets misled
            _, _, poss = self.posterior()
            if int(poss[a].sum()) == 1:
                return self.card(a, int(np.flatnonzero(poss[a])[0]))
            shown = [x for x in mine if x in self.shown_to_opp]
            if shown:
                return shown[0]
            if a not in self.fixed_filler:
                self.fixed_filler[a] = self.rng.choice(mine)
            return self.fixed_filler[a]
        if FILL == 'smart':
            # cards the opponent already knows I hold reveal nothing when it shows my test
            # card; but if my test card may well be the solution (NONE), use cards it hasn't
            # seen so it can't tell which slot was the test
            known = [x for x in mine if x in self.shown_to_opp or x in self.exposed]
            fresh = [x for x in mine if x not in known]
            if fresh and (pnone >= THETA or not known):
                return self.rng.choice(fresh)
            return self.rng.choice(known)
        if FILL == 'rdec':
            _, _, poss = self.posterior()
            if int(poss[a].sum()) == 1:
                return self.card(a, int(np.flatnonzero(poss[a])[0]))
            dec = [x for x in mine if x not in self.shown_to_opp]
            return self.rng.choice(dec) if dec else self.rng.choice(mine)
        if FILL == 'random':
            dec = [x for x in mine if x not in self.shown_to_opp]
            return self.rng.choice(dec) if dec else self.rng.choice(mine)
        shown = [x for x in mine if x in self.shown_to_opp]
        if shown:
            return shown[0]
        if a not in self.fixed_filler:
            self.fixed_filler[a] = self.rng.choice(mine)
        return self.fixed_filler[a]

    def my_turn(self):
        J, marg, poss = self.posterior()
        ks = [int(poss[a].sum()) for a in range(3)]
        if all(k == 1 for k in ks):
            return 'ACCUSE ' + ' '.join(self.card(a, int(np.flatnonzero(poss[a])[0])) for a in range(3))
        bt = np.unravel_index(int(np.argmax(J)), J.shape)
        p = float(J[bt])
        best = 'ACCUSE ' + ' '.join(self.card(a, int(bt[a])) for a in range(3))
        if self.t + 2 > self.turns or p >= PACC:
            return best
        if 3 + 7 * p > self.continue_value(p):
            return best
        # test one open card; category and card are randomised so the order of my
        # tests tells the opponent nothing about my hand or the solution
        open_ = [a for a in range(3) if ks[a] > 1]
        if ORDER == 'det':
            tops = [(float(np.where(poss[a], marg[a], -1).max()), -ks[a], a) for a in open_]
            ta = max(tops)[2]
            ti = int(np.argmax(np.where(poss[ta], marg[ta], -1.0)))
            pn = float(marg[ta][ti])
            guess = [self.card(a, ti) if a == ta else self.filler(a, pn) for a in range(3)]
            self.last_guess = guess
            return 'GUESS ' + ' '.join(guess)
        ta = self.rng.choice(open_)
        pr = np.where(poss[ta], marg[ta], 0.0)
        pr = pr / pr.sum()
        ti = int(self.nrng.choice(self.n[ta], p=pr))
        guess = [self.card(a, ti) if a == ta else self.filler(a) for a in range(3)]
        self.last_guess = guess
        return 'GUESS ' + ' '.join(guess)

    def result(self, shown):
        g = self.last_guess
        self.last_guess = None
        self._post = None
        if not g:
            return
        if shown is None:
            for x in g:
                if x not in self.mine:
                    a, i = self.idx(x)
                    self._mask(a, i, keep=True)
                    self.leak[a] = True
        else:
            # every guessed card that isn't mine and isn't the solution is held by the opponent;
            # assume they pick uniformly among those
            held = []
            for x in g:
                a, gi = self.idx(x)
                v = np.zeros(self.n[a])
                if x not in self.mine:
                    v[:] = 1.0
                    v[gi] = 0.0
                held.append(v)
            for x in g:
                if x in self.mine:
                    self.exposed.add(x)
            hcount = sh(held[0], 0) + sh(held[1], 1) + sh(held[2], 2)
            self.logL = self.logL - np.log(np.maximum(hcount, 1.0))
            if shown not in self.mine:
                self.opp.add(shown)
                a, i = self.idx(shown)
                self._mask(a, i, keep=False)
        mx = self.logL.max()
        if np.isfinite(mx):
            self.logL = self.logL - mx

    # ---------- opponent's guess ----------
    def opp_guess(self, cards):
        self.record_opp_guess(cards)
        match = [x for x in cards if x in self.mine]
        if not match:
            for x in cards:
                a, i = self.idx(x)
                self.opp_none[a].add(i)
            return 'NONE'
        old = [x for x in match if x in self.shown_to_opp]
        if old:
            return 'SHOW ' + old[0]
        J, marg, poss = self.posterior()
        res = self.threat_vectors()

        def key(x):
            a = self.idx(x)[0]
            return (float((marg[a] * res[a]).sum()), self.unshown(a))
        pick = max(match, key=key)
        self.shown_to_opp.add(pick)
        return 'SHOW ' + pick


def build(header):
    ints = []
    card_lines = []
    for line in header:
        cards = []
        for tok in line.split():
            if CARD_RE.match(tok):
                cards.append(tok)
            else:
                ints.extend(int(v) for v in re.findall(r'\d+', tok))
        if cards:
            card_lines.append(cards)
    if len(ints) >= 3:
        A, B, C = ints[0], ints[1], ints[2]
        turns = ints[3] if len(ints) > 3 else 1000
        hand = [x for cl in card_lines for x in cl]
    else:
        deck = card_lines[0] if card_lines else []
        sz = {c: 3 for c in CATS}
        for x in deck:
            sz[x[0]] = int(x[1:])
        A, B, C = sz['P'], sz['W'], sz['L']
        turns = ints[0] if ints else 1000
        hand = [x for cl in card_lines[1:] for x in cl]
    return Bot({'P': A, 'W': B, 'L': C}, turns, hand)


def main():
    header = []
    bot = None
    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue
        toks = line.split()
        head = toks[0].upper()
        if head not in ('TURN', 'RESULT', 'GAME_END'):
            if bot is None:
                header.append(line)
            continue
        if bot is None:
            bot = build(header)
        if head == 'GAME_END':
            break
        if head == 'TURN':
            bot.t += 1
            if len(toks) > 1 and toks[1].upper() == 'YOU':
                out(bot.my_turn())
            else:
                cards = [x for x in toks[1:] if CARD_RE.match(x)
                         and 1 <= int(x[1:]) <= bot.n[AX[x[0]]]]
                out(bot.opp_guess(cards))
        elif head == 'RESULT':
            if len(toks) > 1 and toks[1].upper() == 'SHOWN' and len(toks) > 2:
                bot.result(toks[2])
            else:
                bot.result(None)


main()
