// Auction Duel bot: models the opponent's bidding from its observed bids,
// samples the opponent's hidden value permutation, and picks each bid by
// Monte Carlo rollouts of the rest of the game.
#pragma GCC optimize("O3,unroll-loops")
#include <bits/stdc++.h>
using namespace std;

static double BUDGET = 0.6;    // seconds of thinking per game (soft)
static int JOINT = 1;
static int NFAM = 5;
static int HALVE_FIRST = 0;    // first successive-halving cut (rollouts); 0 = off
// tunables: spite weight, budget cap mult, early-bonus share, first-price fallback, default a(2nd), default a(1st), default sigma, sigma mult
// [10] sigma floor, [11] 1 = optimise clipped points instead of raw difference
// [12] budget cap mult for second-price games ([1] is used for first-price)
// [13] / [14] default opponent family before 2 observations (second / first price)
static double P[15] = {0.5, 1.0, 0.6, 0.5, 0.5, 0.5, 0.35, 0.8, 0.0, 0.0, 0.3, 1.0, 2.0, 1, 2};
// per-rule settings (copied into P when the game's rule is known)
static double PF[15] = {0.5, 1.0, 0.6, 0.5, 0.5, 0.5, 0.35, 0.8, 0.0, 0.0, 0.3, 1.0, 2.0, 1, 2};
static double PS[15] = {0.5, 1.0, 0.6, 0.5, 0.5, 0.5, 0.35, 0.8, 0.0, 0.0, 0.3, 1.0, 2.0, 1, 2};
static int TCANDS = 1;   // add candidate bids around the dominant target t
double Stot = 1;

struct Obs { int g; double oc; int own, se; vector<int> rem; int b; };
struct Model { int fam; double a, sig; };

int K, M, rounds_total;
long long cash0, cashv, opp_cash;
double bf, meanv;
bool firstPrice;
vector<double> vals, sv;
vector<int> seen_, my_cnt, op_cnt;
vector<vector<int>> op_bids;
vector<Obs> obs;
int curg = 0;
mt19937_64 rng(1234567);
chrono::steady_clock::time_point t0;

// CPU time used by this process. Waiting for the judge / opponent does not count, so a
// slow opponent no longer eats our thinking budget.
double now() {
    timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) return ts.tv_sec + ts.tv_nsec * 1e-9;
    return chrono::duration<double>(chrono::steady_clock::now() - t0).count();
}
double urand() { return (rng() >> 11) * (1.0 / 9007199254740992.0); }
double gauss() {
    double u1 = urand(), u2 = urand();
    if (u1 < 1e-300) u1 = 1e-300;
    return sqrt(-2.0 * log(u1)) * cos(2 * M_PI * u2);
}

// ---------- opponent values (rank match on average bids) ----------
vector<double> est_w() {
    vector<pair<double,int>> o;
    for (int g = 0; g < K; g++) if (!op_bids[g].empty()) {
        double s = 0; for (int b : op_bids[g]) s += b;
        o.push_back({s / op_bids[g].size(), g});
    }
    vector<double> w(K, meanv);
    int n = o.size();
    if (!n) return w;
    sort(o.begin(), o.end());
    for (int r = 0; r < n; r++) {
        int idx = (int)nearbyint((r + 0.5) / n * K - 0.5);
        idx = max(0, min(K - 1, idx));
        w[o[r].second] = (sv[idx] * n + meanv) / (n + 1);
    }
    return w;
}

// base bid for value v under family fam
inline double base_of(int fam, double v, double oc, int own, int se, double remv_other, int remg, double myv) {
    double tb = v + ((own == se && M - se > 0) ? v * bf * M / (M - se) : 0.0);
    switch (fam) {
        case 0: return v;
        case 1: return tb;
        case 2: { double rv = remv_other + v * remg; return rv > 0 ? oc * v / rv : oc; }
        case 3: return tb + meanv;
        default: return tb + myv;   // spite bidder that knows my value
    }
}

