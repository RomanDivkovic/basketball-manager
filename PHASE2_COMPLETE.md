# Basketball Manager - Phase 2 Data Import Complete! 🏀

## What We Just Did

Successfully imported **364 NCAA Division I teams** with real-world statistics into Basketball Manager's database!

## Data Source

Found and used your NCAA basketball CSV files from `/Downloads/archive/`:
- **cbb25.csv** - 2024-25 season (364 teams)
- **cbb13-cbb24.csv** - Historical data 2013-2024 (available for future use)

This data includes advanced metrics from real NCAA games:
- Adjusted offensive/defensive efficiency
- Tempo and pace statistics  
- Shooting percentages (2PT, 3PT, eFG%)
- Rebounding percentages
- Turnover rates
- Power ratings and win percentages

## Import Results

```
✅ 364 NCAA D1 Teams Imported
✅ 32 Conferences with realistic prestige distribution
✅ Stats converted to 0-20 Basketball Manager scale
✅ Database: bm-data/basketball_manager.db
```

### Top Conferences by Prestige
1. **SEC** - 16 teams, avg prestige 14.5
2. **Big 12** - 16 teams, avg prestige 12.9
3. **Big Ten** - 18 teams, avg prestige 12.6
4. **Big East** - 11 teams, avg prestige 10.9
5. **ACC** - 18 teams, avg prestige 9.9

### Top 10 Teams (by Power Rating)
1. **Houston** (Big 12) - OFF:15 DEF:18 PRE:19
2. **Duke** (ACC) - OFF:16 DEF:15 PRE:19
3. **Auburn** (SEC) - OFF:15 DEF:13 PRE:19
4. **Florida** (SEC) - OFF:14 DEF:13 PRE:19
5. **Alabama** (SEC) - OFF:15 DEF:11 PRE:18
6. **Tennessee** (SEC) - OFF:12 DEF:15 PRE:18
7. **St. John's** (Big East) - OFF:8 DEF:17 PRE:18
8. **Michigan St.** (Big Ten) - OFF:9 DEF:15 PRE:17
9. **Texas Tech** (Big 12) - OFF:15 DEF:11 PRE:16
10. **Iowa St.** (Big 12) - OFF:13 DEF:14 PRE:16

## How to Use the Importer

### Run with default settings:
```bash
cd bm-tools/NCAAImporter
dotnet run
```

### Import a different year:
```bash
dotnet run ../../bm-data/ncaa-source/cbb24.csv
```

### Full command with custom paths:
```bash
dotnet run <csv_path> <database_path>
```

## Stat Conversion Formula

NCAA real-world stats → Basketball Manager 0-20 scale:

| BM Attribute | Formula |
|--------------|---------|
| **Pace** | Adjusted Tempo (58-78 → 0-20) |
| **Shooting** | Average of: ADJOE, EFG_O, 3P_O |
| **Defense** | Inverse of ADJDE (lower is better) |
| **Rebounding** | Average of: ORB%, DRB% |
| **Ball Handling** | Inverse of turnover rate |
| **Prestige** | Weighted: 50% power rating, 30% WAB, 20% win% |

All attributes automatically clamped to 0-20 range.

## Database Schema

```sql
CREATE TABLE teams (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    conference TEXT NOT NULL,
    prestige INTEGER DEFAULT 10,
    pace INTEGER DEFAULT 10,
    shooting INTEGER DEFAULT 10,
    defense INTEGER DEFAULT 10,
    rebounding INTEGER DEFAULT 10,
    ball_handling INTEGER DEFAULT 10,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

## Next Steps

Now that we have real NCAA teams in the database, you can:

1. ✅ **Connect the C++ engine** to load teams from SQLite
2. ✅ **Test match simulations** with real team attributes
3. ✅ **Generate conference schedules** using the Lua rules engine
4. ✅ **Build the Unity UI** to display team rosters and standings
5. 📊 **Add player generation** (create realistic rosters for each team)
6. 🏆 **Implement tournaments** (March Madness bracket system)

## File Structure

```
basketball-manager/
├── bm-data/
│   ├── ncaa-source/          # Your CSV files
│   │   ├── cbb25.csv         # 2024-25 season (IMPORTED)
│   │   ├── cbb24.csv         # 2023-24 season
│   │   └── cbb13-23.csv      # Historical data
│   └── basketball_manager.db # SQLite database with 364 teams
├── bm-tools/
│   └── NCAAImporter/         # C# import tool
│       ├── Program.cs
│       ├── StatConverter.cs
│       ├── Models/NCAATeamStats.cs
│       └── README.md
└── bm-core/                  # C++ engine (ready to use DB)
```

## Testing the Database

### View all teams:
```bash
sqlite3 bm-data/basketball_manager.db "SELECT * FROM teams LIMIT 10;"
```

### Get SEC teams:
```bash
sqlite3 bm-data/basketball_manager.db "SELECT name, prestige, shooting, defense FROM teams WHERE conference='SEC' ORDER BY prestige DESC;"
```

### Conference statistics:
```bash
sqlite3 bm-data/basketball_manager.db "SELECT conference, COUNT(*) as teams, AVG(prestige) as avg_prestige FROM teams GROUP BY conference ORDER BY avg_prestige DESC;"
```

## Success! 🎉

Your Basketball Manager now has:
- ✅ Real NCAA Division I teams
- ✅ Accurate team attributes based on 2024-25 performance
- ✅ Proper conference structure
- ✅ Realistic prestige distribution
- ✅ Ready for simulation!

The data is MUCH better than the NCAA_Hoops repository because:
- ✅ Clean CSV format (no R scripts needed)
- ✅ Pre-calculated advanced metrics
- ✅ Multiple years available (2013-2025)
- ✅ Direct mapping to BM attributes
