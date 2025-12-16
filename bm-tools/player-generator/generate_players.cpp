#include "db/DatabaseManager.h"
#include "player/PlayerGenerator.h"
#include <iostream>
#include <string>
#include <vector>

using namespace bm;
using namespace bm::player;

struct TeamData {
    int id;
    std::string name;
    int prestige;
};

void CreatePlayersTable(db::DatabaseManager& db) {
    std::cout << "Creating players table...\n";
    
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS players (
            player_id TEXT PRIMARY KEY,
            first_name TEXT NOT NULL,
            last_name TEXT NOT NULL,
            team_id INTEGER,
            position TEXT NOT NULL,
            year_in_school INTEGER,
            height REAL,
            weight REAL,
            jersey_number INTEGER,
            birth_date TEXT,
            
            -- Attributes (0-20 scale)
            pace INTEGER,
            shooting INTEGER,
            ball_control INTEGER,
            defense INTEGER,
            physical INTEGER,
            technical INTEGER,
            
            -- Ratings (0-100 scale)
            current_ability INTEGER,
            potential_ability INTEGER,
            
            -- Status
            active INTEGER DEFAULT 1,
            injured INTEGER DEFAULT 0,
            suspension_matches INTEGER DEFAULT 0,
            international INTEGER DEFAULT 0,
            draft_eligible INTEGER DEFAULT 0,
            nil_value INTEGER DEFAULT 0,
            
            -- Stats
            games_played REAL DEFAULT 0,
            minutes_played REAL DEFAULT 0,
            points_per_game REAL DEFAULT 0,
            rebounds_per_game REAL DEFAULT 0,
            assists_per_game REAL DEFAULT 0,
            
            FOREIGN KEY (team_id) REFERENCES teams(id)
        )
    )";
    
    if (!db.ExecuteSQL(sql)) {
        std::cerr << "Failed to create players table!\n";
        exit(1);
    }
    
    std::cout << "✓ Players table created\n";
}

std::vector<TeamData> LoadTeams(db::DatabaseManager& db) {
    std::cout << "Loading teams from database...\n";
    
    std::vector<TeamData> teams;
    
    auto stmt = db.PrepareStatement("SELECT id, name, prestige FROM teams");
    if (!stmt) {
        std::cerr << "Failed to prepare statement!\n";
        return teams;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TeamData team;
        team.id = sqlite3_column_int(stmt, 0);
        team.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        team.prestige = sqlite3_column_int(stmt, 2);
        teams.push_back(team);
    }
    
    std::cout << "✓ Loaded " << teams.size() << " teams\n";
    return teams;
}

std::string PositionToString(Position pos) {
    switch(pos) {
        case Position::PG: return "PG";
        case Position::SG: return "SG";
        case Position::SF: return "SF";
        case Position::PF: return "PF";
        case Position::C: return "C";
        default: return "PG";
    }
}

bool InsertPlayer(db::DatabaseManager& db, const Player& player, int teamId) {
    std::string sql = R"(
        INSERT INTO players (
            player_id, first_name, last_name, team_id, position,
            year_in_school, height, weight, jersey_number, birth_date,
            pace, shooting, ball_control, defense, physical, technical,
            current_ability, potential_ability,
            active, injured, suspension_matches, international, draft_eligible, nil_value
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    auto stmt = db.PrepareStatement(sql);
    if (!stmt) return false;
    
    sqlite3_bind_text(stmt, 1, player.playerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, player.firstName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, player.lastName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, teamId);
    sqlite3_bind_text(stmt, 5, PositionToString(player.position).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, player.yearInSchool);
    sqlite3_bind_double(stmt, 7, player.height);
    sqlite3_bind_double(stmt, 8, player.weight);
    sqlite3_bind_int(stmt, 9, player.jerseyNumber);
    sqlite3_bind_text(stmt, 10, player.birthDate.c_str(), -1, SQLITE_TRANSIENT);
    
    sqlite3_bind_int(stmt, 11, player.attributes.pace);
    sqlite3_bind_int(stmt, 12, player.attributes.shooting);
    sqlite3_bind_int(stmt, 13, player.attributes.ballControl);
    sqlite3_bind_int(stmt, 14, player.attributes.defense);
    sqlite3_bind_int(stmt, 15, player.attributes.physical);
    sqlite3_bind_int(stmt, 16, player.attributes.technical);
    sqlite3_bind_int(stmt, 17, player.attributes.currentAbility);
    sqlite3_bind_int(stmt, 18, player.attributes.potentialAbility);
    
    sqlite3_bind_int(stmt, 19, player.active ? 1 : 0);
    sqlite3_bind_int(stmt, 20, player.injured ? 1 : 0);
    sqlite3_bind_int(stmt, 21, player.suspensionMatches);
    sqlite3_bind_int(stmt, 22, player.international ? 1 : 0);
    sqlite3_bind_int(stmt, 23, player.draftEligible ? 1 : 0);
    sqlite3_bind_int(stmt, 24, player.nilValue);
    
    int result = sqlite3_step(stmt);
    return (result == SQLITE_DONE);
}

int main(int argc, char* argv[]) {
    std::cout << "=" << std::string(78, '=') << "\n";
    std::cout << "Basketball Manager - Player Generator Tool\n";
    std::cout << "=" << std::string(78, '=') << "\n\n";
    
    // Database path
    std::string dbPath = "../bm-data/basketball_manager.db";
    if (argc > 1) {
        dbPath = argv[1];
    }
    
    // Open database
    db::DatabaseManager db;
    if (!db.Open(dbPath)) {
        std::cerr << "Failed to open database: " << dbPath << "\n";
        return 1;
    }
    std::cout << "✓ Database opened: " << dbPath << "\n\n";
    
    // Create players table
    CreatePlayersTable(db);
    std::cout << "\n";
    
    // Load teams
    auto teams = LoadTeams(db);
    if (teams.empty()) {
        std::cerr << "No teams found in database!\n";
        return 1;
    }
    std::cout << "\n";
    
    // Generate players for each team
    PlayerGenerator generator;
    int totalPlayers = 0;
    int playersPerRoster = 13;
    
    std::cout << "Generating players for " << teams.size() << " teams...\n";
    std::cout << std::string(80, '-') << "\n";
    
    for (size_t i = 0; i < teams.size(); ++i) {
        const auto& team = teams[i];
        
        std::cout << "[" << (i + 1) << "/" << teams.size() << "] " 
                  << team.name << " (Prestige: " << team.prestige << ")... ";
        std::cout.flush();
        
        // Generate roster
        auto roster = generator.GenerateRoster(team.id, team.name, team.prestige, playersPerRoster);
        
        // Insert players into database
        int inserted = 0;
        for (const auto& player : roster) {
            if (InsertPlayer(db, player, team.id)) {
                inserted++;
            }
        }
        
        totalPlayers += inserted;
        std::cout << "✓ " << inserted << " players\n";
    }
    
    std::cout << std::string(80, '-') << "\n";
    std::cout << "\n✅ Generation complete!\n";
    std::cout << "Total players generated: " << totalPlayers << "\n";
    std::cout << "Average per team: " << (totalPlayers / teams.size()) << "\n";
    
    db.Close();
    
    return 0;
}
