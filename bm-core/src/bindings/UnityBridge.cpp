#include "bindings/UnityBridge.h"
#include "db/DatabaseManager.h"
#include "engine/MatchEngine.h"
#include "engine/SeasonManager.h"
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::shared_ptr<bm::db::DatabaseManager> database;
std::unique_ptr<bm::engine::MatchEngine> matchEngine;
std::unique_ptr<bm::engine::SeasonManager> seasonManager;
int currentSeasonDay = 1;
std::shared_ptr<bm::Team> homeTeam;
std::shared_ptr<bm::Team> awayTeam;
std::string lastError;
std::string stateJson;
std::string teamsJson;
std::string calendarJson;
std::string fixturesJson;
std::string standingsJson;
std::string playoffsJson;
std::string championJson;
std::string teamRosterJson;
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
             << ",\"stl\":" << p->stealsThisGame
             << ",\"blk\":" << p->blocksThisGame
             << ",\"to\":" << p->turnoversThisGame
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
         << ",\"homeSteals\":" << state.homeSteals
         << ",\"awaySteals\":" << state.awaySteals
         << ",\"homeBlocks\":" << state.homeBlocks
         << ",\"awayBlocks\":" << state.awayBlocks
         << ",\"homeTurnovers\":" << state.homeTurnovers
         << ",\"awayTurnovers\":" << state.awayTurnovers
         << ",\"homeTimeouts\":" << matchEngine->GetTimeoutsRemaining(true)
         << ",\"awayTimeouts\":" << matchEngine->GetTimeoutsRemaining(false)
         << ",\"homePossession\":" << (state.homeTeamPossession ? "true" : "false")
         << ",\"isGameOver\":" << (state.matchComplete ? "true" : "false")
         << ",\"commentary\":\"" << EscapeJson(state.lastPlay)
         << "\",\"homePlayers\":";
    SerializePlayers(json, state.homeTeam->roster);
    json << ",\"awayPlayers\":";
    SerializePlayers(json, state.awayTeam->roster);

    // Serialize 2D/3D court players and ball
    json << ",\"courtPlayers\":[";
    bool firstCp = true;
    for (const auto& cp : state.courtPlayers) {
        if (!firstCp) json << ",";
        firstCp = false;
        json << "{\"id\":\"" << EscapeJson(cp.playerId)
             << "\",\"name\":\"" << EscapeJson(cp.name)
             << "\",\"teamId\":\"" << EscapeJson(cp.teamId)
             << "\",\"isHome\":" << (cp.isHome ? "true" : "false")
             << ",\"pos\":\"" << PositionToStr(cp.position)
             << "\",\"jersey\":" << cp.jerseyNumber
             << ",\"x\":" << std::fixed << std::setprecision(2) << cp.x
             << ",\"y\":" << std::fixed << std::setprecision(2) << cp.y
             << ",\"hasBall\":" << (cp.hasBall ? "true" : "false")
             << ",\"action\":\"" << EscapeJson(cp.action)
             << "\"}";
    }
    json << "],\"ball\":{"
         << "\"x\":" << std::fixed << std::setprecision(2) << state.ball.x
         << ",\"y\":" << std::fixed << std::setprecision(2) << state.ball.y
         << ",\"z\":" << std::fixed << std::setprecision(2) << state.ball.z
         << ",\"holderId\":\"" << EscapeJson(state.ball.holderPlayerId)
         << "\",\"inFlight\":" << (state.ball.inFlight ? "true" : "false")
         << ",\"phase\":\"" << EscapeJson(state.ball.phase)
         << "\"}";

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
    if (!database || !database->IsOpen()) {
        std::cerr << "[UnityBridge] Cannot initialize season: database not loaded\n";
        return;
    }

    std::vector<std::shared_ptr<bm::Team>> teams;
    auto stmt = database->PrepareStatement("SELECT id FROM teams ORDER BY id");
    if (stmt) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int teamId = sqlite3_column_int(stmt, 0);
            auto team = LoadTeam(std::to_string(teamId).c_str());
            if (team) {
                teams.push_back(team);
            }
        }
        sqlite3_finalize(stmt);
    }

    if (teams.empty()) {
        std::cerr << "[UnityBridge] No teams loaded for season initialization\n";
        return;
    }

    seasonManager = std::make_unique<bm::engine::SeasonManager>();
    seasonManager->InitializeSeason(year, "NBA");
    seasonManager->LoadTeams(teams);
    seasonManager->GenerateSchedule();
    currentSeasonDay = 1;
    std::cout << "[UnityBridge] Season initialized with " << teams.size()
              << " teams and " << seasonManager->GetFixtureDayCount() << " fixture days\n";
}

