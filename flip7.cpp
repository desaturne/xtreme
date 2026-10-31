#include <bits/stdc++.h>
using namespace std;

int N, P;
long long deckCnt[13], discardCnt[13];
long long deckTotal = 0;
vector<array<bool,13>> hand;   // values held by each player this round
vector<int> score;              // round score per player
vector<bool> done;              // stopped or busted this round
vector<int> tableCards;         // all cards on the table this round

void resetRound() {
    for (int i = 0; i < N; i++) { hand[i].fill(false); score[i] = 0; done[i] = false; }
}

void endRoundIfNeeded() {
    for (int i = 0; i < N; i++) if (!done[i]) return;
    for (int c : tableCards) discardCnt[c]++;
    tableCards.clear();
    resetRound();
}

void drawCard(int p, int c) {
    deckCnt[c]--; deckTotal--;
    tableCards.push_back(c);
    if (deckTotal == 0) {               // reshuffle discards (not table cards)
        for (int v = 1; v <= 12; v++) { deckCnt[v] = discardCnt[v]; deckTotal += discardCnt[v]; discardCnt[v] = 0; }
    }
    if (hand[p][c]) { score[p] = 0; done[p] = true; }   // bust
    else { hand[p][c] = true; score[p] += c; }
}

int main() {
    if (!(cin >> N >> P)) return 0;
    hand.assign(N, {}); score.assign(N, 0); done.assign(N, false);
    for (int v = 1; v <= 12; v++) { deckCnt[v] = v; discardCnt[v] = 0; deckTotal += v; }
    resetRound();

    int i;
    while (cin >> i) {
        if (i == -1) break;
        if (i == P) {
            // Draw iff E[score after one draw] > S  <=>  sum_{v not held} d[v]*(S+v) > S*T
            long long S = score[P], lhs = 0;
            for (int v = 1; v <= 12; v++) if (!hand[P][v]) lhs += deckCnt[v] * (S + v);
            int act = (lhs > S * deckTotal) ? 1 : 0;
            cout << act << endl;   // endl flushes
            if (act == 1) { int c; cin >> c; drawCard(P, c); }
            else done[P] = true;
        } else {
            int a; cin >> a;
            if (a == 1) { int c; cin >> c; drawCard(i, c); }
            else done[i] = true;
        }
        endRoundIfNeeded();
    }
    return 0;
}
