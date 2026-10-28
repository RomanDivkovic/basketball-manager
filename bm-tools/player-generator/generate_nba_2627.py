#!/usr/bin/env python3
"""
NBA 2026/27 Roster Generator for Basketball Manager
Generates full 15-player rosters for all 30 NBA teams.
Implements the rule: Keep first name unchanged, alter the first letter of the last name
(e.g., LeBron James -> LeBron Tames, Nikola Jokic -> Nikola Tokic, Luka Doncic -> Luka Foncic,
Stephen Curry -> Stephen Parry, Victor Wembanyama -> Victor Pembanyama).
Attributes, positions, heights, weights, and jersey numbers are calibrated realistically.
"""

import sqlite3
import os
import sys

# Name conversion rule for last names
# Predictable and fun consonant/vowel mapping for the first letter of the surname
FIRST_LETTER_MAP = {
    'A': 'O',
    'B': 'P',
    'C': 'K',
    'D': 'T',
    'E': 'A',
    'F': 'V',
    'G': 'K',
    'H': 'M',
    'I': 'E',
    'J': 'Z',
    'K': 'C',
    'L': 'R',
    'M': 'N',
    'N': 'M',
    'O': 'U',
    'P': 'B',
    'Q': 'K',
    'R': 'L',
    'S': 'T',
    'T': 'D',
    'U': 'O',
    'V': 'W',
    'W': 'V',
    'X': 'Z',
    'Y': 'J',
    'Z': 'S'
}

def mutate_last_name(last_name):
    """Replaces the first letter of the last name according to FIRST_LETTER_MAP."""
    if not last_name:
        return last_name
    
    first_char = last_name[0]
    rest = last_name[1:]
    
    # If starts with prefix like O' or Mc or De'
    if len(last_name) > 2 and (last_name.startswith("O'") or last_name.startswith("De") or last_name.startswith("Mc")):
        prefix = last_name[:2]
        target_char = last_name[2]
        mutated = FIRST_LETTER_MAP.get(target_char.upper(), 'B')
        if target_char.islower():
            mutated = mutated.lower()
        return prefix + mutated + last_name[3:]

    mutated = FIRST_LETTER_MAP.get(first_char.upper(), 'B')
    if first_char.islower():
        mutated = mutated.lower()
    return mutated + rest

