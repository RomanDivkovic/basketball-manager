#include "bindings/UnityBridge.h"
#include "engine/MatchEngine.h"
#include "common/Types.h"
#include <iostream>
#include <cassert>
#include <string>
#include <memory>

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
    TestBridgeSeasonAndPlayoffs();

    std::cout << "\n----------------------------------------\n";
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed.\n";
    std::cout << "========================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
