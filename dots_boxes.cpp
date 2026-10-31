// Dots and Boxes bot
// alpha-beta (negamax) + transposition table, Barker-Korf capture reductions,
// exact chain/loop endgame DP, playout-parity evaluation in the safe-move phase.
#include <bits/stdc++.h>
using namespace std;
typedef unsigned long long u64;

static int R, C, NH, E, NBX;
static int eb[220][2];  // boxes adjacent to an edge (-1 = outside)
static int be[100][4];  // edges of a box
static bool drawn[220];
static int sides[100];
static u64 zob[220], hsh = 0;
static int undrawnCnt = 0;
static int perm[4][220];
static int FULLSAC_N = 32;
static int NPLAY = 2;          // playouts per heuristic leaf
static double TOTAL = 1.5;     // total think budget per game (s)
static double PERMOVE = 0.12;  // per-move cap (s)

static inline int other(int e, int b) { return eb[e][0] == b ? eb[e][1] : eb[e][0]; }
static inline int freeEdge(int b, int except = -1) {
    for (int k = 0; k < 4; k++) {
        int e = be[b][k];
        if (!drawn[e] && e != except) return e;
    }
    return -1;
}
static inline int drawE(int e) {
    drawn[e] = 1, hsh ^= zob[e], undrawnCnt--;
    int c = 0;
    for (int j = 0; j < 2; j++) {
        int b = eb[e][j];
        if (b >= 0 && ++sides[b] == 4) c++;
    }
    return c;
}
static inline void undrawE(int e) {
    drawn[e] = 0, hsh ^= zob[e], undrawnCnt++;
    for (int j = 0; j < 2; j++) {
        int b = eb[e][j];
        if (b >= 0) sides[b]--;
    }
}
static inline bool isSafe(int e) {
    for (int j = 0; j < 2; j++) {
        int b = eb[e][j];
        if (b >= 0 && sides[b] >= 2) return false;
    }
    return true;
}

// ---------------- chain structure ----------------
struct Chain {
    int len;
    bool loop;
    int e1, e2, mid;
};
static int visMark[100], visStamp = 0;
// requires: no 3-sided boxes. Fills chains; returns whether a junction (box with <=1 sides) exists.
static bool getChains(vector<Chain> &ch) {
    ch.clear();
    visStamp++;
    bool junction = false;
    for (int b = 0; b < NBX; b++) {
        if (sides[b] <= 1) junction = true;
        if (sides[b] != 2 || visMark[b] == visStamp) continue;
        // walk one direction to find an end
        int f0 = freeEdge(b), f1 = freeEdge(b, f0);
        int cur = b, e = f1;
        bool loop = false;
        while (true) {
            int n = other(e, cur);
            if (n == b) { loop = true; break; }
            if (n >= 0 && sides[n] == 2) {
                int ne = freeEdge(n, e);
                cur = n, e = ne;
            } else break;
        }
        Chain c;
        c.loop = loop;
        c.mid = -1;
        if (loop) {
            int len = 0;
            cur = b, e = f0;
            do {
                visMark[cur] = visStamp;
                len++;
                int n = other(e, cur);
                int ne = freeEdge(n, e);
                cur = n, e = ne;
            } while (cur != b);
            c.len = len, c.e1 = f0, c.e2 = f0;
        } else {
            c.e1 = e;  // edge from end box 'cur' to outside
            int len = 0;
            int ee = e;
            while (true) {
                visMark[cur] = visStamp;
                len++;
                int ne = freeEdge(cur, ee);
                int n = other(ne, cur);
                if (n >= 0 && sides[n] == 2 && visMark[n] != visStamp) {
                    if (len == 1) c.mid = ne;
                    cur = n, ee = ne;
                } else {
                    c.e2 = ne;
                    break;
                }
            }
            c.len = len;
        }
        ch.push_back(c);
    }
    return junction;
}

