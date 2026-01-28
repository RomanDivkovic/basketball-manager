#!/usr/bin/env python3
"""
NBA Roster Web Scraper for Basketball Manager
Scrapes 2024-25 NBA rosters from Basketball Reference
Generates complete 12-15 player rosters for all 30 teams
"""

import requests
from bs4 import BeautifulSoup
import sqlite3
import time
import re

# NBA teams with their Basketball-Reference URLs and conferences
NBA_TEAMS = {
    # Eastern Conference - Atlantic
    "Boston Celtics": {"url": "BOS", "conf": "Eastern", "prestige": 20},
    "Brooklyn Nets": {"url": "BRK", "conf": "Eastern", "prestige": 14},
    "New York Knicks": {"url": "NYK", "conf": "Eastern", "prestige": 17},
    "Philadelphia 76ers": {"url": "PHI", "conf": "Eastern", "prestige": 16},
    "Toronto Raptors": {"url": "TOR", "conf": "Eastern", "prestige": 13},
    
    # Eastern Conference - Central
    "Chicago Bulls": {"url": "CHI", "conf": "Eastern", "prestige": 14},
    "Cleveland Cavaliers": {"url": "CLE", "conf": "Eastern", "prestige": 17},
    "Detroit Pistons": {"url": "DET", "conf": "Eastern", "prestige": 11},
    "Indiana Pacers": {"url": "IND", "conf": "Eastern", "prestige": 15},
    "Milwaukee Bucks": {"url": "MIL", "conf": "Eastern", "prestige": 18},
    
    # Eastern Conference - Southeast
    "Atlanta Hawks": {"url": "ATL", "conf": "Eastern", "prestige": 14},
    "Charlotte Hornets": {"url": "CHO", "conf": "Eastern", "prestige": 11},
    "Miami Heat": {"url": "MIA", "conf": "Eastern", "prestige": 17},
    "Orlando Magic": {"url": "ORL", "conf": "Eastern", "prestige": 15},
    "Washington Wizards": {"url": "WAS", "conf": "Eastern", "prestige": 10},
    
    # Western Conference - Northwest
    "Denver Nuggets": {"url": "DEN", "conf": "Western", "prestige": 19},
    "Minnesota Timberwolves": {"url": "MIN", "conf": "Western", "prestige": 16},
    "Oklahoma City Thunder": {"url": "OKC", "conf": "Western", "prestige": 17},
    "Portland Trail Blazers": {"url": "POR", "conf": "Western", "prestige": 12},
    "Utah Jazz": {"url": "UTA", "conf": "Western", "prestige": 12},
    
    # Western Conference - Pacific
    "Golden State Warriors": {"url": "GSW", "conf": "Western", "prestige": 18},
    "Los Angeles Clippers": {"url": "LAC", "conf": "Western", "prestige": 16},
    "Los Angeles Lakers": {"url": "LAL", "conf": "Western", "prestige": 19},
    "Phoenix Suns": {"url": "PHO", "conf": "Western", "prestige": 18},
    "Sacramento Kings": {"url": "SAC", "conf": "Western", "prestige": 15},
    
    # Western Conference - Southwest
    "Dallas Mavericks": {"url": "DAL", "conf": "Western", "prestige": 18},
    "Houston Rockets": {"url": "HOU", "conf": "Western", "prestige": 14},
    "Memphis Grizzlies": {"url": "MEM", "conf": "Western", "prestige": 15},
    "New Orleans Pelicans": {"url": "NOP", "conf": "Western", "prestige": 14},
    "San Antonio Spurs": {"url": "SAS", "conf": "Western", "prestige": 13},
}

# Position mapping
POS_MAP = {
    "PG": "PG", "Point Guard": "PG",
    "SG": "SG", "Shooting Guard": "SG",
    "SF": "SF", "Small Forward": "SF",
    "PF": "PF", "Power Forward": "PF",
    "C": "C", "Center": "C",
    "G": "SG", "F": "SF", "F-C": "PF", "G-F": "SF", "C-F": "PF"
}

def parse_height_to_meters(height_str):
    """Convert height string like '6-7' to meters"""
    try:
        if '-' in height_str:
            feet, inches = height_str.split('-')
            total_inches = int(feet) * 12 + int(inches)
            return round(total_inches * 0.0254, 2)
        return 2.0  # default
    except:
        return 2.0

def parse_weight_to_kg(weight_str):
    """Convert weight string like '215' lbs to kg"""
    try:
        lbs = int(re.sub(r'[^0-9]', '', weight_str))
        return int(lbs * 0.453592)
    except:
        return 90

