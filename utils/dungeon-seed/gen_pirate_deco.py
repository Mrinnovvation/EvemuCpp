#!/usr/bin/env python3
"""Decoration for the pirate combat anomalies.

gen_dungeon_seed.py builds each combat anomaly's room from the legacy tables, taking its deco from
dunRoomData -> dunGroupData.  The 30 generic-pirate templates (faction digit 8) reference dunGroupData
groups 1010..1090, and none of those rows exist in sql/base/dunGroupData.sql.gz, so deco() returns 0 for
every one of them: 150 of the 165 seeded combat anomalies get their npc waves and an empty room.  Only the
15 rogue-drone templates (faction digit 6) resolve, which is why the only props in the seed are Drone
Bunkers and Drone Walls.

The nine missing groups line up with the nine structures each pirate faction has in invTypes -- Bunker,
Elevator, Junction, Lookout, Battery, Wall, Barricade, Fence, Barrier -- the same nine kinds the drone
sites use.  This writes those in, scattered the way deco() scatters a zero-position row.

It reads the generated seed SQL (deterministic, in the repo) rather than a live database, so it needs
nothing but the checkout.  Output is deterministic.

    python -I utils/dungeon-seed/gen_pirate_deco.py
"""

import argparse
import math
import os
import random
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SEED_SQL = os.path.join(ROOT, 'sql', 'migrations', '20261011133100-F13-dungeon-seed.sql')
DEFAULT_OUT = os.path.join(ROOT, 'sql', 'migrations', '20261014120000-F13-pirate-anomaly-deco.sql')

ARCH_ANOMALY = 7
DRONE_FACTION = 500022
OBJECT_BASE = 7000000          # his seed uses 5000001+, the legacy editor rows 6000001+

# the nine structure kinds, per faction, in the order the drone set uses them
STRUCTURES = {
    500010: [16796, 16797, 16798, 16799, 16794, 16800, 16801, 16802, 16803],   # Guristas
    500011: [16757, 16758, 16759, 16760, 16761, 16762, 16763, 16764, 16765],   # Angel Cartel
    500012: [16814, 16815, 16816, 16817, 16818, 16819, 16820, 16813, 16821],   # Sansha
    500019: [16766, 16767, 16768, 16769, 16770, 16771, 16772, 16773, 16774],   # Blood Raiders
    500020: [16822, 16823, 16824, 16825, 16826, 16827, 16828, 16829, 16830],   # Serpentis
}
# faction-neutral texture, all published with graphics and already used by the drone sites
PROPS = [2612, 10141, 30802]   # Hollow Asteroid, Debris - Broken Engine Part 2, Debris - Crumpled Metal


def split_tuples(block):
    """the (..),(..) tuples of an INSERT ... VALUES block, respecting quotes"""
    out, depth, cur, quote = [], 0, [], None
    for ch in block:
        if quote:
            cur.append(ch)
            if ch == quote and (len(cur) < 2 or cur[-2] != '\\'):
                quote = None
            continue
        if ch in "'\"":
            quote = ch
            cur.append(ch)
            continue
        if ch == '(':
            depth += 1
            if depth == 1:
                cur = []
                continue
        elif ch == ')':
            depth -= 1
            if depth == 0:
                out.append(''.join(cur))
                continue
        if depth > 0:
            cur.append(ch)
    return out


def split_fields(tup):
    out, cur, quote = [], [], None
    for ch in tup:
        if quote:
            cur.append(ch)
            if ch == quote:
                quote = None
            continue
        if ch in "'\"":
            quote = ch
            cur.append(ch)
            continue
        if ch == ',':
            out.append(''.join(cur).strip())
            cur = []
            continue
        cur.append(ch)
    out.append(''.join(cur).strip())
    return out


def load_insert(sql, table):
    """every VALUES tuple of every INSERT INTO <table> in the file, as lists of raw fields"""
    rows = []
    for m in re.finditer(r'INSERT INTO `?%s`?\s*\([^)]*\)\s*VALUES\s*(.*?);' % re.escape(table), sql, re.S):
        for tup in split_tuples(m.group(1)):
            rows.append(split_fields(tup))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=DEFAULT_OUT)
    args = ap.parse_args()

    sql = open(SEED_SQL, encoding='utf-8').read()
    # dunDungeons: dungeonID, name, status, factionID, archetypeID, uuid, nameID, difficulty, minSec, maxSec, weight
    dungeons = {}
    for r in load_insert(sql, 'dunDungeons'):
        did, faction, arch, difficulty = int(r[0]), int(r[3]), int(r[4]), int(r[7])
        if arch == ARCH_ANOMALY and faction != DRONE_FACTION and faction in STRUCTURES:
            dungeons[did] = (faction, difficulty)
    # dunRooms: roomID, roomName, dungeonID, arrivalX, arrivalY, arrivalZ
    rooms = []
    for r in load_insert(sql, 'dunRooms'):
        did = int(r[2])
        if did in dungeons:
            rooms.append((int(r[0]), did))
    rooms.sort()

    objects = []
    for room_id, did in rooms:
        faction, difficulty = dungeons[did]
        rnd = random.Random(room_id)            # deterministic per room
        kinds = STRUCTURES[faction][:]
        rnd.shuffle(kinds)
        n_struct = 2 + max(1, min(3, (difficulty + 2) // 3))        # 3..5 by difficulty
        n_props = 1 + (room_id % 2)                                 # 1..2
        picks = kinds[:n_struct] + [rnd.choice(PROPS) for _ in range(n_props)]
        for type_id in picks:
            # deco() scatters a zero-position legacy row 5-30 km out, flattened in y
            d = rnd.uniform(5000, 30000)
            a = rnd.uniform(0, 2 * math.pi)
            x, y, z = d * math.cos(a), rnd.uniform(-3000, 3000), d * math.sin(a)
            objects.append((OBJECT_BASE + len(objects) + 1, room_id, type_id, round(x), round(y), round(z)))

    with open(args.out, 'w', encoding='utf-8') as f:
        w = f.write
        w('-- F13 pirate anomaly deco:  the rooms of the five pirate factions\' combat anomalies are empty\n')
        w('--  because the legacy dunGroupData groups their templates reference (1010..1090) are absent from\n')
        w('--  sql/base/dunGroupData.sql.gz, so gen_dungeon_seed.py finds nothing to place.  GENERATED by\n')
        w('--  utils/dungeon-seed/gen_pirate_deco.py - do not edit by hand, change the generator and run it again.\n')
        w('-- +migrate Up\n')
        w('INSERT INTO `dunRoomObjects` (objectID, roomID, typeID, groupID, x, y, z, radius) VALUES\n')
        w(',\n'.join('(%u, %u, %u, 0, %d, %d, %d, 0)' % o for o in objects))
        w(';\n\n')
        w('-- +migrate Down\n')
        w('DELETE FROM dunRoomObjects WHERE objectID BETWEEN %u AND %u;\n' % (OBJECT_BASE, OBJECT_BASE + 999999))

    print('rooms %u, objects %u -> %s' % (len(rooms), len(objects), os.path.relpath(args.out, ROOT)))


if __name__ == '__main__':
    main()
