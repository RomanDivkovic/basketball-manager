# Basketball Manager Engine (bm-core)

C++ simulation engine for Basketball Manager game.

## Structure

```
bm-core/
├── include/                 # Public headers
│   ├── BasketballManager.h # Main API header
│   ├── common/
│   │   └── Types.h        # Common data structures
│   ├── engine/            # Match simulation
│   ├── ai/                # AI systems
│   ├── db/                # Database operations
│   ├── lua/               # Lua integration
│   ├── stats/             # Statistics calculations
│   └── bindings/          # C# interop
└── src/                   # Implementation
    ├── main.cpp           # Entry point
    ├── engine/
    ├── ai/
    ├── db/
    ├── lua/
    ├── stats/
    └── bindings/
```

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

## Output

- **libBMCore.dylib** - Main shared library (for Unity)
- **bm-core** - Standalone executable

## Public API

### Main Header
```cpp
#include <BasketballManager.h>

using namespace bm;
```

### Core Classes
- `MatchEngine` - Basketball match simulation
- `SimulationManager` - Season management
- `DatabaseManager` - Game data persistence
- `LuaRulesEngine` - Custom game logic
- `AICoordinator` - Team AI decisions
- `StatisticsEngine` - Basketball statistics

### Data Types
- `Player` - Player information
- `Team` - Team information
- `MatchState` - Current game state
- `Contract` - Player contracts
- `StatLine` - Game statistics

## Dependencies

- **SQLite3** - Game database
- **Lua 5.4** - Scripting engine
- **C++20** - Language standard

## Features

✅ Possession-based match simulation
✅ Advanced basketball statistics (4-Factors, PER, ORtg/DRtg)
✅ Lua-based rule engine for customization
✅ SQLite database for game data
✅ AI-driven coaching decisions
✅ Player progression and development
✅ NCAA recruiting system
✅ Modding framework

## Next Steps

1. Load NCAA 2025 data
2. Expand match engine simulation
3. Implement AI tactical decisions
4. Connect to Unity via P/Invoke
5. Build modding system UI

See `../../GETTING_STARTED.md` for detailed setup instructions.
