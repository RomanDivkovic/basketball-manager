# Basketball Manager - Architecture & Design

## System Overview

```
┌──────────────────────────────┐
│     Unity UI (C#)            │
│  - Menus & Navigation        │
│  - Stats Dashboards          │
│  - Mod Manager               │
└──────────┬───────────────────┘
           │ (P/Invoke)
           ↓
┌──────────────────────────────┐
│  C++ Simulation Core         │
│  - Match Engine              │
│  - AI Systems                │
│  - Statistics Engine         │
│  - Database Manager          │
└──────────┬───────────────────┘
           │
┌──────────────────────────────┐
│  Lua Rules Engine            │
│  - League Schedules          │
│  - Mod Scripts               │
│  - Game Logic Customization  │
└──────────┬───────────────────┘
           │
┌──────────────────────────────┐
│  SQLite Database             │
│  - Players, Teams, Stats     │
│  - Matches, Seasons          │
│  - Contracts, Recruiting     │
└──────────────────────────────┘
```

## Module Breakdown

### C++ Core (`bm-core/`)

#### Engine Module
- **MatchEngine**: Possession-based match simulation
  - Play selection (offensive sets)
  - Defense logic (switch, drop, hedge, zone)
  - Scoring calculations
  - Fatigue & stamina tracking
  - Foul tracking
  - Substitution handling
  - Live commentary generation

- **SimulationManager**: Season-level management
  - Season initialization
  - Round scheduling
  - Player progression
  - Injury simulation
  - Draft handling

#### AI Module
- **AICoordinator**: High-level team AI
  - Lineup decisions
  - Substitution strategy
  - Timeout decisions
  - Roster management

- **TacticAI**: In-match tactical decisions
  - Offensive play selection
  - Defensive adjustment
  - Mid-match tactical shifts

#### Statistics Module
- **StatisticsEngine**: Advanced basketball metrics
  - Four Factors (eFG%, TOV%, REB%, FT%)
  - Offensive/Defensive Rating
  - Pace calculation
  - Player Efficiency Rating (PER)
  - True Shooting %

#### Database Module
- **DatabaseManager**: SQLite operations
  - Connection management
  - Query execution
  - Schema initialization
  - Transaction handling

- **DatabaseLoader**: Data import
  - CSV parsing
  - JSON import
  - Team/Player data loading
  - Attribute assignment

#### Lua Integration
- **LuaRulesEngine**: Lua VM management
  - Script loading
  - Mod script execution
  - Function callbacks
  - Error handling

- **LuaBindings**: C++ <-> Lua interface
  - Function exports to Lua
  - Data marshalling
  - State management

#### Bindings Module
- **UnityBridge**: C# <-> C++ interop
  - P/Invoke function declarations
  - Memory marshalling
  - Error propagation

### Database Schema

#### Core Tables
- `players`: All player data with attributes
- `teams`: Team information and roster
- `contracts`: Employment/scholarship contracts
- `statistics`: Career statistics
- `matches`: Game results
- `seasons`: Season tracking

#### Attributes (0-20 scale)
- `pace`: Movement speed and acceleration
- `shooting`: Overall shooting ability
- `ball_control`: Dribbling and ball handling
- `defense`: Defensive positioning and capability
- `physical`: Strength and athleticism
- `technical`: Basketball IQ and positioning

### Lua Scripts

#### NCAA Rules (`ncaa_rules.lua`)
- Conference structure
- Season scheduling
- Playoff format
- Eligibility checking
- NIL budget management
- Recruiting rules

#### Recruiting (`recruiting.lua`)
- Recruit targeting
- Offer management
- Commitment tracking
- Transfer portal handling

### Modding System

#### Mod Structure
```
mod-name/
├── mod.json              # Metadata & manifest
├── data/
│   ├── teams.json
│   ├── players.json
│   └── conferences.json
├── rules/
│   ├── schedule.lua
│   └── playoffs.lua
└── assets/
    ├── logos/
    └── uniforms/
```

#### Override Priority
1. Core game data
2. Loaded mods (by priority)
3. Runtime modifications

#### Validation
- Schema compliance
- ID uniqueness
- Reference validity
- Version compatibility

## Data Flow Example: Match Simulation

```
1. Unity UI: "Simulate Match"
   ↓
2. C#: BMUnityBridge.StartMatch("home_id", "away_id")
   ↓
3. C++: MatchEngine::InitializeMatch()
   ↓
4. Load teams/players from database
   ↓
5. Initialize match state
   ↓
6. Loop: SimulatePossession() x ~96
   ├─ PlaySelection() via AICoordinator
   ├─ DefenseLogic() via TacticAI
   ├─ Calculate play outcome
   ├─ Update stats
   ├─ Generate commentary
   ├─ Check fouls/substitutions
   └─ Lua callbacks for custom logic
   ↓
7. Match complete
   ↓
8. Save results to database
   ↓
9. Return commentary to Unity UI
```

## Performance Considerations

### C++ Optimizations
- Multi-threading for match simulation
- Cached statistics calculations
- Binary database format for fast loading

### Memory Management
- Smart pointers for automatic cleanup
- Pool allocation for frequent objects
- Streaming data loading

### Build Configuration
- Release builds with optimizations
- ARM64 native compilation for M1
- Link-time optimization enabled

## Testing Strategy

### Unit Tests
- Statistics calculations
- Database operations
- Lua script execution

### Integration Tests
- Full match simulation
- Season progression
- Mod loading and validation

### Performance Tests
- Match simulation speed
- Database query performance
- Memory usage profiling
