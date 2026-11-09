#include "bindings/UnityBridge.h"
#include "db/DatabaseManager.h"
#include "db/LeagueImport.h"
#include "engine/CareerManager.h"
#include "engine/MatchEngine.h"
#include "engine/SeasonManager.h"
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::shared_ptr<bm::db::DatabaseManager> database;
std::unique_ptr<bm::engine::MatchEngine> matchEngine;
std::unique_ptr<bm::engine::SeasonManager> seasonManager;
std::unique_ptr<bm::engine::CareerManager> career;
bool coachGameUserHome = true;
std::string careerJson;
std::string careerStopJson;
std::string careerScheduleJson;
std::string contractsJson;
std::string freeAgentsJson;
std::string draftJson;
std::string historyJson;
std::string careerSavePath;
std::string careerSaveError;   // empty while saving works; shown to the player
std::string summerJson;
std::string careerMessage;
std::string leagueMessage;
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
std::string talkJson;
std::string coachError;
std::string playbookJson;
std::string feedbackJson;

const char* BreakName(bm::engine::BreakType type) {
    using bm::engine::BreakType;
    switch (type) {
        case BreakType::PreMatch: return "prematch";
        case BreakType::QuarterBreak: return "quarter";
        case BreakType::Halftime: return "halftime";
        case BreakType::Overtime: return "overtime";
        case BreakType::Timeout: return "timeout";
        case BreakType::ShortTimeout: return "shorttimeout";
        default: return "none";
    }
}
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
        "pace, shooting, ball_control, defense, physical, technical, current_ability, "
        "potential_ability, birth_date, height, weight "
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
        player->attributes.potentialAbility = std::max(player->attributes.currentAbility, sqlite3_column_int(roster, 12));
        const char* birth = reinterpret_cast<const char*>(sqlite3_column_text(roster, 13));
        player->birthDate = birth ? birth : "2000-01-01";
        player->height = static_cast<float>(sqlite3_column_double(roster, 14));
        player->weight = static_cast<float>(sqlite3_column_double(roster, 15));
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
             << ",\"instr\":" << (matchEngine ? static_cast<int>(matchEngine->GetInstruction(p->playerId)) : 0)
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
         << "\",\"periodBreak\":" << (matchEngine->IsPeriodBreak() ? "true" : "false")
         << ",\"breakType\":\"" << BreakName(matchEngine->GetBreakType()) << "\""
         << ",\"breakHome\":" << (matchEngine->IsBreakHomeTeam() ? "true" : "false")
         << ",\"talkGiven\":" << (matchEngine->IsTalkGiven() ? "true" : "false")
         << ",\"pendingTimeoutHome\":" << matchEngine->GetPendingTimeout(true)
         << ",\"pendingTimeoutAway\":" << matchEngine->GetPendingTimeout(false)
         << ",\"canTimeoutHome\":" << (matchEngine->CanCallTimeout(true) ? "true" : "false")
         << ",\"homeOffenseScheme\":" << static_cast<int>(matchEngine->GetTacticalManager()->GetHomeOffensiveStrategy())
         << ",\"homeOffenseTarget\":\"" << EscapeJson(matchEngine->GetOffenseTarget(true)) << "\""
         << ",\"homeOffensePlay\":\"" << EscapeJson(matchEngine->GetOffensePlay(true)) << "\""
         << ",\"homeDefenseScheme\":" << static_cast<int>(matchEngine->GetTacticalManager()->GetHomeDefensiveStrategy())
         << ",\"pendingSubs\":" << matchEngine->GetPendingSubstitutions()
         << ",\"feedTotal\":" << matchEngine->GetFeedTotal()
         << ",\"feed\":[";
    {
        // The newest lines; the UI keeps its own cursor into feedTotal so nothing is shown twice
        const auto& feed = matchEngine->GetFeed();
        const size_t first = feed.size() > 60 ? feed.size() - 60 : 0;
        for (size_t i = first; i < feed.size(); ++i) {
            if (i > first) json << ",";
            json << "\"" << EscapeJson(feed[i]) << "\"";
        }
    }
    json << "],\"homePlayers\":";
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

// ---- Career helpers (dated year, contracts, draft, free agency) ----

bm::engine::CareerStop careerLastStop;

bm::engine::SeasonManager* ActiveSeason() {
    return career ? &career->Season() : seasonManager.get();
}

int CareerCalendarDay() {
    if (!career) return 1;
    using bm::engine::CareerPhase;
    switch (career->Phase()) {
        case CareerPhase::Preseason: return 1;
        case CareerPhase::RegularSeason:
            return std::clamp(career->RegularSeasonDay(), 1, std::max(1, career->RegularSeasonLength()));
        default: return std::max(1, career->RegularSeasonLength());
    }
}

std::string StopJson(const bm::engine::CareerStop& stop) {
    std::ostringstream json;
    json << "{\"kind\":\"" << EscapeJson(stop.kind) << "\",\"title\":\"" << EscapeJson(stop.title)
         << "\",\"detail\":\"" << EscapeJson(stop.detail) << "\",\"blocking\":" << (stop.blocking ? "true" : "false")
         << ",\"playerId\":\"" << EscapeJson(stop.playerId) << "\",\"options\":[";
    for (size_t i = 0; i < stop.options.size(); ++i) json << (i ? "," : "") << "\"" << EscapeJson(stop.options[i]) << "\"";
    json << "]}";
    return json.str();
}

