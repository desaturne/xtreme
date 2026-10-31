// Dots and Boxes bot (C port, allocation-free search)
// alpha-beta (negamax) + transposition table, Barker-Korf capture reductions,
// exact chain/loop endgame DP, playout-parity evaluation in the safe-move phase.
#define _POSIX_C_SOURCE 199309L
#pragma GCC optimize("O3")
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef uint64_t u64;
#define MAXE 220
#define MAXB 100
#define MAXPLY 512
#define MAXMV 240

static int R, C, NH, E, NBX;
static int eb[MAXE][2];  // boxes adjacent to an edge (-1 = outside)
static int be[MAXB][4];  // edges of a box
static unsigned char drawn[MAXE];
static int sides[MAXB];
static u64 zob[MAXE], hsh = 0;
static int undrawnCnt = 0;
static int perm[4][MAXE];
static int FULLSAC_N = 32;
static long long HLIM = 4000;  // exact-DP state limit inside midgame playouts
static int NPLAY = 2;          // playouts per heuristic leaf
static double TOTAL = 1.55;    // total think budget per game (s)
static double PERMOVE = 0.6;   // per-move cap (s)

static u64 rngState;
static u64 rnd64(void) {  // splitmix64
    u64 z = (rngState += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static inline int other(int e, int b) { return eb[e][0] == b ? eb[e][1] : eb[e][0]; }
static inline int freeEdge(int b, int except) {
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
static inline int isSafe(int e) {
    int b0 = eb[e][0], b1 = eb[e][1];
    return (b0 < 0 || sides[b0] < 2) && (b1 < 0 || sides[b1] < 2);
}

// ---------------- chain structure ----------------
typedef struct {
    int len, loop, e1, e2, mid;
} Chain;
static int visMark[MAXB], visStamp = 0;
// requires: no 3-sided boxes. Fills ch; returns count; *junction = a box with <=1 sides exists.
static int getChains(Chain *ch, int *junction) {
    int n = 0;
    visStamp++;
    *junction = 0;
    for (int b = 0; b < NBX; b++) {
        if (sides[b] <= 1) *junction = 1;
        if (sides[b] != 2 || visMark[b] == visStamp) continue;
        int f0 = freeEdge(b, -1), f1 = freeEdge(b, f0);
        int cur = b, e = f1, loop = 0;
        for (;;) {
            int nb = other(e, cur);
            if (nb == b) { loop = 1; break; }
            if (nb >= 0 && sides[nb] == 2) {
                int ne = freeEdge(nb, e);
                cur = nb, e = ne;
            } else break;
        }
        Chain c;
        c.loop = loop, c.mid = -1;
        if (loop) {
            int len = 0;
            cur = b, e = f0;
            do {
                visMark[cur] = visStamp;
                len++;
                int nb = other(e, cur);
                int ne = freeEdge(nb, e);
                cur = nb, e = ne;
            } while (cur != b);
            c.len = len, c.e1 = f0, c.e2 = f0;
        } else {
            c.e1 = e;
            int len = 0, ee = e;
            for (;;) {
                visMark[cur] = visStamp;
                len++;
                int ne = freeEdge(cur, ee);
                int nb = other(ne, cur);
                if (nb >= 0 && sides[nb] == 2 && visMark[nb] != visStamp) {
                    if (len == 1) c.mid = ne;
                    cur = nb, ee = ne;
                } else {
                    c.e2 = ne;
                    break;
                }
            }
            c.len = len;
        }
        ch[n++] = c;
    }
    return n;
}

// ---------------- chain/loop DP (memo keyed by zobrist of the multiset) ----------------
static u64 zobT[256][MAXB + 1];
static int dcnt[256];
static unsigned char dtypes[MAXB];
static int ndtypes;
#define DPBITS 19
typedef struct {
    u64 key;
    int val;
} DPE;
static DPE dpTab[1 << DPBITS];

static inline int openVal(int x, int fr) {
    if (x & 128) {
        int L = x & 127;
        int a = -L - fr, b = 8 - L + fr;
        return a < b ? a : b;
    }
    int L = x;
    if (L <= 2) return -L - fr;
    int a = -L - fr, b = 4 - L + fr;
    return a < b ? a : b;
}
// per-calculation memo: linear probing, generation-stamped, never evicts (states <= 4000 < LTSIZE/2)
#define LTBITS 14
static u64 ltKey[1 << LTBITS];
static int ltVal[1 << LTBITS], ltGen[1 << LTBITS], ltCur = 0;
static int dpf(u64 key, int left) {
    if (!left) return 0;
    unsigned idx = (unsigned)(key >> 20) & ((1 << LTBITS) - 1);
    while (ltGen[idx] == ltCur) {
        if (ltKey[idx] == key) return ltVal[idx];
        idx = (idx + 1) & ((1 << LTBITS) - 1);
    }
    int best = -100000;
    for (int i = 0; i < ndtypes; i++) {
        int x = dtypes[i];
        int c = dcnt[x];
        if (!c) continue;
        u64 k2 = key ^ zobT[x][c] ^ zobT[x][c - 1];
        dcnt[x]--;
        int v = openVal(x, dpf(k2, left - 1));
        dcnt[x]++;
        if (v > best) best = v;
    }
    // re-probe: recursion may have filled slots since the first probe
    idx = (unsigned)(key >> 20) & ((1 << LTBITS) - 1);
    while (ltGen[idx] == ltCur) idx = (idx + 1) & ((1 << LTBITS) - 1);
    ltGen[idx] = ltCur, ltKey[idx] = key, ltVal[idx] = best;
    return best;
}
static int dpTop(u64 key, int left) {  // global cache of whole-position results
    DPE *t = &dpTab[key & ((1 << DPBITS) - 1)];
    if (t->key == key) return t->val;
    ltCur++;
    int v = dpf(key, left);
    t->key = key, t->val = v;
    return v;
}
static int orderValue(const unsigned char *s, int n) {
    int x = 0;
    for (int i = n - 1; i >= 0; i--) x = openVal(s[i], x);
    return x;
}
static int cmpLen(const void *a, const void *b) {
    return (*(const unsigned char *)a & 127) - (*(const unsigned char *)b & 127);
}
static int chainValue(const Chain *ch, int n, long long limit) {
    if (!n) return 0;
    unsigned char s[MAXB];
    for (int i = 0; i < n; i++) {
        int L = ch[i].len > 127 ? 127 : ch[i].len;
        s[i] = (unsigned char)(ch[i].loop ? (128 | L) : L);
    }
    // count types
    ndtypes = 0;
    for (int i = 0; i < n; i++) {
        if (!dcnt[s[i]]) dtypes[ndtypes++] = s[i];
        dcnt[s[i]]++;
    }
    long long states = 1;
    u64 key = 0x1234567ULL;
    for (int i = 0; i < ndtypes; i++) {
        int x = dtypes[i];
        key ^= zobT[x][dcnt[x]];
        if (states <= limit) states *= dcnt[x] + 1;
    }
    int res;
    if (states <= limit) {
        res = dpTop(key, n);
    } else {
        unsigned char sh[MAXB], lo[MAXB], lg[MAXB], buf[MAXB];
        int ns = 0, nl = 0, ng = 0;
        for (int i = 0; i < n; i++) {
            if (s[i] & 128) lo[nl++] = s[i];
            else if (s[i] <= 2) sh[ns++] = s[i];
            else lg[ng++] = s[i];
        }
        qsort(sh, ns, 1, cmpLen), qsort(lo, nl, 1, cmpLen), qsort(lg, ng, 1, cmpLen);
        int k = 0, v;
        memcpy(buf, sh, ns), k = ns, memcpy(buf + k, lo, nl), k += nl, memcpy(buf + k, lg, ng), k += ng;
        res = orderValue(buf, k);
        k = ns, memcpy(buf + k, lg, ng), k += ng, memcpy(buf + k, lo, nl), k += nl;
        v = orderValue(buf, k);
        if (v > res) res = v;
        k = ns, memcpy(buf + k, lo, nl), k += nl, memcpy(buf + k, lg, ng), k += ng;
        qsort(buf + ns, nl + ng, 1, cmpLen);
        v = orderValue(buf, k);
        if (v > res) res = v;
    }
    for (int i = 0; i < ndtypes; i++) dcnt[dtypes[i]] = 0;
    return res;
}

// heuristic: play safe moves greedily until none remain, then value the chains by parity
static int heuristic(void) {
    int total = 0, stk[MAXE];
    Chain ch[MAXB];
    for (int p = 0; p < NPLAY; p++) {
        int k = 0, junc;
        for (int i = 0; i < E; i++) {
            int e = perm[p][i];
            if (!drawn[e] && isSafe(e)) drawE(e), stk[k++] = e;
        }
        int n = getChains(ch, &junc);
        int f = chainValue(ch, n, HLIM);
        for (int i = k - 1; i >= 0; i--) undrawE(stk[i]);
        total += (k % 2 == 0) ? f : -f;
    }
    double a = (double)total / NPLAY;
    return (int)(a < 0 ? a - 0.5 : a + 0.5);
}

// ---------------- move generation ----------------
static long long inexact = 0;  // counts uses of non-exact information during a search
enum { GEN_MOVES, GEN_LEAF, GEN_PURE };
static int markE[MAXE], markStamp = 0;
static int hist[MAXE];
static int fullSac = 0;
static inline void addMv(int *mv, int *n, int e) {
    if (e >= 0 && markE[e] != markStamp) markE[e] = markStamp, mv[(*n)++] = e;
}
// short chain (len<=2) through 2-sided box b? fills its edges
static int shortChainEdges(int b, int *out) {
    int f0 = freeEdge(b, -1), f1 = freeEdge(b, f0);
    int len = 1, k = 0;
    int ends[2] = {f0, f1};
    out[k++] = f0, out[k++] = f1;
    for (int d = 0; d < 2; d++) {
        int nb = other(ends[d], b);
        if (nb >= 0 && sides[nb] == 2) {
            if (nb == other(ends[1 - d], b)) return 0;  // degenerate
            len++;
            if (len > 2) return 0;
            int ne = freeEdge(nb, ends[d]);
            int nn = other(ne, nb);
            if (nn >= 0 && sides[nn] == 2) return 0;  // longer
            out[k++] = ne;
        }
    }
    return k;
}
static int gen(int depth, int *mv, int needMoves, int *val, int *nmv) {
    int n = 0;
    int forced = -1, nc = 0;
    int capE[MAXB], capF[MAXB];
    for (int b = 0; b < NBX; b++) {
        if (sides[b] != 3) continue;
        int e = freeEdge(b, -1), nb = other(e, b);
        if (nb < 0 || sides[nb] != 2) { forced = e; break; }
        int f = freeEdge(nb, e), g = other(f, nb);
        int dd;
        if (g < 0 || sides[g] <= 1) dd = 1;
        else if (sides[g] == 2) {
            int h = freeEdge(g, f), k = other(h, g);
            dd = (k >= 0 && sides[k] == 3);
        } else dd = 0;
        if (!dd) { forced = e; break; }
        capE[nc] = e, capF[nc] = f, nc++;
    }
    if (forced >= 0) { mv[0] = forced, *nmv = 1; return GEN_MOVES; }
    markStamp++;
    if (nc) {
        for (int i = 0; i < nc; i++) addMv(mv, &n, capE[i]);
        for (int i = 0; i < nc; i++) addMv(mv, &n, capF[i]);
        *nmv = n;
        return GEN_MOVES;
    }
    int anySafe = 0;
    for (int e = 0; e < E; e++)
        if (!drawn[e] && isSafe(e)) { anySafe = 1; break; }
    Chain ch[MAXB];
    int junc;
    if (anySafe) {
        if (depth <= 0 && !needMoves) { *val = heuristic(); return GEN_LEAF; }
        for (int e = 0; e < E; e++)
            if (!drawn[e] && isSafe(e)) {  // insertion by history score
                int j = n++;
                while (j > 0 && hist[mv[j - 1]] < hist[e]) mv[j] = mv[j - 1], j--;
                mv[j] = e, markE[e] = markStamp;
            }
        if (fullSac) {
            int nch = getChains(ch, &junc);
            for (int i = 0; i < nch; i++) {
                if (ch[i].loop) { addMv(mv, &n, ch[i].e1); continue; }
                addMv(mv, &n, ch[i].mid), addMv(mv, &n, ch[i].e1), addMv(mv, &n, ch[i].e2);
            }
        } else {
            int out[4];
            for (int b = 0; b < NBX; b++) {
                if (sides[b] != 2) continue;
                int k = shortChainEdges(b, out);
                for (int i = 0; i < k; i++) addMv(mv, &n, out[i]);
            }
        }
        *nmv = n;
        return GEN_MOVES;
    }
    int nch = getChains(ch, &junc);
    if (!needMoves) {
        if (!junc) { *val = chainValue(ch, nch, 4000); return GEN_PURE; }
        if (depth <= 0) { *val = chainValue(ch, nch, 4000); return GEN_LEAF; }
    }
    // order: chains by length, then loops
    for (int i = 1; i < nch; i++) {
        Chain x = ch[i];
        int j = i;
        while (j > 0 && (ch[j - 1].loop > x.loop || (ch[j - 1].loop == x.loop && ch[j - 1].len > x.len)))
            ch[j] = ch[j - 1], j--;
        ch[j] = x;
    }
    for (int i = 0; i < nch; i++) {
        Chain *c = &ch[i];
        if (c->loop) { addMv(mv, &n, c->e1); continue; }
        if (c->len <= 2) addMv(mv, &n, c->mid);
        addMv(mv, &n, c->e1);
        if (junc || c->len <= 2) addMv(mv, &n, c->e2);
    }
    *nmv = n;
    return GEN_MOVES;
}

// ---------------- search ----------------
typedef struct {
    u64 key;
    int16_t val, depth;
    uint8_t flag, move;
} TTE;
#define TTBITS 21
static TTE TT[1 << TTBITS];
#define EXACTD 10000
static long long nodes;
static int abortFlag;
static double deadline;
static int mvbuf[MAXPLY][MAXMV];

static int search(int depth, int alpha, int beta, int ply) {
    if ((++nodes & 15) == 0 && now() > deadline) abortFlag = 1;
    if (abortFlag) return 0;
    if (undrawnCnt == 0) return 0;
    TTE *t = &TT[hsh & ((1 << TTBITS) - 1)];
    int ttMove = -1;
    if (t->key == hsh) {
        ttMove = t->move == 255 ? -1 : t->move;
        if (t->depth >= depth) {
            int v = t->val;
            if (t->flag == 0 || (t->flag == 1 && v >= beta) || (t->flag == 2 && v <= alpha)) {
                if (t->depth != EXACTD) inexact++;
                return v;
            }
        }
    }
    long long save = inexact;
    int *mv = mvbuf[ply], n, val;
    int kind = gen(depth, mv, 0, &val, &n);
    if (kind == GEN_PURE) return val;
    if (kind == GEN_LEAF) {  // cache the evaluation for transpositions
        inexact++;
        t->key = hsh, t->val = (int16_t)val, t->depth = 0, t->flag = 0, t->move = 255;
        return val;
    }
    if (ttMove >= 0)
        for (int i = 0; i < n; i++)
            if (mv[i] == ttMove) {
                for (int j = i; j > 0; j--) mv[j] = mv[j - 1];
                mv[0] = ttMove;
                break;
            }
    int a0 = alpha, best = -100000, bm = mv[0];
    for (int i = 0; i < n; i++) {
        int e = mv[i];
        int c = drawE(e), v;
        if (i == 0) {
            v = c > 0 ? c + search(depth, alpha - c, beta - c, ply + 1) : -search(depth - 1, -beta, -alpha, ply + 1);
        } else {
            v = c > 0 ? c + search(depth, alpha - c, alpha + 1 - c, ply + 1)
                      : -search(depth - 1, -alpha - 1, -alpha, ply + 1);
            if (v > alpha && v < beta && !abortFlag)
                v = c > 0 ? c + search(depth, alpha - c, beta - c, ply + 1)
                          : -search(depth - 1, -beta, -alpha, ply + 1);
        }
        undrawE(e);
        if (abortFlag) return 0;
        if (v > best) {
            best = v, bm = e;
            if (v > alpha) alpha = v;
            if (alpha >= beta) {
                if (n > 1) hist[e] += depth * depth + 1;
                break;
            }
        }
    }
    int exact = inexact == save;
    t->key = hsh;
    t->val = (int16_t)best;
    t->depth = (int16_t)(exact ? EXACTD : (depth > 0 ? depth : 0));
    t->flag = best <= a0 ? 2 : (best >= beta ? 1 : 0);
    t->move = (uint8_t)bm;
    return best;
}

static double usedTime = 0;
static double CRITMUL = 1.0;  // time multiplier while few safe moves remain (the parity fight)
static int CRITN = 30;
static int chooseMove(void) {
    double st = now();
    int mv[MAXMV], n, val;
    fullSac = undrawnCnt <= FULLSAC_N;
    gen(1, mv, 1, &val, &n);
    if (n == 1) return mv[0];
    int nsafe = 0;
    for (int e = 0; e < E; e++)
        if (!drawn[e] && isSafe(e)) nsafe++;
    double rem = TOTAL - usedTime;
    // time is spent where decisions remain: late moves are mostly forced or solved exactly
    double q = nsafe / 2.0 + 3;
    double budget = rem / q;
    if (nsafe > 0 && nsafe <= CRITN) budget *= CRITMUL;
    if (budget > rem * 0.4) budget = rem * 0.4;
    if (budget < 0.002) budget = 0.002;
    if (budget > PERMOVE) budget = PERMOVE;
    deadline = st + budget;
    abortFlag = 0;
    int bestMove = mv[0];
    for (int d = 1; d <= 200; d++) {
        inexact = 0;
        for (int i = 0; i < n; i++)
            if (mv[i] == bestMove) {
                for (int j = i; j > 0; j--) mv[j] = mv[j - 1];
                mv[0] = bestMove;
                break;
            }
        int alpha = -1000, beta = 1000, best = -100000, bm = -1;
        for (int i = 0; i < n; i++) {
            int e = mv[i];
            int c = drawE(e), v;
            if (i == 0) {
                v = c > 0 ? c + search(d, alpha - c, beta - c, 0) : -search(d - 1, -beta, -alpha, 0);
            } else {  // null window: only re-search moves that might beat the current best
                v = c > 0 ? c + search(d, alpha - c, alpha + 1 - c, 0) : -search(d - 1, -alpha - 1, -alpha, 0);
                if (v > alpha && !abortFlag)
                    v = c > 0 ? c + search(d, alpha - c, beta - c, 0) : -search(d - 1, -beta, -alpha, 0);
            }
            undrawE(e);
            if (abortFlag) break;
            if (v > best) {
                best = v, bm = e;
                if (v > alpha) alpha = v;
            }
        }
        // a move proven better than the previous best during this iteration is kept even on abort
        if (abortFlag) {
            if (bm >= 0 && bm != mv[0] && d > 1) bestMove = bm;
            break;
        }
        bestMove = bm;
        if (inexact == 0) break;  // solved exactly
        if (now() > deadline) break;
    }
    usedTime += now() - st;
    return bestMove;
}

static int edgeOf(int r1, int c1, int r2, int c2) {
    if (r1 == r2) return r1 * C + (c1 < c2 ? c1 : c2);
    return NH + (r1 < r2 ? r1 : r2) * (C + 1) + c1;
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
static int readLine(char *buf, int sz) {
    if (!fgets(buf, sz, stdin)) return 0;
    int l = (int)strlen(buf);
    while (l > 0 && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = 0;
    return 1;
}

int main(int argc, char **argv) {
    if (argc > 1) TOTAL = atof(argv[1]);
    if (argc > 2) PERMOVE = atof(argv[2]);
    if (argc > 3) NPLAY = atoi(argv[3]);
    if (argc > 4) FULLSAC_N = atoi(argv[4]);
    if (argc > 5) HLIM = atoll(argv[5]);
    if (argc > 6) CRITMUL = atof(argv[6]);
    if (argc > 7) CRITN = atoi(argv[7]);
    char line[512];
    int first = 0;
    while (readLine(line, sizeof line))
        if (strncmp(line, "GAME", 4) == 0) break;
    sscanf(line, "GAME %d %d %d", &R, &C, &first);
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
    rngState = (u64)(R * 131 + C * 7 + first) * 0x2545F4914F6CDD1DULL + 1;
    for (int e = 0; e < E; e++) zob[e] = rnd64();
    for (int x = 0; x < 256; x++) {
        zobT[x][0] = 0;
        for (int c = 1; c <= MAXB; c++) zobT[x][c] = rnd64();
    }
    for (int p = 0; p < 4; p++) {
        for (int e = 0; e < E; e++) perm[p][e] = e;
        for (int i = E - 1; i > 0; i--) {
            int j = (int)(rnd64() % (u64)(i + 1));
            int tmp = perm[p][i];
            perm[p][i] = perm[p][j], perm[p][j] = tmp;
        }
    }
    for (int i = 0; i < (1 << TTBITS); i++) TT[i].key = 0, TT[i].move = 255;
    undrawnCnt = E;
    for (int r = 0; r <= 2 * R; r++) {
        if (!readLine(line, sizeof line)) line[0] = 0;
        int len = (int)strlen(line);
        if (r % 2 == 0) {
            for (int c = 0; c < C; c++)
                if (len > 2 * c + 1 && line[2 * c + 1] == '-') drawE((r / 2) * C + c);
        } else {
            for (int c = 0; c <= C; c++)
                if (len > 2 * c && line[2 * c] == '|') drawE(NH + (r / 2) * (C + 1) + c);
        }
    }
    while (readLine(line, sizeof line)) {
        if (strncmp(line, "YOUR_TURN", 9) == 0) {
            int e = chooseMove();
            drawE(e);
            printEdge(e);
        } else if (strncmp(line, "OPPONENT", 8) == 0) {
            int r1, c1, r2, c2;
            if (sscanf(line, "OPPONENT %d %d %d %d", &r1, &c1, &r2, &c2) == 4) {
                int e = edgeOf(r1, c1, r2, c2);
                if (e >= 0 && e < E && !drawn[e]) drawE(e);
            }
        } else if (strncmp(line, "GAME_END", 8) == 0) break;
    }
    return 0;
}
