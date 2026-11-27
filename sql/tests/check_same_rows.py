#!/usr/bin/env python3
"""Run each original query and the query libglot wrote for it in SQLite and compare the results.

Input file: one case per line, three tab separated fields: mode, original SQL, generated SQL.
  q  select, rows compared as a sorted list
  o  select with ORDER BY, rows compared in order
  u  update, the whole table is compared afterwards
Exit code 0 when every case matches.
"""
import random
import sqlite3
import sys


def make_db():
    db = sqlite3.connect(":memory:")
    db.execute("CREATE TABLE t (a INT, b INT, c INT, d INT, x INT, y INT, z INT)")
    db.execute("CREATE TABLE u (a INT, b INT)")
    rng = random.Random(20240611)
    for _ in range(120):
        db.execute("INSERT INTO t VALUES (?,?,?,?,?,?,?)", [rng.randint(0, 4) for _ in range(7)])
    for _ in range(20):
        db.execute("INSERT INTO u VALUES (?,?)", [rng.randint(0, 4) for _ in range(2)])
    return db


def run(db, sql):
    return db.execute(sql).fetchall()


def main(path):
    bad = 0
    total = 0
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line:
            continue
        mode, original, generated = line.split("\t")
        total += 1
        try:
            if mode == "u":
                d1, d2 = make_db(), make_db()
                d1.execute(original)
                d2.execute(generated)
                r1 = run(d1, "SELECT * FROM t ORDER BY rowid")
                r2 = run(d2, "SELECT * FROM t ORDER BY rowid")
            else:
                db = make_db()
                r1 = run(db, original)
                r2 = run(db, generated)
                if mode == "q":
                    r1 = sorted(r1, key=repr)
                    r2 = sorted(r2, key=repr)
        except sqlite3.Error as e:
            print("FAIL (SQLite error: %s)\n  original : %s\n  generated: %s" % (e, original, generated))
            bad += 1
            continue
        if r1 != r2:
            print("FAIL (different rows, %d against %d)\n  original : %s\n  generated: %s"
                  % (len(r1), len(r2), original, generated))
            bad += 1
    print("%d of %d queries mean the same" % (total - bad, total))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
