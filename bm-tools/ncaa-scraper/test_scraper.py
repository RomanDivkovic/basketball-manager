#!/usr/bin/env python3
"""Test scraper on just a few teams"""
import sys
sys.path.insert(0, '.')
from scrape_rosters import scrape_roster

# Test on a few teams
test_teams = ['duke', 'houston', 'gonzaga', 'kansas', 'kentucky']

print("Testing roster scraper on sample teams...\n")
for team in test_teams:
    print(f"Testing {team}:")
    players = scrape_roster(team)
    if players:
        print(f"  ✓ Found {len(players)} players")
        if players:
            print(f"  Sample: {players[0]['name']} - {players[0]['position']}, {players[0]['year']}")
    else:
        print(f"  ✗ Failed")
    print()
