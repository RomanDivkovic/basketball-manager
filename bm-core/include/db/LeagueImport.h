#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>

namespace bm {
namespace db {

struct LeagueImportResult {
    bool ok = false;
    int teams = 0;
    int players = 0;
    std::vector<std::string> errors;   // human readable, with file and line
};

// Builds a league database (same schema as the shipped one) from teams.csv and players.csv in `folder`.
// Nothing is written unless the whole league validates.
LeagueImportResult ImportLeagueFromCsv(const std::string& folder, const std::string& outDbPath);

struct DraftProspectRow {
    std::string first, last, position, birth;
    int ability = 60, potential = 70;
    int pace = 0, shooting = 0, ballControl = 0, defense = 0, physical = 0, technical = 0;   // 0 = derive from ability
    double height = 0.0, weight = 0.0;                                                        // 0 = derive from position
};

struct DraftClassResult {
    bool ok = false;
    std::vector<DraftProspectRow> prospects;   // in file order (best first)
    std::vector<std::string> errors;           // human readable, with line numbers
};

// Reads a draft class file (first_name,last_name,position,current_ability,... see docs/MODDING.md).
DraftClassResult ReadDraftClassCsv(const std::string& path);

// A made-up 30-team league (invented cities, nicknames and players, kit colours); the same seed gives the same league.
LeagueImportResult GenerateRandomLeague(const std::string& outDbPath, unsigned seed);

// Writes teams.csv, players.csv and README.txt describing the format from an open league database.
bool ExportLeagueTemplate(sqlite3* source, const std::string& folder, std::string* error);

} // namespace db
} // namespace bm