// One player as shown in contract, free agent and draft lists; quotes[y-1] is the salary he asks for y years
std::string PlayerCardJson(const bm::Player& p, bool withQuotes, bool ownPlayer) {
    std::ostringstream json;
    json << "{\"id\":\"" << EscapeJson(p.playerId) << "\",\"name\":\"" << EscapeJson(p.firstName + " " + p.lastName)
         << "\",\"pos\":\"" << PositionToStr(p.position) << "\",\"age\":" << p.age
         << ",\"rating\":" << p.attributes.currentAbility << ",\"potential\":" << p.attributes.potentialAbility
         << ",\"salary\":" << p.salary << ",\"endYear\":" << p.contractEndYear
         << ",\"jersey\":" << p.jerseyNumber
         << ",\"height\":" << std::fixed << std::setprecision(2) << p.height
         << ",\"weight\":" << std::setprecision(0) << p.weight;
    if (career && withQuotes) {
        json << ",\"preferredYears\":" << career->PreferredYears(p) << ",\"quotes\":[";
        for (int y = 1; y <= 5; ++y) json << (y > 1 ? "," : "") << career->Quote(p, y, ownPlayer);
        json << "]";
    }
    json << "}";
    return json.str();
}

std::string BuildCareerJson() {
    if (!career || !career->IsStarted()) return "{\"started\":false}";
    using bm::engine::CareerPhase;
    auto* season = &career->Season();
    auto userTeam = career->UserTeam();
    const std::string userId = career->UserTeamId();

    int wins = 0, losses = 0, confRank = 0, leagueRank = 0;
    std::string conference = userTeam ? userTeam->conferenceId : "";
    {
        auto all = season->GetStandings("");
        for (size_t i = 0; i < all.size(); ++i) {
            if (all[i].team->teamId == userId) { wins = all[i].wins; losses = all[i].losses; leagueRank = static_cast<int>(i) + 1; }
        }
        auto conf = season->GetStandings(conference);
        for (size_t i = 0; i < conf.size(); ++i) if (conf[i].team->teamId == userId) confRank = static_cast<int>(i) + 1;
    }

    std::ostringstream json;
    const int year = career->SeasonYear();
    json << "{\"started\":true,\"date\":\"" << EscapeJson(career->DateLabel()) << "\",\"dayIndex\":" << career->Today()
         << ",\"year\":" << year << ",\"seasonLabel\":\"" << year << "/" << std::setw(2) << std::setfill('0') << ((year + 1) % 100)
         << std::setfill(' ') << "\",\"phase\":\"" << bm::engine::PhaseName(career->Phase()) << "\",\"phaseId\":" << static_cast<int>(career->Phase())
         << ",\"userTeamId\":\"" << EscapeJson(userId) << "\",\"userTeamName\":\"" << EscapeJson(userTeam ? userTeam->name : "")
         << "\",\"conference\":\"" << EscapeJson(conference) << "\",\"wins\":" << wins << ",\"losses\":" << losses
         << ",\"confRank\":" << confRank << ",\"leagueRank\":" << leagueRank
         << ",\"regularDay\":" << career->RegularSeasonDay() << ",\"regularDays\":" << career->RegularSeasonLength()
         << ",\"gamesPlayed\":" << season->GetGamesPlayed() << ",\"totalGames\":" << season->GetTotalGames()
         << ",\"totalWins\":" << (career->CompletedWins() + (career->SeasonCounted() ? 0 : wins))
         << ",\"totalLosses\":" << (career->CompletedLosses() + (career->SeasonCounted() ? 0 : losses))
         << ",\"titles\":" << career->TitlesWon()
         << ",\"nextMilestone\":\"" << EscapeJson(career->NextMilestoneLabel()) << "\",\"daysToMilestone\":" << career->DaysToNextMilestone()
         << ",\"playoffsVisible\":" << (career->PlayoffsVisible() ? "true" : "false")
         << ",\"champion\":\"" << EscapeJson(career->ChampionName()) << "\"";

    // What is waiting: an info message that has not been dismissed, otherwise a blocking decision
    bm::engine::CareerStop stop = (careerLastStop.kind != "none" && !careerLastStop.kind.empty() && !careerLastStop.blocking)
        ? careerLastStop : career->CurrentStop();
    if (stop.kind.empty()) stop.kind = "none";
    json << ",\"stop\":" << StopJson(stop);

    auto game = career->PendingCoachGame();
    json << ",\"coachGame\":{\"pending\":" << (game.pending ? "true" : "false");
    if (game.pending) {
        json << ",\"kind\":\"" << game.kind << "\",\"label\":\"" << EscapeJson(game.label) << "\",\"homeId\":\"" << EscapeJson(game.home->teamId)
             << "\",\"homeName\":\"" << EscapeJson(game.home->name) << "\",\"awayId\":\"" << EscapeJson(game.away->teamId)
             << "\",\"awayName\":\"" << EscapeJson(game.away->name) << "\",\"userIsHome\":" << (game.home->teamId == userId ? "true" : "false");
    }
    json << "}";

    // The user's next fixture and recent results
    std::ostringstream recent;
    bool firstRecent = true;
    std::string nextJson = "{\"has\":false}";
    bool haveNext = false;
    int lastDayWithResults = 0;
    std::vector<std::string> recentList;
    for (int d = 1; d <= season->GetFixtureDayCount(); ++d) {
        const auto* day = season->GetFixtureDay(d);
        if (!day) continue;
        if (day->simulated) lastDayWithResults = d;
        for (const auto& g : day->games) {
            const bool home = g.homeTeam->teamId == userId;
            if (!home && g.awayTeam->teamId != userId) continue;
            const auto& opp = home ? g.awayTeam : g.homeTeam;
            if (g.played) {
                const int mine = home ? g.homeScore : g.awayScore;
                const int theirs = home ? g.awayScore : g.homeScore;
                std::ostringstream one;
                one << "{\"date\":\"" << EscapeJson(career->FixtureDateLabel(d)) << "\",\"opponent\":\"" << EscapeJson(opp->name)
                    << "\",\"home\":" << (home ? "true" : "false") << ",\"win\":" << (mine > theirs ? "true" : "false")
                    << ",\"score\":\"" << mine << "-" << theirs << "\"}";
                recentList.push_back(one.str());
            } else if (!haveNext) {
                haveNext = true;
                std::ostringstream one;
                one << "{\"has\":true,\"date\":\"" << EscapeJson(career->FixtureDateLabel(d)) << "\",\"opponent\":\"" << EscapeJson(opp->name)
                    << "\",\"home\":" << (home ? "true" : "false") << "}";
                nextJson = one.str();
            }
        }
    }
    json << ",\"nextGame\":" << nextJson << ",\"recent\":[";
    const size_t from = recentList.size() > 5 ? recentList.size() - 5 : 0;
    for (size_t i = recentList.size(); i > from; --i) {
        json << (firstRecent ? "" : ",") << recentList[i - 1];
        firstRecent = false;
    }
    json << "],\"leagueResults\":[";
    if (lastDayWithResults > 0) {
        bool first = true;
        for (const auto& g : season->GetFixtureDay(lastDayWithResults)->games) {
            if (!g.played) continue;
            json << (first ? "" : ",") << "{\"home\":\"" << EscapeJson(g.homeTeam->name) << "\",\"away\":\"" << EscapeJson(g.awayTeam->name)
                 << "\",\"homeScore\":" << g.homeScore << ",\"awayScore\":" << g.awayScore << "}";
            first = false;
        }
    }
    json << "],\"resultsDate\":\"" << EscapeJson(lastDayWithResults > 0 ? career->FixtureDateLabel(lastDayWithResults) : "") << "\"";

    json << ",\"inbox\":[";
    int shown = 0;
    for (const auto& ev : career->Inbox()) {
        if (shown++ >= 15) break;
        json << (shown > 1 ? "," : "") << "{\"date\":\"" << EscapeJson(bm::engine::FormatDate(ev.day)) << "\",\"title\":\"" << EscapeJson(ev.title)
             << "\",\"text\":\"" << EscapeJson(ev.text) << "\"}";
    }
    json << "]";

    const int payroll = career->Payroll(userId);
    json << ",\"payroll\":" << payroll << ",\"cap\":" << career->SalaryCap() << ",\"luxury\":" << career->LuxuryLine()
         << ",\"rosterSize\":" << (userTeam ? userTeam->roster.size() : 0) << ",\"minRoster\":" << bm::engine::CareerManager::MinRosterSize()
         << ",\"maxRoster\":" << bm::engine::CareerManager::MaxRosterSize()
         << ",\"expiring\":" << career->ExpiringPlayers().size()
         << ",\"draftReady\":" << (career->DraftReady() ? "true" : "false") << ",\"draftDone\":" << (career->DraftFinished() ? "true" : "false")
         << ",\"userOnClock\":" << (career->UserOnClock() ? "true" : "false")
         << ",\"summerMode\":" << career->SummerLeagueMode() << ",\"inSummer\":" << (career->InSummerLeague() ? "true" : "false")
         << ",\"campPlanned\":" << (career->TrainingPlanSet() ? "true" : "false")
         << ",\"campFocus\":\"" << EscapeJson(career->CampFocus()) << "\",\"campIntensity\":" << career->CampIntensity()
         << ",\"faOpen\":" << (career->FreeAgencyOpen() ? "true" : "false") << ",\"freeAgentCount\":" << career->FreeAgents().size()
         << "}";
    return json.str();
}

