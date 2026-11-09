#!/usr/bin/env python3
"""
NBA kit colours: home kits are always white, the away kit uses the team's main colour and the third kit is black
(grey for the teams whose away kit is already black, red for Portland).

Usage: python3 nba_kit_colors.py <teams.csv> [<league.db>]
Adds away_color and third_color to a teams.csv and, optionally, to a league database.
"""
import csv
import sqlite3
import sys

BLACK = "#111111"
GREY = "#8D9093"
KITS = {
    "Atlanta Hawks": ("#E03A3E", BLACK),
    "Boston Celtics": ("#007A33", BLACK),
    "Brooklyn Nets": (BLACK, GREY),
    "Charlotte Hornets": ("#00788C", BLACK),
    "Chicago Bulls": ("#CE1141", BLACK),
    "Cleveland Cavaliers": ("#860038", BLACK),
    "Dallas Mavericks": ("#0053BC", BLACK),
    "Denver Nuggets": ("#0E2240", BLACK),
    "Detroit Pistons": ("#C8102E", BLACK),
    "Golden State Warriors": ("#1D428A", BLACK),
    "Houston Rockets": ("#CE1141", BLACK),
    "Indiana Pacers": ("#002D62", BLACK),
    "Los Angeles Clippers": ("#C8102E", BLACK),
    "LA Clippers": ("#C8102E", BLACK),
    "Los Angeles Lakers": ("#552583", "#FDB927"),
    "Memphis Grizzlies": ("#5D76A9", BLACK),
    "Miami Heat": ("#98002E", BLACK),
    "Milwaukee Bucks": ("#00471B", BLACK),
    "Minnesota Timberwolves": ("#0C2340", BLACK),
    "New Orleans Pelicans": ("#0C2340", BLACK),
    "New York Knicks": ("#006BB6", BLACK),
    "Oklahoma City Thunder": ("#007AC1", BLACK),
    "Orlando Magic": ("#0077C0", BLACK),
    "Philadelphia 76ers": ("#006BB6", BLACK),
    "Phoenix Suns": ("#1D1160", BLACK),
    "Portland Trail Blazers": (BLACK, "#E03A3E"),
    "Sacramento Kings": ("#5A2D81", BLACK),
    "San Antonio Spurs": (BLACK, GREY),
    "Toronto Raptors": ("#CE1141", BLACK),
    "Utah Jazz": ("#002B5C", BLACK),
    "Washington Wizards": ("#E31837", BLACK),
}


def patch_csv(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
        fields = [c for c in rows[0].keys() if c not in ("away_color", "third_color")] + ["away_color", "third_color"]
    for r in rows:
        away, third = KITS.get(r["name"], ("", ""))
        r["away_color"], r["third_color"] = away, third
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)
    print("teams.csv:", sum(1 for r in rows if r["away_color"]), "of", len(rows), "teams coloured")


def patch_db(path):
    db = sqlite3.connect(path)
    have = {r[1] for r in db.execute("PRAGMA table_info(teams)")}
    for column in ("away_color", "third_color"):
        if column not in have:
            db.execute(f"ALTER TABLE teams ADD COLUMN {column} TEXT DEFAULT ''")
    for name, (away, third) in KITS.items():
        db.execute("UPDATE teams SET away_color = ?, third_color = ? WHERE name = ?", (away, third, name))
    db.commit()
    print("database:", db.execute("SELECT COUNT(*) FROM teams WHERE away_color <> ''").fetchone()[0], "teams coloured")


if __name__ == "__main__":
    patch_csv(sys.argv[1])
    if len(sys.argv) > 2:
        patch_db(sys.argv[2])
