#!/usr/bin/env python3
"""
NCAA Basketball Player Roster Scraper
Scrapes player data from sports-reference.com for all D1 teams
"""

import requests
from bs4 import BeautifulSoup
import csv
import time
import json
from pathlib import Path

# Base URL
BASE_URL = "https://www.sports-reference.com"
SEASON = "2026"  # 2025-26 season

def load_teams():
    """Load team list from our existing NCAA CSV"""
    teams = []
    csv_path = Path(__file__).parent.parent.parent / "bm-data" / "ncaa-source" / "cbb25.csv"
    
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Convert team name to Sports-Reference slug
            # e.g., "Duke" -> "duke", "North Carolina" -> "north-carolina"
            team_slug = row['Team'].lower().replace(' ', '-').replace('.', '').replace('&', '')
            teams.append({
                'name': row['Team'],
                'conference': row['CONF'],
                'slug': team_slug
            })
    
    return teams

def scrape_roster(team_slug):
    """Scrape roster for a specific team"""
    url = f"{BASE_URL}/cbb/schools/{team_slug}/men/{SEASON}.html"
    
    try:
        print(f"Scraping {team_slug}... ", end='', flush=True)
        response = requests.get(url, headers={
            'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36'
        })
        
        if response.status_code != 200:
            print(f"ERROR {response.status_code}")
            return None
        
        soup = BeautifulSoup(response.content, 'html.parser')
        
        # Find the roster table - it's in a comment block that needs to be extracted
        roster_table = None
        comments = soup.find_all(string=lambda text: isinstance(text, str) and 'roster' in text)
        for comment in comments:
            if '<table' in str(comment) and 'id="roster"' in str(comment):
                # Parse the comment as HTML
                roster_soup = BeautifulSoup(str(comment), 'html.parser')
                roster_table = roster_soup.find('table', {'id': 'roster'})
                break
        
        # If not in comments, try normal search
        if not roster_table:
            roster_table = soup.find('table', {'id': 'roster'})
        
        if not roster_table:
            print("No roster table found")
            return None
        
        players = []
        rows = roster_table.find('tbody').find_all('tr')
        
        for row in rows:
            # Skip header rows
            if 'thead' in row.get('class', []):
                continue
            
            cols = row.find_all('td')
            if len(cols) < 6:
                continue
            
            # Extract player data
            name_cell = cols[0]
            player_link = name_cell.find('a')
            if not player_link:
                continue
            
            name = player_link.text.strip()
            player_url = player_link.get('href', '')
            
            # Get position and year from attributes
            number = cols[1].text.strip() if len(cols) > 1 else ""
            year = cols[2].text.strip() if len(cols) > 2 else ""  # FR, SO, JR, SR
            position = cols[3].text.strip() if len(cols) > 3 else ""  # G, F, C
            height = cols[4].text.strip() if len(cols) > 4 else ""
            weight = cols[5].text.strip() if len(cols) > 5 else ""
            
            # Parse stats from last column (e.g., "23.0 Pts, 9.9 Reb, 3.8 Ast")
            stats_text = cols[-1].text.strip() if len(cols) > 7 else ""
            ppg = rpg = apg = 0.0
            
            if stats_text:
                parts = stats_text.split(',')
                for part in parts:
                    part = part.strip()
                    if 'Pts' in part:
                        ppg = float(part.replace('Pts', '').strip())
                    elif 'Reb' in part:
                        rpg = float(part.replace('Reb', '').strip())
                    elif 'Ast' in part:
                        apg = float(part.replace('Ast', '').strip())
            
            player = {
                'name': name,
                'number': number,
                'year': year,
                'position': position,
                'height': height,
                'weight': weight,
                'ppg': ppg,
                'rpg': rpg,
                'apg': apg,
                'player_url': player_url
            }
            
            players.append(player)
        
        print(f"✓ {len(players)} players")
        return players
        
    except Exception as e:
        print(f"ERROR: {e}")
        return None

def main():
    """Main scraper function"""
    print("=" * 80)
    print("NCAA Basketball Player Roster Scraper")
    print("=" * 80)
    print()
    
    # Load teams
    print("Loading team list...")
    teams = load_teams()
    print(f"Found {len(teams)} teams\n")
    
    # Create output directory
    output_dir = Path(__file__).parent.parent.parent / "bm-data" / "ncaa-rosters"
    output_dir.mkdir(exist_ok=True)
    
    # Scrape all rosters
    all_players = []
    failed_teams = []
    
    for i, team in enumerate(teams, 1):
        print(f"[{i}/{len(teams)}] ", end='')
        
        players = scrape_roster(team['slug'])
        
        if players:
            for player in players:
                player['team'] = team['name']
                player['conference'] = team['conference']
                all_players.append(player)
        else:
            failed_teams.append(team['name'])
        
        # Be nice to the server - rate limit (3 seconds to avoid 429 errors)
        time.sleep(3)
        
        # Save progress every 50 teams
        if i % 50 == 0:
            save_progress(all_players, output_dir)
    
    # Save final results
    print(f"\n{'=' * 80}")
    print(f"Scraping complete!")
    print(f"Total players scraped: {len(all_players)}")
    print(f"Failed teams: {len(failed_teams)}")
    if failed_teams:
        print(f"Failed: {', '.join(failed_teams[:10])}" + (" ..." if len(failed_teams) > 10 else ""))
    
    # Save to CSV
    csv_file = output_dir / "ncaa_rosters_2025-26.csv"
    print(f"\nSaving to {csv_file}...")
    
    with open(csv_file, 'w', newline='') as f:
        if all_players:
            writer = csv.DictWriter(f, fieldnames=all_players[0].keys())
            writer.writeheader()
            writer.writerows(all_players)
    
    # Also save as JSON
    json_file = output_dir / "ncaa_rosters_2025-26.json"
    with open(json_file, 'w') as f:
        json.dump(all_players, f, indent=2)
    
    print(f"✓ CSV: {csv_file}")
    print(f"✓ JSON: {json_file}")
    print(f"\n{'=' * 80}")

def save_progress(players, output_dir):
    """Save progress to temporary file"""
    temp_file = output_dir / "temp_progress.json"
    with open(temp_file, 'w') as f:
        json.dump(players, f, indent=2)

if __name__ == "__main__":
    main()