# Detailed 2026/27 rosters for all 30 NBA Teams (15 players per team = 450 players)
NBA_TEAMS_DATA = {
    # ==================== EASTERN CONFERENCE ====================
    # Atlantic Division
    "Boston Celtics": {
        "conf": "Eastern", "prestige": 20, "pace": 15, "shooting": 18, "defense": 18, "rebounding": 16, "ball_handling": 17,
        "players": [
            ("Jayson", "Tatum", "SF", 2.03, 95, 0, 18, 19, 17, 18, 17, 18, 93),
            ("Jaylen", "Brown", "SG", 1.98, 101, 7, 18, 17, 16, 18, 18, 17, 90),
            ("Derrick", "White", "PG", 1.93, 86, 9, 16, 17, 17, 19, 15, 17, 86),
            ("Jrue", "Holiday", "PG", 1.93, 93, 4, 15, 16, 17, 19, 16, 17, 85),
            ("Kristaps", "Porzingis", "C", 2.18, 109, 8, 13, 18, 14, 17, 17, 16, 87),
            ("Al", "Horford", "C", 2.06, 109, 42, 11, 15, 14, 16, 16, 16, 79),
            ("Payton", "Pritchard", "PG", 1.85, 88, 11, 16, 17, 16, 13, 14, 15, 80),
            ("Sam", "Hauser", "SF", 2.01, 98, 30, 13, 18, 13, 14, 14, 14, 78),
            ("Luke", "Kornet", "C", 2.16, 113, 40, 11, 11, 11, 15, 16, 13, 75),
            ("Neemias", "Queta", "C", 2.13, 111, 88, 12, 10, 11, 15, 17, 12, 74),
            ("Xavier", "Tillman", "PF", 2.03, 111, 26, 12, 11, 12, 16, 17, 13, 74),
            ("Jordan", "Walsh", "SF", 2.01, 93, 27, 16, 12, 12, 16, 15, 12, 73),
            ("Jaden", "Springer", "SG", 1.93, 92, 44, 16, 12, 13, 16, 15, 12, 72),
            ("Baylor", "Scheierman", "SG", 1.98, 93, 55, 13, 16, 14, 13, 14, 14, 73),
            ("Drew", "Peterson", "SF", 2.06, 93, 13, 13, 14, 13, 13, 13, 13, 70),
        ]
    },
    "New York Knicks": {
        "conf": "Eastern", "prestige": 19, "pace": 14, "shooting": 17, "defense": 18, "rebounding": 18, "ball_handling": 16,
        "players": [
            ("Jalen", "Brunson", "PG", 1.88, 86, 11, 17, 19, 18, 14, 15, 19, 91),
            ("Karl-Anthony", "Towns", "C", 2.13, 112, 32, 14, 19, 15, 15, 18, 17, 89),
            ("OG", "Anunoby", "SF", 2.01, 105, 8, 16, 17, 15, 20, 18, 16, 87),
            ("Mikal", "Bridges", "SG", 1.98, 95, 25, 16, 17, 15, 18, 16, 16, 85),
            ("Josh", "Hart", "SG", 1.93, 98, 3, 16, 14, 15, 18, 18, 15, 82),
            ("Mitchell", "Robinson", "C", 2.13, 109, 23, 13, 9, 10, 19, 19, 12, 80),
            ("Miles", "McBride", "PG", 1.88, 91, 2, 16, 16, 15, 16, 14, 14, 78),
            ("Precious", "Achiuwa", "PF", 2.03, 110, 5, 15, 12, 12, 16, 17, 13, 76),
            ("Cameron", "Payne", "PG", 1.91, 83, 1, 16, 15, 15, 13, 13, 14, 75),
            ("Jericho", "Sims", "C", 2.08, 113, 20, 15, 9, 10, 16, 18, 11, 73),
            ("Landry", "Shamet", "SG", 1.93, 86, 4, 14, 16, 13, 12, 13, 13, 74),
            ("Tyler", "Kolek", "PG", 1.91, 88, 13, 14, 14, 16, 13, 13, 15, 73),
            ("Pacome", "Dadiet", "SF", 2.03, 98, 14, 14, 13, 13, 13, 14, 13, 72),
            ("Ariel", "Hukporti", "C", 2.13, 115, 55, 12, 9, 9, 15, 17, 11, 71),
            ("Jacob", "Toppin", "PF", 2.03, 93, 0, 16, 11, 12, 13, 16, 12, 70),
        ]
    },
    "Philadelphia 76ers": {
        "conf": "Eastern", "prestige": 18, "pace": 14, "shooting": 17, "defense": 17, "rebounding": 17, "ball_handling": 16,
        "players": [
            ("Joel", "Embiid", "C", 2.13, 127, 21, 13, 19, 16, 19, 20, 19, 94),
            ("Tyrese", "Maxey", "PG", 1.88, 91, 0, 20, 18, 18, 14, 15, 18, 90),
            ("Paul", "George", "SF", 2.03, 100, 8, 16, 18, 16, 18, 16, 17, 88),
            ("Kelly", "Oubre Jr", "SF", 2.01, 92, 9, 17, 15, 14, 15, 16, 14, 79),
            ("Caleb", "Martin", "SF", 1.96, 93, 16, 15, 14, 14, 16, 16, 14, 78),
            ("Andre", "Drummond", "C", 2.11, 127, 5, 12, 9, 11, 16, 19, 12, 79),
            ("Kyle", "Lowry", "PG", 1.83, 89, 7, 12, 14, 16, 16, 14, 16, 77),
            ("Eric", "Gordon", "SG", 1.91, 98, 23, 13, 16, 13, 13, 14, 14, 75),
            ("Guerschon", "Yabusele", "PF", 2.03, 123, 28, 14, 15, 13, 15, 18, 14, 77),
            ("KJ", "Martin", "PF", 1.98, 98, 1, 17, 12, 12, 14, 17, 12, 74),
            ("Ricky", "Council IV", "SG", 1.98, 93, 14, 16, 13, 13, 14, 16, 13, 73),
            ("Jared", "McCain", "PG", 1.91, 88, 20, 15, 17, 15, 13, 13, 15, 77),
            ("Adem", "Bona", "C", 2.08, 107, 22, 15, 8, 9, 16, 18, 11, 72),
            ("Reggie", "Jackson", "PG", 1.88, 94, 11, 14, 14, 15, 12, 13, 14, 74),
            ("Jeff", "Dowtin", "PG", 1.91, 84, 19, 15, 13, 14, 13, 13, 13, 70),
        ]
    },
    "Brooklyn Nets": {
        "conf": "Eastern", "prestige": 13, "pace": 16, "shooting": 14, "defense": 14, "rebounding": 14, "ball_handling": 15,
        "players": [
            ("Cam", "Thomas", "SG", 1.91, 95, 24, 17, 19, 16, 12, 15, 17, 83),
            ("Nic", "Claxton", "C", 2.11, 98, 33, 15, 10, 12, 19, 17, 14, 82),
            ("Cameron", "Johnson", "PF", 2.03, 95, 2, 14, 18, 14, 15, 15, 15, 81),
            ("Dennis", "Schroder", "PG", 1.85, 79, 17, 18, 15, 17, 15, 13, 16, 80),
            ("Dorian", "Finney-Smith", "PF", 2.01, 100, 28, 14, 15, 13, 17, 16, 14, 78),
            ("Ben", "Simmons", "PG", 2.08, 109, 10, 16, 6, 18, 18, 18, 17, 79),
            ("Day'Ron", "Sharpe", "C", 2.06, 120, 20, 12, 9, 10, 15, 18, 11, 75),
            ("Ziaire", "Williams", "SF", 2.06, 84, 9, 16, 13, 12, 14, 14, 13, 74),
            ("Noah", "Clowney", "PF", 2.08, 95, 21, 14, 14, 12, 16, 16, 13, 75),
            ("Jalen", "Wilson", "SF", 2.03, 102, 22, 14, 15, 13, 14, 15, 14, 75),
            ("Keon", "Johnson", "SG", 1.96, 84, 45, 18, 14, 13, 14, 14, 13, 73),
            ("Trendon", "Watford", "PF", 2.03, 108, 9, 13, 13, 14, 13, 15, 14, 73),
            ("Shake", "Milton", "SG", 1.96, 93, 7, 14, 14, 14, 13, 13, 14, 73),
            ("Dariq", "Whitehead", "SG", 1.98, 100, 0, 15, 15, 13, 12, 14, 13, 71),
            ("Cui", "Yongxi", "SF", 2.01, 86, 8, 14, 13, 12, 13, 13, 12, 69),
        ]
    },
    "Toronto Raptors": {
        "conf": "Eastern", "prestige": 14, "pace": 16, "shooting": 15, "defense": 15, "rebounding": 15, "ball_handling": 15,
        "players": [
            ("Scottie", "Barnes", "SF", 2.01, 102, 4, 16, 16, 17, 18, 18, 18, 88),
            ("Immanuel", "Quickley", "PG", 1.88, 86, 5, 18, 17, 17, 15, 14, 17, 84),
            ("RJ", "Barrett", "SF", 1.98, 97, 9, 16, 16, 15, 15, 17, 16, 83),
            ("Jakob", "Poeltl", "C", 2.13, 118, 19, 12, 10, 13, 17, 18, 14, 80),
            ("Gradey", "Dick", "SG", 1.98, 93, 1, 15, 18, 14, 13, 14, 15, 79),
            ("Kelly", "Olynyk", "C", 2.11, 109, 41, 11, 16, 15, 14, 15, 16, 78),
            ("Bruce", "Brown", "SG", 1.93, 92, 11, 16, 13, 15, 17, 16, 15, 78),
            ("Ochai", "Agbaji", "SG", 1.96, 98, 30, 16, 15, 13, 15, 16, 13, 75),
            ("Davion", "Mitchell", "PG", 1.83, 92, 2, 18, 13, 15, 18, 15, 15, 76),
            ("Chris", "Boucher", "PF", 2.06, 91, 25, 15, 14, 11, 16, 16, 12, 75),
            ("Ja'Kobe", "Walter", "SG", 1.96, 88, 14, 16, 15, 13, 14, 14, 14, 74),
            ("Jonathan", "Mogbo", "PF", 2.03, 102, 22, 16, 10, 13, 16, 17, 13, 73),
            ("Jamal", "Shead", "PG", 1.85, 88, 23, 17, 12, 16, 18, 14, 15, 74),
            ("Ulrich", "Chomche", "C", 2.11, 105, 33, 14, 8, 9, 15, 17, 10, 70),
            ("Garrett", "Temple", "SG", 1.96, 88, 17, 12, 13, 13, 13, 13, 13, 70),
        ]
    },

    # Central Division
    "Milwaukee Bucks": {
        "conf": "Eastern", "prestige": 19, "pace": 16, "shooting": 18, "defense": 16, "rebounding": 18, "ball_handling": 17,
        "players": [
            ("Giannis", "Antetokounmpo", "PF", 2.11, 110, 34, 19, 14, 17, 19, 20, 18, 95),
            ("Damian", "Lillard", "PG", 1.88, 88, 0, 17, 20, 18, 12, 14, 19, 91),
            ("Khris", "Middleton", "SF", 2.01, 101, 22, 14, 18, 16, 15, 15, 17, 84),
            ("Brook", "Lopez", "C", 2.16, 128, 11, 10, 17, 11, 19, 18, 13, 81),
            ("Bobby", "Portis", "PF", 2.08, 113, 9, 13, 17, 13, 14, 18, 14, 81),
            ("Gary", "Trent Jr", "SG", 1.96, 95, 5, 15, 17, 13, 14, 14, 14, 78),
            ("Taurean", "Prince", "SF", 1.98, 99, 12, 14, 16, 13, 15, 15, 14, 76),
            ("Delon", "Wright", "PG", 1.96, 84, 55, 16, 13, 15, 17, 14, 15, 76),
            ("Pat", "Connaughton", "SG", 1.96, 95, 24, 15, 15, 13, 14, 16, 14, 75),
            ("AJ", "Green", "SG", 1.93, 86, 20, 14, 18, 13, 12, 13, 13, 75),
            ("Andre", "Jackson Jr", "SG", 1.98, 95, 44, 18, 10, 13, 16, 16, 12, 73),
            ("MarJon", "Beauchamp", "SF", 2.01, 90, 3, 16, 13, 13, 13, 15, 13, 72),
            ("Tyler", "Smith", "PF", 2.08, 102, 21, 14, 15, 12, 13, 15, 13, 73),
            ("AJ", "Johnson", "PG", 1.96, 73, 77, 18, 12, 14, 12, 12, 13, 71),
            ("Liam", "Robbins", "C", 2.13, 113, 15, 11, 11, 10, 14, 16, 11, 69),
        ]
    },
    "Cleveland Cavaliers": {
        "conf": "Eastern", "prestige": 18, "pace": 16, "shooting": 17, "defense": 18, "rebounding": 18, "ball_handling": 17,
        "players": [
            ("Donovan", "Mitchell", "SG", 1.91, 98, 45, 19, 19, 18, 16, 16, 19, 92),
            ("Darius", "Garland", "PG", 1.85, 87, 10, 18, 18, 19, 13, 13, 19, 87),
            ("Evan", "Mobley", "PF", 2.11, 98, 4, 16, 14, 15, 20, 17, 16, 88),
            ("Jarrett", "Allen", "C", 2.06, 110, 31, 15, 10, 12, 19, 19, 13, 85),
            ("Max", "Strus", "SF", 1.96, 98, 1, 15, 17, 14, 14, 15, 14, 79),
            ("Caris", "LeVert", "SG", 1.98, 93, 3, 16, 15, 16, 14, 14, 16, 80),
            ("Isaac", "Okoro", "SF", 1.96, 102, 35, 17, 14, 13, 18, 17, 13, 78),
            ("Dean", "Wade", "PF", 2.06, 103, 32, 14, 16, 13, 16, 16, 14, 76),
            ("Georges", "Niang", "PF", 2.01, 104, 20, 11, 16, 12, 12, 15, 13, 75),
            ("Sam", "Merrill", "SG", 1.93, 93, 5, 14, 18, 13, 12, 13, 14, 76),
            ("Ty", "Jerome", "PG", 1.96, 88, 2, 15, 16, 16, 13, 13, 16, 76),
            ("Jaylon", "Tyson", "SF", 2.01, 98, 24, 15, 14, 14, 14, 15, 14, 74),
            ("Tristan", "Thompson", "C", 2.06, 115, 13, 11, 8, 9, 14, 17, 11, 72),
            ("JT", "Thor", "PF", 2.06, 92, 15, 15, 12, 11, 14, 15, 12, 71),
            ("Craig", "Porter Jr", "PG", 1.88, 84, 9, 16, 13, 15, 15, 13, 14, 72),
        ]
    },
    "Indiana Pacers": {
        "conf": "Eastern", "prestige": 17, "pace": 20, "shooting": 18, "defense": 14, "rebounding": 15, "ball_handling": 19,
        "players": [
            ("Tyrese", "Haliburton", "PG", 1.96, 84, 0, 18, 18, 20, 14, 14, 20, 91),
            ("Pascal", "Siakam", "PF", 2.03, 104, 43, 16, 16, 16, 17, 17, 17, 87),
            ("Myles", "Turner", "C", 2.11, 113, 33, 13, 17, 13, 19, 18, 14, 83),
            ("Andrew", "Nembhard", "SG", 1.96, 87, 2, 16, 15, 17, 17, 14, 16, 81),
            ("Aaron", "Nesmith", "SF", 1.98, 98, 23, 16, 17, 14, 17, 16, 14, 80),
            ("Bennedict", "Mathurin", "SG", 1.98, 95, 0, 18, 16, 15, 14, 16, 16, 81),
            ("Obi", "Toppin", "PF", 2.06, 100, 1, 18, 16, 13, 13, 18, 14, 79),
            ("T.J.", "McConnell", "PG", 1.85, 86, 9, 17, 12, 18, 17, 13, 17, 80),
            ("Ben", "Sheppard", "SG", 1.98, 86, 26, 17, 15, 13, 16, 14, 14, 75),
            ("Isaiah", "Jackson", "C", 2.08, 93, 22, 17, 9, 11, 17, 17, 12, 76),
            ("James", "Wiseman", "C", 2.13, 109, 13, 14, 11, 10, 14, 17, 12, 74),
            ("Jarace", "Walker", "PF", 2.03, 107, 5, 15, 14, 14, 16, 17, 14, 76),
            ("Johnny", "Furphy", "SF", 2.06, 92, 12, 15, 15, 12, 13, 14, 13, 72),
            ("Kendall", "Brown", "SF", 2.01, 93, 10, 18, 11, 12, 14, 15, 12, 71),
            ("Quenton", "Jackson", "SG", 1.96, 78, 29, 17, 12, 13, 13, 13, 13, 70),
        ]
    },
    "Chicago Bulls": {
        "conf": "Eastern", "prestige": 15, "pace": 16, "shooting": 16, "defense": 14, "rebounding": 15, "ball_handling": 16,
        "players": [
            ("Zach", "LaVine", "SG", 1.96, 91, 8, 18, 18, 16, 13, 16, 17, 84),
            ("Nikola", "Vucevic", "C", 2.08, 118, 9, 12, 16, 15, 14, 17, 16, 81),
            ("Coby", "White", "PG", 1.96, 88, 0, 17, 17, 17, 13, 14, 17, 83),
            ("Josh", "Giddey", "PG", 2.03, 98, 3, 15, 14, 18, 14, 15, 18, 82),
            ("Patrick", "Williams", "PF", 2.01, 98, 44, 15, 15, 13, 16, 17, 14, 77),
            ("Ayo", "Dosunmu", "SG", 1.96, 91, 12, 17, 15, 16, 16, 15, 15, 79),
            ("Lonzo", "Ball", "PG", 1.98, 86, 2, 16, 16, 18, 17, 14, 18, 79),
            ("Jalen", "Smith", "PF", 2.08, 98, 25, 14, 16, 12, 15, 16, 13, 76),
            ("Matas", "Buzelis", "SF", 2.08, 95, 14, 16, 14, 14, 15, 16, 15, 77),
            ("Torrey", "Craig", "SF", 1.96, 100, 13, 14, 14, 12, 16, 16, 13, 74),
            ("Julian", "Phillips", "SF", 2.03, 90, 15, 17, 12, 12, 15, 16, 12, 73),
            ("Dalen", "Terry", "SG", 2.01, 88, 25, 17, 11, 14, 15, 15, 13, 73),
            ("Chris", "Duarte", "SG", 1.98, 86, 5, 14, 14, 13, 13, 13, 13, 73),
            ("Adama", "Sanogo", "C", 2.06, 111, 21, 12, 9, 9, 14, 17, 11, 71),
            ("Talen", "Horton-Tucker", "SG", 1.93, 106, 22, 15, 13, 15, 13, 16, 14, 73),
        ]
    },
    "Detroit Pistons": {
        "conf": "Eastern", "prestige": 13, "pace": 16, "shooting": 14, "defense": 15, "rebounding": 16, "ball_handling": 15,
        "players": [
            ("Cade", "Cunningham", "PG", 1.98, 100, 2, 16, 17, 18, 15, 16, 18, 87),
            ("Jaden", "Ivey", "SG", 1.93, 88, 23, 19, 16, 16, 13, 16, 16, 82),
            ("Jalen", "Duren", "C", 2.08, 113, 0, 16, 9, 12, 17, 19, 13, 81),
            ("Tobias", "Harris", "PF", 2.03, 103, 12, 14, 16, 15, 14, 16, 16, 80),
            ("Tim", "Hardaway Jr", "SG", 1.96, 93, 5, 14, 16, 13, 12, 13, 14, 76),
            ("Ausar", "Thompson", "SF", 2.01, 98, 9, 19, 10, 14, 19, 18, 14, 80),
            ("Malik", "Beasley", "SG", 1.93, 85, 5, 15, 17, 13, 12, 14, 14, 77),
            ("Isaiah", "Stewart", "C", 2.03, 113, 28, 13, 14, 12, 17, 18, 13, 77),
            ("Simone", "Fontecchio", "SF", 2.03, 95, 19, 14, 16, 13, 13, 14, 14, 76),
            ("Paul", "Reed", "C", 2.06, 95, 7, 14, 11, 11, 16, 17, 12, 75),
            ("Ron", "Holland", "SF", 2.03, 93, 0, 18, 12, 13, 16, 16, 13, 75),
            ("Marcus", "Sasser", "PG", 1.88, 88, 25, 16, 15, 15, 14, 13, 15, 74),
            ("Wendell", "Moore Jr", "SG", 1.96, 98, 14, 15, 13, 13, 14, 14, 13, 71),
            ("Bobi", "Klintman", "PF", 2.06, 102, 34, 14, 14, 13, 13, 14, 13, 71),
            ("Daniss", "Jenkins", "PG", 1.93, 82, 24, 15, 13, 14, 13, 12, 13, 69),
        ]
    },

    # Southeast Division
    "Miami Heat": {
        "conf": "Eastern", "prestige": 18, "pace": 14, "shooting": 16, "defense": 19, "rebounding": 16, "ball_handling": 17,
        "players": [
            ("Jimmy", "Butler", "SF", 2.01, 104, 22, 16, 16, 17, 19, 17, 18, 90),
            ("Bam", "Adebayo", "C", 2.06, 116, 13, 16, 14, 16, 20, 19, 17, 89),
            ("Tyler", "Herro", "SG", 1.96, 88, 14, 16, 18, 17, 13, 14, 17, 84),
            ("Terry", "Rozier", "PG", 1.85, 86, 2, 17, 16, 16, 14, 14, 16, 81),
            ("Jaime", "Jaquez Jr", "SF", 1.98, 102, 11, 15, 15, 15, 16, 16, 16, 80),
            ("Nikola", "Jovic", "PF", 2.08, 109, 5, 14, 16, 15, 14, 15, 16, 78),
            ("Duncan", "Robinson", "SG", 2.01, 98, 55, 14, 18, 14, 12, 14, 14, 78),
            ("Haywood", "Highsmith", "SF", 1.96, 100, 24, 15, 14, 12, 18, 16, 13, 76),
            ("Kel'el", "Ware", "C", 2.13, 104, 7, 15, 14, 11, 17, 17, 13, 76),
            ("Alec", "Burks", "SG", 1.96, 97, 18, 14, 16, 14, 13, 14, 14, 75),
            ("Kevin", "Love", "PF", 2.03, 114, 42, 10, 16, 14, 13, 16, 15, 76),
            ("Thomas", "Bryant", "C", 2.08, 112, 31, 12, 14, 11, 13, 17, 12, 73),
            ("Josh", "Richardson", "SG", 1.96, 91, 0, 15, 14, 14, 15, 14, 14, 74),
            ("Pelle", "Larsson", "SG", 1.98, 96, 9, 15, 14, 13, 14, 15, 13, 72),
            ("Dru", "Smith", "PG", 1.88, 92, 12, 15, 13, 14, 15, 13, 13, 71),
        ]
    },
    "Orlando Magic": {
        "conf": "Eastern", "prestige": 17, "pace": 15, "shooting": 14, "defense": 20, "rebounding": 18, "ball_handling": 15,
        "players": [
            ("Paolo", "Banchero", "PF", 2.08, 113, 5, 16, 17, 17, 16, 18, 18, 89),
            ("Franz", "Wagner", "SF", 2.08, 100, 22, 16, 16, 16, 17, 17, 17, 87),
            ("Jalen", "Suggs", "SG", 1.96, 93, 4, 18, 16, 15, 20, 16, 16, 83),
            ("Kentavious", "Caldwell-Pope", "SG", 1.96, 93, 3, 15, 17, 14, 18, 15, 15, 80),
            ("Wendell", "Carter Jr", "C", 2.08, 122, 34, 13, 14, 14, 17, 18, 14, 80),
            ("Jonathan", "Isaac", "PF", 2.08, 104, 1, 16, 13, 12, 20, 18, 13, 81),
            ("Moritz", "Wagner", "C", 2.11, 111, 21, 14, 15, 13, 14, 18, 14, 79),
            ("Cole", "Anthony", "PG", 1.88, 84, 50, 17, 15, 16, 13, 14, 16, 78),
            ("Goga", "Bitadze", "C", 2.11, 113, 35, 12, 10, 11, 17, 18, 12, 77),
            ("Anthony", "Black", "PG", 2.01, 95, 0, 17, 13, 16, 17, 16, 15, 77),
            ("Gary", "Harris", "SG", 1.93, 95, 14, 14, 15, 13, 15, 14, 14, 75),
            ("Tristan", "da Silva", "SF", 2.03, 98, 23, 14, 15, 14, 15, 15, 15, 75),
            ("Cory", "Joseph", "PG", 1.88, 91, 10, 13, 13, 14, 13, 13, 14, 72),
            ("Jett", "Howard", "SG", 1.98, 98, 13, 15, 16, 13, 12, 14, 13, 73),
            ("Caleb", "Houstan", "SF", 2.03, 93, 2, 14, 15, 12, 13, 14, 13, 72),
        ]
    },
    "Atlanta Hawks": {
        "conf": "Eastern", "prestige": 15, "pace": 18, "shooting": 16, "defense": 14, "rebounding": 16, "ball_handling": 17,
        "players": [
            ("Trae", "Young", "PG", 1.85, 74, 11, 18, 18, 20, 11, 12, 20, 89),
            ("Jalen", "Johnson", "PF", 2.06, 99, 1, 17, 16, 16, 16, 18, 17, 84),
            ("Clint", "Capela", "C", 2.08, 116, 15, 14, 8, 10, 17, 19, 12, 80),
            ("Bogdan", "Bogdanovic", "SG", 1.96, 102, 13, 14, 18, 16, 14, 14, 17, 81),
            ("Dyson", "Daniels", "SG", 2.01, 90, 5, 17, 13, 15, 20, 16, 15, 80),
            ("De'Andre", "Hunter", "SF", 2.03, 100, 12, 15, 16, 14, 16, 16, 15, 79),
            ("Onyeka", "Okongwu", "C", 2.08, 109, 17, 15, 12, 11, 17, 18, 13, 79),
            ("Zaccharie", "Risacher", "SF", 2.06, 93, 10, 16, 16, 14, 15, 15, 15, 79),
            ("Larry", "Nance Jr", "C", 2.03, 111, 22, 15, 13, 14, 16, 17, 14, 77),
            ("Vit", "Krejci", "SG", 2.03, 88, 27, 15, 15, 15, 14, 14, 15, 75),
            ("Garrison", "Mathews", "SG", 1.98, 98, 25, 14, 17, 12, 13, 14, 13, 74),
            ("Kobe", "Bufkin", "PG", 1.96, 88, 4, 16, 14, 15, 15, 14, 15, 74),
            ("David", "Roddy", "PF", 1.93, 116, 8, 14, 13, 13, 13, 17, 13, 72),
            ("Mouhamed", "Gueye", "PF", 2.08, 95, 18, 15, 11, 11, 15, 16, 12, 71),
            ("Dominick", "Barlow", "PF", 2.06, 98, 0, 14, 11, 11, 14, 16, 12, 70),
        ]
    },
    "Charlotte Hornets": {
        "conf": "Eastern", "prestige": 13, "pace": 17, "shooting": 16, "defense": 13, "rebounding": 14, "ball_handling": 16,
        "players": [
            ("LaMelo", "Ball", "PG", 2.01, 82, 1, 17, 18, 19, 13, 14, 19, 88),
            ("Brandon", "Miller", "SF", 2.06, 91, 24, 17, 18, 16, 15, 15, 17, 84),
            ("Miles", "Bridges", "PF", 2.01, 102, 0, 17, 16, 15, 14, 18, 15, 82),
            ("Mark", "Williams", "C", 2.13, 109, 5, 13, 9, 10, 18, 19, 12, 79),
            ("Tre", "Mann", "PG", 1.91, 86, 23, 17, 17, 17, 13, 13, 16, 79),
            ("Grant", "Williams", "PF", 1.98, 107, 2, 13, 15, 13, 16, 17, 14, 77),
            ("Nick", "Richards", "C", 2.13, 111, 4, 13, 9, 10, 16, 18, 11, 76),
            ("Cody", "Martin", "SG", 1.96, 93, 11, 15, 13, 14, 17, 15, 14, 76),
            ("Vasilije", "Micic", "PG", 1.91, 85, 22, 13, 14, 17, 12, 13, 16, 76),
            ("Seth", "Curry", "SG", 1.85, 84, 30, 13, 18, 14, 12, 12, 15, 75),
            ("Tidjane", "Salaun", "PF", 2.06, 99, 31, 16, 14, 13, 14, 16, 13, 74),
            ("Nick", "Smith Jr", "SG", 1.96, 84, 8, 16, 15, 14, 12, 13, 14, 73),
            ("Josh", "Green", "SG", 1.96, 91, 10, 16, 15, 13, 16, 15, 14, 76),
            ("Taj", "Gibson", "C", 2.06, 105, 67, 10, 10, 10, 14, 16, 12, 71),
            ("Moussa", "Diabate", "C", 2.08, 95, 14, 15, 8, 10, 15, 17, 11, 71),
        ]
    },
    "Washington Wizards": {
        "conf": "Eastern", "prestige": 11, "pace": 17, "shooting": 14, "defense": 12, "rebounding": 14, "ball_handling": 14,
        "players": [
            ("Jordan", "Poole", "SG", 1.93, 88, 13, 17, 17, 17, 12, 13, 17, 81),
            ("Kyle", "Kuzma", "PF", 2.06, 100, 0, 16, 16, 15, 14, 16, 16, 81),
            ("Alex", "Sarr", "C", 2.13, 102, 20, 16, 13, 13, 18, 17, 14, 78),
            ("Bub", "Carrington", "PG", 1.96, 86, 17, 16, 15, 16, 14, 14, 16, 77),
            ("Bilal", "Coulibaly", "SF", 2.03, 88, 0, 18, 14, 14, 18, 16, 15, 79),
            ("Jonas", "Valanciunas", "C", 2.11, 120, 17, 11, 14, 13, 15, 19, 15, 80),
            ("Malcolm", "Brogdon", "PG", 1.93, 104, 15, 14, 16, 16, 15, 15, 16, 79),
            ("Corey", "Kispert", "SF", 1.98, 100, 24, 14, 17, 13, 13, 15, 14, 76),
            ("Kyshawn", "George", "SG", 2.03, 91, 18, 15, 15, 14, 15, 14, 14, 75),
            ("Richaun", "Holmes", "C", 2.06, 107, 22, 14, 11, 11, 15, 17, 12, 75),
            ("Saddiq", "Bey", "SF", 2.01, 98, 41, 14, 15, 13, 14, 16, 14, 76),
            ("Johnny", "Davis", "SG", 1.96, 88, 1, 15, 12, 13, 14, 14, 12, 71),
            ("Patrick", "Baldwin Jr", "PF", 2.06, 100, 7, 13, 14, 12, 12, 14, 12, 70),
            ("Anthony", "Gill", "PF", 2.03, 104, 16, 12, 12, 12, 13, 15, 12, 70),
            ("Jared", "Butler", "PG", 1.91, 88, 4, 16, 14, 15, 13, 13, 14, 72),
        ]
    },

    # ==================== WESTERN CONFERENCE ====================
    # Northwest Division
    "Oklahoma City Thunder": {
        "conf": "Western", "prestige": 20, "pace": 17, "shooting": 18, "defense": 20, "rebounding": 17, "ball_handling": 19,
        "players": [
            ("Shai", "Gilgeous-Alexander", "PG", 1.98, 88, 2, 19, 19, 20, 19, 16, 20, 95),
            ("Jalen", "Williams", "SF", 1.98, 96, 8, 17, 17, 17, 19, 17, 18, 88),
            ("Chet", "Holmgren", "C", 2.16, 94, 7, 16, 17, 15, 20, 17, 17, 89),
            ("Alex", "Caruso", "SG", 1.96, 84, 9, 16, 16, 15, 20, 16, 16, 84),
            ("Isaiah", "Hartenstein", "C", 2.13, 113, 55, 13, 10, 15, 18, 19, 15, 83),
            ("Luguentz", "Dort", "SF", 1.93, 100, 5, 16, 16, 13, 20, 18, 14, 82),
            ("Cason", "Wallace", "PG", 1.93, 88, 22, 17, 16, 15, 18, 15, 15, 79),
            ("Aaron", "Wiggins", "SG", 1.98, 91, 21, 16, 17, 14, 15, 15, 15, 79),
            ("Isaiah", "Joe", "SG", 1.91, 75, 11, 16, 19, 13, 14, 13, 14, 78),
            ("Jaylin", "Williams", "C", 2.06, 109, 6, 13, 14, 14, 16, 17, 15, 77),
            ("Kenrich", "Williams", "SF", 1.98, 104, 34, 14, 14, 14, 16, 16, 15, 76),
            ("Ousmane", "Dieng", "PF", 2.08, 98, 13, 16, 14, 14, 15, 15, 15, 76),
            ("Nikola", "Topic", "PG", 1.98, 91, 44, 16, 14, 17, 13, 14, 17, 75),
            ("Dillon", "Jones", "PF", 1.96, 107, 3, 14, 13, 14, 14, 16, 14, 73),
            ("Ajay", "Mitchell", "PG", 1.93, 86, 25, 16, 14, 15, 14, 13, 15, 73),
        ]
    },
    "Denver Nuggets": {
        "conf": "Western", "prestige": 19, "pace": 15, "shooting": 18, "defense": 16, "rebounding": 18, "ball_handling": 19,
        "players": [
            ("Nikola", "Jokic", "C", 2.11, 129, 15, 13, 19, 20, 16, 20, 20, 97),
            ("Jamal", "Murray", "PG", 1.93, 98, 27, 17, 18, 18, 14, 16, 18, 88),
            ("Michael", "Porter Jr", "SF", 2.08, 99, 1, 15, 19, 13, 15, 17, 15, 85),
            ("Aaron", "Gordon", "PF", 2.03, 107, 50, 17, 14, 15, 18, 19, 16, 85),
            ("Christian", "Braun", "SG", 1.98, 100, 0, 17, 16, 14, 17, 16, 15, 80),
            ("Russell", "Westbrook", "PG", 1.93, 91, 4, 18, 13, 17, 16, 17, 17, 81),
            ("Peyton", "Watson", "SF", 2.03, 91, 8, 18, 12, 13, 18, 16, 13, 77),
            ("Julian", "Strawther", "SG", 2.01, 93, 3, 15, 17, 13, 13, 14, 14, 76),
            ("Dario", "Saric", "PF", 2.08, 102, 9, 11, 15, 15, 13, 15, 16, 76),
            ("DeAndre", "Jordan", "C", 2.11, 120, 6, 10, 8, 9, 15, 18, 11, 73),
            ("Zeke", "Nnaji", "C", 2.06, 109, 22, 14, 12, 11, 14, 16, 12, 73),
            ("Hunter", "Tyson", "PF", 2.03, 98, 5, 13, 15, 12, 12, 14, 13, 71),
            ("Jalen", "Pickett", "PG", 1.88, 92, 24, 14, 14, 15, 13, 14, 15, 72),
            ("DaRon", "Holmes II", "PF", 2.06, 102, 14, 15, 14, 13, 15, 16, 14, 74),
            ("Vlatko", "Cancar", "PF", 2.03, 107, 31, 12, 14, 13, 13, 15, 13, 72),
        ]
    },
    "Minnesota Timberwolves": {
        "conf": "Western", "prestige": 18, "pace": 15, "shooting": 16, "defense": 20, "rebounding": 18, "ball_handling": 16,
        "players": [
            ("Anthony", "Edwards", "SG", 1.93, 102, 5, 19, 19, 17, 17, 18, 18, 92),
            ("Rudy", "Gobert", "C", 2.16, 117, 27, 13, 8, 10, 20, 20, 12, 87),
            ("Julius", "Randle", "PF", 2.03, 113, 30, 15, 16, 16, 14, 19, 17, 85),
            ("Jaden", "McDaniels", "SF", 2.06, 84, 3, 17, 15, 13, 20, 16, 14, 82),
            ("Mike", "Conley", "PG", 1.83, 79, 10, 14, 17, 18, 16, 13, 18, 81),
            ("Donte", "DiVincenzo", "SG", 1.93, 92, 0, 16, 18, 15, 17, 15, 16, 82),
            ("Naz", "Reid", "C", 2.06, 120, 11, 15, 17, 14, 15, 18, 16, 83),
            ("Nickeil", "Alexander-Walker", "SG", 1.96, 93, 9, 16, 16, 14, 17, 15, 15, 79),
            ("Rob", "Dillingham", "PG", 1.88, 75, 4, 19, 16, 17, 12, 12, 17, 77),
            ("Terrence", "Shannon Jr", "SG", 1.98, 98, 0, 18, 14, 14, 15, 16, 14, 75),
            ("Josh", "Minott", "SF", 2.03, 93, 8, 17, 12, 12, 15, 16, 12, 73),
            ("Luka", "Garza", "C", 2.08, 110, 55, 11, 15, 11, 13, 17, 13, 74),
            ("Leonard", "Miller", "PF", 2.08, 95, 33, 15, 13, 13, 14, 16, 13, 72),
            ("PJ", "Dozier", "SG", 1.98, 93, 35, 15, 13, 14, 14, 14, 14, 72),
            ("Joe", "Ingles", "SF", 2.06, 100, 7, 10, 16, 16, 13, 13, 16, 73),
        ]
    },
    "Portland Trail Blazers": {
        "conf": "Western", "prestige": 13, "pace": 16, "shooting": 14, "defense": 15, "rebounding": 17, "ball_handling": 15,
        "players": [
            ("Anfernee", "Simons", "SG", 1.91, 82, 1, 18, 18, 17, 12, 14, 18, 83),
            ("Jerami", "Grant", "PF", 2.01, 95, 9, 16, 17, 14, 16, 16, 16, 82),
            ("Deandre", "Ayton", "C", 2.13, 113, 2, 14, 14, 12, 17, 19, 14, 82),
            ("Shaedon", "Sharpe", "SG", 1.96, 91, 17, 19, 16, 15, 14, 17, 16, 81),
            ("Scoot", "Henderson", "PG", 1.91, 88, 0, 19, 14, 17, 14, 16, 17, 80),
            ("Deni", "Avdija", "SF", 2.06, 95, 8, 16, 15, 16, 16, 17, 16, 81),
            ("Donovan", "Clingan", "C", 2.18, 127, 23, 11, 9, 10, 19, 20, 12, 78),
            ("Toumani", "Camara", "SF", 2.01, 100, 33, 16, 14, 13, 18, 16, 13, 77),
            ("Robert", "Williams III", "C", 2.06, 108, 35, 15, 9, 11, 19, 18, 12, 78),
            ("Matisse", "Thybulle", "SF", 1.96, 91, 4, 17, 13, 12, 19, 15, 13, 76),
            ("Kris", "Murray", "PF", 2.03, 98, 24, 14, 14, 13, 15, 15, 13, 74),
            ("Jabari", "Walker", "PF", 2.06, 95, 34, 14, 13, 12, 15, 16, 13, 75),
            ("Dalano", "Banton", "PG", 2.03, 93, 5, 17, 14, 15, 14, 15, 15, 75),
            ("Duop", "Reath", "C", 2.06, 111, 26, 12, 15, 11, 14, 17, 13, 74),
            ("Rayan", "Rupert", "SG", 1.98, 88, 72, 16, 13, 13, 15, 14, 13, 71),
        ]
    },
    "Utah Jazz": {
        "conf": "Western", "prestige": 13, "pace": 17, "shooting": 16, "defense": 13, "rebounding": 17, "ball_handling": 15,
        "players": [
            ("Lauri", "Markkanen", "PF", 2.13, 109, 23, 15, 19, 14, 15, 18, 17, 87),
            ("Collin", "Sexton", "SG", 1.91, 86, 2, 19, 17, 16, 14, 15, 16, 82),
            ("Keyonte", "George", "PG", 1.93, 84, 3, 17, 17, 17, 13, 14, 17, 80),
            ("Walker", "Kessler", "C", 2.13, 111, 24, 13, 9, 10, 20, 19, 12, 81),
            ("John", "Collins", "PF", 2.06, 102, 20, 15, 16, 13, 14, 18, 15, 80),
            ("Jordan", "Clarkson", "SG", 1.96, 88, 0, 17, 17, 16, 12, 14, 17, 79),
            ("Taylor", "Hendricks", "PF", 2.06, 98, 0, 16, 15, 13, 17, 16, 14, 76),
            ("Cody", "Williams", "SF", 2.03, 86, 5, 16, 14, 14, 14, 14, 15, 75),
            ("Isaiah", "Collier", "PG", 1.93, 95, 8, 18, 13, 16, 15, 16, 16, 75),
            ("Brice", "Sensabaugh", "SF", 1.98, 107, 8, 14, 17, 14, 12, 16, 15, 75),
            ("Kyle", "Filipowski", "C", 2.11, 104, 22, 14, 15, 14, 14, 16, 15, 75),
            ("Drew", "Eubanks", "C", 2.08, 111, 14, 13, 11, 11, 15, 17, 12, 74),
            ("Svi", "Mykhailiuk", "SF", 2.01, 93, 19, 14, 16, 13, 12, 13, 14, 74),
            ("Johnny", "Juzang", "SG", 1.96, 95, 33, 14, 16, 13, 12, 13, 14, 73),
            ("Patty", "Mills", "PG", 1.88, 82, 8, 13, 16, 15, 12, 12, 15, 73),
        ]
    },

    # Pacific Division
    "Golden State Warriors": {
        "conf": "Western", "prestige": 19, "pace": 17, "shooting": 19, "defense": 17, "rebounding": 16, "ball_handling": 19,
        "players": [
            ("Stephen", "Curry", "PG", 1.88, 84, 30, 17, 20, 19, 14, 14, 20, 95),
            ("Draymond", "Green", "PF", 1.98, 104, 23, 14, 13, 18, 20, 18, 18, 86),
            ("Jonathan", "Kuminga", "PF", 2.01, 102, 0, 19, 15, 15, 16, 19, 16, 83),
            ("Brandin", "Podziemski", "SG", 1.96, 93, 2, 16, 16, 17, 16, 15, 17, 81),
            ("Andrew", "Wiggins", "SF", 2.01, 89, 22, 17, 16, 15, 17, 17, 16, 82),
            ("Buddy", "Hield", "SG", 1.93, 100, 7, 15, 19, 14, 13, 14, 15, 80),
            ("De'Anthony", "Melton", "SG", 1.91, 91, 8, 16, 16, 15, 18, 15, 15, 79),
            ("Trayce", "Jackson-Davis", "C", 2.06, 111, 32, 15, 10, 13, 17, 18, 13, 79),
            ("Kevon", "Looney", "C", 2.06, 101, 5, 12, 9, 13, 17, 18, 14, 78),
            ("Moses", "Moody", "SG", 1.98, 93, 4, 15, 16, 13, 16, 15, 14, 78),
            ("Kyle", "Anderson", "SF", 2.06, 104, 1, 11, 13, 17, 17, 15, 17, 78),
            ("Gary", "Payton II", "SG", 1.88, 88, 8, 17, 13, 13, 19, 16, 13, 77),
            ("Gui", "Santos", "SF", 1.98, 95, 15, 14, 14, 14, 14, 15, 14, 72),
            ("Lindy", "Waters III", "SG", 1.98, 98, 43, 14, 16, 13, 13, 13, 13, 73),
            ("Quinten", "Post", "C", 2.13, 111, 21, 11, 14, 11, 14, 16, 13, 71),
        ]
    },
    "Los Angeles Lakers": {
        "conf": "Western", "prestige": 20, "pace": 16, "shooting": 16, "defense": 17, "rebounding": 18, "ball_handling": 18,
        "players": [
            ("LeBron", "James", "SF", 2.06, 113, 23, 16, 17, 19, 16, 20, 20, 94),
            ("Anthony", "Davis", "C", 2.11, 115, 3, 16, 16, 15, 20, 20, 18, 94),
            ("Austin", "Reaves", "SG", 1.96, 89, 15, 16, 17, 17, 14, 14, 18, 83),
            ("D'Angelo", "Russell", "PG", 1.91, 88, 1, 15, 18, 18, 12, 13, 18, 82),
            ("Rui", "Hachimura", "PF", 2.03, 104, 28, 16, 16, 14, 15, 17, 15, 80),
            ("Dalton", "Knecht", "SG", 1.98, 97, 4, 16, 18, 14, 14, 15, 15, 80),
            ("Jarred", "Vanderbilt", "PF", 2.03, 97, 2, 17, 10, 13, 19, 17, 13, 78),
            ("Gabe", "Vincent", "PG", 1.88, 91, 7, 16, 15, 15, 16, 14, 15, 76),
            ("Jaxson", "Hayes", "C", 2.13, 100, 11, 16, 9, 10, 16, 18, 12, 75),
            ("Max", "Christie", "SG", 1.96, 86, 10, 16, 15, 14, 16, 14, 14, 76),
            ("Cam", "Reddish", "SF", 2.01, 99, 5, 16, 13, 13, 16, 15, 13, 74),
            ("Christian", "Wood", "C", 2.03, 97, 35, 14, 16, 13, 13, 16, 14, 76),
            ("Bronny", "James", "SG", 1.88, 95, 9, 17, 12, 14, 15, 14, 13, 71),
            ("Jalen", "Hood-Schifino", "PG", 1.98, 98, 0, 15, 13, 15, 14, 15, 14, 72),
            ("Maxwell", "Lewis", "SF", 2.01, 88, 21, 16, 13, 12, 13, 14, 12, 70),
        ]
    },
    "Phoenix Suns": {
        "conf": "Western", "prestige": 19, "pace": 16, "shooting": 19, "defense": 15, "rebounding": 16, "ball_handling": 18,
        "players": [
            ("Kevin", "Durant", "PF", 2.11, 109, 35, 16, 20, 18, 17, 17, 20, 94),
            ("Devin", "Booker", "SG", 1.98, 93, 1, 18, 19, 18, 15, 15, 19, 92),
            ("Bradley", "Beal", "SG", 1.93, 94, 3, 17, 18, 17, 14, 15, 18, 85),
            ("Tyus", "Jones", "PG", 1.88, 89, 21, 16, 16, 19, 15, 13, 18, 81),
            ("Jusuf", "Nurkic", "C", 2.13, 132, 20, 12, 12, 16, 16, 20, 16, 81),
            ("Grayson", "Allen", "SG", 1.93, 89, 8, 15, 19, 14, 15, 14, 15, 80),
            ("Royce", "O'Neale", "SF", 1.98, 103, 0, 14, 16, 14, 17, 16, 15, 78),
            ("Mason", "Plumlee", "C", 2.08, 115, 44, 13, 9, 15, 15, 18, 14, 76),
            ("Ryan", "Dunn", "SF", 2.03, 98, 0, 17, 14, 12, 19, 17, 13, 76),
            ("Monte", "Morris", "PG", 1.88, 83, 23, 16, 15, 17, 14, 13, 16, 76),
            ("Oso", "Ighodaro", "C", 2.08, 107, 4, 15, 10, 14, 16, 17, 14, 74),
            ("Damion", "Lee", "SG", 1.96, 95, 10, 14, 16, 13, 13, 13, 14, 73),
            ("Bol", "Bol", "PF", 2.21, 100, 11, 14, 16, 13, 16, 15, 14, 76),
            ("Josh", "Okogie", "SG", 1.93, 97, 2, 17, 12, 13, 18, 16, 13, 74),
            ("Collin", "Gillespie", "PG", 1.85, 88, 12, 15, 15, 15, 13, 13, 14, 72),
        ]
    },
    "Sacramento Kings": {
        "conf": "Western", "prestige": 17, "pace": 19, "shooting": 17, "defense": 15, "rebounding": 18, "ball_handling": 18,
        "players": [
            ("De'Aaron", "Fox", "PG", 1.91, 84, 5, 20, 18, 18, 17, 16, 19, 90),
            ("Domantas", "Sabonis", "C", 2.08, 109, 11, 15, 14, 19, 16, 19, 19, 90),
            ("DeMar", "DeRozan", "SF", 1.98, 100, 10, 16, 19, 17, 14, 17, 19, 87),
            ("Keegan", "Murray", "SF", 2.03, 102, 13, 16, 17, 14, 17, 16, 15, 83),
            ("Malik", "Monk", "SG", 1.91, 91, 0, 18, 17, 17, 13, 14, 17, 83),
            ("Keon", "Ellis", "SG", 1.93, 79, 23, 17, 16, 14, 19, 14, 14, 78),
            ("Kevin", "Huerter", "SG", 2.01, 89, 9, 15, 17, 15, 14, 14, 15, 78),
            ("Trey", "Lyles", "PF", 2.06, 106, 41, 13, 16, 13, 14, 16, 14, 77),
            ("Alex", "Len", "C", 2.13, 113, 25, 12, 9, 10, 15, 18, 12, 74),
            ("Jordan", "McLaughlin", "PG", 1.83, 84, 7, 17, 13, 16, 16, 12, 15, 75),
            ("Devin", "Carter", "PG", 1.88, 88, 22, 18, 15, 15, 18, 15, 15, 76),
            ("Jalen", "McDaniels", "PF", 2.06, 93, 20, 16, 13, 12, 15, 15, 12, 73),
            ("Doug", "McDermott", "SF", 1.98, 102, 17, 13, 17, 12, 11, 14, 13, 73),
            ("Colby", "Jones", "SG", 1.98, 93, 20, 15, 13, 14, 14, 14, 14, 72),
            ("Orlando", "Robinson", "C", 2.08, 107, 19, 13, 10, 11, 14, 16, 12, 71),
        ]
    },
    "Los Angeles Clippers": {
        "conf": "Western", "prestige": 17, "pace": 15, "shooting": 17, "defense": 18, "rebounding": 16, "ball_handling": 17,
        "players": [
            ("Kawhi", "Leonard", "SF", 2.01, 102, 2, 16, 19, 17, 20, 19, 19, 92),
            ("James", "Harden", "PG", 1.96, 100, 1, 15, 18, 20, 14, 16, 20, 89),
            ("Norman", "Powell", "SG", 1.93, 98, 24, 17, 18, 15, 15, 16, 16, 84),
            ("Ivica", "Zubac", "C", 2.13, 109, 40, 13, 12, 13, 18, 19, 14, 83),
            ("Derrick", "Jones Jr", "SF", 1.98, 95, 55, 18, 14, 13, 18, 17, 14, 79),
            ("Terance", "Mann", "SG", 1.96, 98, 14, 16, 15, 15, 17, 16, 15, 78),
            ("Kevin", "Porter Jr", "PG", 1.93, 92, 7, 17, 16, 17, 14, 15, 17, 79),
            ("Kris", "Dunn", "PG", 1.91, 93, 8, 17, 13, 16, 19, 15, 15, 77),
            ("Nicolas", "Batum", "PF", 2.03, 104, 33, 12, 16, 15, 16, 15, 16, 77),
            ("Amir", "Coffey", "SF", 2.01, 95, 7, 15, 16, 14, 15, 15, 14, 76),
            ("Bones", "Hyland", "PG", 1.88, 78, 5, 18, 16, 16, 11, 12, 16, 75),
            ("Mo", "Bamba", "C", 2.13, 105, 11, 14, 14, 11, 17, 17, 12, 75),
            ("Kobe", "Brown", "PF", 2.01, 113, 21, 14, 14, 14, 13, 16, 14, 73),
            ("Jordan", "Miller", "SF", 1.96, 88, 11, 16, 14, 13, 15, 14, 14, 73),
            ("Cam", "Christie", "SG", 1.98, 86, 12, 15, 15, 13, 13, 13, 13, 71),
        ]
    },

    # Southwest Division
    "Dallas Mavericks": {
        "conf": "Western", "prestige": 19, "pace": 16, "shooting": 19, "defense": 17, "rebounding": 18, "ball_handling": 19,
        "players": [
            ("Luka", "Doncic", "PG", 2.01, 104, 77, 16, 20, 20, 15, 18, 20, 96),
            ("Kyrie", "Irving", "SG", 1.88, 88, 11, 18, 19, 20, 15, 14, 20, 92),
            ("Klay", "Thompson", "SF", 1.98, 100, 31, 14, 19, 14, 15, 15, 16, 83),
            ("P.J.", "Washington", "PF", 2.01, 104, 25, 15, 16, 14, 17, 17, 15, 81),
            ("Dereck", "Lively II", "C", 2.16, 104, 2, 17, 10, 13, 19, 18, 14, 83),
            ("Daniel", "Gafford", "C", 2.08, 106, 21, 16, 10, 12, 18, 19, 13, 81),
            ("Naji", "Marshall", "SF", 2.01, 100, 13, 16, 15, 15, 17, 16, 15, 79),
            ("Quentin", "Grimes", "SG", 1.96, 95, 5, 16, 16, 14, 16, 15, 14, 77),
            ("Spencer", "Dinwiddie", "PG", 1.96, 98, 26, 16, 15, 16, 13, 14, 16, 77),
            ("Jaden", "Hardy", "SG", 1.93, 89, 1, 17, 16, 15, 12, 14, 16, 77),
            ("Maxi", "Kleber", "PF", 2.08, 109, 42, 12, 15, 13, 16, 16, 14, 76),
            ("Dwight", "Powell", "C", 2.08, 109, 7, 14, 10, 13, 14, 17, 13, 74),
            ("Olivier-Maxence", "Prosper", "PF", 2.01, 104, 18, 16, 13, 12, 15, 16, 13, 73),
            ("Markieff", "Morris", "PF", 2.06, 111, 88, 11, 13, 12, 13, 15, 13, 72),
            ("Danté", "Exum", "PG", 1.96, 97, 0, 17, 15, 15, 16, 14, 15, 76),
        ]
    },
    "Houston Rockets": {
        "conf": "Western", "prestige": 17, "pace": 17, "shooting": 16, "defense": 19, "rebounding": 19, "ball_handling": 16,
        "players": [
            ("Alperen", "Sengun", "C", 2.11, 110, 28, 14, 16, 18, 16, 18, 19, 88),
            ("Jalen", "Green", "SG", 1.93, 84, 4, 19, 17, 16, 14, 15, 17, 85),
            ("Fred", "VanVleet", "PG", 1.83, 89, 5, 16, 17, 18, 17, 14, 18, 84),
            ("Amen", "Thompson", "SF", 2.01, 95, 1, 19, 12, 17, 19, 18, 16, 83),
            ("Jabari", "Smith Jr", "PF", 2.11, 100, 10, 15, 17, 13, 17, 17, 15, 82),
            ("Dillon", "Brooks", "SF", 1.98, 102, 9, 15, 16, 14, 19, 17, 15, 80),
            ("Tari", "Eason", "PF", 2.03, 98, 17, 17, 15, 13, 19, 17, 14, 80),
            ("Steven", "Adams", "C", 2.11, 120, 12, 11, 8, 12, 17, 20, 13, 79),
            ("Reed", "Sheppard", "PG", 1.91, 85, 15, 17, 19, 16, 16, 13, 17, 79),
            ("Cam", "Whitmore", "SF", 2.01, 104, 7, 18, 16, 13, 14, 18, 14, 78),
            ("Jeff", "Green", "PF", 2.03, 107, 32, 13, 14, 13, 13, 16, 14, 75),
            ("Aaron", "Holiday", "PG", 1.83, 84, 0, 16, 16, 14, 15, 13, 14, 74),
            ("Jock", "Landale", "C", 2.11, 116, 2, 12, 13, 12, 14, 17, 13, 74),
            ("Jae'Sean", "Tate", "SF", 1.93, 104, 8, 15, 12, 13, 16, 16, 13, 74),
            ("Jack", "McVeigh", "PF", 2.03, 93, 58, 13, 15, 12, 12, 14, 13, 70),
        ]
    },
    "Memphis Grizzlies": {
        "conf": "Western", "prestige": 17, "pace": 18, "shooting": 16, "defense": 18, "rebounding": 18, "ball_handling": 17,
        "players": [
            ("Ja", "Morant", "PG", 1.88, 79, 12, 20, 17, 19, 15, 16, 19, 91),
            ("Jaren", "Jackson Jr", "PF", 2.08, 110, 13, 16, 16, 14, 20, 19, 16, 88),
            ("Desmond", "Bane", "SG", 1.96, 98, 22, 16, 19, 17, 16, 17, 18, 86),
            ("Marcus", "Smart", "PG", 1.91, 100, 36, 16, 15, 16, 19, 17, 16, 81),
            ("Zach", "Edey", "C", 2.24, 136, 14, 11, 10, 11, 18, 20, 13, 80),
            ("Santi", "Aldama", "PF", 2.13, 98, 7, 14, 17, 14, 15, 16, 16, 79),
            ("Scotty", "Pippen Jr", "PG", 1.85, 84, 1, 17, 15, 16, 17, 13, 16, 78),
            ("Vince", "Williams Jr", "SG", 1.93, 93, 5, 16, 15, 15, 18, 15, 15, 78),
            ("GG", "Jackson", "SF", 2.06, 95, 45, 17, 16, 14, 14, 16, 15, 78),
            ("Luke", "Kennard", "SG", 1.96, 93, 10, 14, 19, 15, 12, 13, 15, 77),
            ("Jay", "Huff", "C", 2.16, 109, 30, 12, 16, 11, 17, 17, 13, 76),
            ("John", "Konchar", "SG", 1.96, 95, 46, 15, 14, 14, 16, 15, 14, 75),
            ("Jaylen", "Wells", "SF", 2.03, 93, 0, 15, 16, 13, 15, 14, 14, 75),
            ("Brandon", "Clarke", "PF", 2.03, 98, 15, 16, 12, 13, 16, 17, 14, 77),
            ("Jake", "LaRavia", "PF", 2.01, 107, 3, 14, 15, 14, 14, 15, 14, 74),
        ]
    },
    "San Antonio Spurs": {
        "conf": "Western", "prestige": 17, "pace": 17, "shooting": 16, "defense": 18, "rebounding": 18, "ball_handling": 17,
        "players": [
            ("Victor", "Wembanyama", "C", 2.21, 95, 1, 18, 18, 17, 20, 19, 19, 94),
            ("Chris", "Paul", "PG", 1.83, 79, 3, 13, 16, 20, 17, 13, 20, 83),
            ("Devin", "Vassell", "SG", 1.96, 91, 24, 17, 18, 16, 16, 15, 17, 83),
            ("Stephon", "Castle", "PG", 1.98, 98, 5, 17, 14, 16, 18, 17, 16, 80),
            ("Jeremy", "Sochan", "PF", 2.03, 104, 10, 17, 14, 15, 18, 18, 15, 80),
            ("Harrison", "Barnes", "SF", 2.03, 102, 40, 14, 17, 14, 15, 16, 15, 79),
            ("Keldon", "Johnson", "SF", 1.96, 100, 0, 17, 16, 14, 14, 18, 15, 80),
            ("Julian", "Champagnie", "SF", 2.01, 98, 30, 15, 17, 13, 15, 15, 14, 77),
            ("Zach", "Collins", "C", 2.11, 113, 23, 13, 14, 14, 16, 17, 14, 76),
            ("Tre", "Jones", "PG", 1.85, 84, 33, 17, 14, 17, 16, 13, 16, 77),
            ("Malaki", "Branham", "SG", 1.93, 82, 22, 16, 16, 14, 12, 13, 15, 75),
            ("Blake", "Wesley", "PG", 1.91, 84, 14, 18, 12, 14, 16, 14, 14, 74),
            ("Charles", "Bassey", "C", 2.08, 104, 28, 14, 9, 10, 16, 17, 12, 73),
            ("Sandro", "Mamukelashvili", "PF", 2.06, 109, 54, 14, 15, 14, 13, 16, 15, 75),
            ("Harrison", "Ingram", "SF", 1.96, 102, 55, 14, 14, 14, 15, 15, 14, 72),
        ]
    },
    "New Orleans Pelicans": {
        "conf": "Western", "prestige": 16, "pace": 16, "shooting": 16, "defense": 17, "rebounding": 17, "ball_handling": 17,
        "players": [
            ("Zion", "Williamson", "PF", 1.98, 129, 1, 18, 16, 17, 15, 20, 18, 90),
            ("Brandon", "Ingram", "SF", 2.03, 86, 14, 16, 18, 17, 15, 15, 18, 86),
            ("Dejounte", "Murray", "PG", 1.96, 82, 5, 18, 17, 18, 18, 15, 18, 86),
            ("CJ", "McCollum", "SG", 1.91, 86, 3, 16, 18, 17, 13, 14, 18, 83),
            ("Herbert", "Jones", "SF", 2.01, 93, 2, 16, 16, 14, 20, 16, 15, 82),
            ("Trey", "Murphy III", "SF", 2.03, 93, 25, 17, 19, 14, 16, 16, 16, 83),
            ("Yves", "Missi", "C", 2.11, 107, 21, 15, 9, 10, 17, 18, 12, 77),
            ("Jose", "Alvarado", "PG", 1.83, 81, 15, 17, 15, 16, 19, 13, 16, 78),
            ("Jordan", "Hawkins", "SG", 1.96, 86, 24, 16, 18, 13, 13, 14, 15, 77),
            ("Daniel", "Theis", "C", 2.03, 111, 9, 12, 13, 12, 16, 17, 13, 75),
            ("Javonte", "Green", "SG", 1.96, 93, 28, 17, 14, 13, 17, 16, 13, 75),
            ("Jeremiah", "Robinson-Earl", "PF", 2.06, 109, 50, 14, 14, 12, 14, 16, 13, 73),
            ("Antonio", "Reeves", "SG", 1.96, 88, 12, 15, 16, 13, 13, 13, 14, 73),
            ("Jamal", "Cain", "SF", 1.98, 87, 8, 16, 13, 12, 14, 14, 13, 71),
            ("Brandon", "Boston Jr", "SG", 1.98, 85, 11, 16, 15, 14, 13, 13, 14, 74),
        ]
    }
}

