import sys
from array import array
import heapq


def main():
    data = sys.stdin.buffer.read().split()
    n = int(data[0]); q = int(data[1]); p = 2

    def read_tree():
        nonlocal p
        adj = [[] for _ in range(n + 1)]
        for _ in range(n - 1):
            u = int(data[p]); v = int(data[p + 1]); w = int(data[p + 2]); p += 3
            adj[u].append((v, w)); adj[v].append((u, w))
        return adj

    g1 = read_tree()
    g2 = read_tree()
    queries = [int(x) for x in data[p:p + q]]
    del data

    # ---- tree 2: DFS order + sparse table giving weighted depth of LCA ----
    tin = [0] * (n + 1)
    wd2 = [0] * (n + 1)
    par2 = [0] * (n + 1)
    P = [0] * n                     # P[i] = wd2[parent of node at position i]
    seen = bytearray(n + 1)
    st = [1]; seen[1] = 1; t = 0
    while st:
        x = st.pop()
        tin[x] = t
        P[t] = wd2[par2[x]] if t else 0
        t += 1
        for y, w in g2[x]:
            if not seen[y]:
                seen[y] = 1; par2[y] = x; wd2[y] = wd2[x] + w; st.append(y)
    sp = [P]
    k = 1
    while (1 << k) <= n:
        prev = sp[-1]; h = 1 << (k - 1)
        sp.append([a if a < b else b for a, b in zip(prev, prev[h:])])
        k += 1
    lg = [0] * (n + 1)
    for i in range(2, n + 1):
        lg[i] = lg[i >> 1] + 1

    # ---- centroid decomposition of tree 1, building one binary tree per centroid ----
    BIG = 1 << 23
    # leaves (one per (centroid level, vertex))
    lK = array('q')       # dist1(v, centroid) + wd2[v]
    lT = array('i')       # tin2[v]
    lpar = array('i')
    lact = bytearray()
    leaves_of = [[] for _ in range(n + 1)]
    # internal nodes
    iL = array('i'); iR = array('i'); ipar = array('i')
    ea = array('i'); eb = array('i'); vl = array('q'); cr = array('q')
    isc = bytearray()

    def new_internal(a, b, cross):
        idx = len(iL)
        iL.append(a); iR.append(b); ipar.append(-1)
        ea.append(-1); eb.append(-1); vl.append(0); cr.append(-1)
        isc.append(cross)
        for c in (a, b):
            if c >= BIG:
                ipar[c - BIG] = idx
            else:
                lpar[c] = idx
        return idx + BIG

    removed = bytearray(n + 1)
    sz = [0] * (n + 1)
    bpar = [0] * (n + 1)
    dist = [0] * (n + 1)
    grp = [0] * (n + 1)
    stack = [1]
    while stack:
        s = stack.pop()
        order = [s]; bpar[s] = 0
        i = 0
        while i < len(order):
            x = order[i]; i += 1
            for y, w in g1[x]:
                if y != bpar[x] and not removed[y]:
                    bpar[y] = x; order.append(y)
        tot = len(order)
        for x in reversed(order):
            sz[x] = 1
        for x in reversed(order):
            if bpar[x]:
                sz[bpar[x]] += sz[x]
        c = s
        while True:
            nxt = 0
            for y, w in g1[c]:
                if y != bpar[c] and not removed[y] and sz[y] * 2 > tot:
                    nxt = y; break
            if not nxt:
                break
            c = nxt
        # distances from c and group membership
        groups = {c: [c]}
        dist[c] = 0; grp[c] = c; bpar[c] = 0
        order = [c]; i = 0
        while i < len(order):
            x = order[i]; i += 1
            for y, w in g1[x]:
                if y != bpar[x] and not removed[y]:
                    bpar[y] = x; dist[y] = dist[x] + w
                    gy = y if x == c else grp[x]
                    grp[y] = gy
                    if x == c:
                        groups[y] = [y]
                    else:
                        groups[gy].append(y)
                    order.append(y)
        heap = []
        for key, verts in groups.items():
            cur = []
            for v in verts:
                lid = len(lK)
                lK.append(dist[v] + wd2[v]); lT.append(tin[v]); lpar.append(-1); lact.append(0)
                leaves_of[v].append(lid)
                cur.append(lid)
            while len(cur) > 1:
                nc = []
                for j in range(0, len(cur) - 1, 2):
                    nc.append(new_internal(cur[j], cur[j + 1], 0))
                if len(cur) & 1:
                    nc.append(cur[-1])
                cur = nc
            heap.append((len(verts), key, cur[0]))
        heapq.heapify(heap)
        while len(heap) > 1:
            s1, k1, r1 = heapq.heappop(heap)
            s2, k2, r2 = heapq.heappop(heap)
            heapq.heappush(heap, (s1 + s2, k1, new_internal(r1, r2, 1)))
        root = heap[0][2]
        if root >= BIG:
            ipar[root - BIG] = -1 - c      # marks root, encodes centroid slot
        else:
            lpar[root] = -1 - c
        removed[c] = 1
        for y, w in g1[c]:
            if not removed[y]:
                stack.append(y)

    # ---- segment tree (max) over centroid slots ----
    N = 1
    while N < n + 1:
        N <<= 1
    seg = [-1] * (2 * N)

    def dd(x, y):
        tx = lT[x]; ty = lT[y]
        if tx < ty:
            l = tx + 1; r = ty
        else:
            l = ty + 1; r = tx
        kk = lg[r - l + 1]
        row = sp[kk]
        m1 = row[l]; m2 = row[r - (1 << kk) + 1]
        if m2 < m1:
            m1 = m2
        return lK[x] + lK[y] - 2 * m1

    active = bytearray(n + 1)
    out = []
    del g2, sz, bpar, dist, grp
    for v in queries:
        a_now = active[v] ^ 1
        active[v] = a_now
        for lf in leaves_of[v]:
            lact[lf] = a_now
            y = lpar[lf]
            while y >= 0:
                a = iL[y]; b = iR[y]
                if a >= BIG:
                    ai = a - BIG
                    a1 = ea[ai]; a2 = eb[ai]; av = vl[ai]; ac = cr[ai]
                elif lact[a]:
                    a1 = a2 = a; av = 0; ac = -1
                else:
                    a1 = -1; ac = -1
                if b >= BIG:
                    bi = b - BIG
                    b1 = ea[bi]; b2 = eb[bi]; bv = vl[bi]; bc = cr[bi]
                elif lact[b]:
                    b1 = b2 = b; bv = 0; bc = -1
                else:
                    b1 = -1; bc = -1
                if a1 < 0:
                    if b1 < 0:
                        n1 = n2 = -1; nv = 0; ncr = -1
                    else:
                        n1 = b1; n2 = b2; nv = bv; ncr = bc
                elif b1 < 0:
                    n1 = a1; n2 = a2; nv = av; ncr = ac
                else:
                    # cross distances between endpoints (farthest pair lies among them)
                    best = dd(a1, b1); x1 = a1; y1 = b1
                    if b2 != b1:
                        d = dd(a1, b2)
                        if d > best:
                            best = d; y1 = b2
                    if a2 != a1:
                        d = dd(a2, b1)
                        if d > best:
                            best = d; x1 = a2; y1 = b1
                        if b2 != b1:
                            d = dd(a2, b2)
                            if d > best:
                                best = d; x1 = a2; y1 = b2
                    if av >= bv:
                        n1 = a1; n2 = a2; nv = av
                    else:
                        n1 = b1; n2 = b2; nv = bv
                    if best > nv:
                        n1 = x1; n2 = y1; nv = best
                    if isc[y]:
                        ncr = ac if ac > bc else bc
                        if best > ncr:
                            ncr = best
                    else:
                        ncr = -1
                if n1 == ea[y] and n2 == eb[y] and nv == vl[y] and ncr == cr[y]:
                    break
                ea[y] = n1; eb[y] = n2; vl[y] = nv; cr[y] = ncr
                py = ipar[y]
                if py < 0:
                    i = N + (-1 - py)
                    seg[i] = ncr
                    i >>= 1
                    while i:
                        l = seg[2 * i]; r = seg[2 * i + 1]
                        seg[i] = l if l > r else r
                        i >>= 1
                    break
                y = py
        out.append(seg[1])
    sys.stdout.write('\n'.join(map(str, out)) + '\n')


main()
