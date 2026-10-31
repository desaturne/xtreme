import sys
from itertools import accumulate, islice
from operator import sub


def small_hull(pts):
    pts = sorted(set(pts))
    if len(pts) > 2:
        lo = []
        for p in pts:
            while len(lo) >= 2 and (lo[-1][0] - lo[-2][0]) * (p[1] - lo[-2][1]) - (lo[-1][1] - lo[-2][1]) * (p[0] - lo[-2][0]) <= 0:
                lo.pop()
            lo.append(p)
        up = []
        for p in reversed(pts):
            while len(up) >= 2 and (up[-1][0] - up[-2][0]) * (p[1] - up[-2][1]) - (up[-1][1] - up[-2][1]) * (p[0] - up[-2][0]) <= 0:
                up.pop()
            up.append(p)
        pts = lo[:-1] + up[:-1]
    q = min(range(len(pts)), key=lambda i: (pts[i][1], pts[i][0]))
    pts = pts[q:] + pts[:q]
    return [p[0] for p in pts], [p[1] for p in pts]


def before(ax, ay, bx, by):
    ha = ay < 0 or (ay == 0 and ax < 0)
    hb = by < 0 or (by == 0 and bx < 0)
    if ha != hb:
        return hb
    return ax * by - ay * bx > 0


def with_point(vx, vy, px, py):
    k = len(vx)
    if k <= 2:
        return small_hull(list(zip(vx, vy)) + [(px, py)])

    def cs(i):
        j = i + 1 if i + 1 < k else 0
        x0 = vx[i]
        y0 = vy[i]
        return (vx[j] - x0) * (py - y0) - (vy[j] - y0) * (px - x0)

    x0, y0 = vx[0], vy[0]
    dx, dy = px - x0, py - y0
    if (vx[1] - x0) * dy - (vy[1] - y0) * dx < 0:
        i0 = 0
    elif (vx[k - 1] - x0) * dy - (vy[k - 1] - y0) * dx > 0:
        i0 = k - 1
    else:
        lo, hi = 1, k - 2
        while lo < hi:
            m = (lo + hi + 1) >> 1
            if (vx[m] - x0) * dy - (vy[m] - y0) * dx >= 0:
                lo = m
            else:
                hi = m - 1
        if cs(lo) >= 0:
            return vx, vy
        i0 = lo
    j0 = i0 + 1 if i0 + 1 < k else 0
    tx, ty = vx[i0] - vx[j0], vy[i0] - vy[j0]
    lo, hi = 0, k
    while lo < hi:
        m = (lo + hi) >> 1
        n1 = m + 1 if m + 1 < k else 0
        if before(vx[n1] - vx[m], vy[n1] - vy[m], tx, ty):
            lo = m + 1
        else:
            hi = m
    u = lo % k
    if cs(u) < 0:
        u = (u - 1) % k
    lo, hi = 0, (u - i0) % k
    while hi - lo > 1:
        m = (lo + hi) >> 1
        if cs((i0 + m) % k) < 0:
            lo = m
        else:
            hi = m
    t = (i0 + lo) % k
    lo, hi = 0, (i0 - u) % k
    while hi - lo > 1:
        m = (lo + hi) >> 1
        if cs((i0 - m) % k) < 0:
            lo = m
        else:
            hi = m
    s = (i0 - lo) % k
    a = (t + 1) % k
    if a <= s:
        rx = vx[a:s + 1]
        ry = vy[a:s + 1]
        q = 0 if a == 0 else -1
    else:
        rx = vx[a:] + vx[:s + 1]
        ry = vy[a:] + vy[:s + 1]
        q = k - a
    dl = cs((s - 1) % k) == 0
    df = cs((t + 1) % k) == 0
    if dl:
        if q == len(rx) - 1:
            q = -1
        rx.pop()
        ry.pop()
    if df and rx:
        if q == 0:
            q = -1
        elif q > 0:
            q -= 1
        del rx[0]
        del ry[0]
    rx.append(px)
    ry.append(py)
    if len(rx) <= 2:
        return small_hull(list(zip(rx, ry)))
    if q < 0 or (py, px) < (ry[q], rx[q]):
        q = len(rx) - 1
    if q:
        rx = rx[q:] + rx[:q]
        ry = ry[q:] + ry[:q]
    return rx, ry


def mink(ax, ay, bx, by):
    if len(ax) < len(bx):
        ax, ay, bx, by = bx, by, ax, ay
    if len(ax) <= 2:
        return small_hull([(x1 + x2, y1 + y2) for x1, y1 in zip(ax, ay) for x2, y2 in zip(bx, by)])
    ex = list(map(sub, islice(ax, 1, None), ax))
    ex.append(ax[0] - ax[-1])
    ey = list(map(sub, islice(ay, 1, None), ay))
    ey.append(ay[0] - ay[-1])
    kb = len(bx)
    lo = 0
    for i in range(kb):
        j = i + 1 if i + 1 < kb else 0
        fx, fy = bx[j] - bx[i], by[j] - by[i]
        hi = len(ex)
        while lo < hi:
            m = (lo + hi) >> 1
            if before(ex[m], ey[m], fx, fy):
                lo = m + 1
            else:
                hi = m
        if lo < len(ex) and ex[lo] * fy - ey[lo] * fx == 0 and ex[lo] * fx + ey[lo] * fy > 0:
            ex[lo] += fx
            ey[lo] += fy
        else:
            ex.insert(lo, fx)
            ey.insert(lo, fy)
        lo += 1
    ex.pop()
    ey.pop()
    return list(accumulate(ex, initial=ax[0] + bx[0])), list(accumulate(ey, initial=ay[0] + by[0]))


def main():
    data = sys.stdin.buffer.read().split()
    n = int(data[0])
    vals = list(map(int, data[1:1 + 3 * n]))
    par = [0] + vals[0::3]
    wx = [0] + vals[1::3]
    wy = [0] + vals[2::3]
    ch = [[] for _ in range(n + 1)]
    root = 1
    for v in range(1, n + 1):
        if par[v] == 0:
            root = v
        else:
            ch[par[v]].append(v)
    order = [root]
    for v in order:
        order.extend(ch[v])
    PX = [None] * (n + 1)
    PY = [None] * (n + 1)
    OX = [0] * (n + 1)
    OY = [0] * (n + 1)
    for v in reversed(order):
        cx, cy = [0], [0]
        ox, oy = wx[v], wy[v]
        for c in ch[v]:
            qx, qy = with_point(PX[c], PY[c], -OX[c], -OY[c])
            PX[c] = PY[c] = None
            ox += OX[c]
            oy += OY[c]
            if len(qx) == 1:
                ox += qx[0]
                oy += qy[0]
            elif len(cx) == 1:
                ox += cx[0]
                oy += cy[0]
                cx, cy = qx, qy
            else:
                cx, cy = mink(cx, cy, qx, qy)
        PX[v], PY[v] = cx, cy
        OX[v], OY[v] = ox, oy
    ox, oy = OX[root], OY[root]
    print(max((x + ox) * (x + ox) + (y + oy) * (y + oy) for x, y in zip(PX[root], PY[root])))


main()
