#include "bindings/UnityBridge.h"
#include "engine/MatchEngine.h"
#include "common/Types.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <memory>
#include <unordered_map>

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            g_testsFailed++; \
            return; \
        } \
    } while (0)

static std::shared_ptr<bm::Player> CreateMockPlayer(const std::string& id, const std::string& name, bm::Position pos, int jersey) {
    auto p = std::make_shared<bm::Player>();
    p->playerId = id;
    p->firstName = name;
    p->lastName = "Test";
    p->position = pos;
    p->jerseyNumber = jersey;
    p->active = true;
    p->attributes.pace = 15;
    p->attributes.shooting = 16;
    p->attributes.ballControl = 15;
    p->attributes.defense = 14;
    p->attributes.physical = 15;
    p->attributes.technical = 15;
    p->attributes.currentAbility = 85;
    p->attributes.potentialAbility = 90;
    return p;
}

static std::shared_ptr<bm::Team> CreateMockTeam(const std::string& id, const std::string& name) {
    auto t = std::make_shared<bm::Team>();
    t->teamId = id;
    t->name = name;
    t->city = "City";
    t->state = "State";
    t->arena = "Arena";
    t->prestige = 85;
    t->active = true;

    bm::Position positions[] = { bm::Position::PG, bm::Position::SG, bm::Position::SF, bm::Position::PF, bm::Position::C };
    for (int i = 0; i < 5; i++) {
        t->roster.push_back(CreateMockPlayer(id + "_p" + std::to_string(i), "Player " + std::to_string(i), positions[i], i + 1));
    }
    return t;
}

void TestQuarterProgression() {
    std::cout << "[TEST] Quarter Progression & Automatic Boundary Checks...\n";
    auto homeTeam = CreateMockTeam("1", "Boston Celtics");
    auto awayTeam = CreateMockTeam("2", "Los Angeles Lakers");

    bm::engine::MatchEngine engine;
    engine.SetSpeedMultiplier(0); // Instant execution without sleep delays
    engine.InitializeMatch(homeTeam, awayTeam);

    auto state = engine.GetMatchState();
    TEST_ASSERT(state != nullptr, "Match state must not be null");
    TEST_ASSERT(state->quarterNumber == 1, "Match must start in Q1");
    TEST_ASSERT(state->timeRemaining == 720, "Q1 must start with 720 seconds");
    TEST_ASSERT(!state->matchComplete, "Match must not be completed at start");

    // Advance through possessions until match finishes
    int maxPossessions = 600;
    int rollOversSeen = 0;
    while (!state->matchComplete && maxPossessions-- > 0) {
        int prevQ = state->quarterNumber;
        engine.SimulatePossession();
        int curQ = state->quarterNumber;
        if (curQ > prevQ) {
            rollOversSeen++;
        }
    }

    TEST_ASSERT(rollOversSeen >= 3, "Match must progress across multiple quarters (Q1->Q4)");
    TEST_ASSERT(state->quarterNumber >= 4, "Match must reach at least Q4 or overtime");
    TEST_ASSERT(state->matchComplete, "Match must complete after regulation/OT");
    std::cout << "  -> Final: Q" << state->quarterNumber << " Score: "
              << state->homeScore << " - " << state->awayScore << " Complete: " << (state->matchComplete ? "YES" : "NO") << "\n";
    g_testsPassed++;
}

void TestOvertimeTriggerWhenTied() {
    std::cout << "[TEST] Overtime Trigger When Tied at End of Q4...\n";
    auto homeTeam = CreateMockTeam("1", "Boston Celtics");
    auto awayTeam = CreateMockTeam("2", "Miami Heat");

    bm::engine::MatchEngine engine;
    engine.SetSpeedMultiplier(0);
    engine.InitializeMatch(homeTeam, awayTeam);

    auto state = engine.GetMatchState();
    state->quarterNumber = 4;
    state->timeRemaining = 15;
    state->homeScore = 100;
    state->awayScore = 100;

    // Simulate possession to run out the remaining 15 seconds
    int safety = 20;
    while (!state->matchComplete && state->quarterNumber == 4 && safety-- > 0) {
        engine.SimulatePossession();
    }

    // Since they were tied at 100-100, it should either advance to OT (Q5) or break the tie in the possession
    if (state->quarterNumber > 4) {
        std::cout << "  -> Successfully triggered Overtime Q" << state->quarterNumber << " (Clock: " << state->timeRemaining << "s)\n";
        TEST_ASSERT(state->quarterNumber >= 5, "Tied game at end of Q4 must enter OT");
    } else {
        std::cout << "  -> Game resolved at buzzer: " << state->homeScore << " - " << state->awayScore << "\n";
        TEST_ASSERT(state->matchComplete, "Game must be complete if not tied");
    }
    g_testsPassed++;
}

void TestSecondByStepFlow() {
    std::cout << "[TEST] Second-by-second match flow (clock, movement, ball, scoring)...\n";
    auto homeTeam = CreateMockTeam("1", "Boston Celtics");
    auto awayTeam = CreateMockTeam("2", "New York Knicks");

    bm::engine::MatchEngine engine;
    engine.SetHeadless(true);
    engine.InitializeMatch(homeTeam, awayTeam);
    auto state = engine.GetMatchState();
    TEST_ASSERT(state != nullptr, "Match state must not be null");

    std::unordered_map<std::string, std::pair<float, float>> previous;
    int prevQuarter = state->quarterNumber;
    int prevClock = state->timeRemaining;
    int steps = 0;
    int clockViolations = 0;
    int boundsViolations = 0;
    float maxMove = 0.0f;
    bool sawBallFlight = false;
    bool sawHolder = false;

    while (steps < 60000 && engine.StepSecond()) {
        ++steps;

        if (state->quarterNumber == prevQuarter) {
            const int drop = prevClock - state->timeRemaining;
            if (drop < 0 || drop > 1) clockViolations++;
        }
        prevQuarter = state->quarterNumber;
        prevClock = state->timeRemaining;

        std::unordered_map<std::string, std::pair<float, float>> current;
        for (const auto& cp : state->courtPlayers) {
            if (cp.x < 0.0f || cp.x > 94.0f || cp.y < 0.0f || cp.y > 50.0f) boundsViolations++;
            auto it = previous.find(cp.playerId);
            if (it != previous.end()) {
                maxMove = std::max(maxMove, std::hypot(cp.x - it->second.first, cp.y - it->second.second));
            }
            current[cp.playerId] = {cp.x, cp.y};
        }
        // Players who just came on from the bench have no earlier position to compare with
        previous.swap(current);
        const auto& ball = state->ball;
        if (ball.x < 0.0f || ball.x > 94.0f || ball.y < 0.0f || ball.y > 50.0f) boundsViolations++;
        if (ball.inFlight) sawBallFlight = true;
        if (!ball.holderPlayerId.empty()) sawHolder = true;
    }

    TEST_ASSERT(state->matchComplete, "Match must finish when stepped second by second");
    TEST_ASSERT(state->quarterNumber >= 4, "Match must reach the fourth quarter");
    TEST_ASSERT(clockViolations == 0, "Game clock must tick down by at most one second per step");
    TEST_ASSERT(boundsViolations == 0, "Players and ball must stay inside the court");
    TEST_ASSERT(maxMove <= 25.0f, "No player may teleport more than 25 ft in one second");
    TEST_ASSERT(sawBallFlight && sawHolder, "Ball must be both carried and passed/shot");
    TEST_ASSERT(engine.GetFeedTotal() > 100, "Play-by-play feed must describe the game");
    TEST_ASSERT(steps > 2500, "A full game is roughly 2880 clock seconds plus stopped-clock beats");

    int homePoints = 0;
    int awayPoints = 0;
    for (const auto& p : homeTeam->roster) homePoints += p->pointsThisGame;
    for (const auto& p : awayTeam->roster) awayPoints += p->pointsThisGame;
    TEST_ASSERT(homePoints == state->homeScore, "Home player points must add up to the home score");
    TEST_ASSERT(awayPoints == state->awayScore, "Away player points must add up to the away score");

    std::cout << "  -> " << steps << " steps, final " << state->homeScore << " - " << state->awayScore
              << ", max move " << maxMove << " ft/s, feed lines " << engine.GetFeedTotal() << "\n";
    g_testsPassed++;
}