std::string BuildScheduleJson() {
    if (!career || !career->IsStarted()) return "{\"games\":[]}";
    auto* season = &career->Season();
    const std::string userId = career->UserTeamId();
    std::ostringstream json;
    json << "{\"games\":[";
    bool first = true;
    for (int d = 1; d <= season->GetFixtureDayCount(); ++d) {
        const auto* day = season->GetFixtureDay(d);
        if (!day) continue;
        for (const auto& g : day->games) {
            const bool home = g.homeTeam->teamId == userId;
            if (!home && g.awayTeam->teamId != userId) continue;
            json << (first ? "" : ",") << "{\"day\":" << d << ",\"date\":\"" << EscapeJson(career->FixtureDateLabel(d))
                 << "\",\"opponent\":\"" << EscapeJson((home ? g.awayTeam : g.homeTeam)->name) << "\",\"home\":" << (home ? "true" : "false")
                 << ",\"played\":" << (g.played ? "true" : "false") << ",\"hasBox\":" << (career->FindGameRecord(career->SeasonYear(), d) ? "true" : "false")
                 << ",\"mine\":" << (home ? g.homeScore : g.awayScore)
                 << ",\"theirs\":" << (home ? g.awayScore : g.homeScore) << "}";
            first = false;
        }
    }
    json << "]}";
    return json.str();
}

std::string BuildContractsJson() {
    if (!career || !career->IsStarted()) return "{\"players\":[]}";
    auto team = career->UserTeam();
    std::vector<std::shared_ptr<bm::Player>> roster = team ? team->roster : std::vector<std::shared_ptr<bm::Player>>();
    std::sort(roster.begin(), roster.end(), [](const auto& a, const auto& b) { return a->attributes.currentAbility > b->attributes.currentAbility; });
    std::ostringstream json;
    json << "{\"year\":" << career->SeasonYear() << ",\"payroll\":" << career->Payroll(career->UserTeamId())
         << ",\"cap\":" << career->SalaryCap() << ",\"luxury\":" << career->LuxuryLine() << ",\"players\":[";
    for (size_t i = 0; i < roster.size(); ++i) {
        const auto& p = roster[i];
        std::string card = PlayerCardJson(*p, true, true);
        card.pop_back();
        card += std::string(",\"expiring\":") + (p->contractEndYear <= career->SeasonYear() ? "true" : "false") + "}";
        json << (i ? "," : "") << card;
    }
    json << "]}";
    return json.str();
}

