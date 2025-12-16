# Contributing to Basketball Manager

Tack för att du överväger att bidra till Basketball Manager! 🏀

## Utvecklingsmiljö

### Förutsättningar

- **macOS**: 12.0+ (M1/M2/M3 optimerad)
- **C++20**: Clang eller GCC
- **CMake**: 3.20+
- **Lua**: 5.4+
- **SQLite3**: 3.36+
- **.NET**: 10.0+ (för data-verktyg)
- **Git**: 2.30+

### Installation

```bash
# Klona repot
git clone https://github.com/RomanDivkovic/basketball-manager.git
cd basketball-manager

# Installera beroenden (macOS)
brew install cmake lua sqlite3

# Bygg C++ core
cd bm-core
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4

# Bygg C# importer
cd ../bm-tools/NCAAImporter
dotnet build
```

## Arbetsflöde

### 1. Skapa en branch

```bash
# Synka med main
git checkout main
git pull origin main

# Skapa feature branch
git checkout -b feature/min-nya-funktion
```

### 2. Gör ändringar

- Följ befintlig kodstil (se nedan)
- Skriv tester för nya funktioner
- Uppdatera dokumentation
- Håll commits atomära och fokuserade

### 3. Testa lokalt

```bash
# C++ build
cd bm-core
cmake --build build -j4

# C# build
cd bm-tools/NCAAImporter
dotnet build --configuration Debug

# Lua syntax
find . -name "*.lua" -exec lua -e "assert(loadfile('{}'))" \;
```

### 4. Commit & Push

```bash
git add .
git commit -m "feat: lägg till ny funktion"
git push origin feature/min-nya-funktion
```

### 5. Skapa Pull Request

- Gå till GitHub
- Klicka "New Pull Request"
- Välj din branch → `dev` (inte `main`)
- Beskriv ändringar tydligt
- Vänta på CI-checks ✅

## CI Pipeline

Varje PR kör automatiskt:

✅ **C++ Build** - Kompilerar bm-core  
✅ **C# Build** - Kompilerar NCAAImporter  
✅ **Code Quality** - Lua syntax, file checks  
✅ **Docs Check** - README och markdown länkar  
✅ **Integration Test** - SQLite, artifacts  

Se [.github/README.md](.github/README.md) för detaljer.

## Kodstil

### C++

```cpp
// CamelCase för klasser
class MatchEngine {
public:
    // camelCase för metoder
    void simulateMatch(const Team& home, const Team& away);
    
private:
    // m_ prefix för member variables
    int m_quarter;
    double m_gameTime;
};

// snake_case för filer
// match_engine.h, match_engine.cpp
```

### C#

```csharp
// PascalCase för allt utom locals
public class StatConverter
{
    public int ConvertToScale(double value)
    {
        // camelCase för locals
        int result = 0;
        return result;
    }
}
```

### Lua

```lua
-- snake_case för allt
local function generate_schedule(teams)
    local games = {}
    return games
end
```

## Commit Messages

Följ [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: lägg till spelarskador i simulering
fix: korrigera turnover-beräkning
docs: uppdatera README med nya funktioner
style: formatera kod enligt stil-guide
refactor: omstrukturera match engine
test: lägg till tester för AI-koordinator
chore: uppdatera beroenden
```

## Projektstruktur

```
basketball-manager/
├── bm-core/          # C++ simulation engine
├── bm-ui/            # Unity UI (Phase 3)
├── bm-data/          # Data files, schemas, rules
├── bm-tools/         # Data importers, utilities
├── docs/             # Documentation
├── mods/             # Example mods
└── .github/          # CI/CD workflows
```

## Vad behöver vi hjälp med?

### 🔴 Hög prioritet
- [ ] Unit tests för C++ engine
- [ ] Player generation algorithm
- [ ] Conference scheduling logic
- [ ] Tournament bracket system

### 🟡 Medel prioritet
- [ ] Unity UI implementation
- [ ] Additional leagues (NBA, EuroLeague)
- [ ] Advanced AI tactics
- [ ] Performance optimizations

### 🟢 Låg prioritet
- [ ] Additional stats tracking
- [ ] Historical data import
- [ ] Modding tools GUI
- [ ] Translation (Svenska, Español, etc.)

## Buggar

Hittade en bug? Skapa ett issue:

1. Gå till [Issues](https://github.com/RomanDivkovic/basketball-manager/issues)
2. Klicka "New Issue"
3. Inkludera:
   - Beskrivning av problemet
   - Steg för att återskapa
   - Förväntad beteende
   - Faktisk beteende
   - System info (macOS version, etc.)
   - Logs/screenshots om möjligt

## Frågor?

- **Issues**: [GitHub Issues](https://github.com/RomanDivkovic/basketball-manager/issues)
- **Discussions**: [GitHub Discussions](https://github.com/RomanDivkovic/basketball-manager/discussions)
- **Email**: divkovicroman@gmail.com

## Licens

Genom att bidra godkänner du att dina bidrag licensieras under samma licens som projektet.

## Tack! 🙏

Dina bidrag gör Basketball Manager bättre för alla!
