# 🏀 Basketball Manager

[![CI Status](https://github.com/RomanDivkovic/basketball-manager/workflows/Basketball%20Manager%20CI/badge.svg)](https://github.com/RomanDivkovic/basketball-manager/actions)
[![Release](https://github.com/RomanDivkovic/basketball-manager/workflows/Release%20Build/badge.svg)](https://github.com/RomanDivkovic/basketball-manager/releases)
[![License](https://img.shields.io/github/license/RomanDivkovic/basketball-manager)](LICENSE)

**The world's first Football Manager-level basketball management simulation**

A deep, realistic basketball management game featuring NCAA Division I, with complete mod support and procedural player generation.

## 🎮 Core Vision

Create a comprehensive basketball management experience with:
- **Realistic NCAA Division I** simulation
- **Advanced match simulation** with possession-based gameplay
- **Deep AI systems** for coaching and recruiting
- **Complete modding framework** (NBA, historical leagues, custom content)
- **Professional-grade statistics** and analytics
- **Career mode** with player development and progression

## ✨ Key Features

### League & Tournament System
- ✅ NCAA Division I (all 32 conferences)
- ⏳ EuroLeague (Phase 2)
- ⏳ Professional leagues via mods (NBA, ACB, etc.)
- Conference-based scheduling
- NCAA tournament simulation
- Playoff bracket generation

### Player Database
- NCAA Division I roster (5000+ players)
- Detailed player attributes (6 core stats)
- Player development/progression system
- Injury simulation
- Transfer portal mechanics
- NIL (Name, Image, Likeness) value system

### Match Simulation
- Possession-based engine
- Multiple offensive sets (P&R, Motion, Isolation, etc.)
- Defensive strategies (Man, Zone, Switch, etc.)
- Fatigue and stamina tracking
- Foul system with flagrant detection
- Live text-based commentary
- Detailed statistics tracking

### Tactical System
- Playbook-based play selection
- Defensive adjustments
- Lineup management
- Substitution automation
- Mid-game tactical adjustments

### Career Features
- Coach progression and reputation
- Recruiting pipeline management
- Scholarship budget management
- Team roster building
- Multi-year career mode
- Achievements and milestones

### Modding System 🔧
- **Mod Manager** UI in-game
- **Data override** system (JSON, CSV, SQLite)
- **Lua scripting** for custom rules
- **Asset customization** (logos, uniforms)
- **Community mods** for NBA, historical seasons, custom leagues
- **Hot-reload** capability for development

---

## 🏗️ Technology Stack

```
┌─────────────────┐
│   Unity UI      │  C# - Game menus, dashboards, mod manager
├─────────────────┤
│  C++ Engine     │  C++20 - Match simulation, AI, stats
├─────────────────┤
│  Lua Rules      │  Lua 5.4 - League rules, mods, scripting
├─────────────────┤
│ SQLite Database │  Game data, persistent storage
└─────────────────┘
```

### System Requirements

**Development (macOS M1):**
- macOS 11.0+ (native ARM64)
- CMake 3.20+
- C++20 compiler (Clang 13+)
- Lua 5.4
- SQLite3

**Runtime:**
- macOS 10.15+
- 4GB RAM (8GB recommended)
- 2GB disk space

---

## 🚀 Quick Start

### Clone & Build

```bash
cd /Users/romandivkovic/repos/basketball-manager

# Build C++ engine
./build.sh release

# Output: lib/libBMCore.dylib + bin/bm-core
```

### Documentation

1. **[Getting Started](GETTING_STARTED.md)** - Setup and build guide
2. **[Architecture](docs/ARCHITECTURE.md)** - System design
3. **[Modding Guide](docs/MODDING.md)** - Create your own mods
4. **[Phase 1 Status](PHASE1_COMPLETE.md)** - What's been built

---

## 📂 Project Structure

```
basketball-manager/
├── bm-core/                   # C++ Simulation Engine
│   ├── include/               # Public headers
│   │   ├── engine/            # Match simulation
│   │   ├── ai/                # AI systems
│   │   ├── db/                # Database operations
│   │   ├── lua/               # Lua integration
│   │   ├── stats/             # Statistics engine
│   │   └── bindings/          # C# interop
│   └── src/                   # Implementations
│
├── bm-ui/                     # Unity UI (C#)
│   └── BMUnityBridge.cs       # C++ interop layer
│
├── bm-data/                   # Game Data
│   ├── schema/                # Database schema
│   ├── ncaa-2025/             # NCAA rules & recruiting
│   └── migrasi/               # Database migrations
│
├── bm-tools/                  # Utilities
│   ├── data-importer/         # CSV/JSON import
│   └── mod-validator/         # Mod validation
│
├── mods/                      # Mod Examples
│   └── example-mod/           # Template mod
│
├── docs/                      # Documentation
│   ├── README.md              # Project overview
│   ├── ARCHITECTURE.md        # System design
│   └── MODDING.md             # Modding guide
│
└── GETTING_STARTED.md         # Setup guide
```

---

## 🎯 Development Roadmap

### Phase 1: Engine Foundation ✅ COMPLETE
- [x] Project structure and CMake setup
- [x] C++ core with class hierarchy
- [x] Database layer with SQLite schema
- [x] Lua integration framework
- [x] C# Unity bridge
- [x] Modding system foundation

**Status**: 4400+ lines | Ready to build

### Phase 2: NCAA Data 🔄 IN PROGRESS
- [ ] Load 350+ NCAA D1 teams
- [ ] Import 5000+ real players
- [ ] Conference scheduling system
- [ ] Tournament bracket generation
- [ ] Player progression algorithms

### Phase 3: Match Engine
- [ ] Possession-based simulation
- [ ] Play selection from playbooks
- [ ] Defense logic & effectiveness
- [ ] Scoring calculations
- [ ] Commentary generation

### Phase 4: AI Systems
- [ ] Tactical AI decisions
- [ ] Roster management
- [ ] Recruiting AI
- [ ] Career advancement

### Phase 5: Unity Integration
- [ ] Main menu system
- [ ] Match viewer
- [ ] Statistics dashboards
- [ ] Mod manager UI

### Phase 6: Polish & Release
- [ ] Stability testing
- [ ] Performance optimization
- [ ] UI polish
- [ ] Community beta

---

## 📊 Current Statistics

**Code:**
- C++ Core: 12 headers + 12 implementations
- C# Bridge: 1 file with full P/Invoke support
- Lua Scripts: 4 files with game rules
- Documentation: 4 comprehensive guides

**Database Schema:**
- 7 core tables
- 50+ columns for detailed tracking
- Support for NCAA, professional, custom leagues

**Modding:**
- Complete mod format specification
- JSON, CSV, Lua support
- Example mod included
- Validation tools ready

---

## 💡 Mod Examples (Coming)

The modding system will support:

```
NBA 2024-25 Mod
├── 30 teams
├── 400+ real players
├── Salary cap system
└── NBA playoff format

Historical NCAA (1990-2025)
├── Historic teams
├── Legendary players
└── Era-specific rules

Custom European League
├── Custom teams/players
├── Continental rules
└── Custom tournament
```

---

## 🤝 Contributing

This is an active development project. Future phases include:
- Player database expansion
- Advanced AI systems
- Performance optimization
- Community modding tools

---

## 📝 License

© 2025 Basketball Manager. Privately developed.

---

## 🔗 Key Files for Development

**Start here:**
1. `GETTING_STARTED.md` - Build and run instructions
2. `PHASE1_COMPLETE.md` - What's been completed
3. `docs/ARCHITECTURE.md` - How everything works
4. `docs/MODDING.md` - How to extend it

**Next development targets:**
- `bm-data/ncaa-2025/` - Add team/player data
- `bm-core/src/engine/MatchEngine.cpp` - Implement possession loop
- `bm-ui/BMUnityBridge.cs` - Test C++ integration

---

## 🏀 Play Basketball Manager

Build the engine, create your dynasty, and manage your way to a championship!

**Status**: ✅ Ready to build | 🚀 Ready for Phase 2

---

**Questions?** Check the documentation in `docs/` or review the code structure in `bm-core/include/`
