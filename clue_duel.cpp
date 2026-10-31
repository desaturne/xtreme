// Clue Duel bot
#include <bits/stdc++.h>
using namespace std;

// MODE 0 = real bot. MODE 1 = old leaky bot, MODE 2 = leaky bot that exploits opponent leaks (testing only)
static int MODE = 0;
static bool modern() { return MODE == 0 || MODE == 3; }  // MODE 3 = test-only 'reader' opponent

static int N[3], T;
static bool mine[3][45], oppHas[3][45], shownToOpp[3][45];
static int oppGuessCnt[3][45];   // times the opponent guessed this (non-mine) card
static bool oppNoneHit[3][45];   // opponent guessed it and I answered NONE
static int solvedCard[3];        // 0 = unknown
static bool oppKnows[3];         // opponent probably knows this category's solution
static const char CH[3] = {'P', 'W', 'L'};
static mt19937 rng(12345);

// ---------- DP over candidate counts (used by the testing-only leaky modes) ----------
static double E[23][23][23];
static double evalSubset(const int n[3], int mask) {
    vector<int> S;
    for (int k = 0; k < 3; k++)
        if (mask >> k & 1) S.push_back(k);
    int m = S.size();
    double total = 0;
    for (int pat = 0; pat < (1 << m); pat++) {
        double p = 1;
        vector<int> H;
        for (int i = 0; i < m; i++) {
            int k = S[i];
            if (pat >> i & 1) p *= 1.0 / n[k];
            else { p *= 1.0 - 1.0 / n[k]; H.push_back(k); }
        }
        if (p <= 0) continue;
        double v;
        if (H.empty()) {
            int nn[3] = {n[0], n[1], n[2]};
            for (int k : S) nn[k] = 1;
            v = E[nn[0]][nn[1]][nn[2]];
        } else {
            double mx = -1, sum = 0;
            for (int k : H) {
                int nn[3] = {n[0], n[1], n[2]};
                nn[k]--;
                double x = E[nn[0]][nn[1]][nn[2]];
                mx = max(mx, x), sum += x;
            }
            v = 0.5 * mx + 0.5 * sum / H.size();
        }
        total += p * v;
    }
    return total;
}
static int bestMask(const int n[3], double *val = nullptr) {
    int best = 0;
    double bv = 1e18;
    for (int mask = 1; mask < 8; mask++) {
        bool ok = true;
        for (int k = 0; k < 3; k++)
            if ((mask >> k & 1) && n[k] <= 1) ok = false;
        if (!ok) continue;
        double v = evalSubset(n, mask);
        if (v < bv - 1e-12) bv = v, best = mask;
    }
    if (val) *val = bv;
    return best;
}
static void buildDP(int ma, int mb, int mc) {
    for (int a = 1; a <= ma; a++)
        for (int b = 1; b <= mb; b++)
            for (int c = 1; c <= mc; c++) {
                if (a == 1 && b == 1 && c == 1) { E[a][b][c] = 0; continue; }
                int n[3] = {a, b, c};
                double v;
                bestMask(n, &v);
                E[a][b][c] = 1 + v;
            }
}

// ---------- knowledge ----------
static vector<int> cands(int k) {
    vector<int> r;
    if (solvedCard[k]) { r.push_back(solvedCard[k]); return r; }
    for (int i = 1; i <= N[k]; i++)
        if (!mine[k][i] && !oppHas[k][i]) r.push_back(i);
    return r;
}
static void refresh() {
    for (int k = 0; k < 3; k++)
        if (!solvedCard[k]) {
            auto c = cands(k);
            if (c.size() == 1) solvedCard[k] = c[0];
        }
}
// prior weight that card (k,i) is the solution, from the opponent's guesses.
// A non-mine card they guessed is either their own (a filler/bluff) or the solution. If they
// have spread guesses over many such cards, each one is weak evidence; if only one or two, strong.
static const double ALPHA = 1.0;
static const double NB = 3.0;
static const int WMODE = 0;  // 0 = strong trust in opponent hints (tested best)
static vector<int> cands(int k);
static double weight(int k, int i) {
    if (WMODE == 0) {
        double w = 1.0;
        if (oppNoneHit[k][i]) w *= 30.0;
        if (oppGuessCnt[k][i] > 0) w *= 6.0 + 2.0 * min(oppGuessCnt[k][i], 5);
        return w;
    }
    if (oppGuessCnt[k][i] == 0) return 1.0;
    auto c = cands(k);
    int n = c.size(), m = 0;
    for (int x : c)
        if (oppGuessCnt[k][x] > 0) m++;
    double w = max(1.0, ALPHA * n / m);
    if (oppNoneHit[k][i]) w *= NB;
    return w;
}
static int bestCand(int k, double *prob = nullptr) {
    auto c = cands(k);
    if (c.empty()) {
        if (prob) *prob = 0;
        return 1;
    }
    double bw = -1, sum = 0;
    vector<int> top;
    for (int x : c) {
        double w = weight(k, x);
        sum += w;
        if (w > bw + 1e-9) bw = w, top.clear();
        if (fabs(w - bw) <= 1e-9) top.push_back(x);
    }
    if (prob) *prob = bw / sum;
    return top[rng() % top.size()];
}
static int oppCount(int k) {  // solution candidates in this category from the opponent's view
    if (oppKnows[k]) return 1;
    int c = 1;
    for (int i = 1; i <= N[k]; i++)
        if (mine[k][i] && !shownToOpp[k][i]) c++;
    return c;
}
// probability the opponent knows each category, and the chance that my last answer
// completed their knowledge (they guessed instead of accusing, so they didn't know before it)
static double pk[3];
static double qNow = 0;
static const double PSAMB = 0.18;  // chance an unexplained card in the opponent's guess is the solution
static double pkProd() { return pk[0] * pk[1] * pk[2]; }

