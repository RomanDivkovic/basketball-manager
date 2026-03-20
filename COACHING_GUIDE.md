# Basketball Manager - Coaching Callouts & Season Mode Guide

## Overview

You now have a fully playable basketball simulation game with:
- **Interactive coaching callouts** - Control team tactics during games
- **Season mode** - Manage a team through a full NCAA season  
- **Enhanced pause menu** - Make real-time strategic decisions
- **Dynamic tactical responses** - Immediate game impact from coach commands

## New Features

### 1. Coaching Callouts System ✅

During any interactive game, you can issue coaching commands that immediately affect how your team plays. The coach can:

#### How to Activate
1. Start an interactive game:
```bash
./bm-tools/match-simulator/MatchSimulator "Duke" "North Carolina" ../bm-data/basketball_manager.db 1 interactive
```

2. Press `p` + ENTER during play every few possessions (checks every 3 possessions)
3. Select option `6. Coach Callout` from the pause menu
4. Choose which team (1 for home, 2 for away)
5. Select your coaching command (1-4):

#### Available Coaching Commands

| Command | Effect | Duration |
|---------|--------|----------|
| **1. MORE OFFENSIVE** | Team shifts to 3-point heavy strategy, reduces defensive intensity | 8 possessions |
| **2. MORE DEFENSIVE** | Team activates full-court press, defensive focus increases | 8 possessions |
| **3. MANAGE TIME** | Team focuses on inside scoring, uses zone defense to control clock | 8 possessions |
| **4. KEEP GOING** | Morale boost, team maintains current strategy at peak intensity | 8 possessions |

#### Example In-Game Coaching

```
╔════════════════════════════════════════════════════════════════╗
║                    🏀 COACHING CALLOUT 🏀                      ║
╠════════════════════════════════════════════════════════════════╣
║  Duke - Asking team to play more OFFENSIVELY!                  ║
║  Effect duration: 8 possessions                                 ║
╚════════════════════════════════════════════════════════════════╝
```

### 2. Season Mode ✅

Play an entire NCAA season as a coach managing your team.

#### How to Start Season Mode
```bash
./bm-tools/match-simulator/MatchSimulator season ../bm-data/basketball_manager.db 6
```

#### Season Mode Features
- **Team Selection** - Choose from all NCAA teams to manage
- **Game Schedule** - Auto-generated conference and non-conference schedule (~30 games)
- **Live Progress** - Track wins/losses and statistics in real-time
- **Standing Updates** - View standings after each game
- **Interactive Play** - Each game can be played manually or auto-simulated

#### Example Season Flow
```
╔════════════════════════════════════════════════════════════════╗
║              BASKETBALL MANAGER - SEASON MODE                  ║
╚════════════════════════════════════════════════════════════════╝

Select your team to manage:
════════════════════════════════════════════════════════════════
 1. Alabama
 2. Appalachian State
 3. Arizona
 ...
 [select your team]

✓ You are managing: Duke

2025 NCAA Season Generated!
Total games to play: 30

[Game 1/30] Duke vs North Carolina
Duke 87 - 82 North Carolina

Continue to next game? (y/n):
```

## Enhanced Pause Menu

When paused during an interactive game, you now have access to:

```
╔════════════════════════════════════════════════════════════════╗
║                        ⏸️  GAME PAUSED                         ║
╠════════════════════════════════════════════════════════════════╣
║  Q1 | 11:43 | Duke 15 - 12 North Carolina                     ║
╠════════════════════════════════════════════════════════════════╣
║  1. Resume Game                                                ║
║  2. Change Speed (Current: x1)                                 ║
║  3. Call Timeout (Home: 3 | Away: 3)                           ║
║  4. Make Substitution                                          ║
║  5. Show Player Stats                                          ║
║  6. Coach Callout (More Offense/Defense/Manage Time)           ║ ← NEW!
║  7. Quit Game                                                  ║
╚════════════════════════════════════════════════════════════════╝
```

## Usage Examples

### Example 1: Interactive Game with Coaching Decisions

```bash
# Start a real-time game
cd /Users/romandivkovic/repos/basketball-manager/build
./bm-tools/match-simulator/MatchSimulator "Duke" "North Carolina" ../bm-data/basketball_manager.db 1 interactive
```

**Gameplay flow:**
1. Watch the game unfold in real-time (Q1, Q2, Q3, Q4)
2. Every few possessions, press 'p' + ENTER to pause
3. Select "Coach Callout" and make strategic decisions
4. Each callout immediately affects the team's play style
5. Complete the full 48-minute game with coaching control

### Example 2: Faster Game with Coaching (2x speed)

```bash
./bm-tools/match-simulator/MatchSimulator "Duke" "North Carolina" ../bm-data/basketball_manager.db 2 interactive
```

Game plays at 2x faster pace, still allows coaching callouts.

### Example 3: Full Season Management

```bash
./bm-tools/match-simulator/MatchSimulator season ../bm-data/basketball_manager.db 6
```

**Season flow:**
1. Select your team
2. Game 1 auto-simulates at 6x speed
3. View results and standings
4. Continue to next game
5. Track entire season standings

