# Basketball Manager - Modding Guide

## Custom leagues (CSV files)

Without files every new career gets its own made-up league (30 invented clubs and players, same seed = same league). Custom files replace it. Each career keeps its league and its autosave (written at the start of every season) in one database file; resuming returns to the start of the last season.

A custom league is a folder with two UTF-8 CSV files. In the career wizard's league step choose **Custom league from my own files**, enter the folder path and press **Check league files**. Errors are reported with file name and line number. **Create template files** writes an example folder. A ready-made example is in `bm-data/leagues/nba-2026-27/`.

### `teams.csv`

| Column | Required | Notes |
| --- | --- | --- |
| `name` | yes | Unique team name |
| `conference` | yes | `Eastern` or `Western` |
| `prestige`, `pace`, `shooting`, `defense`, `rebounding`, `ball_handling` | no | 0-20, default 10 |
| `away_color`, `third_color` | no | Kit colours like `#552583`. Home kits are always white, the away kit uses `away_color`, the third kit `third_color` (black if empty). Teams without colours get a colour made from their name |

Rules: 16-40 teams and at least 8 teams per conference.

### `players.csv`

| Column | Required | Notes |
| --- | --- | --- |
| `team` | yes | Must match a `name` in `teams.csv` |
| `first_name`, `last_name` | yes | |
| `position` | yes | `PG`, `SG`, `SF`, `PF` or `C` |
| `current_ability` | yes | 40-99, the overall rating |
| `potential_ability` | no | 40-99, defaults to `current_ability` |
| `jersey_number` | no | 0-99 |
| `birth_date` | no | `YYYY-MM-DD`; decides age and development |
| `height` / `weight` | no | Metres (1.6-2.4) / kilograms (60-180) |
| `pace`, `shooting`, `ball_control`, `defense`, `physical`, `technical` | no | 1-20 |

Rules: 8-20 players per team (15 recommended).

### Draft class file (`draft-YYYY.csv`)

Same columns as `players.csv` but without `team` and `jersey_number`: `first_name,last_name,position,birth_date,height,weight,pace,shooting,ball_control,defense,physical,technical,current_ability,potential_ability`. Only `first_name`, `last_name`, `position` and `current_ability` are required; at most 300 rows. List prospects best first. The draft has two rounds (60 picks in a 30-team league); a shorter file is topped up with made-up players, extra rows go undrafted.

Every new season (and when a career starts) the game asks which class joins the league in the coming June draft: enter the path to a file (for example `draft-2027.csv`) and press **Load draft file**, or press **Let the game make up the class**. Errors are listed with line numbers. Assistants on vacation always let the game make up the class. `bm-data/leagues/nba-2026-27/draft-2027.csv` holds the 2027 class (60 prospects from Tankathon's big board).

To rebuild the file from Tankathon's current big board (the next draft): `python3 bm-tools/player-generator/build_draft_class_csv.py draft-2027.csv`.

### Rebuilding the NBA file from current rosters

`python3 bm-tools/player-generator/build_nba_league_csv.py <output folder>` downloads current rosters from ESPN and rates players from last season's statistics. Nikola Jokic (98), Luka Doncic (97) and Victor Wembanyama (96) are pinned as the three best; everyone else is capped at 94.

Real player names are trademarked/likeness material; check the legal position before shipping a build that contains them.

## Modding System Architecture

The Basketball Manager modding system allows complete customization through:

### 1. **Mod Structure**

```
my-mod/
├── mod.json                    # Mod metadata
├── rules/
│   ├── schedule.lua           # Custom scheduling
│   └── playoffs.lua           # Tournament rules
├── data/
│   ├── teams.json             # Team definitions
│   ├── players.json           # Player overrides
│   └── conferences.json       # Conference setup
└── assets/
    ├── logos/
    └── uniforms/
```

### 2. **mod.json Format**

```json
{
  "name": "NBA 2024-25 Mod",
  "version": "1.0.0",
  "author": "Your Name",
  "description": "Complete NBA mod with all teams and players",
  "game_version": "1.0.0",
  "priority": 100,
  "enabled": true,
  "dependencies": [],
  "overrides": {
    "ligas": ["nba"],
    "teams": ["nba_2024"],
    "players": ["nba_2024"]
  }
}
```

### 3. **File Types Supported**

#### JSON Format
```json
{
  "teams": [
    {
      "id": "lakers",
      "name": "Los Angeles Lakers",
      "city": "Los Angeles",
      "conference": "Western",
      "players": ["lebron_james", "anthony_davis"]
    }
  ]
}
```

#### CSV Import
```csv
player_id,first_name,last_name,position,height,weight,pace,shooting,defense
lebron_james,LeBron,James,SF,2.06,113,18,17,16
```

#### Lua Scripts
```lua
-- Custom league scheduling
function customSchedule(teams)
    local schedule = {}
    for i = 1, #teams do
        for j = i+1, #teams do
            table.insert(schedule, {
                home = teams[i],
                away = teams[j]
            })
        end
    end
    return schedule
end
```

### 4. **Mod Manager UI**

- **Enable/Disable mods** in-game
- **Prioritize** mod loading order
- **Validate** mod compatibility
- **Hot-reload** Lua scripts without restart

### 5. **Example: NBA Mod**

```json
{
  "id": "nba_2024",
  "teams": 30,
  "overrides": {
    "schedule": "nba_schedule.lua",
    "draft": "nba_draft.lua",
    "salary_cap": true
  }
}
```

### 6. **Data Validation**

Mods are automatically validated for:
- Schema compliance
- Missing required fields
- Circular references
- Duplicate IDs
- Version compatibility

### 7. **Mod API Functions** (Lua)

```lua
-- Available in mod scripts

-- Logging
LogMessage(message)
LogError(error)

-- Database access
GetTeam(team_id)
GetPlayer(player_id)
ModifyAttribute(player_id, attribute, value)

-- Schedule management
GenerateSchedule(season, format)
GeneratePlayoffs(season, teams)

-- Simulation control
SimulatePossession()
GetMatchState()
```

## Creating Your First Mod

1. Copy `mods/example-mod/` to `mods/my-mod/`
2. Edit `mod.json` with your details
3. Add data in `data/` directory
4. Add scripts in `rules/` directory
5. Load in-game via Mod Manager

## Sharing Mods

1. Package as `.bm-mod` (ZIP with manifest)
2. Upload to Basketball Manager Mod Community
3. Community can rate and download

## Advanced: Database Overrides

Direct SQLite modifications:

```lua
-- Override player attributes
UPDATE players 
SET pace = 19, shooting = 20, defense = 18 
WHERE player_id = 'player_123'
```

## Troubleshooting

- **Mod won't load**: Check `mod.json` syntax
- **Data conflicts**: Review priority order
- **Lua errors**: Check game logs
- **Performance issues**: Validate asset sizes