// Joint maximum-likelihood fit of (family, scale) and the opponent's value permutation.
// wref is only used for the remaining-value sum of the pacing family.
Model fit_joint(const vector<double>& wref, vector<double>& wout) {
    Model def = firstPrice ? Model{(int)P[14], P[5], P[6] + 0.1} : Model{(int)P[13], P[4], P[6]};
    wout = wref;
    if (obs.size() < 2) return def;
    static double E[8][8];
    double bestErr = 1e18; Model best = def; vector<double> bestW = wref;
    int full = (1 << K) - 1;
    static double dp[256]; static int par[8][256];
    for (int fam = 0; fam < NFAM; fam++) {
        // precompute base for each obs and each candidate value
        vector<array<double,8>> base(obs.size());
        for (size_t o = 0; o < obs.size(); o++) {
            auto& ob = obs[o];
            double rv = 0; for (int i = 0; i < K; i++) rv += wref[i] * ob.rem[i];
            rv -= wref[ob.g] * ob.rem[ob.g];
            for (int j = 0; j < K; j++) base[o][j] = base_of(fam, sv[j], ob.oc, ob.own, ob.se, rv, ob.rem[ob.g], vals[ob.g]);
        }
        for (int ai = 0; ai < 48; ai++) {
            double a = 0.12 * pow(40.0, ai / 47.0);   // 0.12 .. 4.8
            for (int g = 0; g < K; g++) for (int j = 0; j < K; j++) E[g][j] = 0;
            for (size_t o = 0; o < obs.size(); o++) {
                auto& ob = obs[o];
                for (int j = 0; j < K; j++) {
                    double p = min(ob.oc, a * base[o][j]);
                    E[ob.g][j] += fabs(log((ob.b + 1.0) / (p + 1.0)));
                }
            }
            // assignment DP: goods in order 0..K-1, mask = used value indices
            for (int m = 0; m <= full; m++) dp[m] = 1e300;
            dp[0] = 0;
            for (int m = 0; m < full; m++) {
                if (dp[m] >= 1e299) continue;
                int g = __builtin_popcount(m);
                for (int j = 0; j < K; j++) if (!(m >> j & 1)) {
                    double v = dp[m] + E[g][j];
                    if (v < dp[m | 1 << j]) { dp[m | 1 << j] = v; par[g][m | 1 << j] = j; }
                }
            }
            double err = dp[full] / obs.size();
            if (err < bestErr - 1e-12) {
                bestErr = err;
                best = {fam, a, max(P[10], err * P[7])};
                int m = full;
                for (int g = K - 1; g >= 0; g--) { int j = par[g][m]; bestW[g] = sv[j]; m ^= 1 << j; }
            }
        }
    }
    wout = bestW;
    return best;
}

Model fit_model(const vector<double>& w) {
    Model def = firstPrice ? Model{(int)P[14], P[5], P[6] + 0.1} : Model{(int)P[13], P[4], P[6]};
    if (obs.size() < 2) return def;
    bool have = false; Model best{}; double bestErr = 1e18;
    for (int fam = 0; fam < 4; fam++) {
        vector<double> ratios; vector<array<double,3>> rows;
        for (auto& ob : obs) {
            double rv = 0; for (int i = 0; i < K; i++) rv += w[i] * ob.rem[i];
            rv -= w[ob.g] * ob.rem[ob.g];
            double base = base_of(fam, w[ob.g], ob.oc, ob.own, ob.se, rv, ob.rem[ob.g], vals[ob.g]);
            if (fam != 2) base = min(base, ob.oc);
            if (base > 0) {
                rows.push_back({base, (double)ob.b, ob.oc});
                if (ob.b < ob.oc) ratios.push_back(ob.b / base);
            }
        }
        if (rows.size() < 2 || ratios.empty()) continue;
        sort(ratios.begin(), ratios.end());
        double a = ratios[ratios.size() / 2];
        double err = 0;
        for (auto& r : rows) { double p = min(r[2], a * r[0]); err += fabs(log((r[1] + 1) / (p + 1))); }
        err /= rows.size();
        if (err < bestErr) { bestErr = err; best = {fam, a, max(P[10], err * P[7])}; have = true; }
    }
    return have ? best : def;
}

vector<vector<double>> perm_matrix(const vector<double>& w, const Model& m) {
    vector<vector<double>> C(K, vector<double>(K, 0.0));
    for (auto& ob : obs) {
        double rv = 0; for (int i = 0; i < K; i++) rv += w[i] * ob.rem[i];
        rv -= w[ob.g] * ob.rem[ob.g];
        for (int j = 0; j < K; j++) {
            double base = base_of(m.fam, sv[j], ob.oc, ob.own, ob.se, rv, ob.rem[ob.g], vals[ob.g]);
            double p = min(ob.oc, m.a * base);
            C[ob.g][j] -= fabs(log((ob.b + 1.0) / (p + 1.0))) / m.sig;
        }
    }
    return C;
}

