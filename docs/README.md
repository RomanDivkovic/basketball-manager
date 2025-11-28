-- Basketball Manager Project Structure Documentation

## Directory Structure

```
basketball-manager/
├── CMakeLists.txt                 # Root CMake configuration
├── bm-core/                       # C++ Simulation Engine
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── engine/                # Match simulation
│   │   │   ├── MatchEngine.h
│   │   │   └── SimulationManager.h
│   │   ├── ai/                    # AI decision making
│   │   │   ├── AICoordinator.h
│   │   │   └── TacticAI.h
│   │   ├── db/                    # Database layer
│   │   │   ├── DatabaseManager.h
│   │   │   └── DatabaseLoader.h
│   │   ├── lua/                   # Lua integration
│   │   │   ├── LuaRulesEngine.h
│   │   │   └── LuaBindings.h
│   │   ├── stats/                 # Basketball statistics
│   │   │   └── StatisticsEngine.h
│   │   └── bindings/              # C++ <-> Unity bridge
│   │       └── UnityBridge.h
│   └── src/                       # Implementation files
│       ├── main.cpp
│       ├── engine/
│       ├── ai/
│       ├── db/
│       ├── lua/
│       ├── stats/
│       └── bindings/
│
├── bm-ui/                         # Unity UI Layer
│   └── Assets/                    # Will be Unity project structure
│
├── bm-data/                       # Game data
│   ├── schema/                    # Database schema
│   │   └── schema.lua
│   ├── ncaa-2025/                 # NCAA season data
│   │   ├── ncaa_rules.lua
│   │   ├── recruiting.lua
│   │   ├── teams.csv
│   │   ├── players.csv
│   │   └── conferences.csv
│   └── migrasi/                   # Database migrations
│
├── bm-tools/                      # Utility tools
│   ├── data-importer/             # CSV/JSON importer
│   │   └── CMakeLists.txt
│   └── mod-validator/             # Mod validation tool
│       └── CMakeLists.txt
│
├── mods/                          # Mod examples & templates
│   └── example-mod/
│       ├── mod.json
│       ├── teams.json
│       ├── players.json
│       ├── rules/
│       └── assets/
│
└── docs/                          # Documentation
    ├── API.md
    ├── MODDING.md
    └── ARCHITECTURE.md
```

## Build Instructions

### Prerequisites (macOS M1)
- CMake 3.20+
- Xcode 13+
- SQLite3: `brew install sqlite`
- Lua 5.4: `brew install lua`

### Build C++ Core

```bash
cd /Users/romandivkovic/repos/basketball-manager
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Output
- Compiled library: `build/lib/libBMCore.dylib`
- Binaries: `build/bin/`

## Next Steps

1. **Phase 1 (Current)**
   - [x] C++ project structure
   - [x] Header files with class definitions
   - [x] Implementation stubs
   - [ ] Lua integration & bindings
   - [ ] Database schema refinement

2. **Phase 2**
   - NCAA data loading
   - Match engine expansion
   - AI systems development

3. **Phase 3**
   - Unity UI integration
   - Native plugin loading

## Technology Stack

| Layer | Technology | Version |
|-------|-----------|---------|
| Simulation | C++ | C++20 |
| Rules Engine | Lua | 5.4 |
| UI | Unity | 2022 LTS |
| Database | SQLite3 | 3.x |
| Build System | CMake | 3.20+ |
| Platform | macOS | M1 Native |
