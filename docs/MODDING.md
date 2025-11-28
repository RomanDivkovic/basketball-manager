# Basketball Manager - Modding Guide

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
