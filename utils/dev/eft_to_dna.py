#!/usr/bin/env python3
"""
Turn an EFT-format fit -- the text every fitting site offers as "copy to
clipboard" -- into the ship DNA string this server's /fitdna command takes.

    ./eft_to_dna.py fit.txt
    pbpaste | ./eft_to_dna.py -
    ./eft_to_dna.py - <<'EOF'
    [Catalyst, Belt Rat Catalyst]
    Magnetic Field Stabilizer II
    ...
    EOF

Prints the DNA line and the ready-to-paste command.  Names are resolved to
typeIDs against the running server's own database, so what comes out can
actually exist on this build -- a fit quoting a module from a later expansion
is reported as unresolved rather than silently dropped.

Stdlib only.  Needs the db container up; override with --mysql if you run
MariaDB elsewhere.
"""

import argparse
import re
import subprocess
import sys
from collections import OrderedDict

# "Module Name, Charge Name" | "Item Name xN" | "Item Name"
QTY = re.compile(r"^(?P<name>.+?)\s+x(?P<qty>\d+)$")


def sql(query, mysql_cmd):
    """Run one query and yield rows as lists.  -N strips the header."""
    cmd = mysql_cmd + ["-N", "-B", "-e", query]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    except FileNotFoundError:
        sys.exit("cannot run %r -- is docker on PATH?" % cmd[0])
    if out.returncode != 0:
        err = "\n".join(l for l in out.stderr.splitlines()
                        if "Using a password" not in l)
        sys.exit("query failed:\n%s" % err.strip())
    for line in out.stdout.splitlines():
        if line.strip():
            yield line.split("\t")


def parse_eft(text):
    """-> (hull_name, [(item_name, qty), ...]) in file order."""
    hull, items = None, []
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("["):
            if hull is None:
                # [Hull, Fit Name]  -- the fit name is ours to ignore
                hull = line.lstrip("[").rstrip("]").split(",")[0].strip()
            # a second [..] is a section marker in some exports; skip it
            continue
        # "Light Ion Blaster II, Void S" -- the charge is a separate item
        for piece in [p.strip() for p in line.split(",")]:
            if not piece or piece.lower() in ("empty", "[empty high slot]"):
                continue
            m = QTY.match(piece)
            if m:
                items.append((m.group("name").strip(), int(m.group("qty"))))
            else:
                items.append((piece, 1))
    return hull, items


def resolve(names, mysql_cmd):
    """Exact, case-insensitive name -> (typeID, categoryID).  Missing names omitted."""
    if not names:
        return {}
    quoted = ",".join("'%s'" % n.replace("'", "''") for n in sorted(set(names)))
    query = (
        "SELECT t.typeName, t.typeID, g.categoryID FROM invTypes t "
        "JOIN invGroups g ON g.groupID = t.groupID "
        "WHERE t.typeName IN (%s);" % quoted
    )
    found = {}
    for row in sql(query, mysql_cmd):
        if len(row) >= 3:
            found[row[0].lower()] = (int(row[1]), int(row[2]))
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file", help="EFT text file, or - for stdin")
    ap.add_argument("--mysql", default="docker compose exec -T db mariadb -u evemu -pevemu evemu",
                    help="command that opens a mysql client on the evemu db")
    args = ap.parse_args()

    text = sys.stdin.read() if args.file == "-" else open(args.file).read()
    mysql_cmd = args.mysql.split()

    hull, items = parse_eft(text)
    if hull is None:
        sys.exit("no [Hull, Name] header found -- is this EFT format?")

    wanted = [hull] + [n for n, _ in items]
    found = resolve(wanted, mysql_cmd)

    if hull.lower() not in found:
        sys.exit("hull %r not found in this build's invTypes" % hull)
    hull_id = found[hull.lower()][0]

    # Collapse runs of the same type into typeID;qty, preserving first-seen order
    # so the DNA reads in the same order as the fit.
    order, missing = OrderedDict(), []
    for name, qty in items:
        hit = found.get(name.lower())
        if hit is None:
            missing.append(name)
            continue
        type_id = hit[0]
        order[type_id] = order.get(type_id, 0) + qty

    dna = "%d:%s::" % (hull_id, ":".join("%d;%d" % (t, q) for t, q in order.items()))

    print("hull   : %s (%d)" % (hull, hull_id))
    print("items  : %d distinct, %d total" % (len(order), sum(order.values())))
    if missing:
        print("MISSING: %s" % ", ".join(sorted(set(missing))), file=sys.stderr)
        print("         (not in this build -- the rest of the fit is still usable)",
              file=sys.stderr)
    print()
    print(dna)
    print()
    print("paste into the client:")
    print("  /fitdna %s" % dna)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
