#!/usr/bin/env python3
"""
Builds a custom-league folder (teams.csv + players.csv) with the current NBA rosters.

Rosters, heights, weights, jersey numbers and birth dates come from ESPN's public roster API.
Ratings come from last regular season's statistics (PER weighted by minutes played), then
the top three are pinned: Nikola Jokic 98, Luka Doncic 97, Victor Wembanyama 96; nobody else
goes above 94.

Usage: python3 build_nba_league_csv.py [output_folder] [--season 2026]
  --season is the ESPN season whose statistics are used (2026 = the 2025-26 regular season).

Pick the output folder in the game under New career > League > Custom league.
"""
import csv
import json
import os
import re
import sys
import time
import unicodedata
import urllib.request
from concurrent.futures import ThreadPoolExecutor

ROSTER_URL = "https://site.api.espn.com/apis/site/v2/sports/basketball/nba/teams/{id}/roster"
TEAMS_URL = "https://site.api.espn.com/apis/site/v2/sports/basketball/nba/teams"
STATS_URL = ("https://sports.core.api.espn.com/v2/sports/basketball/leagues/nba/seasons/{season}"
             "/types/2/athletes/{id}/statistics/0")

EASTERN = {"Atlanta Hawks", "Boston Celtics", "Brooklyn Nets", "Charlotte Hornets", "Chicago Bulls",
           "Cleveland Cavaliers", "Detroit Pistons", "Indiana Pacers", "Miami Heat", "Milwaukee Bucks",
           "New York Knicks", "Orlando Magic", "Philadelphia 76ers", "Toronto Raptors", "Washington Wizards"}

PINNED = {"nikola jokic": 98, "luka doncic": 97, "victor wembanyama": 96}
TOP_CAP = 94
ROSTER_SIZE = 15


def fetch(url, retries=4):
    for attempt in range(retries):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"}), timeout=25) as r:
                return json.load(r)
        except Exception:
            time.sleep(0.6 * (attempt + 1))
    return None


def ascii_name(text):
    return unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii").strip()


def key(name):
    return re.sub(r"[^a-z ]", "", ascii_name(name).lower())


def stat_block(stats, name):
    if not stats:
        return {}
    for cat in stats.get("splits", {}).get("categories", []):
        if cat.get("name") == name:
            return {s["name"]: s["value"] for s in cat.get("stats", [])}
    return {}


