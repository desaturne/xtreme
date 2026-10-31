import sys
import csv
import re
import heapq

EDU_RANK = {'p': 0, 'd': 0, 'm': 1, 'u': 2, 'b': 2, 'a': 3}

NUM_RE = re.compile(r'[-+]?\d+(?:\.\d+)?')
TZ_RE = re.compile(r'([+-]?)(\d{1,2}):?(\d{2})?$')
ID_RE = re.compile(r'(.*?)(\d+)$')


def clean(s):
    s = (s.replace('﻿', '').replace('\xa0', ' ')
          .replace('−', '-').replace('–', '-').replace('—', '-'))
    return s.strip().strip('"\'').strip()


def edu_rank(s):
    t = re.sub(r'[^a-z]', '', s.lower())
    return EDU_RANK.get(t[:1], 4)


def num(s):
    m = NUM_RE.search(s)
    return float(m.group()) if m else 0.0


def tz_minutes(s):
    s = clean(s).upper().replace('UTC', '').replace('GMT', '').replace(' ', '')
    m = TZ_RE.match(s)
    if m:
        v = int(m.group(2)) * 60 + int(m.group(3) or 0)
        return -v if m.group(1) == '-' else v
    return round(num(s) * 60)


def domains(parts):
    out = set()
    for p in parts:
        for d in re.split(r'[;|]', p):
            d = ' '.join(d.strip().strip('.').split()).lower()
            if d:
                out.add(d)
    return out


def id_key(eid):
    m = ID_RE.match(eid)
    return (m.group(1), int(m.group(2)), eid) if m else (eid, -1, eid)


def header_key(h):
    h = re.sub(r'[^a-z]', '', h.lower())
    if h == 'id' or ('id' in h and 'expert' in h):
        return 'id'
    if 'domain' in h:
        return 'dom'
    if 'educ' in h or 'degree' in h:
        return 'edu'
    if 'year' in h or 'experience' in h:
        return 'yrs'
    if 'zone' in h or h in ('tz', 'utc', 'offset'):
        return 'tz'
    if 'name' in h:
        return 'name'
    return None


def split(line):
    return [clean(f) for f in next(csv.reader([line], skipinitialspace=True))]


def main():
    text = sys.stdin.buffer.read().decode('utf-8-sig', 'replace')
    lines = [l.rstrip() for l in text.splitlines() if l.strip()]
    if len(lines) < 2:
        return

    crit = split(lines[0]) + [''] * 6
    chair_id = crit[0].upper()
    needed = max(int(num(crit[1])), 0)
    min_edu = edu_rank(crit[2]) if crit[2] else 4
    min_yrs = num(crit[3])
    want_dom = next(iter(domains([crit[4]])), '')
    max_tz = abs(tz_minutes(crit[5])) if crit[5] else float('inf')

    hdr = split(lines[1])
    ncol = max(len(hdr), 6)
    idx = {}
    for i, h in enumerate(hdr):
        k = header_key(h)
        if k and k not in idx:
            idx[k] = i
    for i, k in enumerate(('id', 'name', 'dom', 'edu', 'yrs', 'tz')):
        idx.setdefault(k, i)
    i_id, i_dom, i_edu, i_yrs, i_tz = idx['id'], idx['dom'], idx['edu'], idx['yrs'], idx['tz']
    lo, hi = sorted((idx['name'], i_dom))

    def extract(f):
        if len(f) <= ncol:
            f = f + [''] * (ncol - len(f))
            return f[i_id], [f[i_dom]], f[i_edu], f[i_yrs], f[i_tz]
        extra = len(f) - ncol
        v = {}
        for c in range(ncol):
            if c < lo:
                v[c] = f[c]
            elif c > hi:
                v[c] = f[c + extra]
        mid = f[lo:hi + extra + 1]
        checks = {
            i_id: lambda t: re.search(r'\d', t) and ' ' not in t,
            i_edu: lambda t: re.fullmatch(r'(?i)(ph\.?d|doctor\w*|ma\w*ers?|m\.?s\.?c?|mba|undergrad\w*|bachelor\w*|b\.?[sa]\.?|associates?|a\.?a\.?)', t),
            i_yrs: lambda t: re.fullmatch(r'\d+', t),
            i_tz: lambda t: re.fullmatch(r'(?i)(utc|gmt)?[+-]?\d{1,2}(:\d{2})?', t),
        }
        for c in range(lo + 1, hi):
            if c in checks:
                for k, t in enumerate(mid):
                    if checks[c](t):
                        v[c] = mid.pop(k)
                        break
        return v.get(i_id, ''), mid, v.get(i_edu, ''), v.get(i_yrs, ''), v.get(i_tz, '')

    chair = None
    experts = []
    for line in lines[2:]:
        eid, dom, edu, yrs, tz = extract(split(line))
        e = (line, eid.upper(), domains(dom), edu_rank(edu), num(yrs), tz_minutes(tz))
        if e[1] == chair_id:
            if chair is None:
                chair = e
        else:
            experts.append(e)

    ctz = chair[5] if chair else 0
    cands = [e for e in experts
             if want_dom in e[2] and e[3] <= min_edu and e[4] >= min_yrs
             and abs(e[5] - ctz) <= max_tz]

    if len(cands) > needed:
        cands = heapq.nsmallest(needed, cands,
                                key=lambda e: (e[3], -e[4], abs(e[5] - ctz), id_key(e[1])))
    cands.sort(key=lambda e: (e[3], -e[4], id_key(e[1])))

    out = ([chair[0]] if chair else []) + [e[0] for e in cands]
    if out:
        sys.stdout.write('\n'.join(out) + '\n')


main()