void sample_w(const vector<vector<double>>& C, vector<double>& w) {
    int goods[8], unused[8], nu = K;
    for (int i = 0; i < K; i++) goods[i] = i, unused[i] = i;
    for (int i = K - 1; i > 0; i--) swap(goods[i], goods[rng() % (i + 1)]);
    for (int t = 0; t < K; t++) {
        int g = goods[t];
        double mx = -1e300, ws[8], sum = 0;
        for (int k = 0; k < nu; k++) mx = max(mx, C[g][unused[k]]);
        for (int k = 0; k < nu; k++) { ws[k] = exp(C[g][unused[k]] - mx); sum += ws[k]; }
        double r = urand() * sum; int k = 0;
        while (k < nu - 1 && r > ws[k]) { r -= ws[k]; k++; }
        w[g] = sv[unused[k]];
        unused[k] = unused[--nu];
    }
}

// ---------- my heuristic ----------
inline double eff(double v, int own, int se) {
    if (own != se) return v;
    int rem = M - se;
    double bonus = v * bf * M;
    return rem <= 1 ? v + bonus : v + bonus / rem * P[2];
}

inline long long heur_r(int g, long long cash, int mycnt_g, int opcnt_g, int seen_g,
                        double remv, double wg, double pred) {
    double ve = eff(vals[g], mycnt_g, seen_g);
    double we = eff(wg, opcnt_g, seen_g);
    double t = ve * (1 - P[0]) + we * P[0];
    double cap = remv > 0 ? max(cash * (firstPrice ? P[1] : P[12]) * ve / (remv + ve * 1.5 * bf), cash * vals[g] / remv) : (double)cash;
    double bid = firstPrice ? (pred + 1 <= t ? pred + 1 : min(t * P[3], ve * P[3])) : t;
    // second price, model-aware: if the opponent will outbid my target anyway, push its price up
    if (!firstPrice && P[8] > 0 && pred * P[8] > t) bid = pred * P[8];
    // second price: if the opponent is predicted well below target, my bid only needs to clear it
    if (!firstPrice && P[9] > 0 && pred * P[9] < t) bid = max(pred * P[9], 0.0) + 1;
    double b = min((double)cash, min(bid, cap));
    return b > 0 ? (long long)b : 0;
}

inline long long heur(int g, long long cash, const int* mycnt, const int* opcnt, const int* seen,
                      const int* rem, const double* w, double pred) {
    double remv = 0;
    for (int i = 0; i < K; i++) remv += vals[i] * rem[i];
    return heur_r(g, cash, mycnt[g], opcnt[g], seen[g], remv, w[g], pred);
}

// ---------- rollout ----------
double rollout(int g0, long long mybid, const vector<double>& w, const Model& m,
               const vector<int>& order, const double* mult, const double* coin) {
    long long cash = cashv, oc = opp_cash;
    int mycnt[8], opcnt[8], seen[8];
    double remv = 0, rv = 0;   // my / opponent remaining value, kept incrementally
    for (int i = 0; i < K; i++) {
        mycnt[i] = my_cnt[i]; opcnt[i] = op_cnt[i]; seen[i] = seen_[i];
        remv += vals[i] * (M - seen[i]); rv += w[i] * (M - seen[i]);
    }
    const int fam = m.fam; const double a = m.a, bfM = bf * M;
    const double extra = fam == 3 ? meanv : 0.0;
    const int n = order.size();
    for (int step = -1; step < n; step++) {
        const int g = step < 0 ? g0 : order[step];
        const int se = seen[g];
        const double wg = w[g];
        double base;
        if (fam == 0) base = wg;
        else if (fam == 2) base = rv > 0 ? oc * wg / rv : oc;
        else {
            base = wg + (opcnt[g] == se ? wg * bfM / (M - se) : 0.0);
            if (fam == 3) base += extra; else if (fam == 4) base += vals[g];
        }
        const double mean = a * base;
        double obd = mean * mult[step + 1];
        if (!(obd >= 0)) obd = 0;          // also catches NaN
        if (obd > oc) obd = oc;
        const long long ob = (long long)obd;
        const long long x = step < 0 ? mybid
            : heur_r(g, cash, mycnt[g], opcnt[g], se, remv, wg, mean < oc ? mean : (double)oc);
        if (x > ob || (x == ob && coin[step + 1] < 0.5)) { cash -= firstPrice ? x : ob; mycnt[g]++; }
        else { oc -= firstPrice ? ob : x; opcnt[g]++; }
        seen[g] = se + 1; remv -= vals[g]; rv -= wg;
    }
    double d = (double)(cash - oc);
    for (int i = 0; i < K; i++) {
        d += vals[i] * mycnt[i] * (mycnt[i] == M ? 1 + bf : 1);
        d -= w[i] * opcnt[i] * (opcnt[i] == M ? 1 + bf : 1);
    }
    if (P[11] > 0) { double pts = 5 + 10 * d / Stot; return pts < 0 ? 0 : pts > 10 ? 10 : pts; }
    return d;
}

