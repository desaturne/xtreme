import sys
import heapq
from operator import itemgetter


def main():
    rd = sys.stdin.readline
    wr = sys.stdout.write
    fl = sys.stdout.flush
    line = rd()
    while line and not line.startswith('GAME'):
        line = rd()
    if not line:
        return
    p = line.split()
    R, C, U, V, T, K = int(p[1]), int(p[2]), int(p[3]), int(p[4]), int(p[5]), int(p[6])
    N = R * C
    wall = [-1] * N
    if K == 1:
        rd()
        for r in range(R):
            s = rd()
            for c in range(C):
                wall[r * C + c] = 1 if s[c] == '#' else 0
    nbr = []
    for i in range(N):
        r, c = divmod(i, C)
        nbr.append((i - C if r > 0 else -1, i + 1 if c < C - 1 else -1,
                    i + C if r < R - 1 else -1, i - 1 if c > 0 else -1))
    adj = [tuple(y for y in t if y >= 0) for t in nbr]
    DIRS = 'NESW'
    paint = [0] * N
    seen = [-1] * N
    w = None
    G = 0.9
    D = 6 if U <= 3 else 5
    BR = 2
    W1 = C + 1
    boxi = []
    for i in range(N):
        r, c = divmod(i, C)
        r0 = r - BR if r >= BR else 0
        r1 = r + BR + 1 if r + BR + 1 <= R else R
        c0 = c - BR if c >= BR else 0
        c1 = c + BR + 1 if c + BR + 1 <= C else C
        boxi.append((r1 * W1 + c1, r0 * W1 + c1, r1 * W1 + c0, r0 * W1 + c0))
    fd = [-1] * N
    ffs = [-1] * N
    stamp = [0] * N
    stc = 0
    prevpos = [-1] * U
    lastpos = [-1] * U
    lastmv = ['H'] * U
    fails = [0] * U
    lastenem = set()
    estart = None
    lastsee = []
    lastseen_n = 0
    while True:
        line = rd()
        if not line or line.startswith('GAME_END'):
            break
        if not line.startswith('ROUND'):
            continue
        p = line.split()
        n = int(p[1])
        cells = int(p[2])
        pos = [0] * U
        vis = bytearray(N)
        enemies = set()
        for _ in range(U):
            t = rd().split()
            ui, r, c = int(t[1]), int(t[2]), int(t[3])
            pos[ui] = r * C + c
            for dr in range(-V, V + 1):
                s = rd()
                rr = r + dr
                if rr < 0 or rr >= R:
                    continue
                base = rr * C
                for dc in range(-V, V + 1):
                    cc = c + dc
                    if cc < 0 or cc >= C:
                        continue
                    ch = s[dc + V]
                    idx = base + cc
                    vis[idx] = 1
                    seen[idx] = n
                    if ch == '#':
                        wall[idx] = 1
                        wall[N - 1 - idx] = 1
                        continue
                    if wall[idx] != 0:
                        wall[idx] = 0
                        wall[N - 1 - idx] = 0
                    if ch == '.':
                        paint[idx] = 0
                    elif ch == '+' or ch == 'M':
                        paint[idx] = 1
                    elif ch == '-':
                        paint[idx] = 2
                    elif ch == 'E':
                        paint[idx] = 2
                        enemies.add(idx)
        if w is None:
            sr = sum(x // C for x in pos) / U
            sc = sum(x % C for x in pos) / U
            er, ec = R - 1 - sr, C - 1 - sc
            w = [0.0] * N
            for i in range(N):
                r, c = divmod(i, C)
                da = abs(r - sr) + abs(c - sc)
                db = abs(r - er) + abs(c - ec)
                w[i] = 0.3 + 1.4 * da / (da + db + 1e-9)
        bp = 0
        km = 0
        sw_plus = 0.0
        sw_dot = 0.0
        opn = [i for i in range(N) if wall[i] != 1]
        for i in opn:
            pi = paint[i]
            if pi == 1:
                bp += 1
                if not vis[i]:
                    sw_plus += (n - seen[i]) * w[i]
            elif pi == 2:
                km += 1
            elif not vis[i]:
                sw_dot += (n - seen[i] if seen[i] >= 0 else n) * w[i]
        lost = bp - cells
        if lost < 0:
            lost = 0
        extra = cells - km - lost
        if extra < 0:
            extra = 0
        kp = lost / sw_plus if sw_plus > 0 else 0.0
        kd = extra / sw_dot if sw_dot > 0 else 0.0
        val = [0.0] * N
        for i in opn:
            wi = wall[i]
            pi = paint[i]
            if vis[i]:
                val[i] = 1.0 if pi == 0 else (2.0 if pi == 2 else 0.0)
            elif pi == 2:
                val[i] = 2.0
            elif pi == 1:
                q = kp * (n - seen[i]) * w[i]
                val[i] = 2.0 * (q if q < 1.0 else 1.0)
            else:
                q = kd * ((n - seen[i]) if seen[i] >= 0 else n) * w[i]
                if q > 0.9:
                    q = 0.9
                val[i] = (1.0 + q) * (1.0 if wi == 0 else 0.8)
        pre = [0.0] * ((R + 1) * W1)
        for r in range(R):
            acc = 0.0
            ro = r * C
            a = (r + 1) * W1
            b = r * W1
            for c in range(C):
                acc += val[ro + c]
                pre[a + c + 1] = pre[b + c + 1] + acc
        rem = T - n + 1
        dist = [-1] * N
        own = [-1] * N
        first = [-1] * N
        q = []
        for ui in range(U):
            x = pos[ui]
            if dist[x] < 0:
                dist[x] = 0
                own[x] = ui
                first[x] = x
                q.append(x)
        h = 0
        while h < len(q):
            x = q[h]
            h += 1
            dx = dist[x] + 1
            ox = own[x]
            fx = first[x]
            for y in adj[x]:
                if dist[y] < 0 and wall[y] != 1:
                    dist[y] = dx
                    own[y] = ox
                    first[y] = y if dx == 1 else fx
                    q.append(y)
        tv = [0.0] * N
        for x in q:
            v = val[x]
            if v > 0.3:
                a, b, c, d = boxi[x]
                tv[x] = v + 0.15 * (pre[a] - pre[b] - pre[c] + pre[d] - v)
        bestt = [-1] * U
        bests = [0.0] * U
        for x in q:
            d = dist[x]
            if d == 0 or d > rem:
                continue
            s = tv[x] / (d + 2)
            o = own[x]
            if s > bests[o]:
                bests[o] = s
                bestt[o] = x
        firststep = [-1] * U
        for ui in range(U):
            if bestt[ui] >= 0 and bests[ui] >= 0.1:
                firststep[ui] = first[bestt[ui]]
            else:
                src = pos[ui]
                stc += 1
                stamp[src] = stc
                fd[src] = 0
                ffs[src] = src
                qq = [src]
                hh = 0
                bs = 0.0
                bt = -1
                while hh < len(qq):
                    x = qq[hh]
                    hh += 1
                    d = fd[x] + 1
                    if d > rem:
                        continue
                    for y in adj[x]:
                        if stamp[y] != stc and wall[y] != 1:
                            stamp[y] = stc
                            fd[y] = d
                            ffs[y] = y if d == 1 else ffs[x]
                            qq.append(y)
                            s = tv[y] / (d + 2)
                            if s > bs:
                                bs = s
                                bt = y
                if bt >= 0:
                    firststep[ui] = ffs[bt]
        valc = val[:]
        for e in enemies:
            valc[e] *= 0.5

        def dfs(x, depth, visited):
            best = 0.0
            bpath = ()
            for y in adj[x]:
                if wall[y] == 1 or y in visited:
                    continue
                v = valc[y]
                pth = (y,)
                if depth > 1:
                    visited.add(y)
                    sv, sp = dfs(y, depth - 1, visited)
                    visited.discard(y)
                    v += G * sv
                    pth = (y,) + sp
                if v > best:
                    best = v
                    bpath = pth
            return best, bpath

        for ui in range(U):
            if lastpos[ui] != pos[ui]:
                prevpos[ui] = lastpos[ui]
                fails[ui] = 0
            elif lastmv[ui] != 'H':
                fails[ui] += 1
        still = enemies & lastenem
        lastenem = enemies
        if estart is None:
            estart = [N - 1 - x for x in pos]
        if enemies:
            lastsee = list(enemies)
            lastseen_n = n
        de = None
        HZ = rem if rem <= 8 else 8
        GDv = 1.0 if rem <= 8 else 0.9
        if U <= 3 and len(opn) <= 400:
            de = [10 ** 6] * N
            for (sl, off) in ((estart, n - 1), (lastsee, n - lastseen_n)):
                lim = off + HZ
                dd = [-1] * N
                qq = []
                for sx in sl:
                    if wall[sx] != 1 and dd[sx] < 0:
                        dd[sx] = 0
                        qq.append(sx)
                hh = 0
                while hh < len(qq):
                    x = qq[hh]
                    hh += 1
                    dx = dd[x]
                    v = dx - off
                    if v < de[x]:
                        de[x] = v
                    if dx >= lim:
                        continue
                    dx += 1
                    for y in adj[x]:
                        if dd[y] < 0 and wall[y] != 1:
                            dd[y] = dx
                            qq.append(y)
            bval = valc[:]
        orders = []
        planned = set()
        for ui in range(U):
            x0 = pos[ui]
            blocked = set(planned)
            for uj in range(ui + 1, U):
                blocked.add(pos[uj])
            bsc = -1e18
            bmv = 'H'
            bd = x0
            bpth = ()
            fs = firststep[ui]
            if de is not None:
                beam = []
                for k in range(4):
                    y = nbr[x0][k]
                    if y < 0 or wall[y] == 1 or y in blocked:
                        continue
                    s = valc[y] if de is None else (bval[y] * (2.5 if de[y] <= 1 else 1.0) if valc[y] > 0 else 0.0)
                    if y == fs:
                        s += 0.01
                    if y in enemies:
                        s -= 3.0 if y in still else 0.6
                    if fails[ui] and lastmv[ui] == DIRS[k]:
                        s -= 2.5 * fails[ui]
                    beam.append((s, y, (1 << x0) | (1 << y), k, (y,)))
                if beam:
                    BW = int(min(10000, 600000 / T) / (3 * HZ * U))
                    if BW > 3000:
                        BW = 3000
                    if BW < 20:
                        BW = 20
                    gm = 1.0
                    for t in range(2, HZ + 1):
                        gm *= GDv
                        cand = {}
                        for (sc, x, m, f, pth) in beam:
                            for y in adj[x]:
                                if wall[y] == 1:
                                    continue
                                if (m >> y) & 1:
                                    ns = sc
                                    nm = m
                                else:
                                    vy = bval[y] if valc[y] > 0 else 0.0
                                    if de[y] <= t:
                                        vy *= 2.5
                                    ns = sc + vy * gm
                                    nm = m | (1 << y)
                                key = (y, nm)
                                o = cand.get(key)
                                if o is None or o[0] < ns:
                                    cand[key] = (ns, y, nm, f, pth + (y,))
                        if len(cand) > BW:
                            beam = heapq.nlargest(BW, cand.values(), key=itemgetter(0))
                        else:
                            beam = list(cand.values())
                    best = max(beam, key=itemgetter(0))
                    k = best[3]
                    bd = nbr[x0][k]
                    planned.add(bd)
                    for y in best[4]:
                        valc[y] = 0.0
                        bval[y] = 0.0
                    orders.append(DIRS[k])
                    lastpos[ui] = x0
                    lastmv[ui] = DIRS[k]
                    continue
            for k in range(4):
                y = nbr[x0][k]
                if y < 0 or wall[y] == 1 or y in blocked:
                    continue
                vis2 = {x0, y}
                sv, sp = dfs(y, D - 1, vis2) if D > 1 else (0.0, ())
                s = valc[y] + G * sv
                if y == fs:
                    s += 0.5
                if y in enemies:
                    s -= 3.0 if y in still else 0.6
                if y == prevpos[ui]:
                    s -= 0.7
                if fails[ui] and lastmv[ui] == DIRS[k]:
                    s -= 2.5 * fails[ui]
                if s > bsc:
                    bsc = s
                    bmv = DIRS[k]
                    bd = y
                    bpth = (y,) + sp
            planned.add(bd)
            for y in bpth:
                valc[y] = 0.0
            orders.append(bmv)
            lastpos[ui] = x0
            lastmv[ui] = bmv
        wr('ORDERS ' + ' '.join(orders) + '\n')
        fl()


main()