std::string BuildFreeAgentsJson() {
    if (!career || !career->IsStarted()) return "{\"players\":[]}";
    auto pool = career->FreeAgents();
    std::sort(pool.begin(), pool.end(), [](const auto& a, const auto& b) {
        return a->attributes.currentAbility + 0.3 * std::max(0, a->attributes.potentialAbility - a->attributes.currentAbility) * (a->age <= 24)
             > b->attributes.currentAbility + 0.3 * std::max(0, b->attributes.potentialAbility - b->attributes.currentAbility) * (b->age <= 24);
    });
    const int room = career->SalaryCap() - career->Payroll(career->UserTeamId());
    std::ostringstream json;
    json << "{\"open\":" << (career->FreeAgencyOpen() ? "true" : "false") << ",\"capRoom\":" << room
         << ",\"minimum\":" << bm::engine::CareerManager::MinimumSalary()
         << ",\"rosterSize\":" << (career->UserTeam() ? career->UserTeam()->roster.size() : 0) << ",\"players\":[";
    const size_t limit = std::min<size_t>(pool.size(), 150);
    for (size_t i = 0; i < limit; ++i) json << (i ? "," : "") << PlayerCardJson(*pool[i], true, false);
    json << "]}";
    return json.str();
}

std::string BuildDraftJson() {
    if (!career || !career->IsStarted()) return "{\"ready\":false}";
    std::ostringstream json;
    json << "{\"ready\":" << (career->DraftReady() ? "true" : "false") << ",\"done\":" << (career->DraftFinished() ? "true" : "false")
         << ",\"cursor\":" << career->DraftCursor() << ",\"total\":" << career->DraftTotalPicks()
         << ",\"userOnClock\":" << (career->UserOnClock() ? "true" : "false");
    auto onClock = career->FindTeam(career->DraftTeamOnClock());
    json << ",\"onClock\":\"" << EscapeJson(onClock ? onClock->name : "") << "\",\"yourPicks\":[";
    bool first = true;
    const auto& order = career->DraftOrder();
    for (size_t i = static_cast<size_t>(career->DraftCursor()); i < order.size(); ++i) {
        if (order[i] != career->UserTeamId()) continue;
        json << (first ? "" : ",") << (i + 1);
        first = false;
    }
    json << "],\"prospects\":[";
    auto prospects = career->Prospects();
    std::sort(prospects.begin(), prospects.end(), [](const auto& a, const auto& b) {
        return a->attributes.potentialAbility + a->attributes.currentAbility > b->attributes.potentialAbility + b->attributes.currentAbility;
    });
    const size_t limit = std::min<size_t>(prospects.size(), 90);
    for (size_t i = 0; i < limit; ++i) json << (i ? "," : "") << PlayerCardJson(*prospects[i], false, false);
    json << "],\"results\":[";
    const auto& results = career->DraftResults();
    for (size_t i = 0; i < results.size(); ++i) {
        auto team = career->FindTeam(results[i].teamId);
        json << (i ? "," : "") << "{\"pick\":" << results[i].pick << ",\"team\":\"" << EscapeJson(team ? team->name : "")
             << "\",\"teamId\":\"" << EscapeJson(results[i].teamId) << "\",\"player\":\"" << EscapeJson(results[i].playerName)
             << "\",\"playerId\":\"" << EscapeJson(results[i].playerId) << "\"}";
    }
    json << "]}";
    return json.str();
}

std::string BuildSummerJson() {
    if (!career || !career->IsStarted()) return "{\"games\":[]}";
    const std::string userId = career->UserTeamId();
    std::ostringstream json;
    json << "{\"mode\":" << career->SummerLeagueMode() << ",\"active\":" << (career->InSummerLeague() ? "true" : "false")
         << ",\"started\":" << (career->SummerGames().empty() ? "false" : "true") << ",\"games\":[";
    bool first = true;
    int wins = 0, played = 0;
    for (const auto& g : career->SummerGames()) {
        const bool home = g.home->teamId == userId;
        if (!home && g.away->teamId != userId) continue;
        const int mine = home ? g.homeScore : g.awayScore;
        const int theirs = home ? g.awayScore : g.homeScore;
        if (g.played) { ++played; if (mine > theirs) ++wins; }
        json << (first ? "" : ",") << "{\"date\":\"" << EscapeJson(bm::engine::FormatDate(g.day)) << "\",\"opponent\":\""
             << EscapeJson((home ? g.away : g.home)->name) << "\",\"played\":" << (g.played ? "true" : "false")
             << ",\"mine\":" << mine << ",\"theirs\":" << theirs << "}";
        first = false;
    }
    json << "],\"wins\":" << wins << ",\"played\":" << played << ",\"roster\":[";
    auto roster = career->SummerRoster(userId);
    for (size_t i = 0; i < roster.size(); ++i) json << (i ? "," : "") << PlayerCardJson(*roster[i], false, false);
    json << "]}";
    return json.str();
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
    matchEngine->SetHeadless(true);
    matchEngine->InitializeMatch(homeTeam, awayTeam);
    matchEngine->SetBreaksEnabled(true);
    UpdateStateJson();
}

int RequestTimeout(int isHomeTeam, int full) {
    if (!matchEngine) return 0;
    const int ok = matchEngine->RequestTimeout(isHomeTeam != 0, full != 0) ? 1 : 0;
    UpdateStateJson();
    return ok;
}

void EndMatchBreak() {
    if (!matchEngine) return;
    matchEngine->EndBreak();
    UpdateStateJson();
}

int ApplyTeamTalk(int isHomeTeam, int talkType, int coachMotivation) {
    if (!matchEngine || talkType < 0 || talkType > 6) return 0;
    const int ok = matchEngine->ApplyTeamTalk(isHomeTeam != 0, static_cast<bm::engine::TeamTalkType>(talkType),
                                              coachMotivation) ? 1 : 0;
    UpdateStateJson();
    return ok;
}

const char* GetTeamTalkJson() {
    std::ostringstream json;
    if (!matchEngine || !matchEngine->GetLastTeamTalk().valid) {
        talkJson = "{\"valid\":false,\"summary\":\"\",\"reactions\":[]}";
        return talkJson.c_str();
    }
    const auto& talk = matchEngine->GetLastTeamTalk();
    json << "{\"valid\":true,\"type\":" << static_cast<int>(talk.type)
         << ",\"summary\":\"" << EscapeJson(talk.summary) << "\",\"reactions\":[";
    for (size_t i = 0; i < talk.reactions.size(); ++i) {
        const auto& r = talk.reactions[i];
        if (i > 0) json << ",";
        json << "{\"id\":\"" << EscapeJson(r.playerId) << "\",\"name\":\"" << EscapeJson(r.name)
             << "\",\"mood\":" << r.mood << ",\"delta\":" << std::fixed << std::setprecision(2) << r.delta << "}";
    }
    json << "]}";
    talkJson = json.str();
    return talkJson.c_str();
}

