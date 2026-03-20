#!/usr/bin/env python3
"""
NBA Roster Data Generator for Basketball Manager
Generates realistic NBA rosters based on 2024-25 season data
"""

import sqlite3
import json

# NBA Teams and their players (sample data - 2024-25 season)
NBA_DATA = {
    "Boston Celtics": {
        "conference": "Eastern",
        "prestige": 20,
        "players": [
            {"name": "Jayson Tatum", "pos": "SF", "height": 2.03, "weight": 104, "shooting": 18, "defense": 17},
            {"name": "Jaylen Brown", "pos": "SF", "height": 2.01, "weight": 104, "shooting": 17, "defense": 16},
            {"name": "Derrick White", "pos": "PG", "height": 1.96, "weight": 96, "shooting": 14, "defense": 18},
            {"name": "Al Horford", "pos": "C", "height": 2.06, "weight": 113, "shooting": 12, "defense": 15},
        ]
    },
    "Los Angeles Lakers": {
        "conference": "Western",
        "prestige": 19,
        "players": [
            {"name": "LeBron James", "pos": "SF", "height": 2.06, "weight": 113, "shooting": 16, "defense": 15},
            {"name": "DeAndrea Ayton", "pos": "C", "height": 2.17, "weight": 125, "shooting": 15, "defense": 18},
            {"name": "Austin Reaves", "pos": "SG", "height": 1.96, "weight": 90, "shooting": 14, "defense": 13},
            {"name": "Luka Doncic", "pos": "SG", "height": 2.00, "weight": 104, "shooting": 20, "defense": 12},
            {"name": "Maxi Kleber", "pos": "PF", "height": 2.10, "weight": 109, "shooting": 12, "defense": 15},
        ]
    },
    "Golden State Warriors": {
        "conference": "Western",
        "prestige": 18,
        "players": [
            {"name": "Stephen Curry", "pos": "PG", "height": 1.88, "weight": 86, "shooting": 20, "defense": 11},
           
            {"name": "Andrew Wiggins", "pos": "SF", "height": 2.00, "weight": 99, "shooting": 15, "defense": 16},
            {"name": "Draymond Green", "pos": "PF", "height": 1.98, "weight": 104, "shooting": 10, "defense": 19},
        ]
    },
    "Denver Nuggets": {
        "conference": "Western",
        "prestige": 18,
        "players": [
            {"name": "Nikola Jokic", "pos": "C", "height": 2.11, "weight": 129, "shooting": 18, "defense": 14},
            {"name": "Jamal Murray", "pos": "PG", "height": 1.91, "weight": 90, "shooting": 16, "defense": 13},
            {"name": "Scottie Barnes", "pos": "SF", "height": 2.01, "weight": 107, "shooting": 14, "defense": 17},
            {"name": "Aaron Gordon", "pos": "PF", "height": 2.06, "weight": 109, "shooting": 13, "defense": 17},
        ]
    },
    "Miami Heat": {
        "conference": "Eastern",
        "prestige": 17,
        "players": [
            {"name": "Jimmy Butler", "pos": "SF", "height": 2.01, "weight": 104, "shooting": 15, "defense": 18},
            {"name": "Bam Adebayo", "pos": "C", "height": 2.06, "weight": 115, "shooting": 11, "defense": 18},
            {"name": "Tyler Herro", "pos": "SG", "height": 1.96, "weight": 90, "shooting": 17, "defense": 12},
            {"name": "Jaime Jaquez Jr", "pos": "SF", "height": 2.00, "weight": 102, "shooting": 13, "defense": 15},
        ]
    },
    "New York Knicks": {
        "conference": "Eastern",
        "prestige": 17,
        "players": [
            {"name": "Karl-Anthony Towns", "pos": "C", "height": 2.13, "weight": 116, "shooting": 18, "defense": 11},
            {"name": "Julius Randle", "pos": "PF", "height": 2.05, "weight": 129, "shooting": 14, "defense": 13},
            {"name": "Jalen Brunson", "pos": "PG", "height": 1.83, "weight": 84, "shooting": 17, "defense": 11},
            {"name": "OG Anunoby", "pos": "SF", "height": 2.00, "weight": 100, "shooting": 14, "defense": 17},
        ]
    },
    "Phoenix Suns": {
        "conference": "Western",
        "prestige": 18,
        "players": [
            {"name": "Kevin Durant", "pos": "SF", "height": 2.10, "weight": 108, "shooting": 19, "defense": 14},
            {"name": "Devin Booker", "pos": "SG", "height": 1.98, "weight": 102, "shooting": 18, "defense": 12},
            {"name": "Bradley Beal", "pos": "SG", "height": 1.96, "weight": 99, "shooting": 16, "defense": 13},
            {"name": "Jusuf Nurkic", "pos": "C", "height": 2.13, "weight": 129, "shooting": 12, "defense": 16},
        ]
    },
    "Dallas Mavericks": {
        "conference": "Western",
        "prestige": 17,
        "players": [
            {"name": "Luka Doncic", "pos": "SG", "height": 2.00, "weight": 104, "shooting": 19, "defense": 12},
            {"name": "Kyrie Irving", "pos": "PG", "height": 1.88, "weight": 88, "shooting": 18, "defense": 11},
            {"name": "Derrick Jones Jr", "pos": "SF", "height": 1.98, "weight": 99, "shooting": 13, "defense": 16},
            {"name": "Anthony Davis", "pos": "C", "height": 2.11, "weight": 115, "shooting": 15, "defense": 18},
            {"name": "Klay Thompson", "pos": "SG", "height": 2.01, "weight": 98, "shooting": 19, "defense": 13},
            {"name": "D'Angelo Russell", "pos": "PG", "height": 1.93, "weight": 86, "shooting": 15, "defense": 12},
        ]
    },
}