def estimate_attributes_from_stats(stats):
    """
    Estimate player attributes from per-game stats
    Stats expected: pts, reb, ast, stl, blk, fg%, 3p%
    """
    pts = stats.get('pts', 5)
    reb = stats.get('reb', 2)
    ast = stats.get('ast', 1)
    stl = stats.get('stl', 0.5)
    blk = stats.get('blk', 0.3)
    fg_pct = stats.get('fg_pct', 0.40)
    three_pct = stats.get('three_pct', 0.30)
    
    # Map stats to attributes (scale 1-20)
    shooting = min(20, int(8 + (pts * 0.5) + (fg_pct * 15) + (three_pct * 10)))
    defense = min(20, int(8 + (stl * 4) + (blk * 3) + ((reb / 5) * 8)))
    pace = min(20, int(8 + (pts * 0.3) + (ast * 0.7)))
    ball_control = min(20, int(8 + (ast * 1.5) + (3 if stl > 1 else 0)))
    physical = min(20, int(8 + (reb * 0.8) + (blk * 2)))
    technical = min(20, int(8 + (ast * 1.2) + (fg_pct * 10)))
    
    # Calculate overall ability
    current_ability = int((shooting + defense + pace + ball_control + physical + technical) * 100 / 120)
    
    return {
        'pace': max(1, pace),
        'shooting': max(1, shooting),
        'ball_control': max(1, ball_control),
        'defense': max(1, defense),
        'physical': max(1, physical),
        'technical': max(1, technical),
        'current_ability': min(99, max(30, current_ability)),
        'potential_ability': min(99, current_ability + 5)
    }

def scrape_team_roster_simple(team_name, team_info):
    """
    Scrape roster for a team from Basketball-Reference
    Falls back to simple roster list if detailed stats unavailable
    """
    url = f"https://www.basketball-reference.com/teams/{team_info['url']}/2025.html"
    
    print(f"  Fetching {team_name} from {url}...")
    
    try:
        headers = {
            'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36'
        }
        response = requests.get(url, headers=headers, timeout=10)
        response.raise_for_status()
        
        soup = BeautifulSoup(response.content, 'html.parser')
        
        # Find roster table
        roster_table = soup.find('table', {'id': 'roster'})
        if not roster_table:
            print(f"  ⚠ Roster table not found for {team_name}")
            return []
        
        players = []
        rows = roster_table.find('tbody').find_all('tr')
        
        for row in rows:
            try:
                # Skip header rows
                if row.get('class') and 'thead' in row.get('class'):
                    continue
                
                # Extract player data
                name_cell = row.find('th', {'data-stat': 'player'})
                if not name_cell:
                    continue
                
                name = name_cell.text.strip()
                if not name:
                    continue
                
                # Parse name
                name_parts = name.split()
                if len(name_parts) < 2:
                    continue
                
                first_name = name_parts[0]
                last_name = ' '.join(name_parts[1:])
                
                # Get position
                pos_cell = row.find('td', {'data-stat': 'pos'})
                position = pos_cell.text.strip() if pos_cell else 'SF'
                position = POS_MAP.get(position, 'SF')
                
                # Get height
                height_cell = row.find('td', {'data-stat': 'height'})
                height = parse_height_to_meters(height_cell.text.strip()) if height_cell else 2.0
                
                # Get weight
                weight_cell = row.find('td', {'data-stat': 'weight'})
                weight = parse_weight_to_kg(weight_cell.text.strip()) if weight_cell else 90
                
                # Get jersey number
                number_cell = row.find('th', {'data-stat': 'number'})
                jersey_number = int(number_cell.text.strip()) if number_cell and number_cell.text.strip().isdigit() else 0
                
                # Estimate attributes based on typical NBA player by position
                base_attrs = {
                    'PG': {'pace': 14, 'shooting': 13, 'ball_control': 16, 'defense': 12, 'physical': 10, 'technical': 15, 'ability': 65},
                    'SG': {'pace': 13, 'shooting': 16, 'ball_control': 13, 'defense': 13, 'physical': 12, 'technical': 13, 'ability': 65},
                    'SF': {'pace': 12, 'shooting': 14, 'ball_control': 12, 'defense': 14, 'physical': 14, 'technical': 14, 'ability': 65},
                    'PF': {'pace': 11, 'shooting': 12, 'ball_control': 10, 'defense': 15, 'physical': 16, 'technical': 12, 'ability': 65},
                    'C': {'pace': 9, 'shooting': 11, 'ball_control': 8, 'defense': 16, 'physical': 18, 'technical': 11, 'ability': 65}
                }
                
                attrs = base_attrs.get(position, base_attrs['SF'])
                
                # Add some randomness for variety
                import random
                attrs = {k: max(1, min(20, v + random.randint(-2, 2))) for k, v in attrs.items() if k != 'ability'}
                attrs['ability'] = base_attrs[position]['ability'] + random.randint(-5, 10)
                
                player = {
                    'first_name': first_name,
                    'last_name': last_name,
                    'position': position,
                    'height': height,
                    'weight': weight,
                    'jersey_number': jersey_number,
                    'pace': attrs['pace'],
                    'shooting': attrs['shooting'],
                    'ball_control': attrs['ball_control'],
                    'defense': attrs['defense'],
                    'physical': attrs['physical'],
                    'technical': attrs['technical'],
                    'current_ability': attrs['ability'],
                    'potential_ability': min(99, attrs['ability'] + 5)
                }
                
                players.append(player)
                
            except Exception as e:
                print(f"    ⚠ Error parsing player row: {e}")
                continue
        
        print(f"  ✓ Scraped {len(players)} players for {team_name}")
        return players
        
    except Exception as e:
        print(f"  ✗ Error fetching {team_name}: {e}")
        return []

