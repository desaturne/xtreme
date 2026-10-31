#include <bits/stdc++.h>
using namespace std;
typedef long long ll;
const ll MOD = 998244353;

ll pw(ll b, ll e) { b %= MOD; ll r = 1; while (e) { if (e & 1) r = r * b % MOD; b = b * b % MOD; e >>= 1; } return r; }

int main() {
    int n, m;
    scanf("%d %d", &n, &m);
    vector<ll> mulD(m + 2, 1), sumD(m + 2, 0);
    vector<int> zD(m + 2, 0);
    for (int j = 0; j < n; j++) {
        ll l, r, p, q;
        scanf("%lld %lld %lld %lld", &l, &r, &p, &q);
        if (p == q) { zD[l]++; zD[r + 1]--; continue; }
        ll prob = p % MOD * pw(q, MOD - 2) % MOD;
        ll notp = (1 - prob + MOD) % MOD;
        ll inv = pw(notp, MOD - 2);
        mulD[l] = mulD[l] * notp % MOD;
        mulD[r + 1] = mulD[r + 1] * inv % MOD;
        ll ratio = prob * inv % MOD;
        sumD[l] = (sumD[l] + ratio) % MOD;
        sumD[r + 1] = (sumD[r + 1] - ratio + MOD) % MOD;
    }
    ll P = 1, S = 0; int Z = 0;
    string out;
    out.reserve((size_t)m * 11);
    for (int i = 1; i <= m; i++) {
        P = P * mulD[i] % MOD;
        S = (S + sumD[i]) % MOD;
        Z += zD[i];
        ll ans;
        if (Z == 0) ans = P * ((1 + S) % MOD) % MOD;
        else if (Z == 1) ans = P;
        else ans = 0;
        out += to_string(ans);
        out += (i == m ? '\n' : ' ');
    }
    fputs(out.c_str(), stdout);
    return 0;
}