static int fillerCard[3];
static int pickFiller(int k) {
    if (!modern()) {  // old behaviour: one fixed card
        if (!fillerCard[k]) {
            vector<int> v;
            for (int i = 1; i <= N[k]; i++)
                if (mine[k][i]) v.push_back(i);
            fillerCard[k] = v[rng() % v.size()];
        }
        return fillerCard[k];
    }
    const int DECOY = 2;
    if (DECOY && !solvedCard[k]) {  // decoy: an unshown own card, so a NONE doesn't reveal which slot was the test
        vector<int> un;
        for (int i = 1; i <= N[k]; i++)
            if (mine[k][i] && !shownToOpp[k][i]) un.push_back(i);
        if (!un.empty()) return DECOY == 1 ? un[rng() % un.size()] : un[0];
    }
    // safest filler: one of my cards the opponent has already seen (leaks nothing)
    vector<int> shown;
    for (int i = 1; i <= N[k]; i++)
        if (mine[k][i] && shownToOpp[k][i]) shown.push_back(i);
    if (!shown.empty()) return shown[rng() % shown.size()];
    // otherwise always the same own card, so at most one of my cards is exposed
    if (!fillerCard[k]) {
        vector<int> v;
        for (int i = 1; i <= N[k]; i++)
            if (mine[k][i]) v.push_back(i);
        fillerCard[k] = v[rng() % v.size()];
    }
    return fillerCard[k];
}

static int randomOwn(int k) {  // test-only reader hides its test slot behind random own cards
    vector<int> v;
    for (int i = 1; i <= N[k]; i++)
        if (mine[k][i]) v.push_back(i);
    return v[rng() % v.size()];
}

static bool parseCard(const string &s, int &k, int &i) {
    if (s.size() < 2) return false;
    if (s[0] == 'P') k = 0; else if (s[0] == 'W') k = 1; else if (s[0] == 'L') k = 2; else return false;
    i = atoi(s.c_str() + 1);
    return i >= 1 && i <= N[k];
}
static void say(const string &s) { cout << s << '\n'; cout.flush(); }
static string cardStr(int k, int i) { return string(1, CH[k]) + to_string(i); }

