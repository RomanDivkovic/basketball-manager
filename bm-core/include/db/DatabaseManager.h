#pragma once

#include <memory>
#include <string>
#include <sqlite3.h>

namespace bm {
namespace db {

/**
 * DatabaseManager - Handles all database operations
 * SQLite connection, queries, migrations
 */
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    // Connection management
    bool Open(const std::string& dbPath);
    bool Close();
    bool IsOpen() const { return database != nullptr; }

    // Query execution
    bool ExecuteSQL(const std::string& sql);
    sqlite3_stmt* PrepareStatement(const std::string& sql);

    // Transaction management
    bool BeginTransaction();
    bool CommitTransaction();
    bool RollbackTransaction();

    // Database initialization
    bool InitializeSchema();

    // Get raw connection (for advanced operations)
    sqlite3* GetConnection() const { return database; }

private:
    sqlite3* database;
    bool inTransaction;

    // Schema creation
    void CreatePlayerTable();
    void CreateTeamTable();
    void CreateContractTable();
    void CreateStatisticsTable();
    void CreateMatchTable();
    void CreateSeasonTable();
};

} // namespace db
} // namespace bm
