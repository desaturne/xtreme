import sys


def main():
    MOD = 998244353
    data = sys.stdin.buffer.read().split()
    n, m = int(data[0]), int(data[1])
    mul = [1] * (m + 2)
    add = [0] * (m + 2)
    zc = [0] * (m + 2)
    idx = 2
    for _ in range(n):
        l = int(data[idx]); r = int(data[idx + 1]); p = int(data[idx + 2]); q = int(data[idx + 3])
        idx += 4
        if p == q:
            zc[l] += 1
            zc[r + 1] -= 1
            continue
        prob = p * pow(q, MOD - 2, MOD) % MOD
        notp = (1 - prob) % MOD
        inv = pow(notp, MOD - 2, MOD)
        mul[l] = mul[l] * notp % MOD
        mul[r + 1] = mul[r + 1] * inv % MOD
        ratio = prob * inv % MOD
        add[l] = (add[l] + ratio) % MOD
        add[r + 1] = (add[r + 1] - ratio) % MOD
    P, Sm, Z = 1, 0, 0
    res = []
    for i in range(1, m + 1):
        P = P * mul[i] % MOD
        Sm = (Sm + add[i]) % MOD
        Z += zc[i]
        if Z == 0:
            res.append(P * (1 + Sm) % MOD)
        elif Z == 1:
            res.append(P)
        else:
            res.append(0)
    sys.stdout.write(' '.join(map(str, res)) + '\n')


main()