// ---------------- chain/loop DP ----------------
static unordered_map<string, int> dpMemo;
static int dpf(string &v) {  // sorted multiset; loops encoded len|128
    if (v.empty()) return 0;
    auto it = dpMemo.find(v);
    if (it != dpMemo.end()) return it->second;
    int best = INT_MIN;
    for (size_t i = 0; i < v.size(); i++) {
        if (i && v[i] == v[i - 1]) continue;
        unsigned char x = v[i];
        string rest = v.substr(0, i) + v.substr(i + 1);
        int fr = dpf(rest), val;
        if (x & 128) {
            int L = x & 127;
            val = min(-L - fr, 8 - L + fr);
        } else {
            int L = x;
            if (L <= 2) val = -L - fr;
            else val = min(-L - fr, 4 - L + fr);
        }
        best = max(best, val);
    }
    if (dpMemo.size() >= 200000) dpMemo.clear();
    dpMemo[v] = best;
    return best;
}
static inline int openVal(unsigned char x, int fr) {
    if (x & 128) {
        int L = x & 127;
        return min(-L - fr, 8 - L + fr);
    }
    int L = x;
    return L <= 2 ? -L - fr : min(-L - fr, 4 - L + fr);
}
// value when the opener follows a fixed order (controller answers optimally)
static int orderValue(const string &s) {
    int x = 0;
    for (int i = (int)s.size() - 1; i >= 0; i--) x = openVal((unsigned char)s[i], x);
    return x;
}
static int chainValue(const vector<Chain> &ch) {
    string s;
    for (auto &c : ch) s.push_back((char)(c.loop ? (128 | min(c.len, 127)) : min(c.len, 127)));
    sort(s.begin(), s.end());
    long long states = 1;
    for (size_t i = 0; i < s.size();) {
        size_t j = i;
        while (j < s.size() && s[j] == s[i]) j++;
        states *= (j - i + 1);
        if (states > 4000) break;
        i = j;
    }
    if (states <= 4000) return dpf(s);
    // too many components: try a few natural opening orders
    string shortC, loops, longC;
    for (unsigned char x : s) {
        if (x & 128) loops.push_back(x);
        else if (x <= 2) shortC.push_back(x);
        else longC.push_back(x);
    }
    int best = orderValue(shortC + loops + longC);
    best = max(best, orderValue(shortC + longC + loops));
    string mixed = loops + longC;
    sort(mixed.begin(), mixed.end(), [](unsigned char a, unsigned char b) { return (a & 127) < (b & 127); });
    best = max(best, orderValue(shortC + mixed));
    return best;
}

// heuristic: play safe moves greedily until none remain, then value the chains by parity
static vector<Chain> tmpCh;
static int heuristic() {
    int total = 0;
    int stk[220];
    for (int p = 0; p < NPLAY; p++) {
        int k = 0;
        for (int i = 0; i < E; i++) {
            int e = perm[p][i];
            if (!drawn[e] && isSafe(e)) drawE(e), stk[k++] = e;
        }
        getChains(tmpCh);
        int f = chainValue(tmpCh);
        for (int i = k - 1; i >= 0; i--) undrawE(stk[i]);
        total += (k % 2 == 0) ? f : -f;
    }
    return (int)lround((double)total / NPLAY);
}

// ---------------- move generation ----------------
enum { GEN_MOVES, GEN_LEAF, GEN_PURE };
static int markE[220], markStamp = 0;
static int hist[220];
static bool fullSac = false;  // consider every sacrifice (small positions, searched to the end)
static int gen(int depth, vector<int> &mv, bool needMoves, int &val) {
    mv.clear();
    // captures
    int forced = -1;
    static int capE[100], capF[100];
    int nc = 0;
    for (int b = 0; b < NBX && forced < 0; b++) {
        if (sides[b] != 3) continue;
        int e = freeEdge(b), n = other(e, b);
        if (n < 0 || sides[n] != 2) { forced = e; break; }
        int f = freeEdge(n, e), g = other(f, n);
        bool dd;
        if (g < 0 || sides[g] <= 1) dd = true;
        else if (sides[g] == 2) {
            int h = freeEdge(g, f), k = other(h, g);
            dd = (k >= 0 && sides[k] == 3);
        } else dd = false;
        if (!dd) { forced = e; break; }
        capE[nc] = e, capF[nc] = f, nc++;
    }
    if (forced >= 0) { mv.push_back(forced); return GEN_MOVES; }
    markStamp++;
    if (nc) {
        for (int i = 0; i < nc; i++)
            if (markE[capE[i]] != markStamp) markE[capE[i]] = markStamp, mv.push_back(capE[i]);
        for (int i = 0; i < nc; i++)
            if (markE[capF[i]] != markStamp) markE[capF[i]] = markStamp, mv.push_back(capF[i]);
        return GEN_MOVES;
    }
    // safe moves
    vector<int> safe;
    for (int e = 0; e < E; e++)
        if (!drawn[e] && isSafe(e)) safe.push_back(e);
    vector<Chain> ch;
    if (!safe.empty()) {
        if (depth <= 0 && !needMoves) { val = heuristic(); return GEN_LEAF; }
        sort(safe.begin(), safe.end(), [](int a, int b) { return hist[a] > hist[b]; });
        mv = safe;
        for (int e : mv) markE[e] = markStamp;
        getChains(ch);
        for (auto &c : ch) {
            if (!fullSac && (c.loop || c.len > 2)) continue;
            if (c.loop) {
                if (markE[c.e1] != markStamp) markE[c.e1] = markStamp, mv.push_back(c.e1);
                continue;
            }
            int es[3] = {c.mid, c.e1, c.e2};
            for (int e : es)
                if (e >= 0 && markE[e] != markStamp) markE[e] = markStamp, mv.push_back(e);
        }
        return GEN_MOVES;
    }
    bool junction = getChains(ch);
    if (!needMoves) {
        if (!junction) { val = chainValue(ch); return GEN_PURE; }
        if (depth <= 0) { val = chainValue(ch); return GEN_LEAF; }
    }
    sort(ch.begin(), ch.end(), [](const Chain &a, const Chain &b) {
        if (a.loop != b.loop) return !a.loop;
        return a.len < b.len;
    });
    for (auto &c : ch) {
        int es[3] = {c.mid, c.e1, c.e2};
        if (c.loop) es[0] = -1, es[2] = -1;
        else if (c.len > 2) es[0] = -1;
        if (!junction && !c.loop && c.len > 2) es[2] = -1;  // ends are equivalent when both lead outside
        for (int e : es)
            if (e >= 0 && markE[e] != markStamp) markE[e] = markStamp, mv.push_back(e);
    }
    return GEN_MOVES;
}

