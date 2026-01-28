# Basketball Manager - Interactive Game Mode

## Overview
The Basketball Manager now features a fully interactive console-based game mode where you can control every aspect of the match in real-time!

## How to Play

### Starting an Interactive Game

```bash
cd build/bm-tools/match-simulator
./MatchSimulator "Team1" "Team2" ../../../bm-data/basketball_manager.db 1 interactive
```

**Parameters:**
- `Team1`: Home team name (e.g., "Boston Celtics")
- `Team2`: Away team name (e.g., "Los Angeles Lakers")
- Database path: Path to basketball_manager.db
- Speed: `1` (real-time), `2`, `3`, `4`, or `6` (fastest)
- Mode: `interactive` or `i` to enable interactive mode

### Game Rules

#### Jump Ball & Possession Arrow
- **Q1**: Jump ball at center court determines first possession
- **Q2 & Q3**: Loser of Q1 tip-off starts with the ball (possession arrow)
- **Q4**: Winner of Q1 tip-off gets possession back
- **Overtime**: New jump ball for each OT period

#### Timeouts
- Each team starts with **3 timeouts per half**
- Timeouts reset at halftime (start of Q3)
- 30-second duration allows substitutions and slight fatigue recovery

### Interactive Controls

During gameplay, you'll see prompts every 3 possessions:
```
[Press 'p' + ENTER to pause, or just ENTER to continue]:
```

Press **p** and then **ENTER** to pause the game.

### Pause Menu Options

When paused, you'll see:

```
╔════════════════════════════════════════════════════════════════╗
║                        ⏸️  GAME PAUSED                         ║
╠════════════════════════════════════════════════════════════════╣
║  Q1 | 11:23 | Boston Celtics 12 - 10 Los Angeles Lakers       ║
╠════════════════════════════════════════════════════════════════╣
║  1. Resume Game                                                ║
║  2. Change Speed (Current: x1)                                 ║
║  3. Call Timeout (Home: 3 | Away: 3)                          ║
║  4. Make Substitution                                          ║
║  5. Show Player Stats                                          ║
║  6. Quit Game                                                  ║
╚════════════════════════════════════════════════════════════════╝
```

#### 1. Resume Game
Continue playing from where you paused.

#### 2. Change Speed
Adjust simulation speed on-the-fly:
- **1x**: Real-time (400ms per possession) - best for watching closely
- **2x**: 2x faster (200ms) - moderate speed
- **3x**: 3x faster (133ms) - quick games
- **4x**: 4x faster (100ms) - very fast
- **6x**: 6x fastest (67ms) - instant results

#### 3. Call Timeout
- Select which team (Home or Away) calls the timeout
- Costs 1 timeout from that team's remaining count
- Players recover **5% fatigue** during timeout
- Opportunity to make substitutions

**Timeout Flow:**
1. Select team calling timeout
2. 30-second timeout begins
3. Slight fatigue recovery for on-court players
4. Option to make substitutions (y/n)
5. If yes, view lineup and bench, then select players to swap

#### 4. Make Substitution
Manually substitute players without calling timeout.

**Steps:**
1. Choose team (Home or Away)
2. View current lineup (players 1-5) with fatigue % and minutes
3. View bench (players 6+) with their fatigue %
4. Select player to take OUT (1-5)
5. Select player to bring IN (6+)
6. Substitution confirmed

**Example:**
```
[CURRENT LINEUP]
  1. Jayson Tatum (Fatigue: 45%, Minutes: 18.2)
  2. Jaylen Brown (Fatigue: 52%, Minutes: 20.1)
  3. Al Horford (Fatigue: 38%, Minutes: 15.3)
  ...

[BENCH]
  6. Derrick White (Fatigue: 10%)
  7. Robert Williams (Fatigue: 15%)
  ...

Player to substitute OUT (1-5): 2
Player to substitute IN (6+): 6

[SUB] Brown → White
```

#### 5. Show Player Stats
View real-time statistics for any team.

**Stats Displayed:**
- **PTS**: Points scored this game
- **REB**: Total rebounds (offensive + defensive)
- **AST**: Assists
- **MIN**: Minutes played
- **FATIGUE**: Current fatigue percentage (0-100%)
- **\***: Indicates currently on court

**Example Output:**
```
╔════════════════════════════════════════════════════════════════╗
║  Boston Celtics                                                ║
╠════════════════════════════════════════════════════════════════╣
║  Player                    PTS  REB  AST  MIN  FATIGUE         ║
╠════════════════════════════════════════════════════════════════╣
║  J. Tatum                  18    5    2  22.3      58% *       ║
║  J. Brown                  14    3    1  20.1      52% *       ║
║  A. Horford                 8    9    1  18.5      42% *       ║
║  D. White                   6    2    3  15.8      38% *       ║
║  R. Williams                4    6    0  13.2      35% *       ║
║  R. 1                       2    1    0   4.5      12%         ║
╚════════════════════════════════════════════════════════════════╝
  (* = currently on court)
```

#### 6. Quit Game
Exit the simulation immediately.

## Game Display

### Play-by-Play Format
Every possession is displayed with:
- **Quarter**: Q1, Q2, Q3, Q4, or OT
- **Game Clock**: Minutes:Seconds remaining
- **Score**: Current home - away score
- **Play Description**: What happened

