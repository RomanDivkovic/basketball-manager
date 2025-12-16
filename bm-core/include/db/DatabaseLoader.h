#pragma once

#include "common/Types.h"
#include <memory>
#include <string>
#include <vector>

namespace bm {
namespace db {

class DatabaseManager;
using bm::Team;
using bm::Player;

/**
 * DatabaseLoader - Loads game data from CSV, JSON, or SQLite
 * Handles NCAA 2025 data population
 */
class DatabaseLoader {
public:
    explicit DatabaseLoader(std::shared_ptr<DatabaseManager> dbManager);
    ~DatabaseLoader();

    // Load NCAA data
    bool LoadNCAATeams(const std::string& dataPath);
    bool LoadNCAAPlayers(const std::string& dataPath);
    bool LoadConferences(const std::string& dataPath);

    // CSV import
    bool ImportCSV(const std::string& csvPath, const std::string& tableName);

    // JSON import
    bool ImportJSON(const std::string& jsonPath);

private:
    std::shared_ptr<DatabaseManager> dbManager;

    // Parsing helpers
    std::vector<std::string> ParseCSVLine(const std::string& line);
    void ParsePlayerAttributes(const std::string& jsonData, std::shared_ptr<Player> player);
};

} // namespace db
} // namespace bm
