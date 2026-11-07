#!/usr/bin/env python3
"""
Builds a draft-class CSV (best prospect first) from Tankathon's public big board.

Usage: python3 build_draft_class_csv.py <output.csv> [count]
Default count is 80 (60 picks plus undrafted players). Ratings follow the board rank; attributes come
from position, size and college/league per-game numbers. Real prospect names: check the legal position
before shipping them.
"""
import csv
import html
import random
import re
import sys
import unicodedata
import urllib.request

URL = "https://www.tankathon.com/big_board"
FIELDS = ["first_name", "last_name", "position", "birth_date", "height", "weight", "pace", "shooting",
          "ball_control", "defense", "physical", "technical", "current_ability", "potential_ability"]


def ascii_name(text):
    return unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii").strip()


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def load_rows():
    req = urllib.request.Request(URL, headers={"User-Agent": "Mozilla/5.0"})
    page = urllib.request.urlopen(req, timeout=30).read().decode("utf-8", "ignore")
    blocks = page.split('<div class="mock-row" data-pos="')[1:]
    rows = []
    for b in blocks:
        pos = b[:b.index('"')]
        name = re.search(r'class="mock-row-name">(.*?)</div>', b, re.S)
        meas = re.search(r"<div>(\d+)&#39;(\d+)&quot;</div><div>(\d+)", b)
        age = re.search(r"([\d.]+) yrs", b)
        stats_block = re.search(r'stats-per-game".*?</div></div></div>', b, re.S)
        stats = {}
        if stats_block:
            for label, value in re.findall(r'class="label">(\w+)</div><div class="value[^"]*">([\d.]+)', stats_block.group(0)):
                stats[label] = float(value)
        if not (name and meas and age):
            continue
        rows.append({
            "name": ascii_name(html.unescape(name.group(1)).strip()), "pos": pos,
            "height": round((int(meas.group(1)) * 12 + int(meas.group(2))) * 0.0254, 2),
            "weight": int(round(int(meas.group(3)) * 0.45359)), "age": float(age.group(1)), "stats": stats,
        })
    return rows


def build(out_path, count):
    unique, seen = [], set()
    for row in load_rows():
        if row["name"].lower() not in seen:
            seen.add(row["name"].lower())
            unique.append(row)
    rows = unique[:count]
    if not rows:
        sys.exit("No prospects found - the page layout may have changed.")
    random.seed(7)
    out = []
    for i, r in enumerate(rows):
        first, _, last = r["name"].partition(" ")
        pos = r["pos"] if r["pos"] in ("PG", "SG", "SF", "PF", "C") else {"G": "SG", "F": "SF"}.get(r["pos"], "SF")
        cur = int(clamp(round(77 - i * 0.30), 55, 77))
        pot = int(clamp(round(96 - i * 0.5), 62, 96))
        pot = max(pot, cur + 2)
        s = r["stats"]
        base = cur / 5.0
        shooting = base + (s.get("pts", 10) - 11) / 4.0
        ball = base + (s.get("ast", 2) - 2.0) / 1.0 + (0.8 if pos == "PG" else 0)
        defense = base + (s.get("blk", 0.5) * 1.3 + s.get("stl", 0.8) * 1.6 - 2.0) / 1.2
        physical = base + (s.get("reb", 4) - 4.5) / 1.5 + (r["weight"] - 95) / 14.0
        technical = base + (s.get("ast", 2) - 2.0) / 2.0
        pace = base + (1.0 if pos in ("PG", "SG") else (-1.0 if pos == "C" else 0))
        year = 2026 - int(r["age"]) if r["age"] else 2006
        birth = f"{year}-{random.randint(1, 12):02d}-{random.randint(1, 28):02d}"
        out.append([first, last or "-", pos, birth, f"{r['height']:.2f}", r["weight"],
                    *[int(clamp(round(v), 3, 20)) for v in (pace, shooting, ball, defense, physical, technical)], cur, pot])
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(FIELDS)
        w.writerows(out)
    print(f"Wrote {len(out)} prospects to {out_path}: " + ", ".join(f"{o[0]} {o[1]}" for o in out[:8]))


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else "draft-2027.csv", int(sys.argv[2]) if len(sys.argv) > 2 else 80)
