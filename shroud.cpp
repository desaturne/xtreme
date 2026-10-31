#include <bits/stdc++.h>
using namespace std;
typedef unsigned long long u64;

struct St {
    double sc;
    int x, f, len;
    u64 h;
    short path[40];
};

int R, C, U, V, T, K, N;
vector<int> wall, paint, seen;
vector<array<int, 4>> nbr;
vector<vector<int>> adj;
const char DIRS[5] = "NESW";

int main() {
    ios::sync_with_stdio(false);
    string line;
    while (getline(cin, line) && line.compare(0, 4, "GAME") != 0) {}
    if (line.compare(0, 4, "GAME") != 0) return 0;
    {
        stringstream ss(line);
        string g;
        ss >> g >> R >> C >> U >> V >> T >> K;
    }
    N = R * C;
    wall.assign(N, -1);
    paint.assign(N, 0);
    seen.assign(N, -1);
    if (K == 1) {
        getline(cin, line);
        for (int r = 0; r < R; r++) {
            getline(cin, line);
            for (int c = 0; c < C; c++) wall[r * C + c] = (c < (int)line.size() && line[c] == '#') ? 1 : 0;
        }
    }
    nbr.resize(N);
    adj.resize(N);
    for (int i = 0; i < N; i++) {
        int r = i / C, c = i % C;
        nbr[i] = {r > 0 ? i - C : -1, c < C - 1 ? i + 1 : -1, r < R - 1 ? i + C : -1, c > 0 ? i - 1 : -1};
        for (int k = 0; k < 4; k++)
            if (nbr[i][k] >= 0) adj[i].push_back(nbr[i][k]);
    }
    u64 zs = 88172645463325252ULL;
    vector<u64> zob(N);
    for (int i = 0; i < N; i++) {
        zs ^= zs << 13, zs ^= zs >> 7, zs ^= zs << 17;
        zob[i] = zs;
    }
    vector<double> w;
    const int BR = 2;
    int W1 = C + 1;
    vector<array<int, 4>> boxi(N);
    for (int i = 0; i < N; i++) {
        int r = i / C, c = i % C;
        int r0 = max(r - BR, 0), r1 = min(r + BR + 1, R), c0 = max(c - BR, 0), c1 = min(c + BR + 1, C);
        boxi[i] = {r1 * W1 + c1, r0 * W1 + c1, r1 * W1 + c0, r0 * W1 + c0};
    }
    vector<int> fd(N, -1), ffs(N, -1), stamp(N, 0);
    int stc = 0;
    vector<int> lastpos(U, -1), fails(U, 0);
    vector<char> lastmv(U, 'H');
    vector<char> lastenem(N, 0);
    vector<int> estart, lastsee;
    bool haveStart = false;
    int lastseen_n = 0;
    vector<char> vis(N), isEnemy(N), still(N);
    vector<double> val(N), tv(N), valc(N), pre((R + 1) * W1), phi(N), phin(N);
    vector<int> dist(N), own(N), first(N), q, de(N), dd(N), qq;
    vector<St> beam, cand;
    vector<int> htab;
    while (getline(cin, line)) {
        if (line.compare(0, 8, "GAME_END") == 0) break;
        if (line.compare(0, 5, "ROUND") != 0) continue;
        int n = 0, cells = 0;
        {
            stringstream ss(line);
            string g;
            ss >> g >> n >> cells;
        }
        vector<int> pos(U, 0);
        fill(vis.begin(), vis.end(), 0);
        fill(isEnemy.begin(), isEnemy.end(), 0);
        vector<int> enemies;
        for (int k = 0; k < U; k++) {
            getline(cin, line);
            stringstream ss(line);
            string g;
            int ui = k, r = 0, c = 0;
            ss >> g >> ui >> r >> c;
            if (ui < 0 || ui >= U) ui = k;
            if (r < 0 || r >= R || c < 0 || c >= C) r = 0, c = 0;
            pos[ui] = r * C + c;
            for (int dr = -V; dr <= V; dr++) {
                string s;
                getline(cin, s);
                int rr = r + dr;
                if (rr < 0 || rr >= R) continue;
                for (int dc = -V; dc <= V; dc++) {
                    int cc = c + dc;
                    if (cc < 0 || cc >= C) continue;
                    if (dc + V >= (int)s.size()) continue;
                    char ch = s[dc + V];
                    int idx = rr * C + cc;
                    vis[idx] = 1;
                    seen[idx] = n;
                    if (ch == '#') {
                        wall[idx] = 1;
                        wall[N - 1 - idx] = 1;
                        continue;
                    }
                    if (wall[idx] != 0) wall[idx] = 0, wall[N - 1 - idx] = 0;
                    if (ch == '.') paint[idx] = 0;
                    else if (ch == '+' || ch == 'M') paint[idx] = 1;
                    else if (ch == '-') paint[idx] = 2;
                    else if (ch == 'E') {
                        paint[idx] = 2;
                        if (!isEnemy[idx]) isEnemy[idx] = 1, enemies.push_back(idx);
                    }
                }
            }
        }
        if (w.empty()) {
            double sr = 0, sc = 0;
            for (int x : pos) sr += x / C, sc += x % C;
            sr /= U, sc /= U;
            double er = R - 1 - sr, ec = C - 1 - sc;
            w.assign(N, 0);
            for (int i = 0; i < N; i++) {
                int r = i / C, c = i % C;
                double da = fabs(r - sr) + fabs(c - sc), db = fabs(r - er) + fabs(c - ec);
                w[i] = 0.3 + 1.4 * da / (da + db + 1e-9);
            }
        }
        int bp = 0, km = 0;
        double sw_plus = 0, sw_dot = 0;
        vector<int> opn;
        for (int i = 0; i < N; i++)
            if (wall[i] != 1) opn.push_back(i);
        for (int i : opn) {
            int pi = paint[i];
            if (pi == 1) {
                bp++;
                if (!vis[i]) sw_plus += (n - seen[i]) * w[i];
            } else if (pi == 2) km++;
            else if (!vis[i]) sw_dot += (seen[i] >= 0 ? n - seen[i] : n) * w[i];
        }
        bool bigreg = T > 12 && (int)opn.size() > 100;
        double EVV = bigreg ? 3.0 : 2.0;
        int lost = max(bp - cells, 0);
        int extra = max(cells - km - lost, 0);
        double kp = sw_plus > 0 ? lost / sw_plus : 0.0;
        double kd = sw_dot > 0 ? extra / sw_dot : 0.0;
        fill(val.begin(), val.end(), 0.0);
        for (int i : opn) {
            int wi = wall[i], pi = paint[i];
            double hf = bigreg ? (w[i] - 0.3) / 1.4 : 1.0;
            if (vis[i]) val[i] = pi == 0 ? hf : (pi == 2 ? EVV : 0.0);
            else if (pi == 2) val[i] = EVV;
            else if (pi == 1) {
                double qv = kp * (n - seen[i]) * w[i];
                val[i] = 2.0 * min(qv, 1.0);
            } else {
                double qv = kd * (seen[i] >= 0 ? n - seen[i] : n) * w[i];
                if (qv > 0.9) qv = 0.9;
                val[i] = (1.0 + qv) * (wi == 0 ? 1.0 : 0.8) * hf;
            }
        }
        fill(pre.begin(), pre.end(), 0.0);
        for (int r = 0; r < R; r++) {
            double acc = 0;
            int ro = r * C, a = (r + 1) * W1, b = r * W1;
            for (int c = 0; c < C; c++) {
                acc += val[ro + c];
                pre[a + c + 1] = pre[b + c + 1] + acc;
            }
        }
        int rem = T - n + 1;
        if (rem < 1) rem = 1;
        fill(dist.begin(), dist.end(), -1);
        q.clear();
        for (int ui = 0; ui < U; ui++) {
            int x = pos[ui];
            if (dist[x] < 0) dist[x] = 0, own[x] = ui, first[x] = x, q.push_back(x);
        }
        for (size_t h = 0; h < q.size(); h++) {
            int x = q[h], dx = dist[x] + 1, ox = own[x], fx = first[x];
            for (int y : adj[x])
                if (dist[y] < 0 && wall[y] != 1) {
                    dist[y] = dx, own[y] = ox, first[y] = dx == 1 ? y : fx;
                    q.push_back(y);
                }
        }
        fill(tv.begin(), tv.end(), 0.0);
        for (int x : q) {
            double v = val[x];
            if (v > 0.3) {
                auto &bx = boxi[x];
                tv[x] = v + 0.15 * (pre[bx[0]] - pre[bx[1]] - pre[bx[2]] + pre[bx[3]] - v);
            }
        }
        vector<int> bestt(U, -1);
        vector<double> bests(U, 0.0);
        for (int x : q) {
            int d = dist[x];
            if (d == 0 || d > rem) continue;
            double s = tv[x] / (d + 2);
            int o = own[x];
            if (s > bests[o]) bests[o] = s, bestt[o] = x;
        }
        vector<int> firststep(U, -1);
        for (int ui = 0; ui < U; ui++) {
            if (bestt[ui] >= 0 && bests[ui] >= 0.1) firststep[ui] = first[bestt[ui]];
            else {
                int src = pos[ui];
                stc++;
                stamp[src] = stc, fd[src] = 0, ffs[src] = src;
                qq.clear();
                qq.push_back(src);
                double bs = 0;
                int bt = -1;
                for (size_t hh = 0; hh < qq.size(); hh++) {
                    int x = qq[hh], d = fd[x] + 1;
                    if (d > rem) continue;
                    for (int y : adj[x])
                        if (stamp[y] != stc && wall[y] != 1) {
                            stamp[y] = stc, fd[y] = d, ffs[y] = d == 1 ? y : ffs[x];
                            qq.push_back(y);
                            double s = tv[y] / (d + 2);
                            if (s > bs) bs = s, bt = y;
                        }
                }
                if (bt >= 0) firststep[ui] = ffs[bt];
            }
        }
        valc = val;
        for (int e : enemies) valc[e] *= 0.5;
        for (int ui = 0; ui < U; ui++) {
            if (lastpos[ui] != pos[ui]) fails[ui] = 0;
            else if (lastmv[ui] != 'H') fails[ui]++;
        }
        fill(still.begin(), still.end(), 0);
        for (int e : enemies)
            if (lastenem[e]) still[e] = 1;
        fill(lastenem.begin(), lastenem.end(), 0);
        for (int e : enemies) lastenem[e] = 1;
        if (!haveStart) {
            haveStart = true;
            for (int x : pos) estart.push_back(N - 1 - x);
        }
        if (!enemies.empty()) lastsee = enemies, lastseen_n = n;
        bool shortg = T <= 12;
        bool small = (int)opn.size() <= 100;
        int HZMAX;
        double CONT, GDF, TB;
        int SLK;
        if (shortg) HZMAX = 8, CONT = 8.0, SLK = 0, GDF = 0.9, TB = 0.0;
        else HZMAX = small ? 3 : 12, CONT = 30.0, SLK = 1, GDF = 0.8, TB = small ? 3.0 : 0.0;
        int HZ = rem <= HZMAX ? rem : HZMAX;
        double GDv = rem <= HZMAX ? 1.0 : GDF;
        fill(de.begin(), de.end(), 1000000);
        for (int pass = 0; pass < 2; pass++) {
            vector<int> &sl = pass == 0 ? estart : lastsee;
            int off = pass == 0 ? n - 1 : n - lastseen_n;
            int lim = off + HZ + SLK;
            fill(dd.begin(), dd.end(), -1);
            qq.clear();
            for (int sx : sl)
                if (wall[sx] != 1 && dd[sx] < 0) dd[sx] = 0, qq.push_back(sx);
            for (size_t hh = 0; hh < qq.size(); hh++) {
                int x = qq[hh], dx = dd[x], v = dx - off;
                if (v < de[x]) de[x] = v;
                if (dx >= lim) continue;
                dx++;
                for (int y : adj[x])
                    if (dd[y] < 0 && wall[y] != 1) dd[y] = dx, qq.push_back(y);
            }
        }
        auto cmul = [&](int dv, int t) -> double { return dv <= t + SLK ? CONT : 1.0; };
        if (TB > 0) {
            fill(phi.begin(), phi.end(), 0.0);
            priority_queue<pair<double, int>> pq;
            for (int i : opn)
                if (valc[i] > 0) phi[i] = valc[i], pq.push({valc[i], i});
            while (!pq.empty()) {
                auto [v, x] = pq.top();
                pq.pop();
                if (v < phi[x]) continue;
                double nv = v * 0.8;
                if (nv < 0.01) continue;
                for (int y : adj[x])
                    if (wall[y] != 1 && phi[y] < nv) phi[y] = nv, pq.push({nv, y});
            }
            for (int i : opn) {
                double m = 0;
                for (int y : adj[i])
                    if (wall[y] != 1 && phi[y] > m) m = phi[y];
                phin[i] = m * 0.8;
            }
        }
        string orders = "ORDERS";
        vector<int> planned;
        auto inv = [&](const vector<int> &v, int x) { return find(v.begin(), v.end(), x) != v.end(); };
        for (int ui = 0; ui < U; ui++) {
            int x0 = pos[ui];
            vector<int> blocked = planned;
            for (int uj = ui + 1; uj < U; uj++) blocked.push_back(pos[uj]);
            int fs = firststep[ui];
            beam.clear();
            for (int k = 0; k < 4; k++) {
                int y = nbr[x0][k];
                if (y < 0 || wall[y] == 1 || inv(blocked, y)) continue;
                double s = valc[y] > 0 ? valc[y] * cmul(de[y], 1) : 0.0;
                if (bigreg && valc[y] > 0) {
                    int cnt = 4 - (int)adj[y].size();
                    for (int z : adj[y])
                        if (z != x0 && (wall[z] == 1 || (paint[z] == 1 && valc[z] <= 0))) cnt++;
                    s += 0.15 * cnt;
                }
                if (y == fs) s += 0.01;
                if (isEnemy[y]) s -= still[y] ? 3.0 : 0.6;
                if (fails[ui] && lastmv[ui] == DIRS[k]) s -= 2.5 * fails[ui];
                St st;
                st.sc = s, st.x = y, st.f = k, st.len = 1, st.path[0] = (short)y;
                st.h = zob[x0] ^ zob[y];
                beam.push_back(st);
            }
            if (beam.empty()) {
                planned.push_back(x0);
                orders += " H";
                lastpos[ui] = x0, lastmv[ui] = 'H';
                continue;
            }
            int BW = (int)(min(10000.0, 600000.0 / T) / (3.0 * HZ * U) * 10.0);
            if (BW > 3000) BW = 3000;
            if (BW < 20) BW = 20;
            double gm = 1.0;
            for (int t = 2; t <= HZ; t++) {
                gm *= GDv;
                cand.clear();
                size_t need = beam.size() * 8 + 16, hs = 1;
                while (hs < need) hs <<= 1;
                if (htab.size() < hs) htab.resize(hs);
                fill(htab.begin(), htab.begin() + hs, -1);
                for (auto &b : beam) {
                    for (int y : adj[b.x]) {
                        if (wall[y] == 1) continue;
                        bool in = (y == x0);
                        for (int j = 0; j < b.len && !in; j++)
                            if (b.path[j] == y) in = true;
                        double ns;
                        u64 nh;
                        if (in) ns = b.sc, nh = b.h;
                        else {
                            double vy = valc[y] > 0 ? valc[y] : 0.0;
                            vy *= cmul(de[y], t);
                            if (bigreg && dist[y] >= 0 && own[y] != ui) vy *= 0.8;
                            if (bigreg && vy > 0) {
                                int cnt = 4 - (int)adj[y].size();
                                for (int z : adj[y]) {
                                    if (z == b.x) continue;
                                    bool blk = wall[z] == 1 || (paint[z] == 1 && valc[z] <= 0) || z == x0;
                                    for (int j = 0; j < b.len && !blk; j++)
                                        if (b.path[j] == z) blk = true;
                                    if (blk) cnt++;
                                }
                                vy += 0.15 * cnt;
                            }
                            ns = b.sc + vy * gm;
                            nh = b.h ^ zob[y];
                        }
                        u64 key = nh * 0x9E3779B97F4A7C15ULL ^ (u64)(y + 1) * 0xC2B2AE3D27D4EB4FULL;
                        size_t idx = (key >> 17) & (hs - 1);
                        int found = -1;
                        while (htab[idx] >= 0) {
                            St &o = cand[htab[idx]];
                            if (o.x == y && o.h == nh) {
                                found = htab[idx];
                                break;
                            }
                            idx = (idx + 1) & (hs - 1);
                        }
                        if (found >= 0) {
                            St &o = cand[found];
                            if (o.sc < ns) {
                                o.sc = ns, o.f = b.f, o.len = b.len + 1;
                                memcpy(o.path, b.path, sizeof(short) * b.len);
                                o.path[b.len] = (short)y;
                            }
                        } else {
                            htab[idx] = (int)cand.size();
                            St o;
                            o.sc = ns, o.x = y, o.h = nh, o.f = b.f, o.len = b.len + 1;
                            memcpy(o.path, b.path, sizeof(short) * b.len);
                            o.path[b.len] = (short)y;
                            cand.push_back(o);
                        }
                    }
                }
                if ((int)cand.size() > BW) {
                    nth_element(cand.begin(), cand.begin() + BW, cand.end(), [](const St &a, const St &b) { return a.sc > b.sc; });
                    cand.resize(BW);
                }
                swap(beam, cand);
            }
            if (TB > 0)
                for (auto &b : beam) b.sc += TB * gm * GDv * phin[b.x];
            int bi = 0;
            for (int i = 1; i < (int)beam.size(); i++)
                if (beam[i].sc > beam[bi].sc) bi = i;
            int k = beam[bi].f;
            planned.push_back(nbr[x0][k]);
            for (int j = 0; j < beam[bi].len; j++) valc[beam[bi].path[j]] = 0.0;
            orders += ' ';
            orders += DIRS[k];
            lastpos[ui] = x0, lastmv[ui] = DIRS[k];
        }
        cout << orders << '\n' << flush;
    }
    return 0;
}