int main(int argc, char **argv) {
    ios::sync_with_stdio(false);
    if (argc > 1) MODE = atoi(argv[1]);
    string line;
    while (getline(cin, line))
        if (line.rfind("GAME ", 0) == 0) break;
    {
        istringstream is(line);
        string g;
        is >> g >> N[0] >> N[1] >> N[2] >> T;
    }
    rng.seed(N[0] * 1000003u + N[1] * 1009u + N[2] * 31u + T + MODE * 7u);
    if (!modern()) buildDP((N[0] + 1) / 2, (N[1] + 1) / 2, (N[2] + 1) / 2);

    int turnCount = 0;
    int lastGuess[3] = {0, 0, 0};
    int lastTested = -1;
    while (getline(cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        istringstream is(line);
        string cmd;
        is >> cmd;
        if (cmd == "HAND") {
            string c;
            int k, i;
            while (is >> c)
                if (parseCard(c, k, i)) mine[k][i] = true;
            refresh();
        } else if (cmd == "TURN") {
            string who;
            is >> who;
            turnCount++;
            if (who == "YOU") {
                refresh();
                bool allSolved = solvedCard[0] && solvedCard[1] && solvedCard[2];
                bool lastChance = turnCount + 2 > T;
                double pAll = 1, rexp = 0;
                for (int k = 0; k < 3; k++) {
                    double p;
                    bestCand(k, &p);
                    pAll *= p;
                    if (!solvedCard[k]) rexp += cands(k).size() / 2.0;
                }
                // accuse now (3 + 7p) vs. continuing while the opponent may accuse next turn
                double ropp = 0;
                for (int k = 0; k < 3; k++) ropp += (1 - pk[k]) * oppCount(k) / 2.0;
                double pwin = 1.0 / (1.0 + exp((rexp - ropp) / 2.0));
                // if I keep going and lose the race, I can often still bail out with a 3-point accusation
                double contValue = 10.0 * pwin + 2.0 * (1 - pwin);
                const int DR = 4;
                bool danger = modern() && DR == 2 && 3 + 7 * pAll > (1 - qNow) * contValue;
                if (modern() && DR == 3) danger = qNow > 0.5;
                if (modern() && DR == 4) danger = 3 + 7 * pAll > (1 - qNow) * 10;
                if (MODE == 2) danger = oppCount(0) == 1 && oppCount(1) == 1 && oppCount(2) == 1;
                qNow = 0;
                bool gamble = MODE == 2 && pAll > 0.5;
                if (allSolved || lastChance || danger || gamble) {
                    say("ACCUSE " + cardStr(0, bestCand(0)) + " " + cardStr(1, bestCand(1)) + " " +
                        cardStr(2, bestCand(2)));
                    continue;
                }
                int n[3];
                for (int k = 0; k < 3; k++) n[k] = cands(k).size();
                int mask;
                if (!modern()) {
                    mask = bestMask(n);
                } else {
                    // test exactly one unknown card (the others are fillers), so a NONE
                    // never hands the opponent more than the one category we just learned.
                    // Pick the category where the top candidate is most likely the solution.
                    int bk = -1;
                    double bp = -1;
                    for (int k = 0; k < 3; k++) {
                        if (n[k] <= 1) continue;
                        double p;
                        bestCand(k, &p);
                        if (p > bp + 1e-12) bp = p, bk = k;
                    }
                    mask = bk >= 0 ? 1 << bk : 1;
                }
                lastTested = mask;
                for (int k = 0; k < 3; k++)
                    lastGuess[k] = (mask >> k & 1) ? bestCand(k) : (MODE == 3 ? randomOwn(k) : pickFiller(k));
                say("GUESS " + cardStr(0, lastGuess[0]) + " " + cardStr(1, lastGuess[1]) + " " +
                    cardStr(2, lastGuess[2]));
            } else {  // OPP GUESS x y z
                string g, c;
                is >> g;
                int gk[3], gi[3], cnt = 0;
                while (cnt < 3 && is >> c) {
                    int k, i;
                    if (parseCard(c, k, i)) gk[cnt] = k, gi[cnt] = i, cnt++;
                }
                vector<int> held;
                for (int j = 0; j < cnt; j++)
                    if (mine[gk[j]][gi[j]]) held.push_back(j);
                for (int j = 0; j < cnt; j++)
                    if (!mine[gk[j]][gi[j]]) oppGuessCnt[gk[j]][gi[j]]++;
                double before = pkProd();
                if (held.empty()) {
                    say("NONE");
                    for (int j = 0; j < cnt; j++) {
                        int k = gk[j], i = gi[j];
                        if (MODE == 2 && !oppHas[k][i]) oppKnows[k] = true;
                        if (!oppHas[k][i]) oppNoneHit[k][i] = true;
                        double ps;  // chance this card is the solution (then they now know it)
                        if (oppHas[k][i]) ps = 0;
                        else if (solvedCard[k]) ps = solvedCard[k] == i ? 1 : 0;
                        else ps = max(1.0 / cands(k).size(), PSAMB);
                        pk[k] = 1 - (1 - pk[k]) * (1 - ps);
                    }
                    if (MODE == 3) {  // reader: if exactly one card in a NONE guess is "new", it is the test card = solution
                        int cntNew = 0, nk = -1, ni = -1;
                        for (int j = 0; j < cnt; j++) {
                            int k = gk[j], i = gi[j];
                            if (!oppHas[k][i] && !solvedCard[k] && oppGuessCnt[k][i] == 1) cntNew++, nk = k, ni = i;
                        }
                        if (cntNew == 1) solvedCard[nk] = ni;
                    }
                } else {
                    // reveal the card that leaks the least
                    int best = -1;
                    double bs = -1e18;
                    for (int j : held) {
                        int k = gk[j], i = gi[j];
                        double s;
                        if (shownToOpp[k][i]) s = 1e9;
                        else if (oppCount(k) == 1) s = 1e8;
                        else s = oppCount(k) * 1000.0 + (rng() % 100);
                        if (s > bs) bs = s, best = j;
                    }
                    int k = gk[best], i = gi[best];
                    shownToOpp[k][i] = true;
                    say("SHOW " + cardStr(k, i));
                    if (oppCount(k) == 1) pk[k] = 1;
                }
                double after = pkProd();
                qNow = before < 1 ? max(0.0, (after - before) / (1 - before)) : after;
            }
        } else if (cmd == "RESULT") {
            string r;
            is >> r;
            if (r == "SHOWN") {
                string c;
                int k, i;
                is >> c;
                if (parseCard(c, k, i)) oppHas[k][i] = true;
            } else {
                for (int k = 0; k < 3; k++)
                    if (lastGuess[k] && !mine[k][lastGuess[k]]) {
                        solvedCard[k] = lastGuess[k];
                        if (lastTested >> k & 1) pk[k] = max(pk[k], 0.8);  // they may read our guess
                    }
            }
            refresh();
        } else if (cmd == "GAME_END") {
            break;
        }
    }
    return 0;
}
