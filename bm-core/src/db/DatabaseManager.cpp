#include "db/DatabaseManager.h"
#include <iostream>
#include <stdexcept>

namespace bm {
namespace db {

DatabaseManager::DatabaseManager() : database(nullptr), inTransaction(false) {
}

DatabaseManager::~DatabaseManager() {
    Close();
}

bool DatabaseManager::Open(const std::string& dbPath) {
    int rc = sqlite3_open(dbPath.c_str(), &database);
    
    if (rc != SQLITE_OK) {
        std::cerr << "[DB] Failed to open database: " << sqlite3_errmsg(database) << "\n";
        return false;
    }
    
    std::cout << "[DB] Database opened: " << dbPath << "\n";
    return true;
}

bool DatabaseManager::Close() {
    if (database) {
        if (inTransaction) RollbackTransaction();
        
        int rc = sqlite3_close(database);
        if (rc != SQLITE_OK) {
            std::cerr << "[DB] Error closing database\n";
            return false;
        }
        database = nullptr;
    }
    return true;
}

bool DatabaseManager::ExecuteSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "[DB] SQL Error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

sqlite3_stmt* DatabaseManager::PrepareStatement(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(database, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "[DB] Prepare error: " << sqlite3_errmsg(database) << "\n";
        return nullptr;
    }
    
    return stmt;
}

bool DatabaseManager::BeginTransaction() {
    if (inTransaction) return false;
    
    if (!ExecuteSQL("BEGIN TRANSACTION;")) {
        return false;
    }
    
    inTransaction = true;
    return true;
}

bool DatabaseManager::CommitTransaction() {
    if (!inTransaction) return false;
    
    if (!ExecuteSQL("COMMIT;")) {
        return false;
    }
    
    inTransaction = false;
    return true;
}

bool DatabaseManager::RollbackTransaction() {
    if (!inTransaction) return false;
    
    if (!ExecuteSQL("ROLLBACK;")) {
        return false;
    }
    
    inTransaction = false;
    return true;
}

bool DatabaseManager::InitializeSchema() {
    std::cout << "[DB] Initializing schema\n";
    
    CreatePlayerTable();
    CreateTeamTable();
    CreateContractTable();
    CreateStatisticsTable();
    CreateMatchTable();
    CreateSeasonTable();
    
    return true;
}

void DatabaseManager::CreatePlayerTable() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS players (
            player_id TEXT PRIMARY KEY,
            first_name TEXT NOT NULL,
            last_name TEXT NOT NULL,
            position TEXT,
            height REAL,
            weight REAL,
            birth_date TEXT,
            jersey_number INTEGER,
            team_id TEXT,
            age INTEGER,
            
            -- Attributes (0-20 scale)
            pace INTEGER,
            shooting INTEGER,
            ballcontrol INTEGER,
            defense INTEGER,
            physical INTEGER,
            technical INTEGER,
            
            -- Status
            active BOOLEAN DEFAULT 1,
            injured BOOLEAN DEFAULT 0,
            suspension INTEGER DEFAULT 0,
            
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";
    
    ExecuteSQL(sql);
}

void DatabaseManager::CreateTeamTable() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS teams (
            team_id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            conference TEXT,
            city TEXT,
            arena TEXT,
            founded INTEGER,
            logo_path TEXT,
            colors TEXT,
            
            active BOOLEAN DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";
    
    ExecuteSQL(sql);
}

void DatabaseManager::CreateContractTable() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS contracts (
            contract_id TEXT PRIMARY KEY,
            player_id TEXT NOT NULL,
            team_id TEXT NOT NULL,
            start_year INTEGER,
            end_year INTEGER,
            annual_salary INTEGER,
            total_guaranteed INTEGER,
            
            FOREIGN KEY(player_id) REFERENCES players(player_id),
            FOREIGN KEY(team_id) REFERENCES teams(team_id)
        );
    )";
    
    ExecuteSQL(sql);
}

void DatabaseManager::CreateStatisticsTable() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS statistics (
            stat_id TEXT PRIMARY KEY,
            player_id TEXT NOT NULL,
            season INTEGER,
            
            games_played INTEGER,
            minutes REAL,
            points REAL,
            rebounds REAL,
            assists REAL,
            steals REAL,
            blocks REAL,
            turnovers REAL,
            fouls REAL,
            
            fg_made INTEGER,
            fg_attempts INTEGER,
            three_made INTEGER,
            three_attempts INTEGER,
            ft_made INTEGER,
            ft_attempts INTEGER,
            
            FOREIGN KEY(player_id) REFERENCES players(player_id)
        );
    )";
    
    ExecuteSQL(sql);
}

void DatabaseManager::CreateMatchTable() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS matches (
            match_id TEXT PRIMARY KEY,
            season INTEGER,
            round INTEGER,
            home_team_id TEXT NOT NULL,
            away_team_id TEXT NOT NULL,
            home_score INTEGER,
            away_score INTEGER,
            status TEXT,
            match_date TEXT,
            
            FOREIGN KEY(home_team_id) REFERENCES teams(team_id),
            FOREIGN KEY(away_team_id) REFERENCES teams(team_id)
        );
    )";
    
    ExecuteSQL(sql);
}

void DatabaseManager::CreateSeasonTable() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS seasons (
            season_id TEXT PRIMARY KEY,
            year INTEGER UNIQUE,
            status TEXT,
            current_round INTEGER,
            start_date TEXT,
            end_date TEXT
        );
    )";
    
    ExecuteSQL(sql);
}

} // namespace db
} // namespace bm