def create_nba_rosters(db_path):
    """Create NBA rosters in the database"""
    
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    # Create tables if they don't exist
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS teams (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            conference TEXT NOT NULL,
            prestige INTEGER DEFAULT 10,
            pace INTEGER DEFAULT 10,
            shooting INTEGER DEFAULT 10,
            defense INTEGER DEFAULT 10,
            rebounding INTEGER DEFAULT 10,
            ball_handling INTEGER DEFAULT 10,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    ''')
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS players (
            player_id TEXT PRIMARY KEY,
            first_name TEXT NOT NULL,
            last_name TEXT NOT NULL,
            team_id INTEGER,
            position TEXT,
            year_in_school INTEGER DEFAULT 1,
            height REAL,
            weight REAL,
            jersey_number INTEGER,
            birth_date TEXT,
            pace INTEGER DEFAULT 10,
            shooting INTEGER DEFAULT 10,
            ball_control INTEGER DEFAULT 10,
            defense INTEGER DEFAULT 10,
            physical INTEGER DEFAULT 10,
            technical INTEGER DEFAULT 10,
            current_ability INTEGER DEFAULT 50,
            potential_ability INTEGER DEFAULT 50,
            active INTEGER DEFAULT 1,
            injured INTEGER DEFAULT 0,
            suspension_matches INTEGER DEFAULT 0,
            international INTEGER DEFAULT 0,
            draft_eligible INTEGER DEFAULT 0,
            nil_value INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(team_id) REFERENCES teams(id)
        )
    ''')
    
    conn.commit()
    
    team_count = 0
    player_count = 0
    
    for team_name, team_data in NBA_DATA.items():
        print(f"Importing team: {team_name}...")
        
        try:
            # Insert team
            cursor.execute('''
                INSERT OR REPLACE INTO teams (name, conference, prestige, shooting, defense)
                VALUES (?, ?, ?, 16, 14)
            ''', (team_name, team_data["conference"], team_data["prestige"]))
            
            team_id = cursor.lastrowid
            team_count += 1
            
            # Insert players
            for i, player in enumerate(team_data["players"]):
                player_id = f"nba_{team_name.replace(' ', '_').lower()}_{i+1}"
                
                first_name, last_name = player["name"].split(' ', 1)
                
                cursor.execute('''
                    INSERT OR REPLACE INTO players (
                        player_id, first_name, last_name, team_id, position,
                        height, weight, jersey_number,
                        pace, shooting, ball_control, defense, physical, technical,
                        current_ability, potential_ability,
                        active, injured, year_in_school
                    )
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    player_id, first_name, last_name, team_id, player["pos"],
                    player.get("height", 2.0), player.get("weight", 100), i + 1,
                    12, player.get("shooting", 14), 12, player.get("defense", 12), 14, 13,
                    70, 80,
                    1, 0, 4
                ))
                
                player_count += 1
            
            conn.commit()
            print(f"  ✓ Imported {len(team_data['players'])} players")
        
        except Exception as e:
            print(f"  ✗ Error importing {team_name}: {e}")
            conn.rollback()
    
    conn.close()
    
    print(f"\n✓ NBA roster import complete!")
    print(f"  Teams imported: {team_count}")
    print(f"  Players imported: {player_count}")

if __name__ == "__main__":
    import sys
    db_path = sys.argv[1] if len(sys.argv) > 1 else "basketball_manager.db"
    create_nba_rosters(db_path)
