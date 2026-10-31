import sys


def main():
    data = sys.stdin.buffer.read().split()
    n = int(data[0])
    m1 = 0
    m2 = 0
    for v in map(int, data[1:1 + n]):
        if v > m1:
            m2 = m1
            m1 = v
        elif v > m2:
            m2 = v
    x = max(1, m1 // 2)
    if n > 1:
        x = max(x, m2 - 1)
    print(x)


main()