def import_nba_rosters_to_db(db_path, limit_teams=None):
    """Import scraped NBA rosters to SQLite database"""
    
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
    
    total_teams = 0
    total_players = 0
    
    teams_to_process = list(NBA_TEAMS.items())
    if limit_teams:
        teams_to_process = teams_to_process[:limit_teams]
    
    for team_name, team_info in teams_to_process:
        print(f"\n{team_name} ({team_info['conf']} Conference)")
        
        # Check if team already exists
        cursor.execute('SELECT id FROM teams WHERE name = ?', (team_name,))
        existing = cursor.fetchone()
        
        if existing:
            print(f"  ⚠ {team_name} already exists in database, updating...")
            team_id = existing[0]
            cursor.execute('''
                UPDATE teams SET conference = ?, prestige = ? WHERE id = ?
            ''', (team_info['conf'], team_info['prestige'], team_id))
            # Delete old players
            cursor.execute('DELETE FROM players WHERE team_id = ?', (team_id,))
        else:
            # Insert new team
            cursor.execute('''
                INSERT INTO teams (name, conference, prestige, shooting, defense, pace)
                VALUES (?, ?, ?, 14, 14, 12)
            ''', (team_name, team_info['conf'], team_info['prestige']))
            team_id = cursor.lastrowid
            total_teams += 1
        
        # Scrape roster
        players = scrape_team_roster_simple(team_name, team_info)
        
        if not players:
            print(f"  ⚠ No players found for {team_name}, skipping...")
            conn.rollback()
            continue
        
        # Insert players
        for idx, player in enumerate(players):
            player_id = f"nba_{team_name.replace(' ', '_').lower()}_{idx+1}"
            
            try:
                cursor.execute('''
                    INSERT INTO players (
                        player_id, first_name, last_name, team_id, position,
                        height, weight, jersey_number,
                        pace, shooting, ball_control, defense, physical, technical,
                        current_ability, potential_ability,
                        active, injured, year_in_school
                    )
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    player_id, player['first_name'], player['last_name'], team_id, player['position'],
                    player['height'], player['weight'], player.get('jersey_number', idx + 1),
                    player['pace'], player['shooting'], player['ball_control'],
                    player['defense'], player['physical'], player['technical'],
                    player['current_ability'], player['potential_ability'],
                    1, 0, 4  # active=1, injured=0, year=4 (veteran)
                ))
                total_players += 1
            except Exception as e:
                print(f"    ✗ Error inserting {player['first_name']} {player['last_name']}: {e}")
        
        conn.commit()
        print(f"  ✓ Imported {len(players)} players")
        
        # Be respectful with rate limiting
        time.sleep(2)
    
    conn.close()
    
    print(f"\n{'='*60}")
    print(f"✓ NBA roster scraping complete!")
    print(f"  Teams processed: {total_teams}")
    print(f"  Total players imported: {total_players}")
    print(f"{'='*60}")

if __name__ == "__main__":
    import sys
    
    db_path = sys.argv[1] if len(sys.argv) > 1 else "../../../bm-data/basketball_manager.db"
    
    print("╔" + "="*58 + "╗")
    print("║" + " "*8 + "NBA Roster Scraper - Basketball Manager" + " "*9 + "║")
    print("║" + " "*12 + "Importing 2024-25 NBA Rosters" + " "*16 + "║")
    print("╚" + "="*58 + "╝\n")
    
    # Import all 30 teams (or use 3 for testing)
    limit = 30  # Set to 3 for testing, 30 for full import
    import_nba_rosters_to_db(db_path, limit_teams=limit)
