#include "bindings/UnityBridge.h"
#include "db/DatabaseManager.h"
#include "engine/MatchEngine.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

std::shared_ptr<bm::db::DatabaseManager> database;
std::unique_ptr<bm::engine::MatchEngine> matchEngine;
std::shared_ptr<bm::Team> homeTeam;
std::shared_ptr<bm::Team> awayTeam;
std::string lastError;
std::string stateJson;
std::string commentary;

std::shared_ptr<bm::Team> LoadTeam(const char* teamId) {
    if (!database || !database->IsOpen() || !teamId) return nullptr;

    auto team = std::make_shared<bm::Team>();
    auto statement = database->PrepareStatement(
        "SELECT id, name, conference, prestige FROM teams WHERE id = ?");
    if (!statement) return nullptr;

    sqlite3_bind_int(statement, 1, std::stoi(teamId));
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        return nullptr;
    }

    team->teamId = std::to_string(sqlite3_column_int(statement, 0));
    team->name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    team->conferenceId = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
    team->prestige = sqlite3_column_int(statement, 3);
    team->active = true;
    sqlite3_finalize(statement);

    auto roster = database->PrepareStatement(
        "SELECT player_id, first_name, last_name, position, current_ability "
        "FROM players WHERE team_id = ? ORDER BY jersey_number");
    if (!roster) return nullptr;
    sqlite3_bind_int(roster, 1, std::stoi(teamId));
    while (sqlite3_step(roster) == SQLITE_ROW) {
        auto player = std::make_shared<bm::Player>();
        player->playerId = reinterpret_cast<const char*>(sqlite3_column_text(roster, 0));
        player->firstName = reinterpret_cast<const char*>(sqlite3_column_text(roster, 1));
        player->lastName = reinterpret_cast<const char*>(sqlite3_column_text(roster, 2));
        auto position = reinterpret_cast<const char*>(sqlite3_column_text(roster, 3));
        player->position = position[0] == 'P' && position[1] == 'G' ? bm::Position::PG
            : position[0] == 'S' ? bm::Position::SG
            : position[0] == 'C' ? bm::Position::C
            : position[0] == 'P' ? bm::Position::PF : bm::Position::SF;
        player->attributes.currentAbility = sqlite3_column_int(roster, 4);
        player->active = true;
        team->roster.push_back(player);
    }
    sqlite3_finalize(roster);
    return team;
}

void UpdateStateJson() {
    if (!matchEngine || !matchEngine->GetMatchState()) {
        stateJson = "{}";
        return;
    }
    const auto& state = *matchEngine->GetMatchState();
    std::ostringstream json;
    json << "{\"homeTeam\":\"" << state.homeTeam->name
         << "\",\"awayTeam\":\"" << state.awayTeam->name
         << "\",\"homeScore\":" << state.homeScore
         << ",\"awayScore\":" << state.awayScore
         << ",\"quarter\":" << state.quarterNumber
         << ",\"timeRemaining\":" << state.timeRemaining
         << ",\"commentary\":\"" << state.lastPlay << "\"}";
    stateJson = json.str();
}

}

extern "C" {

static const char* ENGINE_VERSION = "1.1.0";

void InitializeMatch(const char* homeTeamId, const char* awayTeamId) {
    homeTeam = LoadTeam(homeTeamId);
    awayTeam = LoadTeam(awayTeamId);
    if (!homeTeam || !awayTeam) {
        lastError = "Unable to load match teams";
        matchEngine.reset();
        return;
    }
    matchEngine = std::make_unique<bm::engine::MatchEngine>();
    matchEngine->InitializeMatch(homeTeam, awayTeam);
    UpdateStateJson();
}

int SimulatePossession() {
    if (!matchEngine || !matchEngine->GetMatchState() || matchEngine->GetMatchState()->matchComplete) return 0;
    auto elapsed = matchEngine->SimulatePossession();
    UpdateStateJson();
    return elapsed;
}

void SimulateMatch() {
    if (matchEngine) matchEngine->SimulateFullMatch();
    UpdateStateJson();
}

const char* GetMatchResult() {
    UpdateStateJson();
    return stateJson.c_str();
}

const char* GetLiveCommentary() {
    if (!matchEngine) return lastError.c_str();
    commentary = matchEngine->GetPossessionCommentary();
    return commentary.c_str();
}

const char* GetMatchStateJson() {
    UpdateStateJson();
    return stateJson.c_str();
}

int IsMatchComplete() {
    return matchEngine && matchEngine->GetMatchState() && matchEngine->GetMatchState()->matchComplete;
}

void SetSimulationSpeed(int speedMultiplier) {
    if (!matchEngine) return;
    speedMultiplier = std::clamp(speedMultiplier, 1, 6);
    matchEngine->SetSimulationSpeed(static_cast<bm::engine::SimulationSpeed>(speedMultiplier));
}

void SetPauseEnabled(int enabled) {
    if (matchEngine) matchEngine->SetPauseEnabled(enabled != 0);
}

void CallTimeout(int isHomeTeam) {
    if (matchEngine) matchEngine->CallTimeout(isHomeTeam != 0);
}

void ApplyCoachingCallout(int isHomeTeam, int callout) {
    if (!matchEngine || callout < 0 || callout > 3) return;
    matchEngine->ApplyCoachingCallout(isHomeTeam != 0,
        static_cast<bm::engine::CoachingCallout>(callout));
}

void InitializeSeason(int year) {
    std::cout << "[UnityBridge] InitializeSeason: " << year << "\n";
}

void SimulateRound(int roundNumber) {
    std::cout << "[UnityBridge] SimulateRound: " << roundNumber << "\n";
}

void LoadDatabase(const char* dbPath) {
    database = std::make_shared<bm::db::DatabaseManager>();
    if (!database->Open(dbPath ? dbPath : "")) {
        lastError = "Unable to open database";
        database.reset();
    }
}

void SaveDatabase(const char* dbPath) {
    std::cout << "[UnityBridge] SaveDatabase: " << dbPath << "\n";
}

void InitializeAI(const char* teamId) {
    std::cout << "[UnityBridge] InitializeAI: " << teamId << "\n";
}

void GetAILineup(char* outBuffer, int bufferSize) {
    if (!outBuffer || bufferSize <= 0) return;
    std::string lineup = "";
    if (homeTeam) {
        for (size_t i = 0; i < std::min<size_t>(5, homeTeam->roster.size()); ++i) {
            if (!lineup.empty()) lineup += ',';
            lineup += homeTeam->roster[i]->firstName + " " + homeTeam->roster[i]->lastName;
        }
    }
    std::snprintf(outBuffer, static_cast<size_t>(bufferSize), "%s", lineup.c_str());
}

const char* GetEngineVersion() {
    return ENGINE_VERSION;
}

} // extern "C"