// ---------------- search ----------------
struct TTE {
    u64 key;
    int16_t val, depth;
    uint8_t flag, move;
};
static const int TTBITS = 20;
static vector<TTE> TT(1 << TTBITS);
static const int EXACTD = 10000;
static long long nodes;
static bool abortFlag;
static chrono::steady_clock::time_point deadline;
static long long inexact;

static int search(int depth, int alpha, int beta) {
    if ((++nodes & 127) == 0 && chrono::steady_clock::now() > deadline) abortFlag = true;
    if (abortFlag) return 0;
    if (undrawnCnt == 0) return 0;
    TTE &t = TT[hsh & ((1 << TTBITS) - 1)];
    int ttMove = -1;
    if (t.key == hsh) {
        ttMove = t.move == 255 ? -1 : t.move;
        if (t.depth >= depth) {
            int v = t.val;
            bool ok = (t.flag == 0) || (t.flag == 1 && v >= beta) || (t.flag == 2 && v <= alpha);
            if (ok) {
                if (t.depth != EXACTD) inexact++;
                return v;
            }
        }
    }
    long long save = inexact;
    vector<int> mv;
    int val;
    int kind = gen(depth, mv, false, val);
    if (kind == GEN_PURE) return val;
    if (kind == GEN_LEAF) { inexact++; return val; }
    if (ttMove >= 0) {
        auto it = find(mv.begin(), mv.end(), ttMove);
        if (it != mv.end()) rotate(mv.begin(), it, it + 1);
    }
    int a0 = alpha, best = INT_MIN, bm = mv[0];
    bool firstMv = true;
    for (int e : mv) {
        int c = drawE(e), v;
        if (firstMv) {
            if (c > 0) v = c + search(depth, alpha - c, beta - c);
            else v = -search(depth - 1, -beta, -alpha);
        } else {
            if (c > 0) v = c + search(depth, alpha - c, alpha + 1 - c);
            else v = -search(depth - 1, -alpha - 1, -alpha);
            if (v > alpha && v < beta && !abortFlag) {
                if (c > 0) v = c + search(depth, alpha - c, beta - c);
                else v = -search(depth - 1, -beta, -alpha);
            }
        }
        firstMv = false;
        undrawE(e);
        if (abortFlag) return 0;
        if (v > best) {
            best = v, bm = e;
            if (v > alpha) alpha = v;
            if (alpha >= beta) {
                if (mv.size() > 1) hist[e] += depth * depth + 1;
                break;
            }
        }
    }
    bool exact = inexact == save;
    t.key = hsh;
    t.val = best;
    t.depth = exact ? EXACTD : max(depth, 0);
    t.flag = best <= a0 ? 2 : (best >= beta ? 1 : 0);
    t.move = bm;
    return best;
}

