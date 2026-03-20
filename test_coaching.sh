#!/bin/bash

# Test script for coaching callout system
# This script simulates a game with coaching callouts

cd /Users/romandivkovic/repos/basketball-manager/build

echo "Testing Coaching Callout System"
echo "================================"
echo ""
echo "This test will:"
echo "1. Start a game in interactive mode"
echo "2. Wait for pause opportunity"
echo "3. Access the coaching menu"
echo "4. Apply a coaching callout"
echo ""
echo "Instructions:"
echo "1. Press 'p' + ENTER when prompted to pause (after a few possessions)"
echo "2. Select option 6 for 'Coach Callout'"
echo "3. Select which team (1 or 2)"
echo "4. Select coaching callout (1-4)"
echo ""
echo "Starting game..."
echo ""

# Run the match simulator in interactive mode
# Duke vs North Carolina at real-time speed
./bm-tools/match-simulator/MatchSimulator "Duke" "North Carolina" ../bm-data/basketball_manager.db 1 interactive