long long choose_bid(int n, int g) {
    curg = g;
    vector<double> w0 = est_w(), w1, w;
    Model m;
    if (JOINT) {
        fit_joint(w0, w1);
        m = fit_joint(w1, w);
        if (obs.size() < 2) w = w0;
    } else { w = w0; m = fit_model(w); }
    static vector<double> oracle;   // test-only: AD_ORACLE="w1 w2 ..." gives the true opponent values
    if (oracle.empty()) if (const char* e = getenv("AD_ORACLE")) { istringstream os(e); double x; while (os >> x) oracle.push_back(x); }
    if ((int)oracle.size() == K) w = oracle;
    vector<int> rem(K);
    for (int i = 0; i < K; i++) rem[i] = M - seen_[i];
    double rv = 0; for (int i = 0; i < K; i++) rv += w[i] * rem[i];
    rv -= w[g] * rem[g];
    double pred = min((double)opp_cash, m.a * base_of(m.fam, w[g], opp_cash, op_cnt[g], seen_[g], rv, rem[g], vals[g]));
    long long hb = heur(g, cashv, my_cnt.data(), op_cnt.data(), seen_.data(), rem.data(), w.data(), pred);

    set<long long> c = {0, hb, cashv};
    for (double f : {0.6, 0.7, 0.8, 0.85, 0.9, 0.95, 0.97, 1.0, 1.03, 1.05, 1.1, 1.2, 1.35, 1.5}) {
        long long p = (long long)(pred * f); c.insert(p); c.insert(p + 1); c.insert(p - 1);
    }
    for (double f : {0.25, 0.5, 0.75, 0.9, 1.1, 1.25, 1.5, 2.0}) c.insert((long long)(hb * f));
    for (double f : {0.1, 0.25, 0.5}) c.insert((long long)(cashv * f));
    if (TCANDS) {
        double t = eff(vals[g], my_cnt[g], seen_[g]) * (1 - P[0]) + eff(w[g], op_cnt[g], seen_[g]) * P[0];
        for (double f : {0.8, 0.9, 0.95, 1.0, 1.05, 1.1}) c.insert((long long)(t * f));
    }
    vector<long long> cands;
    for (long long x : c) if (x >= 0 && x <= cashv) cands.push_back(x);

    int rounds_left = rounds_total - n + 1;
    double tb = max(0.001, (BUDGET - now()) / max(1, rounds_left));
    double dl = now() + tb;

    vector<int> rest;
    for (int i = 0; i < K; i++) for (int k = 0; k < rem[i]; k++) rest.push_back(i);
    { auto it = find(rest.begin(), rest.end(), g); if (it != rest.end()) rest.erase(it); }
    if (rest.size() > 62) rest.resize(62);

    auto C = perm_matrix(w, m);
    if ((int)oracle.size() == K) for (int i = 0; i < K; i++) for (int j = 0; j < K; j++) C[i][j] = (sv[j] == oracle[i]) ? 0 : -1e9;
    vector<double> score(cands.size(), 0.0);
    vector<int> active(cands.size());
    iota(active.begin(), active.end(), 0);
    vector<double> ws(K);
    double mult[64], coin[64];
    int cnt = 0, nextCut = HALVE_FIRST;
    while (true) {
        sample_w(C, ws);
        vector<int> order = rest;
        for (int i = (int)order.size() - 1; i > 0; i--) swap(order[i], order[rng() % (i + 1)]);
        for (size_t k = 0; k <= order.size(); k++) { mult[k] = exp(gauss() * m.sig); coin[k] = urand(); }
        for (int i : active) score[i] += rollout(g, cands[i], ws, m, order, mult, coin);
        cnt++;
        // successive halving: all active candidates share the same rollouts, so totals compare fairly
        if (HALVE_FIRST > 0 && cnt == nextCut && active.size() > 3) {
            sort(active.begin(), active.end(), [&](int a, int b) { return score[a] > score[b]; });
            active.resize(max<size_t>(3, (active.size() + 1) / 2));
            nextCut = cnt * 2;
        }
        if (cnt >= 50000) break;
        if (cnt >= 2 && (cnt & 7) == 0 && now() > dl) break;
        if (cnt >= 2 && now() > BUDGET * 1.25) break;
    }
    if (getenv("AD_DBG")) fprintf(stderr, "rolls %d\n", cnt);
    int bi = active[0];
    for (int i : active) {
        if (score[i] > score[bi] + 1e-9 ||
            (fabs(score[i] - score[bi]) <= 1e-9 && llabs(cands[i] - hb) < llabs(cands[bi] - hb))) bi = i;
    }
    return cands[bi];
}

void on_result(int won, int ob, int price) {
    int g = curg;
    Obs o; o.g = g; o.oc = opp_cash; o.own = op_cnt[g]; o.se = seen_[g];
    o.rem.resize(K); for (int i = 0; i < K; i++) o.rem[i] = M - seen_[i];
    o.b = ob;
    obs.push_back(o);
    seen_[g]++;
    op_bids[g].push_back(ob);
    if (won) { my_cnt[g]++; cashv -= price; } else { op_cnt[g]++; opp_cash -= price; }
    if (cashv < 0) cashv = 0;
    if (opp_cash < 0) opp_cash = 0;
}

int main() {
    t0 = chrono::steady_clock::now();
    if (const char* e = getenv("AD_BUDGET")) BUDGET = atof(e);
    if (const char* e = getenv("AD_JOINT")) JOINT = atoi(e);
    if (const char* e = getenv("AD_NFAM")) NFAM = atoi(e);
    if (const char* e = getenv("AD_HALVE")) HALVE_FIRST = atoi(e);
    if (const char* e = getenv("AD_P")) { string t(e); for (char& ch : t) if (ch == ',') ch = ' ';
        istringstream ps(t); for (int i = 0; i < 15 && (ps >> P[i]); i++);
        memcpy(PF, P, sizeof P); memcpy(PS, P, sizeof P); }
    auto rd = [](const char* name, double* arr) {
        const char* e = getenv(name);
        if (!e) return;
        string t(e);
        for (char& ch : t) if (ch == ',') ch = ' ';
        istringstream ps(t);
        for (int i = 0; i < 15 && (ps >> arr[i]); i++) {}
    };
    rd("AD_PF", PF); rd("AD_PS", PS);
    if (const char* e = getenv("AD_TC")) TCANDS = atoi(e);
    string line;
    while (getline(cin, line)) {
        istringstream ss(line);
        string cmd;
        if (!(ss >> cmd)) continue;
        if (cmd == "GAME") {
            long long B; string rule;
            ss >> K >> M >> cash0 >> B >> rule;
            K = max(1, min(K, 8)); M = max(1, M);
            cashv = opp_cash = cash0; bf = B / 100.0; firstPrice = (rule == "first");
            memcpy(P, firstPrice ? PF : PS, sizeof P);
            rounds_total = K * M;
            seen_.assign(K, 0); my_cnt.assign(K, 0); op_cnt.assign(K, 0); op_bids.assign(K, {});
        } else if (cmd == "VALUES") {
            vals.assign(K, 0);
            for (int i = 0; i < K; i++) ss >> vals[i];
            sv = vals; sort(sv.begin(), sv.end());
            meanv = accumulate(vals.begin(), vals.end(), 0.0) / K;
            Stot = max(1.0, meanv * K * M);
            rng.seed(1234567 + (uint64_t)cash0 * 31 + K * 7 + M);
        } else if (cmd == "ROUND") {
            int n, g; ss >> n >> g; g--;
            long long b = 0;
            if (g >= 0 && g < K && seen_[g] < M) {
                try { b = choose_bid(n, g); } catch (...) { b = 0; }
            }
            curg = (g >= 0 && g < K) ? g : 0;
            if (b < 0) b = 0;
            if (b > cashv) b = cashv;
            printf("BID %lld\n", b);
            fflush(stdout);
        } else if (cmd == "RESULT") {
            int won, ob, price; ss >> won >> ob >> price;
            on_result(won, ob, price);
        } else if (cmd == "GAME_END") {
            break;
        }
    }
    return 0;
}