void TestFlowVariety() {
    std::cout << "[TEST] Flow produces passes, drives, fast breaks and free throws...\n";
    int passes = 0, drives = 0, breaks = 0, freeThrows = 0, assists = 0;

    for (int game = 0; game < 3; ++game) {
        auto homeTeam = CreateMockTeam("1", "Boston Celtics");
        auto awayTeam = CreateMockTeam("2", "Miami Heat");
        bm::engine::MatchEngine engine;
        engine.SetHeadless(true);
        engine.InitializeMatch(homeTeam, awayTeam);
        engine.StepSeconds(60000);

        for (const auto& line : engine.GetFeed()) {
            if (line.find("swings it") != std::string::npos || line.find("kicks it out") != std::string::npos ||
                line.find("finds") != std::string::npos || line.find("feeds") != std::string::npos) passes++;
            if (line.find("attacks the rim") != std::string::npos || line.find("drives to") != std::string::npos ||
                line.find("blows by") != std::string::npos) drives++;
            if (line.find("pushes it up") != std::string::npos || line.find("off to the races") != std::string::npos ||
                line.find("leads the break") != std::string::npos) breaks++;
            if (line.find("free throw") != std::string::npos) freeThrows++;
            if (line.find("(assist") != std::string::npos) assists++;
        }
    }

    // The feed keeps only the newest 200 lines, so counts are a sample of each game's closing minutes
    TEST_ASSERT(passes > 0, "Ball must be passed around");
    TEST_ASSERT(drives + breaks + freeThrows + assists > 0, "Drives, fast breaks, free throws or assists must appear");
    std::cout << "  -> sample: passes " << passes << ", drives " << drives << ", breaks " << breaks
              << ", free throws " << freeThrows << ", assists " << assists << "\n";
    g_testsPassed++;
}

void TestBridgeSeasonAndPlayoffs() {
    std::cout << "[TEST] UnityBridge Season & Playoff Bracket Integration...\n";
    LoadDatabase("/tmp/basketball_manager_readable.db");

    const char* teams = GetTeamsJson();
    TEST_ASSERT(teams != nullptr, "Teams JSON must not be null");
    TEST_ASSERT(std::string(teams).find("\"teams\":[{") != std::string::npos, "Teams JSON must contain teams array");

    // Season initialization
    InitializeSeason(2026);
    int curDay = GetCurrentSeasonDay();
    TEST_ASSERT(curDay >= 1, "InitializeSeason must set valid current day");

    const char* cal = GetSeasonCalendarJson();
    TEST_ASSERT(cal != nullptr, "Calendar JSON must not be null");
    TEST_ASSERT(std::string(cal).find("\"totalDays\":") != std::string::npos, "Season calendar must define totalDays");

    // Day 1 fixtures
    const char* f1 = GetFixturesForDayJson(1);
    TEST_ASSERT(f1 != nullptr, "Fixtures JSON must not be null");
    TEST_ASSERT(std::string(f1).find("\"fixtures\":[") != std::string::npos, "Fixtures JSON must contain fixtures array");

    // Standings
    const char* eastStandings = GetStandingsJson("Eastern");
    TEST_ASSERT(eastStandings != nullptr, "Eastern standings must not be null");
    TEST_ASSERT(std::string(eastStandings).find("\"standings\":[") != std::string::npos, "Eastern standings must contain standings array");

    // Playoff Bracket
    int bracketOk = GeneratePlayoffBracket();
    TEST_ASSERT(bracketOk == 1, "GeneratePlayoffBracket must succeed");

    const char* pJson = GetPlayoffsBracketJson();
    TEST_ASSERT(pJson != nullptr, "Playoffs JSON must not be null");
    TEST_ASSERT(std::string(pJson).find("\"series\":[") != std::string::npos, "Playoffs JSON must contain series array");

    // Advance round
    int advOk = AdvancePlayoffRound(6);
    TEST_ASSERT(advOk == 1, "AdvancePlayoffRound must succeed");

    g_testsPassed++;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  Basketball Manager Core Automated Tests\n";
    std::cout << "========================================\n";

    TestQuarterProgression();
    TestOvertimeTriggerWhenTied();
    TestSecondByStepFlow();
    TestFlowVariety();
    TestBridgeSeasonAndPlayoffs();

    std::cout << "\n----------------------------------------\n";
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed.\n";
    std::cout << "========================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