def position(abbr, height_m, apg, rpg):
    if abbr == "C":
        return "C"
    if abbr == "G":
        return "PG" if apg >= 4.5 or height_m < 1.93 else "SG"
    if abbr == "F":
        return "PF" if (height_m >= 2.03 or rpg >= 7.0) else "SF"
    return {"PG": "PG", "SG": "SG", "SF": "SF", "PF": "PF"}.get(abbr, "SF")


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def build(out_dir, season):
    os.makedirs(out_dir, exist_ok=True)
    teams = fetch(TEAMS_URL)["sports"][0]["leagues"][0]["teams"]
    team_list = [(t["team"]["id"], t["team"]["displayName"]) for t in teams]

    def load_roster(item):
        tid, name = item
        data = fetch(ROSTER_URL.format(id=tid))
        return name, (data or {}).get("athletes", [])

    with ThreadPoolExecutor(max_workers=6) as pool:
        rosters = dict(pool.map(load_roster, team_list))

    athletes = [(team, a) for team, lst in rosters.items() for a in lst]
    print(f"{len(rosters)} teams, {len(athletes)} players. Fetching statistics...")

    def load_stats(item):
        return fetch(STATS_URL.format(season=season, id=item[1]["id"]), retries=2)

    with ThreadPoolExecutor(max_workers=10) as pool:
        all_stats = list(pool.map(load_stats, athletes))

    players = []
    for (team, a), stats in zip(athletes, all_stats):
        general = stat_block(stats, "general")
        offense = stat_block(stats, "offensive")
        defense = stat_block(stats, "defensive")
        minutes = general.get("minutes", 0.0)
        games = general.get("gamesPlayed", 0.0)
        per = general.get("PER", 0.0)
        height_m = round(a.get("height", 78) * 0.0254, 2)
        weight_kg = round(a.get("weight", 200) * 0.45359, 0)
        apg = offense.get("avgAssists", 0.0) or (offense.get("assists", 0) / games if games else 0.0)
        rpg = general.get("avgRebounds", 0.0)
        spg = defense.get("avgSteals", 0.0)
        bpg = defense.get("avgBlocks", 0.0)
        ppg = offense.get("avgPoints", 0.0)
        three = offense.get("threePointPct", 0.0) if offense.get("avgThreePointFieldGoalsAttempted", 0) >= 1.5 else 33.0
        ft = offense.get("freeThrowPct", 0.0) or 72.0
        age = a.get("age", 25)
        dob = (a.get("dateOfBirth") or "2000-01-01")[:10]

        # Ability from PER, trusted more the more minutes were played; rookies and bit-part players start low
        reliability = clamp(minutes / 1100.0, 0.0, 1.0)
        box = ppg * 0.55 + apg * 0.7 + rpg * 0.35 + (spg + bpg) * 1.2
        value = 0.55 * per + 0.45 * box
        if minutes >= 150:
            per_adj = 11.0 + (value - 11.0) * reliability
        else:
            per_adj = 9.0
        ability = 66.0 + (per_adj - 8.0) * 1.6
        if minutes < 150:
            ability = 66.0 + (6 if age <= 22 else 0)
        players.append({
            "team": team, "first": ascii_name(a.get("firstName", "")), "last": ascii_name(a.get("lastName", "")),
            "pos_abbr": (a.get("position") or {}).get("abbreviation", "F"), "jersey": a.get("jersey") or "0",
            "dob": dob, "height": height_m, "weight": int(weight_kg), "age": age,
            "ability": ability, "ppg": ppg, "apg": apg, "rpg": rpg, "spg": spg, "bpg": bpg, "three": three, "ft": ft,
            "ato": general.get("assistTurnoverRatio", 1.5), "minutes": minutes,
        })

    # Pin the top three, squeeze everybody else under them
    for p in players:
        k = key(p["first"] + " " + p["last"])
        if k in PINNED:
            p["ability"] = PINNED[k]
        else:
            a = p["ability"]
            if a > 90:
                a = 90 + (a - 90) * 0.6
            p["ability"] = clamp(a, 60, TOP_CAP)
    for p in players:
        p["ability"] = int(round(p["ability"]))

    # 15 per team: best players first, but keep a spread of positions through ability alone
    by_team = {}
    for p in players:
        by_team.setdefault(p["team"], []).append(p)
    rows = []
    team_rows = []
    for team, lst in sorted(by_team.items()):
        lst.sort(key=lambda p: (-p["ability"], -p["minutes"]))
        keep = lst[:ROSTER_SIZE]
        top8 = sum(p["ability"] for p in keep[:8]) / 8.0
        team_rows.append((team, top8))
        for p in keep:
            height = p["height"]
            pos = position(p["pos_abbr"], height, p["apg"], p["rpg"])
            base = p["ability"] / 5.0
            shooting = base + (p["three"] - 35.0) / 4.0 + (p["ft"] - 75.0) / 8.0 + (p["ppg"] - 12.0) / 12.0
            ball = base + (p["apg"] - 3.0) / 1.2 + (0.8 if pos == "PG" else 0.0)
            defence = base + (p["spg"] * 1.6 + p["bpg"] * 1.3 - 2.0) / 1.2 + (0.8 if pos in ("C", "PF") else 0.0)
            physical = base + (p["rpg"] - 5.0) / 1.6 + (p["weight"] - 100) / 14.0
            technical = base + (p["apg"] - 3.0) / 2.5 + (p["ato"] - 1.8) * 0.5
            pace = base + (1.2 if pos in ("PG", "SG") else (-1.2 if pos == "C" else 0.0)) - max(0, p["age"] - 31) * 0.4
            potential = p["ability"]
            if p["age"] <= 21:
                potential += 11
            elif p["age"] <= 23:
                potential += 7
            elif p["age"] <= 26:
                potential += 3
            potential = min(potential, 97 if p["ability"] < 96 else p["ability"])
            rows.append({
                "team": team, "first_name": p["first"], "last_name": p["last"], "position": pos,
                "jersey_number": p["jersey"], "birth_date": p["dob"], "height": f"{height:.2f}", "weight": p["weight"],
                "pace": int(clamp(round(pace), 3, 20)), "shooting": int(clamp(round(shooting), 3, 20)),
                "ball_control": int(clamp(round(ball), 3, 20)), "defense": int(clamp(round(defence), 3, 20)),
                "physical": int(clamp(round(physical), 3, 20)), "technical": int(clamp(round(technical), 3, 20)),
                "current_ability": p["ability"], "potential_ability": int(potential),
            })

    team_rows.sort(key=lambda t: -t[1])
    prestige = {name: int(round(20 - 12 * i / max(1, len(team_rows) - 1))) for i, (name, _) in enumerate(team_rows)}
    with open(os.path.join(out_dir, "teams.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["name", "conference", "prestige", "pace", "shooting", "defense", "rebounding", "ball_handling"])
        for name in sorted(by_team):
            members = [r for r in rows if r["team"] == name]
            avg = lambda col: int(round(sum(r[col] for r in members) / len(members)))
            w.writerow([name, "Eastern" if name in EASTERN else "Western", prestige[name], avg("pace"), avg("shooting"),
                        avg("defense"), avg("physical"), avg("ball_control")])
    try:
        import nba_kit_colors
        nba_kit_colors.patch_csv(os.path.join(out_dir, "teams.csv"))
    except ImportError:
        pass
    fields = ["team", "first_name", "last_name", "position", "jersey_number", "birth_date", "height", "weight", "pace",
              "shooting", "ball_control", "defense", "physical", "technical", "current_ability", "potential_ability"]
    with open(os.path.join(out_dir, "players.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for r in sorted(rows, key=lambda r: (r["team"], -r["current_ability"])):
            w.writerow(r)

    best = sorted(rows, key=lambda r: -r["current_ability"])[:10]
    print("Top 10:", ", ".join(f"{r['first_name']} {r['last_name']} {r['current_ability']}" for r in best))
    print(f"Wrote {len(rows)} players for {len(by_team)} teams to {out_dir}")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    season = 2026
    if "--season" in sys.argv:
        season = int(sys.argv[sys.argv.index("--season") + 1])
        args = [a for a in args if a != str(season)]
    build(args[0] if args else "nba-2026-27", season)
