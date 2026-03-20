#!/bin/bash

# Test script for Basketball Manager - Coaching Callouts & Season Play
# This demonstrates the new features:
# 1. Interactive coaching callouts during games
# 2. Season mode with team management

cd /Users/romandivkovic/repos/basketball-manager/build

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║     Basketball Manager - Coaching & Season Mode Testing        ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "New Features:"
echo "=============="
echo "✓ Coaching Callouts - During pause menu, select option 6"
echo "  - Ask team to be MORE OFFENSIVE"
echo "  - Ask team to be MORE DEFENSIVE"  
echo "  - Ask team to MANAGE TIME (slow down pace)"
echo "  - Ask team to KEEP GOING (morale boost)"
echo ""
echo "✓ Season Mode - Play through an entire season as a coach"
echo "  Usage: ./bm-tools/match-simulator/MatchSimulator season"
echo ""
echo "Test Options:"
echo "============="
echo "1. Single Game with Coaching (Interactive Mode)"
echo "2. Single Game with Coaching (Speed x2)"
echo "3. Season Mode"
echo ""
read -p "Select test (1-3): " choice

case $choice in
    1)
        echo ""
        echo "Starting Duke vs North Carolina - INTERACTIVE MODE"
        echo "Press 'p' + ENTER during play to access pause menu"
        echo "Select option 6 for coaching callouts"
        echo ""
        ./bm-tools/match-simulator/MatchSimulator "Duke" "North Carolina" ../bm-data/basketball_manager.db 1 interactive
        ;;
    2)
        echo ""
        echo "Starting Duke vs North Carolina - SPEED x2"
        echo ""
        ./bm-tools/match-simulator/MatchSimulator "Duke" "North Carolina" ../bm-data/basketball_manager.db 2 interactive
        ;;
    3)
        echo ""
        echo "Starting Season Mode - Select your team!"
        echo ""
        ./bm-tools/match-simulator/MatchSimulator season ../bm-data/basketball_manager.db 6
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac
