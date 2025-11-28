# Basketball Manager - Getting Started

## Project Setup Guide

### Prerequisites

**macOS M1 Setup:**

```bash
# Install Homebrew dependencies
brew install cmake sqlite lua

# Verify installations
cmake --version    # Should be 3.20+
sqlite3 --version
lua -v
```

### Directory Structure Overview

```
basketball-manager/
├── bm-core/              # C++ simulation engine (CMake project)
├── bm-ui/                # Unity project (C# UI)
├── bm-data/              # Game data and databases
│   ├── schema/           # Database schema definitions
│   └── ncaa-2025/        # NCAA 2025 season data
├── bm-tools/             # Utility tools (data importer, mod validator)
├── mods/                 # Game mods and extensions
├── docs/                 # Documentation
└── build.sh              # Build script
```

### Building the C++ Engine

```bash
# Clone/enter the repository
cd /Users/romandivkovic/repos/basketball-manager

# Create build directory
mkdir -p build
cd build

# Configure with CMake (optimized for M1)
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      ..

# Build
make -j4

# Output files
# - lib/libBMCore.dylib      (Main library)
# - bin/bm-core              (Executable)
# - bin/bm-data-importer     (Data import tool)
# - bin/bm-mod-validator     (Mod validation tool)
```

### Quick Build Script

```bash
# Run from project root
./build.sh release
```

### Next: Unity Integration

1. **Create Unity Project**
   - Version: 2022 LTS
   - Target: macOS

2. **Add C# Bridge**
   - Copy `bm-ui/BMUnityBridge.cs` to Assets/
   - Add plugin: `lib/libBMCore.dylib`

3. **Test Connection**
   ```csharp
   BMUnityBridge.StartSeason(2025);
   ```

## Key Files

### Configuration
- `CMakeLists.txt` - Build configuration
- `bm-core/CMakeLists.txt` - C++ core settings

### Core Engine
- `bm-core/src/engine/` - Match simulation
- `bm-core/src/ai/` - AI decision making
- `bm-core/src/db/` - Database operations
- `bm-core/src/lua/` - Lua integration
- `bm-core/src/stats/` - Statistics calculations

### Data & Rules
- `bm-data/schema/schema.lua` - Database schema
- `bm-data/ncaa-2025/ncaa_rules.lua` - NCAA rules
- `bm-data/ncaa-2025/recruiting.lua` - Recruiting system

### Modding
- `mods/example-mod/` - Example mod structure
- `docs/MODDING.md` - Modding guide

### Documentation
- `docs/README.md` - Project overview
- `docs/ARCHITECTURE.md` - System design
- `docs/MODDING.md` - Modding guide

## Common Commands

```bash
# Build release version
cd /Users/romandivkovic/repos/basketball-manager/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make

# Build debug version
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

# Run tests (when added)
ctest

# Import data
./bin/bm-data-importer ncaa_teams.csv game.db teams

# Validate mod
./bin/bm-mod-validator ../mods/example-mod
```

## C++ Project Structure

### Include Hierarchy
```
include/
├── engine/        # Match simulation (public headers)
├── ai/            # AI systems
├── db/            # Database layer
├── lua/           # Lua integration
├── stats/         # Statistics engine
├── bindings/      # C# interop
└── common/        # Shared types
```

### Source Organization
```
src/
├── engine/        # Match engine implementation
├── ai/            # AI implementation
├── db/            # Database implementation
├── lua/           # Lua binding implementation
├── stats/         # Statistics implementation
└── bindings/      # Unity bridge implementation
```

## Development Workflow

### Phase 1: Engine Foundation ✅ COMPLETE
- [x] Project structure
- [x] CMake configuration
- [x] Header files with class definitions
- [x] Implementation stubs
- [x] Common types and structures
- [x] Lua integration framework
- [x] Database schema

### Phase 2: NCAA Data Population 🚀 NEXT
- [ ] Load NCAA 2025 teams
- [ ] Implement recruiting system
- [ ] Add player progression
- [ ] Implement schedule generation
- [ ] Add playoff system

### Phase 3: Match Engine Enhancement
- [ ] Possession-based simulation
- [ ] Play selection AI
- [ ] Defense logic
- [ ] Commentary generation
- [ ] Statistics tracking

### Phase 4: AI Systems
- [ ] Tactical AI
- [ ] Roster management AI
- [ ] Player development AI
- [ ] Recruiting AI

### Phase 5: Unity Integration
- [ ] UI framework
- [ ] C++ plugin loading
- [ ] Data visualization
- [ ] Game loop integration

### Phase 6: Modding System
- [ ] Mod manager UI
- [ ] Data override system
- [ ] Lua script hot-reload
- [ ] Mod marketplace integration

## Troubleshooting

### Build Issues
**Problem**: CMake can't find SQLite
```bash
# Solution: Specify path
cmake .. -DSQLITE3_INCLUDE_DIRS=/usr/local/include
```

**Problem**: Lua not found
```bash
# Solution: Install via Homebrew
brew install lua
```

### Runtime Issues
**Problem**: Database file not found
- Check `bm-data/` directory exists
- Verify database path is correct

**Problem**: Lua script errors
- Check script syntax in editor
- Review game logs for error messages

## Support & Next Steps

1. **Read Documentation**
   - `docs/ARCHITECTURE.md` - System design
   - `docs/MODDING.md` - How to create mods

2. **Explore Example Code**
   - `mods/example-mod/` - Mod structure
   - `bm-core/src/` - Engine implementation

3. **Build & Test**
   - Compile the engine
   - Test database operations
   - Validate mod system

Happy coding! 🏀