void SimulateRound(int roundNumber) {
    if (!seasonManager) return;
    seasonManager->SimulateDay(roundNumber, true, nullptr, false, 6);
}

int GetCurrentSeasonDay() {
    return currentSeasonDay;
}

int GetTotalSeasonDays() {
    return seasonManager ? seasonManager->GetFixtureDayCount() : 0;
}

int SimulateSeasonDay(int speedMultiplier) {
    if (!seasonManager) return 0;
    int totalDays = seasonManager->GetFixtureDayCount();
    if (currentSeasonDay > totalDays) return 0;

    bool success = seasonManager->SimulateDay(currentSeasonDay, true, nullptr, false, speedMultiplier > 0 ? speedMultiplier : 6);
    if (success) {
        currentSeasonDay++;
        return 1;
    }
    return 0;
}

const char* GetSeasonCalendarJson() {
    std::ostringstream json;
    int totalDays = seasonManager ? seasonManager->GetFixtureDayCount() : 0;
    int gamesPlayed = seasonManager ? seasonManager->GetGamesPlayed() : 0;
    int totalGames = seasonManager ? seasonManager->GetTotalGames() : 0;
    bool isComplete = seasonManager ? seasonManager->IsSeasonComplete() : false;

    json << "{"
         << "\"currentDay\":" << currentSeasonDay
         << ",\"totalDays\":" << totalDays
         << ",\"gamesPlayed\":" << gamesPlayed
         << ",\"totalGames\":" << totalGames
         << ",\"isSeasonComplete\":" << (isComplete ? "true" : "false")
         << "}";

    calendarJson = json.str();
    return calendarJson.c_str();
}

const char* GetFixturesForDayJson(int dayNumber) {
    if (!seasonManager) {
        fixturesJson = "{\"dayNumber\":0,\"label\":\"No Season\",\"simulated\":false,\"fixtures\":[]}";
        return fixturesJson.c_str();
    }

    const auto* day = seasonManager->GetFixtureDay(dayNumber);
    if (!day) {
        fixturesJson = "{\"dayNumber\":" + std::to_string(dayNumber) + ",\"label\":\"Day " + std::to_string(dayNumber) + "\",\"simulated\":false,\"fixtures\":[]}";
        return fixturesJson.c_str();
    }

    std::ostringstream json;
    json << "{"
         << "\"dayNumber\":" << day->dayNumber
         << ",\"label\":\"" << EscapeJson(day->label) << "\""
         << ",\"simulated\":" << (day->simulated ? "true" : "false")
         << ",\"fixtures\":[";

    bool first = true;
    for (const auto& g : day->games) {
        if (!first) json << ",";
        first = false;
        json << "{"
             << "\"homeTeamId\":\"" << EscapeJson(g.homeTeam->teamId) << "\""
             << ",\"homeTeamName\":\"" << EscapeJson(g.homeTeam->name) << "\""
             << ",\"awayTeamId\":\"" << EscapeJson(g.awayTeam->teamId) << "\""
             << ",\"awayTeamName\":\"" << EscapeJson(g.awayTeam->name) << "\""
             << ",\"homeScore\":" << g.homeScore
             << ",\"awayScore\":" << g.awayScore
             << ",\"played\":" << (g.played ? "true" : "false")
             << "}";
    }
    json << "]}";

    fixturesJson = json.str();
    return fixturesJson.c_str();
}

const char* GetStandingsJson(const char* conference) {
    if (!seasonManager) {
        standingsJson = "{\"conference\":\"\",\"standings\":[]}";
        return standingsJson.c_str();
    }

    std::string confStr = conference ? conference : "";
    auto records = seasonManager->GetStandings(confStr);

    std::ostringstream json;
    json << "{"
         << "\"conference\":\"" << EscapeJson(confStr) << "\""
         << ",\"standings\":[";

    bool first = true;
    int rank = 1;
    for (const auto& rec : records) {
        if (!first) json << ",";
        first = false;
        json << "{"
             << "\"rank\":" << rank++
             << ",\"teamId\":\"" << EscapeJson(rec.team->teamId) << "\""
             << ",\"teamName\":\"" << EscapeJson(rec.team->name) << "\""
             << ",\"conference\":\"" << EscapeJson(rec.team->conferenceId) << "\""
             << ",\"wins\":" << rec.wins
             << ",\"losses\":" << rec.losses
             << ",\"homeWins\":" << rec.homeWins
             << ",\"homeLosses\":" << rec.homeLosses
             << ",\"awayWins\":" << rec.awayWins
             << ",\"awayLosses\":" << rec.awayLosses
             << ",\"pointsFor\":" << rec.pointsFor
             << ",\"pointsAgainst\":" << rec.pointsAgainst
             << ",\"winPercentage\":" << std::fixed << std::setprecision(3) << rec.winPercentage
             << ",\"confWins\":" << rec.conferenceWins
             << ",\"confLosses\":" << rec.conferenceLosses
             << "}";
    }
    json << "]}";

    standingsJson = json.str();
    return standingsJson.c_str();
}

int GeneratePlayoffBracket() {
    if (!seasonManager) {
        InitializeSeason(2026);
    }
    if (!seasonManager) return 0;
    seasonManager->GeneratePlayoffBracket();
    return 1;
}

int AdvancePlayoffRound(int speedMultiplier) {
    if (!seasonManager) {
        InitializeSeason(2026);
    }
    if (!seasonManager) return 0;
    if (!seasonManager->ArePlayoffsGenerated()) {
        seasonManager->GeneratePlayoffBracket();
    }
    return seasonManager->AdvancePlayoffRound(speedMultiplier > 0 ? speedMultiplier : 6) ? 1 : 0;
}

int SimulatePlayoffGame(const char* seriesId, int speedMultiplier) {
    if (!seasonManager) {
        InitializeSeason(2026);
    }
    if (!seasonManager || !seriesId) return 0;
    return seasonManager->SimulatePlayoffGame(seriesId, speedMultiplier > 0 ? speedMultiplier : 6) ? 1 : 0;
}

const char* GetPlayoffsBracketJson() {
    if (!seasonManager) {
        InitializeSeason(2026);
    }
    if (!seasonManager) {
        playoffsJson = "{\"generated\":false,\"isComplete\":false,\"currentRound\":0,\"series\":[]}";
        return playoffsJson.c_str();
    }

    if (!seasonManager->ArePlayoffsGenerated()) {
        seasonManager->GeneratePlayoffBracket();
    }

    const auto& bracket = seasonManager->GetPlayoffBracket();
    std::ostringstream json;
    json << "{"
         << "\"generated\":true"
         << ",\"isComplete\":" << (seasonManager->ArePlayoffsComplete() ? "true" : "false")
         << ",\"currentRound\":" << seasonManager->GetCurrentPlayoffRound()
         << ",\"series\":[";

    bool first = true;
    for (const auto& s : bracket) {
        if (!first) json << ",";
        first = false;
        json << "{"
             << "\"seriesId\":\"" << EscapeJson(s.seriesId) << "\""
             << ",\"roundName\":\"" << EscapeJson(s.roundName) << "\""
             << ",\"conference\":\"" << EscapeJson(s.conference) << "\""
             << ",\"roundIndex\":" << s.roundIndex
             << ",\"seriesIndex\":" << s.seriesIndex
             << ",\"team1Id\":\"" << (s.team1 ? EscapeJson(s.team1->teamId) : "") << "\""
             << ",\"team1Name\":\"" << (s.team1 ? EscapeJson(s.team1->name) : "TBD") << "\""
             << ",\"team1Seed\":" << s.team1Seed
             << ",\"team1Wins\":" << s.team1Wins
             << ",\"team2Id\":\"" << (s.team2 ? EscapeJson(s.team2->teamId) : "") << "\""
             << ",\"team2Name\":\"" << (s.team2 ? EscapeJson(s.team2->name) : "TBD") << "\""
             << ",\"team2Seed\":" << s.team2Seed
             << ",\"team2Wins\":" << s.team2Wins
             << ",\"targetWins\":" << s.targetWins
             << ",\"complete\":" << (s.complete ? "true" : "false")
             << ",\"winnerId\":\"" << (s.winner ? EscapeJson(s.winner->teamId) : "") << "\""
             << ",\"winnerName\":\"" << (s.winner ? EscapeJson(s.winner->name) : "") << "\""
             << ",\"games\":[";
        
        bool firstGame = true;
        for (const auto& g : s.games) {
            if (!firstGame) json << ",";
            firstGame = false;
            json << "{"
                 << "\"homeTeamName\":\"" << EscapeJson(g.homeTeam->name) << "\""
                 << ",\"awayTeamName\":\"" << EscapeJson(g.awayTeam->name) << "\""
                 << ",\"homeScore\":" << g.homeScore
                 << ",\"awayScore\":" << g.awayScore
                 << "}";
        }
        json << "]}";
    }
    json << "]}";

    playoffsJson = json.str();
    return playoffsJson.c_str();
}

