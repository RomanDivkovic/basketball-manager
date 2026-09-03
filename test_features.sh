#!/bin/bash

# Test script for Basketball Manager - Coaching Callouts & Season Play
# This demonstrates the new features:
# 1. Interactive coaching callouts during games
# 2. Season mode with team management

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
DB_PATH="${BM_TEST_DB:-/tmp/basketball_manager_readable.db}"

if [[ ! -f "$DB_PATH" ]]; then
    python3 "$ROOT_DIR/bm-tools/create-test-db.py" "$DB_PATH" || exit 1
fi

cd "$BUILD_DIR" || exit 1

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
        echo "Starting Lakeview Lions vs Cedar Valley Hawks - INTERACTIVE MODE"
        echo "Press 'p' + ENTER during play to access pause menu"
        echo "Select option 6 for coaching callouts"
        echo ""
        ./bm-tools/match-simulator/MatchSimulator "Lakeview Lions" "Cedar Valley Hawks" "$DB_PATH" 1 interactive
        ;;
    2)
        echo ""
        echo "Starting Lakeview Lions vs Cedar Valley Hawks - SPEED x2"
        echo ""
        ./bm-tools/match-simulator/MatchSimulator "Lakeview Lions" "Cedar Valley Hawks" "$DB_PATH" 2 interactive
        ;;
    3)
        echo ""
        echo "Starting Season Mode - Select your team!"
        echo ""
        ./bm-tools/match-simulator/MatchSimulator season "$DB_PATH" 6
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac
