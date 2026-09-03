#include "db/DatabaseManager.h"
#include "engine/SeasonManager.h"
#include "common/Types.h"
#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

using namespace bm;
using namespace bm::engine;

static std::vector<std::shared_ptr<Team>> LoadAllTeamsFromDB(db::DatabaseManager& db) {
    std::vector<std::shared_ptr<Team>> allTeams;

    auto stmt = db.PrepareStatement("SELECT id, name, conference, prestige FROM teams WHERE active = 1");
    if (!stmt) return allTeams;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto team = std::make_shared<Team>();
        int teamId = sqlite3_column_int(stmt, 0);
        team->teamId = std::to_string(teamId);
        const char* namePtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* confPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        team->name = namePtr ? std::string(namePtr) : "";
        team->conferenceId = confPtr ? std::string(confPtr) : "";
        team->prestige = sqlite3_column_int(stmt, 3);
        team->active = true;

        // Load a minimal roster (not needed for scheduling but keep shape)
        auto rosterStmt = db.PrepareStatement(R"(
            SELECT player_id, first_name, last_name, position
            FROM players WHERE team_id = ?
        )");
        if (rosterStmt) {
            sqlite3_bind_int(rosterStmt, 1, teamId);
            while (sqlite3_step(rosterStmt) == SQLITE_ROW) {
                auto player = std::make_shared<Player>();
                const char* pidPtr = reinterpret_cast<const char*>(sqlite3_column_text(rosterStmt, 0));
                const char* fnPtr = reinterpret_cast<const char*>(sqlite3_column_text(rosterStmt, 1));
                const char* lnPtr = reinterpret_cast<const char*>(sqlite3_column_text(rosterStmt, 2));
                const char* posPtr = reinterpret_cast<const char*>(sqlite3_column_text(rosterStmt, 3));
                player->playerId = pidPtr ? std::string(pidPtr) : "";
                player->firstName = fnPtr ? std::string(fnPtr) : "";
                player->lastName = lnPtr ? std::string(lnPtr) : "";
                std::string posStr = posPtr ? std::string(posPtr) : "";
                if (posStr == "PG") player->position = Position::PG;
                else if (posStr == "SG") player->position = Position::SG;
                else if (posStr == "SF") player->position = Position::SF;
                else if (posStr == "PF") player->position = Position::PF;
                else if (posStr == "C") player->position = Position::C;
                team->roster.push_back(player);
            }
            sqlite3_finalize(rosterStmt);
        }

        allTeams.push_back(team);
    }
    sqlite3_finalize(stmt);

    return allTeams;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: validate-schedule <db_path>\n";
        return 2;
    }

    const std::string dbPath = argv[1];
    db::DatabaseManager db;
    if (!db.Open(dbPath)) return 3;

    auto teams = LoadAllTeamsFromDB(db);
    if (teams.empty()) {
        std::cerr << "No teams loaded from DB\n";
        return 4;
    }

    SeasonManager season;
    season.InitializeSeason(2025, "NCAA");
    season.LoadTeams(teams);
    season.GenerateNCAASchedule();

    const int days = season.GetFixtureDayCount();
    std::cout << "Fixture days: " << days << "\n";

    bool ok = true;
    for (int d = 1; d <= days; ++d) {
        auto fixtures = season.GetFixturesForDay(d);
        std::unordered_map<std::string,int> counts;
        for (const auto& g : fixtures) {
            counts[g.homeTeam->teamId]++;
            counts[g.awayTeam->teamId]++;
        }

        for (const auto& [tid, cnt] : counts) {
            if (cnt > 1) {
                ok = false;
                std::cout << "ERROR: Team " << tid << " plays " << cnt << " times on day " << d << "\n";
            }
        }
    }

    if (ok) {
        std::cout << "Schedule validation passed: no team plays more than once per day.\n";
    } else {
        std::cout << "Schedule validation FAILED.\n";
    }

    db.Close();
    return ok ? 0 : 1;
}