int StepMatchSeconds(int seconds) {
    if (!matchEngine || !matchEngine->GetMatchState() || seconds <= 0) return 0;
    const int played = matchEngine->StepSeconds(std::min(seconds, 600));
    UpdateStateJson();
    return played;
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
    if (career || !seasonManager) return;
    seasonManager->SimulateDay(roundNumber, true, nullptr, false, 6);
}

int GetCurrentSeasonDay() {
    if (career) return CareerCalendarDay();
    return currentSeasonDay;
}

int GetTotalSeasonDays() {
    auto* season = ActiveSeason();
    return season ? season->GetFixtureDayCount() : 0;
}

int SimulateSeasonDay(int speedMultiplier) {
    if (career || !seasonManager) return 0;
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
    auto* season = ActiveSeason();
    int totalDays = season ? season->GetFixtureDayCount() : 0;
    int gamesPlayed = season ? season->GetGamesPlayed() : 0;
    int totalGames = season ? season->GetTotalGames() : 0;
    bool isComplete = season ? season->IsSeasonComplete() : false;

    json << "{"
         << "\"currentDay\":" << (career ? CareerCalendarDay() : currentSeasonDay)
         << ",\"totalDays\":" << totalDays
         << ",\"gamesPlayed\":" << gamesPlayed
         << ",\"totalGames\":" << totalGames
         << ",\"isSeasonComplete\":" << (isComplete ? "true" : "false")
         << "}";

    calendarJson = json.str();
    return calendarJson.c_str();
}

const char* GetFixturesForDayJson(int dayNumber) {
    auto* season = ActiveSeason();
    if (!season) {
        fixturesJson = "{\"dayNumber\":0,\"label\":\"No Season\",\"simulated\":false,\"fixtures\":[]}";
        return fixturesJson.c_str();
    }

    const auto* day = season->GetFixtureDay(dayNumber);
    if (!day) {
        fixturesJson = "{\"dayNumber\":" + std::to_string(dayNumber) + ",\"label\":\"Day " + std::to_string(dayNumber) + "\",\"simulated\":false,\"fixtures\":[]}";
        return fixturesJson.c_str();
    }

    const std::string dayLabel = career ? career->FixtureDateLabel(day->dayNumber) : day->label;
    std::ostringstream json;
    json << "{"
         << "\"dayNumber\":" << day->dayNumber
         << ",\"label\":\"" << EscapeJson(dayLabel) << "\""
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
    auto* season = ActiveSeason();
    if (!season) {
        standingsJson = "{\"conference\":\"\",\"standings\":[]}";
        return standingsJson.c_str();
    }

    std::string confStr = conference ? conference : "";
    auto records = season->GetStandings(confStr);

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
    if (career) return 0;   // the calendar decides when the playoffs start
    if (!seasonManager) {
        InitializeSeason(2026);
    }
    if (!seasonManager) return 0;
    seasonManager->GeneratePlayoffBracket();
    return 1;
}

int AdvancePlayoffRound(int speedMultiplier) {
    if (career) return 0;
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
    if (career) return 0;
    if (!seasonManager) {
        InitializeSeason(2026);
    }
    if (!seasonManager || !seriesId) return 0;
    return seasonManager->SimulatePlayoffGame(seriesId, speedMultiplier > 0 ? speedMultiplier : 6) ? 1 : 0;
}

const char* GetPlayoffsBracketJson() {
    if (!career && !seasonManager) {
        InitializeSeason(2026);
    }
    auto* season = ActiveSeason();
    if (!season || (career && !season->ArePlayoffsGenerated())) {
        playoffsJson = "{\"generated\":false,\"isComplete\":false,\"currentRound\":0,\"series\":[]}";
        return playoffsJson.c_str();
    }

    if (!season->ArePlayoffsGenerated()) {
        season->GeneratePlayoffBracket();
    }

    const auto& bracket = season->GetPlayoffBracket();
    std::ostringstream json;
    json << "{"
         << "\"generated\":true"
         << ",\"isComplete\":" << (season->ArePlayoffsComplete() ? "true" : "false")
         << ",\"currentRound\":" << season->GetCurrentPlayoffRound()
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
    auto* season = ActiveSeason();
    return (season && season->ArePlayoffsComplete()) ? 1 : 0;
}

const char* GetPlayoffChampionJson() {
    auto* season = ActiveSeason();
    if (!season || !season->ArePlayoffsComplete() || !season->GetChampion()) {
        championJson = "{\"hasChampion\":false}";
        return championJson.c_str();
    }

    auto champ = season->GetChampion();
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
    bool hasColors = false;
    if (auto info = database->PrepareStatement("PRAGMA table_info(teams)")) {
        while (sqlite3_step(info) == SQLITE_ROW) {
            const char* column = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
            if (column && std::string(column) == "away_color") hasColors = true;
        }
        sqlite3_finalize(info);
    }
    auto stmt = database->PrepareStatement(hasColors
        ? "SELECT id, name, conference, prestige, away_color, third_color FROM teams ORDER BY id"
        : "SELECT id, name, conference, prestige, '', '' FROM teams ORDER BY id");
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
        const char* awayColor = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const char* thirdColor = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        json << "{\"id\":\"" << id
             << "\",\"name\":\"" << EscapeJson(name ? name : "")
             << "\",\"conference\":\"" << EscapeJson(conf ? conf : "")
             << "\",\"awayColor\":\"" << EscapeJson(awayColor ? awayColor : "")
             << "\",\"thirdColor\":\"" << EscapeJson(thirdColor ? thirdColor : "")
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
    // During a career the live (in-memory) roster is the truth: it has signings, draft picks and progression
    std::shared_ptr<bm::Team> team = career && teamId ? career->FindTeam(teamId) : nullptr;
    if (!team) team = LoadTeam(teamId);
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
             << ",\"age\":" << p->age
             << ",\"salary\":" << p->salary
             << ",\"contractEnd\":" << p->contractEndYear
             << ",\"stamina\":" << stamina
             << ",\"onCourt\":" << (p->onCourt ? "true" : "false")
             << ",\"injured\":" << (p->injured ? "true" : "false")
             << ",\"mood\":" << std::fixed << std::setprecision(2) << p->seasonMood
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

    auto off = static_cast<bm::engine::OffensiveStrategy>(std::clamp(offensiveStrategy, 0, 8));
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

    // Queued until the next dead ball (immediate during breaks)
    const bool ok = matchEngine->RequestSubstitution(isHomeTeam != 0, playerOutId, playerInId);
    UpdateStateJson();
    return ok ? 1 : 0;
}

