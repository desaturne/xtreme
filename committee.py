import sys
import re

EDU_WORDS = ('phd', 'doctor', 'master', 'maters', 'msc', 'undergrad', 'bachelor', 'associate')


def norm_text(s):
    s = s.replace('﻿', '').replace('−', '-').replace('–', '-').replace('—', '-')
    return s.replace(' ', ' ').strip().strip('"').strip("'").strip()


def edu_rank(s):
    """0 PhD, 1 Masters, 2 Undergraduate, 3 Associates, 4 unknown (lower is better)."""
    t = re.sub(r'[^a-z]', '', s.lower())
    if not t:
        return 4
    if t.startswith(('ph', 'doc', 'dr')):
        return 0
    if t.startswith(('ma', 'ms', 'mst', 'mtech', 'mba')):
        return 1
    if t.startswith(('un', 'ug', 'ba', 'bs', 'bach', 'bt', 'be')):
        return 2
    if t.startswith(('as', 'aa')):
        return 3
    return 4


def parse_num(s):
    s = norm_text(s)
    try:
        return int(s)
    except ValueError:
        pass
    try:
        return float(s)
    except ValueError:
        m = re.search(r'[+-]?\d+(?:\.\d+)?', s)
        return float(m.group()) if m else 0


def parse_tz(s):
    """Offset in minutes. Accepts '-02:00', '+5', '-2', '5.5', 'UTC+3', '+0530'."""
    s = norm_text(s).upper().replace('UTC', '').replace('GMT', '').replace(' ', '')
    if not s:
        return 0
    m = re.match(r'^([+-]?)(\d+)(?::(\d+))?(?:\.(\d+))?$', s)
    if m:
        sign = -1 if m.group(1) == '-' else 1
        hs = m.group(2)
        mins = 0
        if m.group(3) is None and m.group(4) is None and len(hs) == 4:  # +0530
            hs, mins = hs[:2], int(hs[2:])
        h = int(hs)
        if m.group(3):
            mins = int(m.group(3))
        if m.group(4):
            mins = round(float('0.' + m.group(4)) * 60)
        return sign * (h * 60 + mins)
    return round(parse_num(s) * 60)


def split_fields(line):
    """Split on commas, but respect double-quoted fields."""
    out, cur, q = [], [], False
    for ch in line:
        if ch == '"':
            q = not q
        elif ch == ',' and not q:
            out.append(''.join(cur))
            cur = []
        else:
            cur.append(ch)
    out.append(''.join(cur))
    return [norm_text(f) for f in out]


def id_key(eid):
    m = re.match(r'^(.*?)(\d+)$', eid)
    if m:
        return (m.group(1), int(m.group(2)), eid)
    return (eid, -1, eid)


