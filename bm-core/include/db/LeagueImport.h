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

// Writes teams.csv, players.csv and README.txt describing the format from an open league database.
bool ExportLeagueTemplate(sqlite3* source, const std::string& folder, std::string* error);

} // namespace db
} // namespace bm