int SetOffenseScheme(int isHomeTeam, int scheme, const char* targetPlayerId, const char* playId) {
    if (!matchEngine || scheme < 0 || scheme > 8) return 0;
    std::string error;
    const bool ok = matchEngine->SetOffenseScheme(isHomeTeam != 0, static_cast<bm::engine::OffensiveStrategy>(scheme),
                                                  targetPlayerId ? targetPlayerId : "", playId ? playId : "", &error);
    coachError = ok ? std::string() : error;
    UpdateStateJson();
    return ok ? 1 : 0;
}

const char* GetCoachError() {
    return coachError.c_str();
}

int SetDefenseScheme(int isHomeTeam, int scheme) {
    if (!matchEngine || scheme < 0 || scheme > 5) return 0;
    auto tm = matchEngine->GetTacticalManager();
    const auto def = static_cast<bm::engine::DefensiveStrategy>(scheme);
    if (isHomeTeam != 0) tm->SetHomeDefensiveStrategy(def);
    else tm->SetAwayDefensiveStrategy(def);
    UpdateStateJson();
    return 1;
}

const char* GetPlaybookJson() {
    std::ostringstream json;
    json << "{\"plays\":[";
    bool first = true;
    for (const auto& play : bm::engine::MatchEngine::GetPlaybook()) {
        if (!first) json << ",";
        first = false;
        json << "{\"id\":\"" << EscapeJson(play.id) << "\",\"name\":\"" << EscapeJson(play.name)
             << "\",\"scheme\":" << static_cast<int>(play.scheme) << "}";
    }
    json << "]}";
    playbookJson = json.str();
    return playbookJson.c_str();
}

int GiveFeedback(int isHomeTeam, const char* playerId, int instruction, int coachMotivation) {
    if (!matchEngine || !playerId || instruction < 1 || instruction > 3) return 0;
    const bool ok = matchEngine->GiveFeedback(isHomeTeam != 0, playerId,
                                              static_cast<bm::engine::PlayerInstruction>(instruction), coachMotivation);
    UpdateStateJson();
    return ok ? 1 : 0;
}

const char* GetFeedbackJson() {
    std::ostringstream json;
    if (!matchEngine || !matchEngine->GetLastFeedback().valid) {
        feedbackJson = "{\"valid\":false}";
        return feedbackJson.c_str();
    }
    const auto& fb = matchEngine->GetLastFeedback();
    json << "{\"valid\":true,\"id\":\"" << EscapeJson(fb.playerId) << "\",\"name\":\"" << EscapeJson(fb.name)
         << "\",\"instruction\":" << static_cast<int>(fb.instruction) << ",\"mood\":" << fb.mood
         << ",\"text\":\"" << EscapeJson(fb.text) << "\"}";
    feedbackJson = json.str();
    return feedbackJson.c_str();
}

const char* GetEngineVersion() {
    return ENGINE_VERSION;
}

// ---- League files ----

int ImportLeagueFolder(const char* folder, const char* outDbPath) {
    if (!folder || !outDbPath) { leagueMessage = "No folder given"; return 0; }
    auto result = bm::db::ImportLeagueFromCsv(folder, outDbPath);
    if (!result.ok) {
        leagueMessage.clear();
        for (const auto& e : result.errors) leagueMessage += e + "\n";
        return 0;
    }
    leagueMessage = "League is valid: " + std::to_string(result.teams) + " teams and " + std::to_string(result.players) + " players.";
    return 1;
}

int GenerateRandomLeagueDb(const char* outDbPath, int seed) {
    if (!outDbPath) { leagueMessage = "No file given"; return 0; }
    auto result = bm::db::GenerateRandomLeague(outDbPath, static_cast<unsigned>(seed));
    if (!result.ok) {
        leagueMessage.clear();
        for (const auto& e : result.errors) leagueMessage += e + "\n";
        return 0;
    }
    leagueMessage = "Random league created: " + std::to_string(result.teams) + " teams and " + std::to_string(result.players) + " players.";
    return 1;
}

int ExportLeagueTemplate(const char* folder) {
    if (!folder || !database || !database->IsOpen()) { leagueMessage = "No league is loaded"; return 0; }
    std::string error;
    if (!bm::db::ExportLeagueTemplate(database->GetConnection(), folder, &error)) { leagueMessage = error; return 0; }
    leagueMessage = std::string("Template written to ") + folder;
    return 1;
}

const char* GetLeagueMessage() {
    return leagueMessage.c_str();
}

// ---- Career: a dated year with contracts, draft, free agency, summer league and camp ----

