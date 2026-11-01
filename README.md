# 🏀 Basketball Manager 2026

[![CI Status](https://github.com/RomanDivkovic/basketball-manager/workflows/Basketball%20Manager%20CI/badge.svg)](https://github.com/RomanDivkovic/basketball-manager/actions)
[![License](https://img.shields.io/github/license/RomanDivkovic/basketball-manager)](LICENSE)

**The comprehensive, Football Manager-depth basketball management simulation**

Basketball Manager combines a high-performance **C++20 simulation core (`BMCore`)** with a rich **Unity front-end (`UI-unity-bm26`)**, simulating full NBA seasons, tactical strategies, live 2D/3D match court visualizers, automated playoff brackets, and in-depth coach career management.

---

## 🌟 Key Features

### 1. Match Engine & Interactive Simulation
- **Possession-by-Possession Dynamics**: Full 48-minute regulation games with automatic quarter rollover (Q1 → Q4) and 5-minute overtime (OT) rules.
- **2D & 3D Isometric Visual Court**: Real-time spatial tracking of all 10 players and basketball in 3D space (94ft × 50ft court), parquet hardwood lines, pulsing ball-handler aura, and parabolic shot trajectory arcs.
- **Live In-Game Coaching**: Mid-game tactical callouts (*More Offensive*, *More Defensive*, *Manage Clock*), timeouts, and manual substitutions with fatigue penalties.
- **Variable Simulation Speeds**: Step possession manually or run at 1x, 2x, 4x, or 6x speed.

### 2. Season Calendar & Playoff Tournament System
- **82-Game Season Schedule**: Realistic NBA fixture generation with conference tracking.
- **Live Standings**: Eastern and Western conferences with home/away splits, conference records, and differential stats.
- **NBA Play-In & Playoff Brackets**: Automated 7–10 seed Play-In deciders followed by standard 4-round Best-of-7 series culminating in the NBA Finals and World Championship crowning.

### 3. Roster Management & Tactical Presets
- **Active 15-Man Roster**: Starter vs. bench depth chart with player ratings across Shooting, Defense, Physical, Playmaking, and Stamina.
- **Tactical Directives**: Offense focus (Balanced, Inside, 3-Point, Pace & Space, Isolation, Fast Break), Defensive schemes (Man-to-Man, 2-3 Zone, 3-2 Zone, Full Press, Pack Paint), and Tempo settings.

### 4. Head Coach Career & Profile (FM Style)
- **Coach Office**: Customizable profile attributes (Offensive Coaching, Defensive Coaching, Player Development, Man Management).
- **Dual Persistence Architecture**: Instant local `PlayerPrefs` (JSON) save with cloud REST backend sync capability for production.

---

## 🏗️ Architecture & Technology Stack

```
┌──────────────────────────────────────────────────────────────┐
│             Unity 2022+ Front-End (UI-unity-bm26)            │
│  - MatchHUDController (Live Scoreboard, Court Visualizer)     │
│  - BMUnityBridge.cs (C# P/Invoke Marshaling)                 │
└──────────────────────────────┬───────────────────────────────┘
                               │ Native C FFI (UnityBridge)
┌──────────────────────────────▼───────────────────────────────┐
│                 C++20 Simulation Core (BMCore)               │
│  - MatchEngine: Spatial coordinates, possession loop, OT     │
│  - SeasonManager: 82-day schedule, standings, playoff series │
│  - SQLite Database: NBA & NCAA rosters, attributes, history  │
└──────────────────────────────────────────────────────────────┘
```

---

## 🚀 Quick Start & Building

### Prerequisites (macOS ARM64 / M-Series)
- CMake 3.20+
- Clang with C++20 support
- SQLite3 (`brew install sqlite`)
- Lua 5.4 (`brew install lua`)

### Build Core & Run Automated Tests

```bash
# Configure and build C++ engine and automated test suite
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target BMCore bm-core-tests -j4

# Run automated tests
./bm-tools/bm-core-tests
```

### Launch in Unity
1. Open Unity Hub and add the submodule folder `UI-unity-bm26`.
2. The controller auto-bootstraps in play mode (`[RuntimeInitializeOnLoadMethod]`), loading `libBMCore.dylib` directly from `Assets/Plugins/macOS/`.
3. Switch between tabs: **Live Match**, **Season Calendar**, **Standings**, **Roster & Tactics**, **NBA Playoffs**, and **Coach Office**.

---

## 📂 Repository Layout

- `bm-core/`: Core simulation engine (MatchEngine, SeasonManager, TacticalManager, UnityBridge C exports).
- `bm-tools/`: Command-line tools and automated test suite (`bm-core-tests`).
- `UI-unity-bm26/`: Unity user interface submodule.
- `docs/`: Technical architectural specifications.
- `GETTING_STARTED.md`: Comprehensive local environment setup guide.

---

## 📝 License & Contributions

© 2026 Basketball Manager. All rights reserved.