# NCAA Data Importer

C# tool for importing NCAA basketball team statistics into the Basketball Manager database.

## Overview

This tool reads CSV files containing NCAA Division I team statistics and converts them into Basketball Manager's team attributes (0-20 scale). It populates the SQLite database with real-world NCAA teams and their characteristics.

## Data Source

The tool expects CSV files with the following columns:
- `TEAM`: Team name
- `CONF`: Conference abbreviation
- `G`, `W`: Games played and wins
- `ADJOE`: Adjusted offensive efficiency (points per 100 possessions)
- `ADJDE`: Adjusted defensive efficiency (points allowed per 100 possessions)
- `ADJ_T`: Adjusted tempo (possessions per 40 minutes)
- `EFG_O`, `EFG_D`: Effective field goal percentages
- `3P_O`, `3P_D`: Three-point shooting percentages
- `ORB`, `DRB`: Offensive and defensive rebound percentages
- `TOR`, `TORD`: Turnover percentages
- `BARTHAG`: Power rating
- `WAB`: Wins above bubble

## Stat Conversion

NCAA statistics are converted to BM attributes as follows:

| BM Attribute | Source Stats | Calculation |
|--------------|--------------|-------------|
| **Pace** | Adjusted Tempo | Direct mapping (58-78 → 0-20) |
| **Shooting** | ADJOE, EFG_O, 3P_O | Average of offensive metrics |
| **Defense** | ADJDE | Inverse mapping (lower is better) |
| **Rebounding** | ORB, DRB | Average of rebound percentages |
| **Ball Handling** | TOR | Inverse of turnover rate |
| **Prestige** | BARTHAG, WAB, Win% | Weighted combination |

## Usage

```bash
cd bm-tools/NCAAImporter
dotnet run [csv_path] [db_path]
```

### Examples

```bash
# Use default paths
dotnet run

# Specify custom CSV
dotnet run ../../bm-data/ncaa-source/cbb25.csv

# Specify both CSV and database
dotnet run ../../bm-data/ncaa-source/cbb24.csv ../../bm-data/basketball_manager.db
```

### Default Paths
- **CSV Input**: `../../bm-data/ncaa-source/cbb25.csv`
- **Database Output**: `../../bm-data/basketball_manager.db`

## Output

The tool will:
1. ✅ Read NCAA team statistics from CSV
2. ✅ Initialize/verify database schema
3. ✅ Clear existing team data
4. ✅ Convert stats to BM attributes (0-20 scale)
5. ✅ Insert ~364 NCAA D1 teams
6. ✅ Display summary statistics

### Example Output

```
🏀 Basketball Manager - NCAA Data Importer
==========================================

📂 Reading CSV: cbb25.csv
✅ Loaded 364 teams

💾 Database: basketball_manager.db
✅ Database schema initialized
🗑️  Cleared existing team data

📊 Importing teams...
─────────────────────────────────────────────────────────────
  ✓ 50 teams imported...
  ✓ 100 teams imported...
  ✓ 150 teams imported...
  ...
─────────────────────────────────────────────────────────────
✅ Imported: 364 teams

✨ Import completed successfully!

📈 Summary Statistics
═════════════════════════════════════════════════════════════

🏆 Top Conferences by Team Count:
  ACC              18 teams
  B10              18 teams
  SEC              16 teams
  B12              16 teams
  ...

⭐ Top 10 Teams by Power Rating:
   1. Houston                    (B12   ) OFF:18 DEF:19 PRE:20
   2. Duke                       (ACC   ) OFF:19 DEF:18 PRE:20
   3. Auburn                     (SEC   ) OFF:19 DEF:18 PRE:20
   ...

═════════════════════════════════════════════════════════════
```

## Database Schema

Teams are inserted into the `teams` table:

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

## Dependencies

- .NET 8.0
- CsvHelper 30.0.1
- Microsoft.Data.Sqlite 8.0.0

## Building

```bash
dotnet restore
dotnet build
```

## Notes

- The tool uses **cbb25.csv** (2024-25 season) by default
- All 364 NCAA Division I teams are imported
- Duplicate team names are skipped
- Stats are automatically scaled to 0-20 range
- Conference names are normalized (e.g., "B10" → "Big Ten")
