#!/usr/bin/env python3
"""lzss.py - compressor for the format decoded by lzdec.h (optimal parsing).

usage: lzss.py <in> <out>
Output = raw compressed stream (no header); the caller records the sizes.
"""
import sys

WIN, MINLEN, MAXLEN = 4096, 3, 18

def compress(data):
    n = len(data)
    # best[i] = (cost in bits from i to end, action) ; action = None (literal) or (off,len)
    INF = 1 << 60
    cost = [INF] * (n + 1)
    act = [None] * (n + 1)
    cost[n] = 0
    # index positions by 3-byte prefix for match search
    from collections import defaultdict
    idx = defaultdict(list)
    for i in range(n - 2):
        idx[data[i:i+3]].append(i)
    for i in range(n - 1, -1, -1):
        cost[i] = cost[i + 1] + 9
        act[i] = None
        if i + MINLEN <= n:
            for j in idx.get(data[i:i+3], ()):
                if j >= i: break
                if i - j > WIN: continue
                l = 3
                while l < MAXLEN and i + l < n and data[j + l] == data[i + l]:
                    l += 1
                # try all lengths (shorter may be optimal)
                for ln in range(MINLEN, l + 1):
                    c = cost[i + ln] + 17
                    if c < cost[i]:
                        cost[i] = c
                        act[i] = (i - j, ln)
    out = bytearray()
    i = 0
    while i < n:
        ctrl = 0
        items = []
        for b in range(8):
            if i >= n:
                break
            a = act[i]
            if a is None:
                ctrl |= 0x80 >> b
                items.append(bytes([data[i]]))
                i += 1
            else:
                off, ln = a
                o = off - 1
                items.append(bytes([o >> 4, ((o & 15) << 4) | (ln - 3)]))
                i += ln
        out.append(ctrl)
        for it in items:
            out += it
    return bytes(out)

def decompress(src, outlen):
    dst = bytearray()
    ctrl = bits = 0
    s = 0
    while len(dst) < outlen:
        if not bits:
            ctrl = src[s]; s += 1; bits = 8
        if ctrl & 0x80:
            dst.append(src[s]); s += 1
        else:
            b1, b2 = src[s], src[s+1]; s += 2
            ln = (b2 & 15) + 3
            off = ((b1 << 4) | (b2 >> 4)) + 1
            for _ in range(ln):
                dst.append(dst[-off])
        ctrl = (ctrl << 1) & 0xFF
        bits -= 1
    return bytes(dst)

if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    c = compress(data)
    assert decompress(c, len(data)) == data, 'self-check failed'
    open(sys.argv[2], 'wb').write(c)
    print(f'{sys.argv[1]}: {len(data)} -> {len(c)} bytes')
