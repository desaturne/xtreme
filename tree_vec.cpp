#include <bits/stdc++.h>
using namespace std;
typedef long long ll;
typedef __int128 L;
struct P { ll x, y; };
static inline L cr(P a, P b) { return (L)a.x * b.y - (L)a.y * b.x; }
static inline P sub(P a, P b) { return {a.x - b.x, a.y - b.y}; }
static inline P add(P a, P b) { return {a.x + b.x, a.y + b.y}; }

vector<P> hull(vector<P> v) {
    sort(v.begin(), v.end(), [](const P& a, const P& b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
    v.erase(unique(v.begin(), v.end(), [](const P& a, const P& b) { return a.x == b.x && a.y == b.y; }), v.end());
    if (v.size() <= 1) return v;
    vector<P> h(2 * v.size());
    int k = 0;
    for (size_t i = 0; i < v.size(); i++) {
        while (k >= 2 && cr(sub(h[k - 1], h[k - 2]), sub(v[i], h[k - 2])) <= 0) k--;
        h[k++] = v[i];
    }
    for (int i = (int)v.size() - 2, t = k + 1; i >= 0; i--) {
        while (k >= t && cr(sub(h[k - 1], h[k - 2]), sub(v[i], h[k - 2])) <= 0) k--;
        h[k++] = v[i];
    }
    h.resize(k - 1);
    return h;
}

void reorder(vector<P>& p) {
    size_t s = 0;
    for (size_t i = 1; i < p.size(); i++)
        if (p[i].y < p[s].y || (p[i].y == p[s].y && p[i].x < p[s].x)) s = i;
    rotate(p.begin(), p.begin() + s, p.end());
}

vector<P> mink(vector<P> a, vector<P> b) {
    if (a.size() == 1) { for (auto& q : b) q = add(q, a[0]); return b; }
    if (b.size() == 1) { for (auto& q : a) q = add(q, b[0]); return a; }
    reorder(a); reorder(b);
    size_t n = a.size(), m = b.size(), i = 0, j = 0;
    vector<P> r;
    r.reserve(n + m);
    while (i < n || j < m) {
        r.push_back(add(a[i % n], b[j % m]));
        P e1 = sub(a[(i + 1) % n], a[i % n]), e2 = sub(b[(j + 1) % m], b[j % m]);
        L c = cr(e1, e2);
        if (j == m || (i < n && c > 0)) i++;
        else if (i == n || c < 0) j++;
        else { i++; j++; }
    }
    return r;
}

int main() {
    int n;
    if (scanf("%d", &n) != 1) return 0;
    vector<int> par(n + 1);
    vector<P> w(n + 1);
    vector<vector<int>> ch(n + 1);
    int root = 1;
    for (int v = 1; v <= n; v++) {
        scanf("%d %lld %lld", &par[v], &w[v].x, &w[v].y);
        if (par[v] == 0) root = v; else ch[par[v]].push_back(v);
    }
    vector<int> ord{root};
    for (size_t i = 0; i < ord.size(); i++)
        for (int c : ch[ord[i]]) ord.push_back(c);
    vector<vector<P>> poly(n + 1);
    for (int i = n - 1; i >= 0; i--) {
        int v = ord[i];
        vector<P> cur{w[v]};
        for (int c : ch[v]) {
            vector<P> q = move(poly[c]);
            q.push_back({0, 0});
            cur = mink(cur, hull(q));
            vector<P>().swap(poly[c]);
        }
        poly[v] = hull(cur);
    }
    L best = 0;
    for (auto& p : poly[root]) {
        L val = (L)p.x * p.x + (L)p.y * p.y;
        if (val > best) best = val;
    }
    if (best == 0) { puts("0"); return 0; }
    string s;
    while (best > 0) { s += char('0' + (int)(best % 10)); best /= 10; }
    reverse(s.begin(), s.end());
    puts(s.c_str());
}