int ArePlayoffsComplete() {
    return (seasonManager && seasonManager->ArePlayoffsComplete()) ? 1 : 0;
}

const char* GetPlayoffChampionJson() {
    if (!seasonManager || !seasonManager->ArePlayoffsComplete() || !seasonManager->GetChampion()) {
        championJson = "{\"hasChampion\":false}";
        return championJson.c_str();
    }

    auto champ = seasonManager->GetChampion();
    std::ostringstream json;
    json << "{"
         << "\"hasChampion\":true"
         << ",\"teamId\":\"" << EscapeJson(champ->teamId) << "\""
         << ",\"teamName\":\"" << EscapeJson(champ->name) << "\""
         << ",\"conference\":\"" << EscapeJson(champ->conferenceId) << "\""
         << "}";

    championJson = json.str();
    return championJson.c_str();
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

const char* GetTeamRosterDetailsJson(const char* teamId) {
    auto team = LoadTeam(teamId);
    if (!team) {
        teamRosterJson = "{\"teamId\":\"\",\"teamName\":\"\",\"players\":[]}";
        return teamRosterJson.c_str();
    }

    std::ostringstream json;
    json << "{\"teamId\":\"" << EscapeJson(team->teamId) << "\""
         << ",\"teamName\":\"" << EscapeJson(team->name) << "\""
         << ",\"conference\":\"" << EscapeJson(team->conferenceId) << "\""
         << ",\"prestige\":" << team->prestige
         << ",\"players\":[";

    bool first = true;
    for (const auto& p : team->roster) {
        if (!p) continue;
        if (!first) json << ",";
        first = false;
        int stamina = std::clamp(static_cast<int>((1.0f - p->fatigueLevel) * 100.0f), 0, 100);
        json << "{\"id\":\"" << EscapeJson(p->playerId) << "\""
             << ",\"firstName\":\"" << EscapeJson(p->firstName) << "\""
             << ",\"lastName\":\"" << EscapeJson(p->lastName) << "\""
             << ",\"name\":\"" << EscapeJson(p->firstName + " " + p->lastName) << "\""
             << ",\"pos\":\"" << PositionToStr(p->position) << "\""
             << ",\"jersey\":" << p->jerseyNumber
             << ",\"rating\":" << p->attributes.currentAbility
             << ",\"potential\":" << p->attributes.potentialAbility
             << ",\"pace\":" << p->attributes.pace
             << ",\"shooting\":" << p->attributes.shooting
             << ",\"ballControl\":" << p->attributes.ballControl
             << ",\"defense\":" << p->attributes.defense
             << ",\"physical\":" << p->attributes.physical
             << ",\"technical\":" << p->attributes.technical
             << ",\"stamina\":" << stamina
             << ",\"onCourt\":" << (p->onCourt ? "true" : "false")
             << "}";
    }
    json << "]}";

    teamRosterJson = json.str();
    return teamRosterJson.c_str();
}

void SetTeamTactics(int isHomeTeam, int offensiveStrategy, int defensiveStrategy, int pace) {
    if (!matchEngine) return;
    auto tm = matchEngine->GetTacticalManager();
    if (!tm) return;

    auto off = static_cast<bm::engine::OffensiveStrategy>(std::clamp(offensiveStrategy, 0, 5));
    auto def = static_cast<bm::engine::DefensiveStrategy>(std::clamp(defensiveStrategy, 0, 5));

    if (isHomeTeam != 0) {
        tm->SetHomeOffensiveStrategy(off);
        tm->SetHomeDefensiveStrategy(def);
    } else {
        tm->SetAwayOffensiveStrategy(off);
        tm->SetAwayDefensiveStrategy(def);
    }
}

int SwapMatchPlayers(int isHomeTeam, const char* playerOutId, const char* playerInId) {
    if (!matchEngine || !playerOutId || !playerInId) return 0;
    auto tm = matchEngine->GetTacticalManager();
    if (!tm) return 0;

    auto targetTeam = (isHomeTeam != 0) ? homeTeam : awayTeam;
    if (!targetTeam) return 0;

    std::shared_ptr<bm::Player> pOut = nullptr;
    std::shared_ptr<bm::Player> pIn = nullptr;

    for (auto& p : targetTeam->roster) {
        if (p->playerId == playerOutId) pOut = p;
        if (p->playerId == playerInId) pIn = p;
    }

    if (!pOut || !pIn) return 0;

    bool success = tm->ManualSubstitution(targetTeam, pOut, pIn, 0);
    if (success) {
        UpdateStateJson();
        return 1;
    }
    return 0;
}

const char* GetEngineVersion() {
    return ENGINE_VERSION;
}

} // extern "C"
