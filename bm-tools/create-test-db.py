#!/usr/bin/env python3
"""Create a small, readable database for repeatable simulator smoke tests."""

import sqlite3
import sys


TEAMS = [
    ("Lakeview Lions", "Eastern", 78),
    ("Cedar Valley Hawks", "Eastern", 74),
    ("Redwood State Bears", "Eastern", 71),
    ("Summit College Wolves", "Eastern", 68),
    ("Harbor Point Mariners", "Eastern", 65),
    ("Prairie Tech Bisons", "Western", 76),
    ("Pine Ridge Eagles", "Western", 73),
    ("River City Mustangs", "Western", 70),
    ("Desert State Scorpions", "Western", 67),
    ("Blue Mountain Miners", "Western", 64),
]

FIRST_NAMES = [
    "Marcus", "Jordan", "Evan", "Caleb", "Darius", "Malcolm", "Andre",
    "Julian", "Isaiah", "Noah", "Tyler", "Cameron", "Mason",
]
LAST_NAMES = [
    "Bennett", "Carter", "Davis", "Ellis", "Foster", "Hayes", "Jackson",
    "King", "Lewis", "Morgan", "Parker", "Reed", "Turner",
]
POSITIONS = ["PG", "SG", "SF", "PF", "C"]


def create_database(path):
    connection = sqlite3.connect(path)
    connection.executescript(
        """
        DROP TABLE IF EXISTS players;
        DROP TABLE IF EXISTS teams;
        CREATE TABLE teams (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            conference TEXT NOT NULL,
            prestige INTEGER DEFAULT 10,
            active INTEGER DEFAULT 1
        );
        CREATE TABLE players (
            player_id TEXT PRIMARY KEY,
            first_name TEXT NOT NULL,
            last_name TEXT NOT NULL,
            team_id INTEGER,
            position TEXT NOT NULL,
            year_in_school INTEGER DEFAULT 1,
            height REAL DEFAULT 1.9,
            weight REAL DEFAULT 90,
            jersey_number INTEGER,
            birth_date TEXT DEFAULT '2004-01-01',
            pace INTEGER DEFAULT 10,
            shooting INTEGER DEFAULT 10,
            ball_control INTEGER DEFAULT 10,
            defense INTEGER DEFAULT 10,
            physical INTEGER DEFAULT 10,
            technical INTEGER DEFAULT 10,
            current_ability INTEGER DEFAULT 50,
            potential_ability INTEGER DEFAULT 60,
            active INTEGER DEFAULT 1,
            injured INTEGER DEFAULT 0,
            suspension_matches INTEGER DEFAULT 0,
            international INTEGER DEFAULT 0,
            draft_eligible INTEGER DEFAULT 0,
            nil_value INTEGER DEFAULT 0,
            FOREIGN KEY (team_id) REFERENCES teams(id)
        );
        """
    )

    for team_index, (team_name, conference, prestige) in enumerate(TEAMS):
        cursor = connection.execute(
            "INSERT INTO teams (name, conference, prestige) VALUES (?, ?, ?)",
            (team_name, conference, prestige),
        )
        team_id = cursor.lastrowid
        for player_index in range(13):
            first_name = FIRST_NAMES[(team_index * 3 + player_index) % len(FIRST_NAMES)]
            last_name = LAST_NAMES[(team_index + player_index) % len(LAST_NAMES)]
            position = POSITIONS[player_index % len(POSITIONS)]
            rating = prestige - 20 + (player_index % 6)
            connection.execute(
                """
                INSERT INTO players (
                    player_id, first_name, last_name, team_id, position,
                    year_in_school, jersey_number, pace, shooting, ball_control,
                    defense, physical, technical, current_ability, potential_ability
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    f"{team_id}_{player_index + 1}", first_name, last_name,
                    team_id, position, (player_index % 4) + 1, player_index + 1,
                    rating, rating, rating, rating, rating, rating, rating * 2,
                    min(100, rating * 2 + 10),
                ),
            )
    connection.commit()
    connection.close()
    print(f"Created {path} with {len(TEAMS)} teams and {len(TEAMS) * 13} players.")


if __name__ == "__main__":
    create_database(sys.argv[1] if len(sys.argv) > 1 else "/tmp/basketball_manager_readable.db")