def main():
    data = sys.stdin.buffer.read().decode('utf-8', 'replace')
    lines = [l.rstrip() for l in data.replace('\r\n', '\n').replace('\r', '\n').split('\n')]
    lines = [l for l in lines if l.strip()]
    if lines:
        lines[0] = lines[0].lstrip('﻿')
    if len(lines) < 2:
        if lines:
            pass
        return

    crit = split_fields(lines[0])
    chair_id = crit[0].upper()
    needed = int(parse_num(crit[1])) if len(crit) > 1 else 0
    min_edu = edu_rank(crit[2]) if len(crit) > 2 else 4
    min_years = parse_num(crit[3]) if len(crit) > 3 else 0
    domain = re.sub(r'\s+', ' ', crit[4].strip().strip(';').strip().lower()) if len(crit) > 4 else ''
    tz_diff = abs(parse_tz(crit[5])) if len(crit) > 5 and crit[5] else float('inf')

    header = [re.sub(r'[^a-z]', '', h.lower()) for h in split_fields(lines[1])]
    ncol = len(header)

    def col(pred):
        for i, h in enumerate(header):
            if pred(h):
                return i
        return None

    i_id = col(lambda h: h in ('expertid', 'id') or ('id' in h and 'expert' in h))
    if i_id is None:
        i_id = col(lambda h: h.endswith('id'))
    i_dom = col(lambda h: 'domain' in h)
    i_edu = col(lambda h: 'educ' in h or 'degree' in h)
    i_yrs = col(lambda h: 'year' in h or 'experience' in h)
    i_tz = col(lambda h: 'zone' in h or h == 'tz' or 'timezone' in h)
    i_name = col(lambda h: 'name' in h)
    defaults = [0, 1, 2, 3, 4, 5]
    if i_id is None: i_id = defaults[0]
    if i_name is None: i_name = defaults[1]
    if i_dom is None: i_dom = defaults[2]
    if i_edu is None: i_edu = defaults[3]
    if i_yrs is None: i_yrs = defaults[4]
    if i_tz is None: i_tz = defaults[5]
    if ncol < 6:
        ncol = 6

    free_cols = sorted(c for c in (i_name, i_dom) if c is not None)

    def extract(f):
        """Return (id, domains_set, edu, years, tz) even if name/domains contain stray commas."""
        if len(f) == ncol or len(f) < ncol:
            g = lambda i: f[i] if i < len(f) else ''
            return g(i_id), [g(i_dom)], g(i_edu), g(i_yrs), g(i_tz)
        # Too many fields: anchor fixed columns from left/right, free text in the middle.
        extra = len(f) - ncol
        lo, hi = free_cols[0], free_cols[-1]
        vals = {}
        for c in range(ncol):
            if c < lo:
                vals[c] = f[c]
            elif c > hi:
                vals[c] = f[c + extra]
        middle = f[lo:hi + extra + 1]
        # fixed columns that sit between the two free columns: detect by type
        for c in range(lo + 1, hi):
            if c in (i_id, i_edu, i_yrs, i_tz):
                for k, tok in enumerate(middle):
                    ok = False
                    if c == i_edu:
                        ok = edu_rank(tok) < 4 and any(w in tok.lower() for w in EDU_WORDS)
                    elif c == i_yrs:
                        ok = re.fullmatch(r'\d+', tok) is not None
                    elif c == i_tz:
                        ok = re.fullmatch(r'(UTC|GMT)?[+-]?\d{1,2}(:\d{2})?', tok.upper()) is not None
                    elif c == i_id:
                        ok = re.search(r'\d', tok) is not None and ' ' not in tok
                    if ok:
                        vals[c] = tok
                        middle = middle[:k] + middle[k + 1:]
                        break
        g = lambda i: vals.get(i, '')
        return g(i_id), middle, g(i_edu), g(i_yrs), g(i_tz)

    chair = None
    experts = []
    for line in lines[2:]:
        f = split_fields(line)
        eid, dom_fields, edu, yrs, tz = extract(f)
        doms = set()
        for dfield in dom_fields:
            for d in re.split(r'[;|]', dfield):
                d = re.sub(r'\s+', ' ', d.strip().strip('.').strip().lower())
                if d:
                    doms.add(d)
        e = (line, eid.upper(), doms, edu_rank(edu), parse_num(yrs), parse_tz(tz))
        if e[1] == chair_id:
            if chair is None:
                chair = e
        else:
            experts.append(e)

    chair_tz = chair[5] if chair else 0

    cands = [
        e for e in experts
        if domain in e[2]
        and e[3] <= min_edu
        and e[4] >= min_years
        and abs(e[5] - chair_tz) <= tz_diff
    ]

    # selection precedence: education, years desc, time-zone proximity, id
    cands.sort(key=lambda e: (e[3], -e[4], abs(e[5] - chair_tz), id_key(e[1])))
    chosen = cands[:max(needed, 0)]

    # output order: education, years desc, id asc
    chosen.sort(key=lambda e: (e[3], -e[4], id_key(e[1])))

    out = []
    if chair:
        out.append(chair[0])
    out.extend(e[0] for e in chosen)
    sys.stdout.write('\n'.join(out) + ('\n' if out else ''))


main()
