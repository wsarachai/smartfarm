# -*- coding: utf-8 -*-
"""Geometric check of PlacePartsPT.pas, for water-temp-node STM32WL_PT.

    python hardware/scripts/check_floorplan.py

Answers two questions before Altium is opened: does anything fall off the
160 x 120 board, and does any pair of components sit closer than the
ComponentClearance rule allows.

WHY THIS EXISTS, WHICH IS THE PART WORTH READING
    The first version of this check carried a hand-typed table of footprint
    bounding boxes.  It passed, and Altium's DRC then found two component
    clearance violations it had missed -- because TO220-VERT-STAG's silk body
    runs from +1.60 to +6.30 ABOVE its pad row, so the part is 10.4 x 9.6 mm
    and the hand-typed box said 7.5 x 4.4.  A footprint model typed out by
    hand is the same class of mistake as a symbol whose pin numbers were typed
    out by hand: it looks right and nothing disagrees with it.

    So nothing here is typed.  Extents are derived by reading the actual
    AddPadPT / AddRowPT / AddSilkBoxPT / AddSilkPT calls out of
    MakeFootprintsPT.pas, and the placement is read out of PlacePartsPT.pas.
    Against the saved board this reproduces Altium's DRC exactly: the same two
    violations, no false positives and no misses.

    CDSOD323_BRN-M is the one exception and is stated below: it is an SMD land
    from a vendor library, not built by MakeFootprintsPT.pas, so there is no
    source to derive it from.
"""

import itertools
import os
import re
import sys
import warnings

# _num() evals small Pascal expressions; some fold to shapes Python
# warns about but evaluates correctly.
warnings.filterwarnings('ignore', category=SyntaxWarning)

HERE = os.path.dirname(os.path.abspath(__file__))

BOARD_W, BOARD_H = 160.0, 120.0
SILK_W = 0.20      # AddSilkPT track width, from MakeFootprintsPT.pas
GAP = 0.254        # ComponentClearance = 10 mil, read out of the board's Rules6

# The only footprint not built by MakeFootprintsPT.pas: vendor SMD land.
EXTRA_FP = {'CDSOD323_BRN-M': (-1.60, -0.90, 1.60, 0.90)}


def _num(text, consts):
    text = text.strip()
    try:
        return float(text)
    except ValueError:
        pass
    if text in consts:
        return consts[text]
    expr = re.sub(r'[A-Za-z_]\w*', lambda m: repr(consts.get(m.group(0), 0.0)), text)
    return float(eval(expr, {'__builtins__': {}}))


def _extents(body, consts):
    xs, ys = [], []

    def pad(x, y, w, h):
        xs.extend([x - w / 2, x + w / 2])
        ys.extend([y - h / 2, y + h / 2])

    def silk(x1, y1, x2, y2):
        xs.extend([min(x1, x2) - SILK_W / 2, max(x1, x2) + SILK_W / 2])
        ys.extend([min(y1, y2) - SILK_W / 2, max(y1, y2) + SILK_W / 2])

    for m in re.finditer(r"AddPadPT\(Comp,\s*[^,]+,\s*([^;]+?)\);", body):
        a = [_num(p, consts) for p in m.group(1).split(',')[:5]]
        pad(a[0], a[1], a[2], a[3])
    for m in re.finditer(r"AddRowPT\(Comp,\s*([^;]+?)\);", body):
        a = m.group(1).split(',')
        n = int(_num(a[0], consts))
        x0, y0 = _num(a[1], consts), _num(a[2], consts)
        pitch, dia = _num(a[3], consts), _num(a[4], consts)
        for i in range(n):
            pad(x0 + i * pitch, y0, dia, dia)
    for m in re.finditer(r"AddSilkBoxPT\(Comp,\s*([^;]+?)\);", body):
        silk(*[_num(p, consts) for p in m.group(1).split(',')[:4]])
    for m in re.finditer(r"AddSilkPT\(Comp,\s*([^;]+?)\);", body):
        silk(*[_num(p, consts) for p in m.group(1).split(',')[:4]])

    return (min(xs), min(ys), max(xs), max(ys)) if xs else None


def _locals(body, extra=None):
    c = dict(extra or {})
    for m in re.finditer(r"^\s*(\w+)\s*:=\s*([^;]+);", body, re.M):
        try:
            c[m.group(1)] = _num(m.group(2), c)
        except Exception:
            pass
    return c


def footprints():
    """Return {pattern: (x1, y1, x2, y2)} derived from MakeFootprintsPT.pas."""
    src = open(os.path.join(HERE, 'MakeFootprintsPT.pas'), encoding='utf-8').read()
    out = dict(EXTRA_FP)
    for m in re.finditer(r"Procedure (Make_\w+)\(([^)]*)\)(.*?)\nEnd;", src, re.S):
        name, params, body = m.group(1), m.group(2), m.group(3)
        named = re.findall(r"NewCompPT\(Lib,\s*'([^']+)'", body)
        if named:
            e = _extents(body, _locals(body))
            if e:
                out[named[0]] = e
            continue
        # parameterised builder: resolve it at each of its call sites
        pnames = [p.strip() for seg in params.split(';')
                  for p in seg.split(':')[0].split(',')
                  if p.strip() and p.strip() != 'Lib']
        for call in re.finditer(re.escape(name) + r"\(Lib,\s*([^;]+?)\);", src):
            args = [a.strip() for a in call.group(1).split(',')]
            bind, pattern = {}, None
            for pn, av in zip(pnames, args):
                if av.startswith("'"):
                    v = av.strip("'")
                    if pattern is None and ('-' in v or v.isupper()):
                        pattern = v
                else:
                    try:
                        bind[pn] = float(av)
                    except ValueError:
                        pass
            if pattern:
                e = _extents(body, _locals(body, bind))
                if e:
                    out[pattern] = e
    return out


def placement():
    """Return [(designator, x, y, rotation)] from PlacePartsPT.pas."""
    src = open(os.path.join(HERE, 'PlacePartsPT.pas'), encoding='utf-8').read()
    return [(d, float(x), float(y), float(r)) for d, x, y, r in re.findall(
        r"PlaceOne\(Board,\s*'([A-Z0-9]+)',\s*(-?[\d.]+),\s*(-?[\d.]+),\s*(-?[\d.]+)",
        src)]


def des_to_pattern():
    """Designator -> footprint, as PlacePartsPT.pas and the BOM have it."""
    m = {}
    for d in ['R%d' % n for n in list(range(1, 26)) + [38, 39, 40, 41, 42]]:
        m[d] = 'AXIAL-R-P1016'
    for d in ('D10', 'D11', 'D12'):
        m[d] = 'DO41-P762'
    for d in ('D1', 'D2', 'D3', 'D4', 'D5', 'D6'):
        m[d] = 'CDSOD323_BRN-M'
    for d in ('C1', 'C2', 'C8', 'C10', 'C22'):
        m[d] = 'CERAMIC-P508'
    for d in ('C11', 'C19'):
        m[d] = 'RADIAL-D8-P35'
    for d in ('Q2', 'Q3'):
        m[d] = 'TO220-VERT-STAG'
    for d in ('J1', 'J2', 'J3', 'J4', 'J5', 'J6'):
        m[d] = 'PHX-MC15-3-G-35-PT'
    for d in ('J9', 'J10', 'J11', 'J12'):
        m[d] = 'PHX-MC15-4-G-35-PT'
    m.update({'D9': 'DO15-P1270', 'C21': 'RADIAL-D5-P20', 'Q4': 'TO92-INLINE-P254',
              'Q1': 'SOT23-3-M', 'F1': 'FUSEHOLDER-5X20-P226', 'U3': 'MOD-TCA9548A',
              'U7': 'MOD-DFR0570', 'J7': 'HDR2X20-BOX-PT', 'CN6': 'HDR1X8-P254-PT',
              'J13': 'HDR1X3-P254-PT', 'J14': 'PHX-MC15-2-G-35-PT'})
    return m


def main():
    fp, dmap, parts = footprints(), des_to_pattern(), placement()
    problems = 0

    unknown = sorted({d for d, _, _, _ in parts if dmap.get(d) not in fp})
    if unknown:
        print('no footprint extent for:', unknown)
        problems += len(unknown)

    boxes = {}
    for d, x, y, rot in parts:
        x1, y1, x2, y2 = fp[dmap[d]]
        if rot % 180 == 90:
            x1, y1, x2, y2 = -y2, x1, -y1, x2
        boxes[d] = (x + x1, y + y1, x + x2, y + y2)

    print('components: %d   footprints derived: %d' % (len(parts), len(fp)))

    print('\noff the board (0..%g x 0..%g mm):' % (BOARD_W, BOARD_H))
    off = [(d, b) for d, b in sorted(boxes.items())
           if b[0] < 0 or b[1] < 0 or b[2] > BOARD_W or b[3] > BOARD_H]
    for d, b in off:
        print('  %-5s %7.2f %7.2f -> %7.2f %7.2f' % ((d,) + b))
    if not off:
        print('  none')
    problems += len(off)

    print('\ncomponent pairs closer than %.3f mm (ComponentClearance):' % GAP)
    hits = []
    for a, b in itertools.combinations(sorted(boxes), 2):
        ax1, ay1, ax2, ay2 = boxes[a]
        bx1, by1, bx2, by2 = boxes[b]
        g = max(max(bx1 - ax2, ax1 - bx2), max(by1 - ay2, ay1 - by2))
        if g < GAP:
            hits.append((g, a, b))
    for g, a, b in sorted(hits):
        print('  %-5s vs %-5s  %s %6.2f mm' %
              (a, b, 'OVERLAP' if g < 0 else 'gap    ', g))
    if not hits:
        print('  none')
    problems += len(hits)

    tight = sorted(
        (max(max(boxes[b][0] - boxes[a][2], boxes[a][0] - boxes[b][2]),
             max(boxes[b][1] - boxes[a][3], boxes[a][1] - boxes[b][3])), a, b)
        for a, b in itertools.combinations(sorted(boxes), 2))
    print('\nfive tightest pairs, for reference:')
    for g, a, b in tight[:5]:
        print('  %-5s vs %-5s  %6.2f mm' % (a, b, g))

    print('\n%s' % ('FAIL: %d problem(s)' % problems if problems else 'PASS'))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