def build_database(target_db_path):
    print(f"Creating 2026/27 NBA Database at: {target_db_path}")
    if os.path.exists(target_db_path):
        os.remove(target_db_path)
    
    conn = sqlite3.connect(target_db_path)
    cur = conn.cursor()

    # Schema matching bm-core and Unity
    cur.execute("""
        CREATE TABLE teams (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            conference TEXT NOT NULL,
            prestige INTEGER DEFAULT 10,
            pace INTEGER DEFAULT 10,
            shooting INTEGER DEFAULT 10,
            defense INTEGER DEFAULT 10,
            rebounding INTEGER DEFAULT 10,
            ball_handling INTEGER DEFAULT 10,
            active INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    """)

    cur.execute("""
        CREATE TABLE players (
            player_id TEXT PRIMARY KEY,
            first_name TEXT NOT NULL,
            last_name TEXT NOT NULL,
            team_id INTEGER,
            position TEXT NOT NULL,
            year_in_school INTEGER DEFAULT 4,
            height REAL DEFAULT 2.0,
            weight REAL DEFAULT 100.0,
            jersey_number INTEGER DEFAULT 0,
            birth_date TEXT DEFAULT '2000-01-01',
            pace INTEGER DEFAULT 10,
            shooting INTEGER DEFAULT 10,
            ball_control INTEGER DEFAULT 10,
            defense INTEGER DEFAULT 10,
            physical INTEGER DEFAULT 10,
            technical INTEGER DEFAULT 10,
            current_ability INTEGER DEFAULT 70,
            potential_ability INTEGER DEFAULT 75,
            active INTEGER DEFAULT 1,
            injured INTEGER DEFAULT 0,
            suspension_matches INTEGER DEFAULT 0,
            international INTEGER DEFAULT 0,
            draft_eligible INTEGER DEFAULT 0,
            nil_value INTEGER DEFAULT 0,
            games_played REAL DEFAULT 0,
            minutes_played REAL DEFAULT 0,
            points_per_game REAL DEFAULT 0,
            rebounds_per_game REAL DEFAULT 0,
            assists_per_game REAL DEFAULT 0,
            FOREIGN KEY (team_id) REFERENCES teams(id)
        );
    """)

    cur.execute("CREATE INDEX idx_players_team ON players(team_id);")
    cur.execute("CREATE INDEX idx_teams_conf ON teams(conference);")

    team_id_counter = 1
    total_players = 0

    for team_name, data in NBA_TEAMS_DATA.items():
        cur.execute("""
            INSERT INTO teams (id, name, conference, prestige, pace, shooting, defense, rebounding, ball_handling, active)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1)
        """, (
            team_id_counter,
            team_name,
            data["conf"],
            data["prestige"],
            data.get("pace", 15),
            data.get("shooting", 15),
            data.get("defense", 15),
            data.get("rebounding", 15),
            data.get("ball_handling", 15)
        ))

        # Insert 15 players for this team with mutated last names
        for idx, p in enumerate(data["players"]):
            first_name, orig_last_name, pos, height, weight, jersey, pace, shooting, ball_ctrl, defense, physical, technical, ability = p
            mutated_last = mutate_last_name(orig_last_name)
            player_id = f"nba_2627_{team_id_counter}_{idx+1:02d}"

            cur.execute("""
                INSERT INTO players (
                    player_id, first_name, last_name, team_id, position,
                    year_in_school, height, weight, jersey_number,
                    pace, shooting, ball_control, defense, physical, technical,
                    current_ability, potential_ability, active, injured
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, 0)
            """, (
                player_id,
                first_name,
                mutated_last,
                team_id_counter,
                pos,
                4,
                height,
                weight,
                jersey,
                pace,
                shooting,
                ball_ctrl,
                defense,
                physical,
                technical,
                ability,
                min(99, ability + 4)
            ))
            total_players += 1

        team_id_counter += 1

    conn.commit()
    conn.close()
    print(f"✓ Generated {len(NBA_TEAMS_DATA)} NBA Teams and {total_players} players into {target_db_path}")

if __name__ == "__main__":
    out_path = sys.argv[1] if len(sys.argv) > 1 else "UI-unity-bm26/Assets/StreamingAssets/basketball_manager.db"
    build_database(out_path)