int CareerStart(const char* userTeamId, int startYear, int seed) {
    if (!database || !database->IsOpen() || !userTeamId) { careerMessage = "The league database is not loaded"; return 0; }
    std::vector<std::shared_ptr<bm::Team>> teams;
    auto stmt = database->PrepareStatement("SELECT id FROM teams ORDER BY id");
    if (stmt) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto team = LoadTeam(std::to_string(sqlite3_column_int(stmt, 0)).c_str());
            if (team) teams.push_back(team);
        }
        sqlite3_finalize(stmt);
    }
    bool found = false;
    for (const auto& t : teams) if (t->teamId == userTeamId) found = true;
    if (teams.size() < 4 || !found) { careerMessage = "The team could not be found in this league"; return 0; }

    seasonManager.reset();
    career = std::make_unique<bm::engine::CareerManager>();
    bm::engine::CareerManager::Config config;
    config.userTeamId = userTeamId;
    config.startYear = startYear;
    config.seed = static_cast<unsigned>(seed);
    career->Start(teams, config);
    careerLastStop = bm::engine::CareerStop();
    careerLastStop.kind = "none";
    careerMessage.clear();
    return 1;
}

static bool WriteCareerSave(const std::string& path, std::string* error) {
    if (!career || path.empty()) { if (error) *error = "No career or file"; return false; }
    // The previous save stays next to the file as a fallback
    std::error_code copyError;
    if (std::filesystem::exists(path, copyError)) std::filesystem::copy_file(path, path + ".bak", std::filesystem::copy_options::overwrite_existing, copyError);
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        if (error) *error = db ? sqlite3_errmsg(db) : "cannot open the file";
        if (db) sqlite3_close(db);
        return false;
    }
    const bool ok = career->SaveState(db, error);
    sqlite3_close(db);
    return ok;
}

const char* CareerGetSaveError() {
    return careerSaveError.c_str();
}

int CareerEnableAutosave(const char* leagueDbPath) {
    if (!career || !leagueDbPath) return 0;
    careerSavePath = leagueDbPath;
    career->SetSeasonStartHook([]() {
        std::string error;
        careerSaveError = WriteCareerSave(careerSavePath, &error) ? std::string() : error;
    });
    std::string error;
    if (!WriteCareerSave(careerSavePath, &error)) { careerSaveError = error; return 0; }
    careerSaveError.clear();
    return 1;
}

int CareerHasSave(const char* leagueDbPath, const char* userTeamId) {
    if (!leagueDbPath || !userTeamId) return 0;
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(leagueDbPath, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) { if (db) sqlite3_close(db); return 0; }
    int found = 0;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT value FROM career_state WHERE key = 'user_team'", -1, &st, nullptr) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char* team = sqlite3_column_text(st, 0);
            found = team && std::string(reinterpret_cast<const char*>(team)) == userTeamId ? 1 : 0;
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return found;
}

int CareerResume(const char* userTeamId, int seed, const char* leagueDbPath) {
    if (!userTeamId || !leagueDbPath) return 0;
    if (!CareerStart(userTeamId, 2026, seed)) return 0;
    sqlite3* db = nullptr;
    std::string error;
    bool ok = sqlite3_open_v2(leagueDbPath, &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK && career->LoadState(db, &error);
    if (db) sqlite3_close(db);
    if (!ok) {
        careerMessage = error.empty() ? "The saved career could not be opened" : error;
        CareerEnd();
        return 0;
    }
    careerSavePath = leagueDbPath;
    career->SetSeasonStartHook([]() {
        std::string saveError;
        careerSaveError = WriteCareerSave(careerSavePath, &saveError) ? std::string() : saveError;
    });
    careerSaveError.clear();
    careerMessage.clear();
    return 1;
}

void CareerEnd() {
    career.reset();
    careerLastStop = bm::engine::CareerStop();
    careerLastStop.kind = "none";
}

const char* CareerGetStateJson() {
    careerJson = BuildCareerJson();
    return careerJson.c_str();
}

const char* CareerContinue() {
    if (!career) { careerStopJson = "{\"kind\":\"none\"}"; return careerStopJson.c_str(); }
    careerLastStop = career->Continue();
    careerStopJson = StopJson(careerLastStop);
    return careerStopJson.c_str();
}

const char* CareerStepDay() {
    if (!career) { careerStopJson = "{\"kind\":\"none\"}"; return careerStopJson.c_str(); }
    bm::engine::CareerStop stop = career->StepOne();
    if (stop.kind == "day") {
        careerLastStop = bm::engine::CareerStop();
        careerLastStop.kind = "none";
    } else {
        careerLastStop = stop;
    }
    careerStopJson = StopJson(stop);
    return careerStopJson.c_str();
}

int CareerAnswerQuestion(int option) {
    if (!career || !career->AnswerQuestion(option)) return 0;
    careerLastStop = bm::engine::CareerStop();
    careerLastStop.kind = "none";
    return 1;
}

static std::string boxScoreJson;

const char* CareerGetBoxScoreJson(int day) {
    const bm::engine::GameRecord* r = career ? career->FindGameRecord(career->SeasonYear(), day) : nullptr;
    if (!r) { boxScoreJson = "{\"found\":false}"; return boxScoreJson.c_str(); }
    auto side = [](const std::vector<bm::engine::BoxLine>& lines) {
        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& l = lines[i];
            out << (i ? "," : "") << "{\"name\":\"" << EscapeJson(l.name) << "\",\"pos\":\"" << l.position << "\",\"min\":" << l.minutes
                << ",\"pts\":" << l.points << ",\"reb\":" << l.rebounds << ",\"ast\":" << l.assists << ",\"stl\":" << l.steals
                << ",\"blk\":" << l.blocks << ",\"tov\":" << l.turnovers << ",\"pf\":" << l.fouls << "}";
        }
        out << "]";
        return out.str();
    };
    std::ostringstream json;
    json << "{\"found\":true,\"date\":\"" << EscapeJson(r->date) << "\",\"label\":\"" << EscapeJson(r->label)
         << "\",\"homeName\":\"" << EscapeJson(r->homeName) << "\",\"awayName\":\"" << EscapeJson(r->awayName)
         << "\",\"homeScore\":" << r->homeScore << ",\"awayScore\":" << r->awayScore << ",\"userHome\":" << (r->userHome ? "true" : "false")
         << ",\"home\":" << side(r->home) << ",\"away\":" << side(r->away) << "}";
    boxScoreJson = json.str();
    return boxScoreJson.c_str();
}