### Example 4: Single Fast Game (No Coaching Pause)

```bash
./bm-tools/match-simulator/MatchSimulator "Duke" "North Carolina" ../bm-data/basketball_manager.db 6
```

Fastest simulation, no interactive options - great for testing or running many games.

## Tactical System Details

### How Coaching Callouts Affect Play

Each callout maps to specific offensive and defensive strategies:

**MORE_OFFENSIVE:**
- Offensive: THREE_POINT_HEAVY (70% 3PT attempts)
- Defense: BALANCED_DEFENSE
- Result: High-scoring, pace-and-space game

**MORE_DEFENSIVE:**
- Offensive: BALANCED (50% 2PT, 50% 3PT)
- Defense: PRESS (full-court pressure)
- Result: Defensive grind, turnovers, fast breaks

**MANAGE_TIME:**
- Offensive: INSIDE_FOCUSED (low-post focus, 2PT heavy)
- Defense: ZONE_2_3 (classic zone)
- Result: Controlled, clock-managed game

**KEEP_GOING:**
- Offensive: UNCHANGED (same as current)
- Defense: UNCHANGED (same as current)
- Result: Morale boost, maintain current pace

### Momentum Decay

Each coaching callout affects team play for **8 possessions**, then gradually fades as the team reverts to baseline strategy. This creates strategic depth - you must time your callouts wisely:

- **Early game:** Set tone with defensive or offensive callout
- **Late game:** Use callouts to adjust to opponent tendencies
- **Clutch time:** "KEEP GOING" for morale when close in score

## Building & Running From Source

```bash
# Navigate to the repository
cd /Users/romandivkovic/repos/basketball-manager

# Build (from scratch)
rm -rf build && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 ..
make -j4

# Run interactive game
./bm-tools/match-simulator/MatchSimulator "Duke" "North Carolina" ../bm-data/basketball_manager.db 1 interactive

# Run season mode
./bm-tools/match-simulator/MatchSimulator season ../bm-data/basketball_manager.db 6
```

## Technical Architecture

### New Components Added

1. **CoachingCallout Enum** (MatchEngine.h)
   - MORE_OFFENSIVE, MORE_DEFENSIVE, MANAGE_TIME, KEEP_GOING

2. **Coaching State Variables** (MatchEngine.cpp)
   - homeCoachingMomentum, awayCoachingMomentum
   - homeLastCallout, awayLastCallout
   - COACHING_MOMENTUM_DURATION = 8 possessions

3. **ApplyCoachingCallout Method** (MatchEngine.cpp)
   - Applies tactical changes via TacticalManager
   - Sets coaching momentum duration
   - Displays coaching message to user

4. **Pause Menu Enhancement** (MatchEngine.cpp)
   - Added case 6 for coaching callouts
   - Integrated with team selection
   - Calls ApplyCoachingCallout with user choice

5. **Season Mode Support** (simulate_match.cpp)
   - LoadAllTeams function
   - Season mode detection and initialization
   - Integration with SeasonManager class

## Files Modified

```
bm-core/
├── include/
│   └── engine/
│       ├── MatchEngine.h         [+CoachingCallout enum, state vars]
│       └── SeasonManager.h       [already existed]
├── src/
│   └── engine/
│       ├── MatchEngine.cpp       [+ApplyCoachingCallout, pause menu]
│       └── SeasonManager.cpp     [already existed]
├── CMakeLists.txt               [+SeasonManager.cpp in sources]

bm-tools/
└── match-simulator/
    └── simulate_match.cpp       [+season mode, LoadAllTeams]
```

## Performance & Compatibility

- **Compile Time:** ~30 seconds (with -j4 on M1 Mac)
- **Single Game:** 1-2 seconds at 6x speed, 1-2 minutes at real-time
- **Season (30 games):** ~1-2 minutes at 6x speed
- **Platform:** macOS M1 (ARM64) - fully compatible
- **Dependencies:** SQLite3, Lua 5.4, C++20

## Next Steps / Future Enhancement Ideas

1. **Playoff Mode** - SeasonManager already has playoff structure
2. **Season Stats Tracking** - Full player stats across season
3. **Recruiting/Free Agency** - Manage roster between seasons
4. **Advanced Coaching AI** - Computer coaches making callouts
5. **Multiple Season Save Files** - Persistent career mode
6. **Detailed Stat Sheets** - Full box scores and analytics
7. **Conference Tournaments** - NCAA tournament simulation

## Troubleshooting

**"No pause menu appearing"**
- Make sure you run with `interactive` or `i` flag
- Press 'p' + ENTER exactly (case-sensitive, 'p' only)

**"Coaching callout has no effect"**
- Callouts take effect on the NEXT possession
- Effect lasts exactly 8 possessions then fades

**"Season mode crashes when selecting team"**
- Ensure database file path is correct
- Make sure database has teams and rosters loaded

## Questions & Support

Refer to the main Basketball Manager documentation:
- `README.md` - Project overview
- `GAME_READY.md` - Interactive features
- `docs/ARCHITECTURE.md` - System designssdss
m v   