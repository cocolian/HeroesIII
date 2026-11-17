"""Calibrate transition selection against stored view IDs in real SoD maps.

The corpus is fixed and de-duplicated by terrain bytes. New downloads are
withheld for validation rather than silently added to the calibration set.
"""
import hashlib
from collections import Counter, defaultdict
from pathlib import Path
from h3m_terrain import read_terrain

ASSETS = {2:'grastl.def', 0:'dirttl.def', 8:'watrtl.def'}
NEIGHBOURS = [(-1,-1),(0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0)]
CORPUS = ['wayfarer','war-of-seven-realms','empire-of-the-world-2',
          'ira-the-mage','to-tame-a-land','lord-of-the-rings']


def geometry(kind, frame):
    """Group original texture alternatives; ignore geometrically inert flips."""
    index, x, y = frame
    ranges = [(0,3),(4,7),(8,11),(12,15),(16,17),(18,19)]
    if kind == 2:
        ranges += [(20,23),(24,27),(28,31),(32,35),(36,37),(38,39),(49,72)]
    elif kind == 0:
        ranges += [(21,44)]
    else:
        ranges += [(20,32)]
    first = next((a for a,b in ranges if a <= index <= b), index)
    if first in (4,24):
        y = False
    if first in (8,28):
        x = False
    if (kind == 2 and first == 49) or (kind == 0 and first == 21) or (kind == 8 and first == 20):
        x = y = False
    return first, bool(x), bool(y)


def observations(m):
    size, tiles = m['size'], m['tiles']
    for z in range(m['levels']):
        for y in range(1,size-1):
            for x in range(1,size-1):
                tile = tiles[(z*size+y)*size+x]
                kind = tile[0]
                if kind not in ASSETS:
                    continue
                neighbours = [tiles[(z*size+y+dy)*size+x+dx][0] for dx,dy in NEIGHBOURS]
                if any(n not in (0,1,2,8) for n in neighbours):
                    continue
                digits = [(0 if n == kind else 1 if n == 0 else 2) if kind == 2
                          else int(n in (1,8)) if kind == 0 else int(n != 8)
                          for n in neighbours]
                yield kind, digits, (tile[1],bool(tile[6]&1),bool(tile[6]&2)), (x,y,z)


def code_of(kind, digits):
    base = 3 if kind == 2 else 2
    return sum(d*base**i for i,d in enumerate(digits))


def calibrate(root, tables):
    models = {kind:defaultdict(Counter) for kind in ASSETS}
    sources, seen = [], set()
    for slug in CORPUS:
        path = Path(root)/'maps/community'/slug/'1.h3m'
        m = read_terrain(path)
        digest = hashlib.sha256(b''.join(bytes(t) for t in m['tiles'])).hexdigest()
        if digest in seen:
            continue
        seen.add(digest)
        count = 0
        for kind,digits,frame,position in observations(m):
            if not any(digits):
                continue
            count += 1
            for fx,fy in [(False,False),(True,False),(False,True),(True,True)]:
                reflected = [digits[NEIGHBOURS.index((-dx if fx else dx,-dy if fy else dy))]
                             for dx,dy in NEIGHBOURS]
                f = frame[0],frame[1]^fx,frame[2]^fy
                models[kind][code_of(kind,reflected)][f] += 1
        sources.append({'file':str(path.relative_to(root)),
                        'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                        'terrainSha256':digest,'tiles':len(m['tiles']),
                        'eligibleTransitions':count,'terrainOffset':m['terrainOffset']})
    corrected = {}
    for kind,model in models.items():
        changed = 0
        for code,counts in model.items():
            # Broad neighbourhoods can legally use multiple corner families.
            # Choose the most frequent original geometry, then retain its
            # original texture alternatives; never mix conflicting corners.
            families = Counter()
            for f,n in counts.items():
                families[geometry(kind,f)] += n
            family = min(families, key=lambda f:(-families[f],f))
            options = [list(f) for f,n in sorted(counts.items(),key=lambda v:(-v[1],v[0]))
                       if geometry(kind,f) == family][:8]
            previous = tables[ASSETS[kind]][code]
            if any(geometry(kind,f) != family for f in previous):
                changed += 1
            tables[ASSETS[kind]][code] = options
        corrected[ASSETS[kind]] = {'calibratedNeighbourhoods':len(model),
                                  'correctedGeometryChoices':changed}
    return {'method':'Stored original H3M view IDs and XY flags; dominant geometry per neighbourhood',
            'sources':sources,'calibration':corrected,
            'withheldMaps':['seven-lakes','the-lone-knight','jennifers-dream-journey','jedi-story'],
            'remainingLimit':'Eight neighbours cannot fully encode the editor corner dependencies; unobserved patterns retain the pixel-based fallback.'}