**Examples:**
```
[Q1 11:43] 0-0   | Horford wins the tip, taps to White!
[Q1 11:26] 0-2   | Anthony finds Reaves for two!
[Q1 10:52] 5-4   | Al finds Horford for a 3!
[Q2 08:15] 24-22 | Davis rejects Tatum!
[Q3 05:30] 56-58 | Brown fouled by James - hits 2/2 at the line.
[Q4 00:45] 98-98 | Tatum for a 3! Clutch shot!
```

### Substitution Logs
Automatic and manual substitutions are displayed:
```
[SUB] Reserve 1 → Jaylen Brown (fatigue 64%)
[SUB] Brown → White
```

### Quarter Summaries
At the end of each quarter:
```
Q1 FINAL: Boston Celtics 28 - 26 Los Angeles Lakers
Q2 FINAL: Boston Celtics 54 - 52 Los Angeles Lakers
```

### Final Score
```
╔════════════════════════════════════════════════════════════════╗
║                     FINAL SCORE                                ║
╠════════════════════════════════════════════════════════════════╣
║  Boston Celtics                   113                          ║
║  Los Angeles Lakers                88                          ║
╚════════════════════════════════════════════════════════════════╝
```

## Strategy Tips

### When to Call Timeouts
- **Momentum Shifts**: Opponent goes on a scoring run
- **Fatigue Management**: Your starters are above 70% fatigue
- **Quarter End**: Preserve timeout before half/end of game
- **Critical Moments**: Close game in Q4, need to draw up a play

### Substitution Strategy
- **Fatigue Threshold**: Sub out players above 65-70% fatigue
- **Minute Management**: Ensure starters don't play 40+ minutes
- **Foul Trouble**: Sub out players with 4+ fouls
- **Matchups**: Bring in defensive specialists or offensive threats
- **Rest Starters**: Give key players breaks in Q2/Q3 blowouts

### Speed Control
- **x1**: Watch important games possession-by-possession
- **x2-x3**: Balanced viewing, can follow action but faster
- **x4-x6**: Sim through blowouts or routine games

## Example Session

```bash
# Start an interactive game
./MatchSimulator "Duke" "North Carolina" ../../../bm-data/basketball_manager.db 1 interactive

# Game starts with Q1 tip-off
[Q1 11:43] 0-0 | Cameron wins the tip, taps to Coleman!

# Watch a few possessions
[Q1 11:26] 0-2 | Bennett finds Martinez for two!
[Q1 11:06] 2-2 | Cameron finds Coleman for two!

# Press 'p' to pause
[Press 'p' + ENTER to pause, or just ENTER to continue]: p

# Pause menu appears - choose option 3 (Call Timeout)
Enter choice: 3

# Call timeout for home team (Duke)
Call timeout for: (1) Duke or (2) North Carolina: 1

# Make substitutions during timeout
Make substitutions during timeout? (y/n): y

# View lineup and make swap
Player to substitute OUT (1-5): 2
Player to substitute IN (6+): 7
[SUB] Moore → Johnson

# Resume and watch Duke with fresh legs!
Enter choice: 1
[RESUMING GAME...]
```

## Features Showcase

### ✅ Realistic Basketball Rules
- Jump ball & possession arrow system
- 3 timeouts per half (reset at halftime)
- 48-minute regulation (12 min quarters)
- Overtime periods (5 minutes each)
- Fatigue system affecting performance

### ✅ Interactive Control
- Pause anytime during action
- Change game speed on-the-fly
- Call strategic timeouts
- Make substitutions
- View live statistics

### ✅ Detailed Play-by-Play
- Every possession logged
- Passes, steals, blocks, rebounds
- 2PT and 3PT shots
- Fouls and free throws
- Turnovers and defensive stops

### ✅ Roster Management
- 12-player rotations
- Automatic roster padding (if team has <12 players)
- Fatigue tracking (0-100%)
- Minutes played management
- Position-based substitutions

## Coming Soon

### Future Enhancements
- 🔜 **AI Coach Suggestions**: Get recommended subs/timeouts
- 🔜 **Play Calling**: Choose offensive/defensive plays
- 🔜 **Injury System**: Players can get injured mid-game
- 🔜 **Advanced Stats**: +/-, TS%, ORtg/DRtg live tracking
- 🔜 **Save/Load Games**: Pause and resume later
- 🔜 **Replay System**: Review key plays
- 🔜 **Multiple Camera Angles**: Different view modes
- 🔜 **Commentary**: Play-by-play announcer

## Troubleshooting

### Game Won't Pause
- Make sure you added `interactive` or `i` as the 5th parameter
- Wait for the pause prompt (every 3 possessions)
- Type `p` and press ENTER

### Substitution Not Working
- Check player is not already on court (IN player)
- Check player is currently playing (OUT player must be 1-5)
- Verify player is not injured

### No Timeouts Available
- Each team only has 3 timeouts per half
- Timeouts reset at halftime (Q3 start)
- Can't call timeout if count is 0

## Have Fun!

This is your chance to coach your favorite teams in real-time! Make strategic decisions, manage your roster, and guide your team to victory. Whether you're simming a quick game at 6x speed or carefully managing every possession at 1x, the Basketball Manager gives you complete control.

**Now get out there and coach!** 🏀🔥