const char* CareerVacation(const char* target) {
    if (!career) { careerStopJson = "{\"kind\":\"none\"}"; return careerStopJson.c_str(); }
    careerLastStop = career->Vacation(target ? target : "week");
    careerStopJson = StopJson(careerLastStop);
    return careerStopJson.c_str();
}

void CareerDismissStop() {
    careerLastStop = bm::engine::CareerStop();
    careerLastStop.kind = "none";
}

const char* CareerGetScheduleJson() {
    careerScheduleJson = BuildScheduleJson();
    return careerScheduleJson.c_str();
}

// The coach's own match is always played as the engine's home side; the result is mapped back to the real venue
int CareerBeginCoachGame() {
    if (!career) return 0;
    auto game = career->PendingCoachGame();
    if (!game.pending) return 0;
    coachGameUserHome = game.home->teamId == career->UserTeamId();
    homeTeam = coachGameUserHome ? game.home : game.away;
    awayTeam = coachGameUserHome ? game.away : game.home;
    matchEngine = std::make_unique<bm::engine::MatchEngine>();
    matchEngine->SetHeadless(true);
    matchEngine->InitializeMatch(homeTeam, awayTeam);
    matchEngine->SetBreaksEnabled(true);
    UpdateStateJson();
    return 1;
}

int CareerFinishCoachGame() {
    if (!career || !matchEngine || !matchEngine->GetMatchState()) return 0;
    const auto& state = *matchEngine->GetMatchState();
    if (!state.matchComplete) return 0;
    int home = state.homeScore;
    int away = state.awayScore;
    if (!coachGameUserHome) std::swap(home, away);
    careerLastStop = bm::engine::CareerStop();
    careerLastStop.kind = "none";
    return career->RecordCoachGame(home, away) ? 1 : 0;
}

int CareerSimCoachGame() {
    if (!career) return 0;
    careerLastStop = bm::engine::CareerStop();
    careerLastStop.kind = "none";
    return career->SimulateCoachGame() ? 1 : 0;
}

const char* CareerGetContractsJson() {
    contractsJson = BuildContractsJson();
    return contractsJson.c_str();
}

int CareerRenew(const char* playerId, int years) {
    if (!career || !playerId) return 0;
    return career->RenewContract(playerId, years, &careerMessage) ? 1 : 0;
}

int CareerRelease(const char* playerId) {
    if (!career || !playerId) return 0;
    return career->ReleasePlayer(playerId, &careerMessage) ? 1 : 0;
}

const char* CareerGetFreeAgentsJson() {
    freeAgentsJson = BuildFreeAgentsJson();
    return freeAgentsJson.c_str();
}

int CareerSignFreeAgent(const char* playerId, int years) {
    if (!career || !playerId) return 0;
    return career->SignFreeAgent(playerId, years, &careerMessage) ? 1 : 0;
}

const char* CareerGetMessage() {
    return careerMessage.c_str();
}

const char* CareerGetDraftJson() {
    draftJson = BuildDraftJson();
    return draftJson.c_str();
}

void CareerDraftAdvance() {
    if (career) career->DraftAdvanceToUser();
}

int CareerDraftPick(const char* playerId) {
    if (!career || !playerId) return 0;
    return career->DraftPickUser(playerId, &careerMessage) ? 1 : 0;
}

void CareerDraftAuto() {
    if (career) career->DraftAutoFinish();
}

int CareerLoadDraftClass(const char* path) {
    if (!career || !path) return 0;
    return career->LoadDraftClass(path, &careerMessage) ? 1 : 0;
}

void CareerUseGeneratedDraftClass() {
    if (career) career->UseGeneratedDraftClass();
}

const char* CareerGetHistoryJson() {
    std::ostringstream json;
    json << "{\"retiredPlayers\":" << (career ? career->RetiredPlayerCount() : 0) << ",\"hallOfFame\":[";
    if (career) {
        const auto& hall = career->HallOfFame();
        for (size_t i = hall.size(); i-- > 0;) {   // newest first
            const auto& e = hall[i];
            json << (i + 1 < hall.size() ? "," : "")
                 << "{\"name\":\"" << EscapeJson(e.name) << "\",\"position\":\"" << e.position
                 << "\",\"team\":\"" << EscapeJson(e.teamName) << "\",\"year\":" << e.year
                 << ",\"peak\":" << e.peak << ",\"seasons\":" << e.seasons << ",\"number\":" << e.number
                 << ",\"numberRetired\":" << (e.numberRetired ? "true" : "false") << "}";
        }
    }
    json << "],\"retiredNumbers\":[";
    if (career) {
        const auto& numbers = career->RetiredNumbers();
        for (size_t i = numbers.size(); i-- > 0;) {
            const auto& n = numbers[i];
            json << (i + 1 < numbers.size() ? "," : "")
                 << "{\"team\":\"" << EscapeJson(n.teamName) << "\",\"name\":\"" << EscapeJson(n.playerName)
                 << "\",\"number\":" << n.number << ",\"year\":" << n.year
                 << ",\"mine\":" << (n.teamId == career->UserTeamId() ? "true" : "false") << "}";
        }
    }
    json << "]}";
    historyJson = json.str();
    return historyJson.c_str();
}

const char* CareerGetSummerJson() {
    summerJson = BuildSummerJson();
    return summerJson.c_str();
}

void CareerSetSummerMode(int mode) {
    if (career) career->SetSummerLeagueMode(std::clamp(mode, 0, 2));
}

void CareerSetTrainingPlan(const char* focus, int intensity, int coachDevelopment) {
    if (career) career->SetTrainingPlan(focus ? focus : "", intensity, coachDevelopment);
}

void CareerSetMentoring(const char* mentorId, const char* menteeId) {
    if (career) career->SetMentoring(mentorId ? mentorId : "", menteeId ? menteeId : "");
}

int CareerAssistantFillRoster() {
    return career && career->AssistantFillRoster() ? 1 : 0;
}

} // extern "C"
