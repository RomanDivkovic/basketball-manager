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
std::string teamsJson;
std::string commentary;

std::string EscapeJson(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        if (c == '"') ss << "\\\"";
        else if (c == '\\') ss << "\\\\";
        else if (c == '\n') ss << " ";
        else if (c == '\r') continue;
        else ss << c;
    }
    return ss.str();
}

std::shared_ptr<bm::Team> LoadTeam(const char* teamId) {
    if (!database || !database->IsOpen() || !teamId || teamId[0] == '\0') return nullptr;

    auto team = std::make_shared<bm::Team>();
    sqlite3_stmt* statement = nullptr;
    int numericId = 0;
    bool isNumeric = true;
    for (const char* p = teamId; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            isNumeric = false;
            break;
        }
    }

    if (isNumeric) {
        numericId = std::stoi(teamId);
        statement = database->PrepareStatement(
            "SELECT id, name, conference, prestige FROM teams WHERE id = ?");
        if (!statement) return nullptr;
        sqlite3_bind_int(statement, 1, numericId);
    } else {
        statement = database->PrepareStatement(
            "SELECT id, name, conference, prestige FROM teams WHERE name LIKE ? LIMIT 1");
        if (!statement) return nullptr;
        std::string pattern = std::string("%") + teamId + "%";
        sqlite3_bind_text(statement, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }

    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        // Fallback to first available team if not found
        statement = database->PrepareStatement(
            "SELECT id, name, conference, prestige FROM teams ORDER BY id LIMIT 1");
        if (!statement || sqlite3_step(statement) != SQLITE_ROW) {
            if (statement) sqlite3_finalize(statement);
            return nullptr;
        }
    }

    int dbTeamId = sqlite3_column_int(statement, 0);
    team->teamId = std::to_string(dbTeamId);
    team->name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    team->conferenceId = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
    team->prestige = sqlite3_column_int(statement, 3);
    team->active = true;
    sqlite3_finalize(statement);

    auto roster = database->PrepareStatement(
        "SELECT player_id, first_name, last_name, position, jersey_number, "
        "pace, shooting, ball_control, defense, physical, technical, current_ability "
        "FROM players WHERE team_id = ? ORDER BY jersey_number");
    if (!roster) return nullptr;
    sqlite3_bind_int(roster, 1, dbTeamId);
    while (sqlite3_step(roster) == SQLITE_ROW) {
        auto player = std::make_shared<bm::Player>();
        player->playerId = reinterpret_cast<const char*>(sqlite3_column_text(roster, 0));
        player->firstName = reinterpret_cast<const char*>(sqlite3_column_text(roster, 1));
        player->lastName = reinterpret_cast<const char*>(sqlite3_column_text(roster, 2));
        auto position = reinterpret_cast<const char*>(sqlite3_column_text(roster, 3));
        player->position = position[0] == 'P' && position[1] == 'G' ? bm::Position::PG
            : position[0] == 'S' && position[1] == 'G' ? bm::Position::SG
            : position[0] == 'C' ? bm::Position::C
            : position[0] == 'P' ? bm::Position::PF : bm::Position::SF;
        player->jerseyNumber = sqlite3_column_int(roster, 4);
        player->attributes.pace = sqlite3_column_int(roster, 5);
        player->attributes.shooting = sqlite3_column_int(roster, 6);
        player->attributes.ballControl = sqlite3_column_int(roster, 7);
        player->attributes.defense = sqlite3_column_int(roster, 8);
        player->attributes.physical = sqlite3_column_int(roster, 9);
        player->attributes.technical = sqlite3_column_int(roster, 10);
        player->attributes.currentAbility = sqlite3_column_int(roster, 11);
        player->active = true;
        player->onCourt = false;
        player->fatigueLevel = 0.0f;
        player->pointsThisGame = 0;
        player->reboundsThisGame = 0;
        player->assistsThisGame = 0;
        player->foulsThisGame = 0;
        player->minutesPlayedThisGame = 0.0f;
        team->roster.push_back(player);
    }
    sqlite3_finalize(roster);
    return team;
}

const char* PositionToStr(bm::Position pos) {
    switch (pos) {
        case bm::Position::PG: return "PG";
        case bm::Position::SG: return "SG";
        case bm::Position::SF: return "SF";
        case bm::Position::PF: return "PF";
        case bm::Position::C:  return "C";
        default: return "F";
    }
}

void SerializePlayers(std::ostringstream& json, const std::vector<std::shared_ptr<bm::Player>>& roster) {
    json << "[";
    bool first = true;
    for (const auto& p : roster) {
        if (!p) continue;
        if (!first) json << ",";
        first = false;
        int stamina = std::clamp(static_cast<int>((1.0f - p->fatigueLevel) * 100.0f), 0, 100);
        json << "{\"id\":\"" << EscapeJson(p->playerId)
             << "\",\"name\":\"" << EscapeJson(p->firstName + " " + p->lastName)
             << "\",\"pos\":\"" << PositionToStr(p->position)
             << "\",\"jersey\":" << p->jerseyNumber
             << ",\"rating\":" << p->attributes.currentAbility
             << ",\"pts\":" << p->pointsThisGame
             << ",\"reb\":" << p->reboundsThisGame
             << ",\"ast\":" << p->assistsThisGame
             << ",\"fls\":" << p->foulsThisGame
             << ",\"stamina\":" << stamina
             << ",\"onCourt\":" << (p->onCourt ? "true" : "false")
             << "}";
    }
    json << "]";
}

void UpdateStateJson() {
    if (!matchEngine || !matchEngine->GetMatchState()) {
        stateJson = "{}";
        return;
    }
    const auto& state = *matchEngine->GetMatchState();
    std::ostringstream json;
    json << "{\"homeTeam\":\"" << EscapeJson(state.homeTeam->name)
         << "\",\"awayTeam\":\"" << EscapeJson(state.awayTeam->name)
         << "\",\"homeTeamId\":\"" << EscapeJson(state.homeTeam->teamId)
         << "\",\"awayTeamId\":\"" << EscapeJson(state.awayTeam->teamId)
         << "\",\"homeScore\":" << state.homeScore
         << ",\"awayScore\":" << state.awayScore
         << ",\"quarter\":" << state.quarterNumber
         << ",\"timeRemaining\":" << state.timeRemaining
         << ",\"homeRebounds\":" << state.homeRebounds
         << ",\"awayRebounds\":" << state.awayRebounds
         << ",\"homeFouls\":" << state.homeFouls
         << ",\"awayFouls\":" << state.awayFouls
         << ",\"homeTimeouts\":" << matchEngine->GetTimeoutsRemaining(true)
         << ",\"awayTimeouts\":" << matchEngine->GetTimeoutsRemaining(false)
         << ",\"homePossession\":" << (state.homeTeamPossession ? "true" : "false")
         << ",\"isGameOver\":" << (state.matchComplete ? "true" : "false")
         << ",\"commentary\":\"" << EscapeJson(state.lastPlay)
         << "\",\"homePlayers\":";
    SerializePlayers(json, state.homeTeam->roster);
    json << ",\"awayPlayers\":";
    SerializePlayers(json, state.awayTeam->roster);
    json << "}";
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

const char* GetTeamsJson() {
    if (!database || !database->IsOpen()) {
        teamsJson = "{\"teams\":[]}";
        return teamsJson.c_str();
    }
    auto stmt = database->PrepareStatement(
        "SELECT id, name, conference, prestige FROM teams ORDER BY id");
    if (!stmt) {
        teamsJson = "{\"teams\":[]}";
        return teamsJson.c_str();
    }
    std::ostringstream json;
    json << "{\"teams\":[";
    bool first = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) json << ",";
        first = false;
        int id = sqlite3_column_int(stmt, 0);
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* conf = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int prestige = sqlite3_column_int(stmt, 3);
        json << "{\"id\":\"" << id
             << "\",\"name\":\"" << EscapeJson(name ? name : "")
             << "\",\"conference\":\"" << EscapeJson(conf ? conf : "")
             << "\",\"prestige\":" << prestige << "}";
    }
    sqlite3_finalize(stmt);
    json << "]}";
    teamsJson = json.str();
    return teamsJson.c_str();
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