static double usedTime = 0;
static bool dbg = getenv("DBG") != nullptr;
static int dbgDepth, dbgVal;
static int chooseMove() {
    auto st = chrono::steady_clock::now();
    vector<int> mv;
    int val;
    fullSac = undrawnCnt <= FULLSAC_N;
    gen(1, mv, true, val);
    if (mv.size() == 1) return mv[0];
    double rem = TOTAL - usedTime;
    double budget = min(PERMOVE, max(0.002, rem / max(4.0, undrawnCnt / 3.0)));
    deadline = st + chrono::microseconds((long long)(budget * 1e6));
    abortFlag = false;
    int bestMove = mv[0];
    for (int d = 1; d <= 200; d++) {
        inexact = 0;
        int alpha = -1000, beta = 1000, best = INT_MIN, bm = -1;
        // root ordering: previous best first
        auto it = find(mv.begin(), mv.end(), bestMove);
        if (it != mv.end()) rotate(mv.begin(), it, it + 1);
        for (int e : mv) {
            int c = drawE(e), v;
            if (c > 0) v = c + search(d, alpha - c, beta - c);
            else v = -search(d - 1, -beta, -alpha);
            undrawE(e);
            if (abortFlag) break;
            if (v > best) best = v, bm = e, alpha = max(alpha, v);
        }
        if (abortFlag) break;
        bestMove = bm, dbgDepth = d, dbgVal = best;
        if (inexact == 0) break;  // solved exactly
        if (chrono::steady_clock::now() > deadline) break;
    }
    usedTime += chrono::duration<double>(chrono::steady_clock::now() - st).count();
    if (dbg) {
        for (int r = 0; r <= 2 * R; r++) {
            string row;
            for (int c = 0; c <= 2 * C; c++) {
                if (r % 2 == 0 && c % 2 == 0) row += '+';
                else if (r % 2 == 0) { int e = (r / 2) * C + c / 2; row += e == bestMove ? '*' : drawn[e] ? '-' : ' '; }
                else if (c % 2 == 0) { int e = NH + (r / 2) * (C + 1) + c / 2; row += e == bestMove ? '*' : drawn[e] ? '|' : ' '; }
                else row += char('0' + sides[(r / 2) * C + c / 2]);
            }
            fprintf(stderr, "%s\n", row.c_str());
        }
        fprintf(stderr, "depth %d val %d nmoves %zu\n", dbgDepth, dbgVal, mv.size());
    }
    return bestMove;
}

static int edgeOf(int r1, int c1, int r2, int c2) {
    if (r1 == r2) return r1 * C + min(c1, c2);
    return NH + min(r1, r2) * (C + 1) + c1;
}
static void printEdge(int e) {
    int r1, c1, r2, c2;
    if (e < NH) r1 = r2 = e / C, c1 = e % C, c2 = c1 + 1;
    else {
        int x = e - NH;
        r1 = x / (C + 1), c1 = c2 = x % (C + 1), r2 = r1 + 1;
    }
    printf("LINE %d %d %d %d\n", r1, c1, r2, c2);
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc > 1) TOTAL = atof(argv[1]);
    if (argc > 2) PERMOVE = atof(argv[2]);
    if (argc > 3) NPLAY = atoi(argv[3]);
    if (argc > 4) FULLSAC_N = atoi(argv[4]);
    char buf[256];
    string line;
    auto readLine = [&](string &s) -> bool {
        if (!fgets(buf, sizeof buf, stdin)) return false;
        s = buf;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        return true;
    };
    while (readLine(line))
        if (line.rfind("GAME", 0) == 0) break;
    int first;
    sscanf(line.c_str(), "GAME %d %d %d", &R, &C, &first);
    NH = (R + 1) * C;
    E = NH + R * (C + 1);
    NBX = R * C;
    for (int e = 0; e < E; e++) eb[e][0] = eb[e][1] = -1;
    for (int r = 0; r < R; r++)
        for (int c = 0; c < C; c++) {
            int b = r * C + c;
            int es[4] = {r * C + c, (r + 1) * C + c, NH + r * (C + 1) + c, NH + r * (C + 1) + c + 1};
            for (int k = 0; k < 4; k++) {
                be[b][k] = es[k];
                if (eb[es[k]][0] < 0) eb[es[k]][0] = b; else eb[es[k]][1] = b;
            }
        }
    mt19937_64 rng(R * 131 + C * 7 + first);
    for (int e = 0; e < E; e++) zob[e] = rng();
    for (int p = 0; p < 4; p++) {
        for (int e = 0; e < E; e++) perm[p][e] = e;
        shuffle(perm[p], perm[p] + E, rng);
    }
    undrawnCnt = E;
    vector<string> rows(2 * R + 1);
    for (int i = 0; i < 2 * R + 1; i++) readLine(rows[i]);
    for (int r = 0; r <= R; r++)
        for (int c = 0; c < C; c++)
            if ((int)rows[2 * r].size() > 2 * c + 1 && rows[2 * r][2 * c + 1] == '-') drawE(r * C + c);
    for (int r = 0; r < R; r++)
        for (int c = 0; c <= C; c++)
            if ((int)rows[2 * r + 1].size() > 2 * c && rows[2 * r + 1][2 * c] == '|') drawE(NH + r * (C + 1) + c);
    while (readLine(line)) {
        if (line.rfind("YOUR_TURN", 0) == 0) {
            int e = chooseMove();
            drawE(e);
            printEdge(e);
        } else if (line.rfind("OPPONENT", 0) == 0) {
            int r1, c1, r2, c2;
            sscanf(line.c_str(), "OPPONENT %d %d %d %d", &r1, &c1, &r2, &c2);
            int e = edgeOf(r1, c1, r2, c2);
            if (!drawn[e]) drawE(e);
        } else if (line.rfind("GAME_END", 0) == 0) break;
    }
    return 0;
}
