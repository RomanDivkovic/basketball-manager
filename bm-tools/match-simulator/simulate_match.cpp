#include "db/DatabaseManager.h"
#include "engine/MatchEngine.h"
#include "common/Types.h"
#include <iostream>
#include <string>
#include <memory>

using namespace bm;
using namespace bm::engine;

std::shared_ptr<Team> LoadTeam(db::DatabaseManager& db, const std::string& teamName) {
    std::cout << "Loading team: " << teamName << "...\n";
    
    auto team = std::make_shared<Team>();
    
    // Load team data
    std::string sql = "SELECT id, name, conference, prestige FROM teams WHERE name = ?";
    auto stmt = db.PrepareStatement(sql);
    if (!stmt) {
        std::cerr << "Failed to prepare team query!\n";
        return nullptr;
    }
    
    sqlite3_bind_text(stmt, 1, teamName.c_str(), -1, SQLITE_TRANSIENT);
    
    int teamId = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        teamId = sqlite3_column_int(stmt, 0);
        team->teamId = std::to_string(teamId);
        // Make copies of strings from sqlite3
        const char* namePtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* confPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        team->name = namePtr ? std::string(namePtr) : "";
        team->conferenceId = confPtr ? std::string(confPtr) : "";
        team->prestige = sqlite3_column_int(stmt, 3);
        team->city = "";
        team->state = "";
        team->arena = "";
        team->active = true;
    } else {
        std::cerr << "Team not found: " << teamName << "\n";
        return nullptr;
    }
    
    // Load roster
    std::string rosterSql = R"(
        SELECT player_id, first_name, last_name, position, year_in_school,
               height, weight, jersey_number, birth_date,
               pace, shooting, ball_control, defense, physical, technical,
               current_ability, potential_ability,
               active, injured, suspension_matches, international, draft_eligible, nil_value
        FROM players WHERE team_id = ?
    )";
    
    auto rosterStmt = db.PrepareStatement(rosterSql);
    if (!rosterStmt) {
        std::cerr << "Failed to prepare roster query!\n";
        return nullptr;
    }
    
    sqlite3_bind_int(rosterStmt, 1, teamId);
    
    int playerCount = 0;
    while (sqlite3_step(rosterStmt) == SQLITE_ROW) {
        auto player = std::make_shared<Player>();
        
        // Make copies of all string pointers
        const char* pidPtr = reinterpret_cast<const char*>(sqlite3_column_text(rosterStmt, 0));
        const char* fnPtr = reinterpret_cast<const char*>(sqlite3_column_text(rosterStmt, 1));
        const char* lnPtr = reinterpret_cast<const char*>(sqlite3_column_text(rosterStmt, 2));
        const char* posPtr = reinterpret_cast<const char*>(sqlite3_column_text(rosterStmt, 3));
        const char* bdPtr = reinterpret_cast<const char*>(sqlite3_column_text(rosterStmt, 8));
        
        player->playerId = pidPtr ? std::string(pidPtr) : "";
        player->firstName = fnPtr ? std::string(fnPtr) : "";
        player->lastName = lnPtr ? std::string(lnPtr) : "";
        
        std::string posStr = posPtr ? std::string(posPtr) : "";
        if (posStr == "PG") player->position = Position::PG;
        else if (posStr == "SG") player->position = Position::SG;
        else if (posStr == "SF") player->position = Position::SF;
        else if (posStr == "PF") player->position = Position::PF;
        else if (posStr == "C") player->position = Position::C;
        
        player->yearInSchool = sqlite3_column_int(rosterStmt, 4);
        player->height = sqlite3_column_double(rosterStmt, 5);
        player->weight = sqlite3_column_double(rosterStmt, 6);
        player->jerseyNumber = sqlite3_column_int(rosterStmt, 7);
        player->birthDate = bdPtr ? std::string(bdPtr) : "";
        
        player->attributes.pace = sqlite3_column_int(rosterStmt, 9);
        player->attributes.shooting = sqlite3_column_int(rosterStmt, 10);
        player->attributes.ballControl = sqlite3_column_int(rosterStmt, 11);
        player->attributes.defense = sqlite3_column_int(rosterStmt, 12);
        player->attributes.physical = sqlite3_column_int(rosterStmt, 13);
        player->attributes.technical = sqlite3_column_int(rosterStmt, 14);
        player->attributes.currentAbility = sqlite3_column_int(rosterStmt, 15);
        player->attributes.potentialAbility = sqlite3_column_int(rosterStmt, 16);
        
        player->active = sqlite3_column_int(rosterStmt, 17) != 0;
        player->injured = sqlite3_column_int(rosterStmt, 18) != 0;
        player->suspensionMatches = sqlite3_column_int(rosterStmt, 19);
        player->international = sqlite3_column_int(rosterStmt, 20) != 0;
        player->draftEligible = sqlite3_column_int(rosterStmt, 21) != 0;
        player->nilValue = sqlite3_column_int(rosterStmt, 22);
        
        player->onCourt = false;
        player->minutesPlayedThisGame = 0;
        player->fatigueLevel = 0.0f;
        player->pointsThisGame = 0;
        player->reboundsThisGame = 0;
        player->assistsThisGame = 0;
        player->foulsThisGame = 0;
        
        team->roster.push_back(player);
        playerCount++;
    }
    
    std::cout << "✓ Loaded " << team->name << " with " << playerCount << " players (Prestige: " 
              << team->prestige << ")\n";
    
    return team;
}

int main(int argc, char* argv[]) {
    std::cout << "╔════════════════════════════════════════════════╗\n";
    std::cout << "║     Basketball Manager - Match Simulator       ║\n";
    std::cout << "╚════════════════════════════════════════════════╝\n\n";
    
    // Parse arguments
    std::string homeTeamName = "Duke";
    std::string awayTeamName = "North Carolina";
    std::string dbPath = "../bm-data/basketball_manager.db";
    
    if (argc >= 3) {
        homeTeamName = argv[1];
        awayTeamName = argv[2];
    }
    if (argc >= 4) {
        dbPath = argv[3];
    }
    
    int speedMultiplier = 1;
    if (argc >= 5) {
        speedMultiplier = std::stoi(argv[4]);
        if (speedMultiplier != 1 && speedMultiplier != 2 && speedMultiplier != 3 && 
            speedMultiplier != 4 && speedMultiplier != 6) {
            std::cerr << "Invalid speed multiplier. Must be 1, 2, 3, 4, or 6\n";
            speedMultiplier = 1;
        }
    }
    
    bool interactiveMode = false;
    if (argc >= 6) {
        std::string mode = argv[5];
        if (mode == "interactive" || mode == "i") {
            interactiveMode = true;
        }
    }
    
    std::cout << "Database: " << dbPath << "\n";
    std::cout << "Match: " << homeTeamName << " vs " << awayTeamName << "\n";
    std::cout << "Speed: " << speedMultiplier << "x\n";
    std::cout << "Mode: " << (interactiveMode ? "INTERACTIVE (pause/timeout enabled)" : "Auto-play") << "\n\n";
    
    // Open database
    db::DatabaseManager db;
    if (!db.Open(dbPath)) {
        std::cerr << "Failed to open database: " << dbPath << "\n";
        return 1;
    }
    
    // Load teams
    auto homeTeam = LoadTeam(db, homeTeamName);
    auto awayTeam = LoadTeam(db, awayTeamName);
    
    if (!homeTeam || !awayTeam) {
        std::cerr << "\nFailed to load teams!\n";
        std::cerr << "Usage: " << argv[0] << " <home_team> <away_team> [db_path] [speed_multiplier]\n";
        std::cerr << "Example: " << argv[0] << " Duke \"North Carolina\" ../bm-data/basketball_manager.db 2\n";
        std::cerr << "Speed multiplier: 1 (real-time), 2, 3, 4, 6 (6x fastest)\n";
        return 1;
    }
    
    std::cout << "\n";
    
    // Create match engine
    MatchEngine engine;
    engine.SetSimulationSpeed(static_cast<bm::engine::SimulationSpeed>(speedMultiplier));
    engine.SetPauseEnabled(interactiveMode);
    engine.InitializeMatch(homeTeam, awayTeam);
    
    if (interactiveMode) {
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                    INTERACTIVE MODE ENABLED                    ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  • Press 'p' + ENTER to pause during play                     ║\n";
        std::cout << "║  • Call timeouts, make substitutions, change speed            ║\n";
        std::cout << "║  • View live stats and control game flow                      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    }
    
    // Simulate match
    engine.SimulateFullMatch();
    
    // Get final state
    auto state = engine.GetMatchState();
    if (state) {
        std::cout << "\n════════════════════════════════════════════════\n";
        std::cout << "                  STATISTICS\n";
        std::cout << "════════════════════════════════════════════════\n";
        std::cout << "Rebounds: " << state->homeTeam->name << " " << state->homeRebounds 
                  << " - " << state->awayRebounds << " " << state->awayTeam->name << "\n";
        std::cout << "Fouls: " << state->homeTeam->name << " " << state->homeFouls 
                  << " - " << state->awayFouls << " " << state->awayTeam->name << "\n";
        std::cout << "════════════════════════════════════════════════\n\n";
    }
    
    db.Close();
    
    return 0;
}
