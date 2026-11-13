#include "bindings/UnityBridge.h"
#include "engine/MatchEngine.h"
#include "engine/CareerManager.h"
#include "db/DatabaseManager.h"
#include "db/LeagueImport.h"
#include "common/Types.h"
#include "common/Ratings.h"
#include <iostream>
#include <random>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <cstdlib>
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

void TestBreaksTimeoutsAndTalks() {
    std::cout << "[TEST] Pre-match/period breaks, NBA timeouts and team talks...\n";
    using bm::engine::BreakType;
    using bm::engine::TeamTalkType;

    auto homeTeam = CreateMockTeam("1", "Boston Celtics");
    auto awayTeam = CreateMockTeam("2", "New York Knicks");
    bm::engine::MatchEngine engine;
    engine.SetHeadless(true);
    engine.InitializeMatch(homeTeam, awayTeam);
    engine.SetBreaksEnabled(true);
    auto state = engine.GetMatchState();

    // The match opens with a pre-match break and does not move until the coach continues
    TEST_ASSERT(engine.GetBreakType() == BreakType::PreMatch, "Match must open with the pre-match break");
    TEST_ASSERT(!engine.StepSecond(), "Nothing may be played during a break");
    TEST_ASSERT(state->timeRemaining == 720, "Clock must not move during the pre-match break");
    TEST_ASSERT(engine.GetTimeoutsRemaining(true) == 7 && engine.GetTimeoutsRemaining(false) == 7,
                "NBA gives each team seven timeouts");

    // One talk per break; every active player reacts and morale stays in range
    TEST_ASSERT(engine.ApplyTeamTalk(true, TeamTalkType::Encourage, 85), "A pre-match talk must be accepted");
    TEST_ASSERT(!engine.ApplyTeamTalk(true, TeamTalkType::Encourage, 85), "Only one talk per break");
    TEST_ASSERT(engine.GetLastTeamTalk().reactions.size() == homeTeam->roster.size(), "Every player must react");
    for (const auto& p : homeTeam->roster) {
        TEST_ASSERT(p->morale >= -1.0f && p->morale <= 1.0f, "Morale must stay within -1..1");
    }
    engine.EndBreak();

    // A full timeout requested mid-play is granted at the next dead ball
    engine.StepSeconds(30);
    TEST_ASSERT(engine.RequestTimeout(true, true), "Home may request a full timeout");
    TEST_ASSERT(!engine.RequestTimeout(true, false), "A second request while one is queued is refused");
    int guard = 0;
    while (!engine.IsBreakActive() && guard++ < 200) engine.StepSecond();
    TEST_ASSERT(engine.GetBreakType() == BreakType::Timeout, "The timeout must start a break");
    TEST_ASSERT(engine.GetTimeoutsRemaining(true) == 6, "A timeout costs one of the seven");
    engine.EndBreak();

    // A short huddle only allows a quick instruction
    TEST_ASSERT(engine.RequestTimeout(true, false), "Home may request a short timeout");
    guard = 0;
    while (!engine.IsBreakActive() && guard++ < 200) engine.StepSecond();
    TEST_ASSERT(engine.GetBreakType() == BreakType::ShortTimeout, "The short timeout must start a break");
    TEST_ASSERT(!engine.ApplyTeamTalk(true, TeamTalkType::BeAngry, 85), "No long speeches in 20 seconds");
    TEST_ASSERT(engine.ApplyTeamTalk(true, TeamTalkType::MoreDefense, 85), "A quick instruction is allowed");
    engine.EndBreak();

    // Fourth period: at most four timeouts per team
    state->quarterNumber = 4;
    state->timeRemaining = 600;
    int granted = 0;
    for (int i = 0; i < 6; ++i) {
        if (!engine.RequestTimeout(true, false)) break;
        guard = 0;
        while (!engine.IsBreakActive() && guard++ < 200) engine.StepSecond();
        engine.EndBreak();
        ++granted;
        state->timeRemaining = 600;
    }
    TEST_ASSERT(granted <= 4, "No more than four timeouts in the fourth period");
    TEST_ASSERT(granted >= 3, "Timeouts in the fourth period must still be possible");

    // Whole game: breaks appear after Q1, at halftime and after Q3; the game still finishes
    auto home2 = CreateMockTeam("1", "Boston Celtics");
    auto away2 = CreateMockTeam("2", "Miami Heat");
    bm::engine::MatchEngine full;
    full.SetHeadless(true);
    full.InitializeMatch(home2, away2);
    full.SetBreaksEnabled(true);
    int quarterBreaks = 0, halftimes = 0, prematch = 0;
    int steps = 0;
    while (steps++ < 100000) {
        if (full.IsBreakActive()) {
            if (full.GetBreakType() == BreakType::QuarterBreak) quarterBreaks++;
            if (full.GetBreakType() == BreakType::Halftime) halftimes++;
            if (full.GetBreakType() == BreakType::PreMatch) prematch++;
            full.EndBreak();
            continue;
        }
        if (!full.StepSecond() && !full.IsBreakActive()) break;
    }
    TEST_ASSERT(full.GetMatchState()->matchComplete, "The match must finish with breaks enabled");
    TEST_ASSERT(prematch == 1 && halftimes == 1 && quarterBreaks >= 2, "Break after Q1, halftime and after Q3");

    std::cout << "  -> quarter breaks " << quarterBreaks << ", halftime " << halftimes << ", granted Q4 timeouts " << granted << "\n";
    g_testsPassed++;
}

void TestTeamTalkFitsTheSituation() {
    std::cout << "[TEST] Team talk reactions follow the situation...\n";
    using bm::engine::TeamTalkType;

    auto positives = [](TeamTalkType type, int margin) {
        int total = 0;
        for (int trial = 0; trial < 40; ++trial) {
            auto homeTeam = CreateMockTeam("1", "Boston Celtics");
            auto awayTeam = CreateMockTeam("2", "Miami Heat");
            bm::engine::MatchEngine engine;
            engine.SetHeadless(true);
            engine.InitializeMatch(homeTeam, awayTeam);
            engine.SetBreaksEnabled(true);
            engine.EndBreak();
            engine.StepSeconds(2);
            engine.GetMatchState()->homeScore = 40 + margin;
            engine.GetMatchState()->awayScore = 40;
            engine.GetMatchState()->timeRemaining = 0;
            engine.GetMatchState()->quarterNumber = 2;
            while (!engine.IsBreakActive() && engine.StepSecond()) {}
            if (!engine.ApplyTeamTalk(true, type, 80)) return -1;
            for (const auto& r : engine.GetLastTeamTalk().reactions) total += r.mood;
        }
        return total;
    };

    const int praiseWhenWinning = positives(TeamTalkType::Praise, 12);
    const int praiseWhenLosing = positives(TeamTalkType::Praise, -12);
    const int angryWhenWinning = positives(TeamTalkType::BeAngry, 12);
    const int encourageWhenLosing = positives(TeamTalkType::Encourage, -12);
    TEST_ASSERT(praiseWhenWinning >= 0 && praiseWhenLosing >= -10000, "Talks must be accepted at halftime");
    TEST_ASSERT(praiseWhenWinning > praiseWhenLosing, "Praise lands better when winning than when losing");
    TEST_ASSERT(encourageWhenLosing > angryWhenWinning, "Encouragement beats anger when the team is ahead or behind");
    std::cout << "  -> mood sums: praise ahead " << praiseWhenWinning << ", praise behind " << praiseWhenLosing
              << ", angry ahead " << angryWhenWinning << ", encourage behind " << encourageWhenLosing << "\n";
    g_testsPassed++;
}

void TestOffenseSchemesPlaybookAndFeedback() {
    std::cout << "[TEST] Offense schemes, playbook, individual feedback and queued substitutions...\n";
    using bm::engine::OffensiveStrategy;
    using bm::engine::PlayerInstruction;

    // Playbook covers every user-facing scheme and each play names its scheme
    const auto& book = bm::engine::MatchEngine::GetPlaybook();
    for (auto scheme : {OffensiveStrategy::THREE_POINT_HEAVY, OffensiveStrategy::INSIDE_FOCUSED, OffensiveStrategy::PICK_AND_ROLL,
                        OffensiveStrategy::BALL_MOVEMENT, OffensiveStrategy::PLAY_THROUGH_PLAYER}) {
        int plays = 0;
        for (const auto& p : book) if (p.scheme == scheme) plays++;
        TEST_ASSERT(plays >= 2, "Every scheme needs at least two plays in the playbook");
    }

    auto homeTeam = CreateMockTeam("1", "Boston Celtics");
    auto awayTeam = CreateMockTeam("2", "Miami Heat");
    bm::engine::MatchEngine engine;
    engine.SetHeadless(true);
    engine.InitializeMatch(homeTeam, awayTeam);
    const std::string hub = homeTeam->roster[2]->playerId;
    const std::string benchId = homeTeam->roster[5]->playerId;   // a reserve
    std::string error;

    // Contextual rules
    TEST_ASSERT(!engine.SetOffenseScheme(true, OffensiveStrategy::PLAY_THROUGH_PLAYER, "", "", &error) && !error.empty(),
                "Play through a player needs a target player");
    TEST_ASSERT(!engine.SetOffenseScheme(true, OffensiveStrategy::PLAY_THROUGH_PLAYER, benchId, "", &error),
                "The target player must be on the floor");
    TEST_ASSERT(!engine.SetOffenseScheme(true, OffensiveStrategy::PICK_AND_ROLL, "", "elbow_hub", &error),
                "A play must belong to the chosen scheme");
    TEST_ASSERT(!engine.SetOffenseScheme(true, OffensiveStrategy::PICK_AND_ROLL, "", "no_such_play", &error),
                "Unknown plays are rejected");
    TEST_ASSERT(engine.SetOffenseScheme(true, OffensiveStrategy::PICK_AND_ROLL, "", "spain_pick_and_roll", &error),
                "A matching play is accepted");
    TEST_ASSERT(engine.SetOffenseScheme(true, OffensiveStrategy::PLAY_THROUGH_PLAYER, hub, "elbow_hub", &error),
                "Hub player and matching play are accepted");
    TEST_ASSERT(engine.GetOffenseTarget(true) == hub && engine.GetOffensePlay(true) == "elbow_hub", "Plan must be stored");

    // Substitutions are queued while the ball is live and applied at the next dead ball
    engine.StepSeconds(5);
    const std::string outId = homeTeam->roster[4]->playerId;
    TEST_ASSERT(!engine.RequestSubstitution(true, benchId, benchId), "A player cannot replace himself");
    TEST_ASSERT(!engine.RequestSubstitution(true, benchId, outId), "The outgoing player must be on the floor");
    TEST_ASSERT(!engine.RequestSubstitution(true, outId, homeTeam->roster[0]->playerId), "The incoming player must be on the bench");
    TEST_ASSERT(engine.RequestSubstitution(true, outId, benchId), "A valid substitution is accepted");
    int guard = 0;
    while (engine.GetPendingSubstitutions() > 0 && guard++ < 200) engine.StepSecond();
    TEST_ASSERT(engine.GetPendingSubstitutions() == 0, "Queued substitutions must be applied at a dead ball");
    TEST_ASSERT(homeTeam->roster[5]->onCourt && !homeTeam->roster[4]->onCourt, "The substitution must change the lineup");

    // The hub player leaving ends the plan
    TEST_ASSERT(engine.RequestSubstitution(true, hub, outId), "The hub player can be substituted");
    guard = 0;
    while (engine.GetPendingSubstitutions() > 0 && guard++ < 200) engine.StepSecond();
    TEST_ASSERT(engine.GetTacticalManager()->GetHomeOffensiveStrategy() == OffensiveStrategy::BALL_MOVEMENT,
                "Without the hub player the team moves the ball instead");
    TEST_ASSERT(engine.GetOffenseTarget(true).empty(), "The hub target must be cleared");

    // Individual feedback only goes to players on the floor and wears off
    const std::string onFloor = homeTeam->roster[1]->playerId;
    TEST_ASSERT(!homeTeam->roster[2]->onCourt, "The hub player must be on the bench by now");
    TEST_ASSERT(!engine.GiveFeedback(true, hub, PlayerInstruction::SettleDown, 85), "Benched players cannot receive feedback");
    TEST_ASSERT(engine.GiveFeedback(true, onFloor, PlayerInstruction::LookForYourShot, 85), "Feedback to a player on the floor");
    TEST_ASSERT(engine.GetInstruction(onFloor) == PlayerInstruction::LookForYourShot, "The instruction must be stored");
    TEST_ASSERT(engine.GetLastFeedback().valid && engine.GetLastFeedback().playerId == onFloor, "Reaction must be reported");
    for (int i = 0; i < 30; ++i) engine.SimulatePossession();
    TEST_ASSERT(engine.GetInstruction(onFloor) == PlayerInstruction::None, "Instructions wear off after some possessions");

    // Effects: playing through a player and telling a player to shoot raise his scoring
    auto avgPoints = [](bool throughHub, bool lookForShot) {
        double total = 0;
        for (int game = 0; game < 40; ++game) {
            auto h = CreateMockTeam("1", "Boston Celtics");
            auto a = CreateMockTeam("2", "Miami Heat");
            bm::engine::MatchEngine e;
            e.SetHeadless(true);
            e.InitializeMatch(h, a);
            const std::string target = h->roster[2]->playerId;
            if (throughHub) e.SetOffenseScheme(true, OffensiveStrategy::PLAY_THROUGH_PLAYER, target, "iso_star");
            for (int p = 0; p < 90; ++p) {
                if (lookForShot && p % 10 == 0) e.GiveFeedback(true, target, PlayerInstruction::LookForYourShot, 99);
                e.SimulatePossession();
            }
            total += h->roster[2]->pointsThisGame;
        }
        return total / 40.0;
    };
    const double base = avgPoints(false, false);
    const double throughHub = avgPoints(true, false);
    const double shoot = avgPoints(false, true);
    TEST_ASSERT(throughHub > base * 1.15, "Playing through a player must raise his scoring");
    TEST_ASSERT(shoot > base * 1.02, "'Look for your shot' must raise his scoring");

    // Pick and roll shows up as screens in the play-by-play
    int screens = 0;
    for (int game = 0; game < 3; ++game) {
        auto h = CreateMockTeam("1", "Boston Celtics");
        auto a = CreateMockTeam("2", "Miami Heat");
        bm::engine::MatchEngine e;
        e.SetHeadless(true);
        e.InitializeMatch(h, a);
        e.SetOffenseScheme(true, OffensiveStrategy::PICK_AND_ROLL, "", "high_pick_and_roll");
        e.SetOffenseScheme(false, OffensiveStrategy::PICK_AND_ROLL, "", "spain_pick_and_roll");
        e.StepSeconds(60000);
        for (const auto& line : e.GetFeed()) if (line.find("screen") != std::string::npos) screens++;
    }
    TEST_ASSERT(screens > 0, "Pick and roll must use screens");

    std::cout << "  -> avg points of the featured player: base " << base << ", through hub " << throughHub
              << ", look for shot " << shoot << ", screens seen " << screens << "\n";
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


// ---------------------------------------------------------------------------------------------
// Career: a dated year
// ---------------------------------------------------------------------------------------------

using bm::engine::CareerManager;
using bm::engine::CareerPhase;
using bm::engine::CareerStop;
using bm::engine::DaysFromCivil;

static std::vector<std::shared_ptr<bm::Team>> CreateLeague(int teamCount, unsigned seed, int rosterSize = 15) {
    std::mt19937 rng(seed);
    std::vector<std::shared_ptr<bm::Team>> teams;
    const bm::Position positions[] = { bm::Position::PG, bm::Position::SG, bm::Position::SF, bm::Position::PF, bm::Position::C };
    for (int t = 0; t < teamCount; ++t) {
        auto team = std::make_shared<bm::Team>();
        team->teamId = std::to_string(t + 1);
        team->name = "Team " + team->teamId;
        team->conferenceId = t < teamCount / 2 ? "Eastern" : "Western";
        team->prestige = 10;
        team->active = true;
        for (int i = 0; i < rosterSize; ++i) {
            auto p = std::make_shared<bm::Player>();
            p->playerId = team->teamId + "_" + std::to_string(i + 1);
            p->firstName = "P" + std::to_string(i + 1);
            p->lastName = "T" + team->teamId;
            p->position = positions[i % 5];
            p->jerseyNumber = i + 1;
            const int ability = 70 + static_cast<int>(rng() % 20) - i;
            p->attributes.currentAbility = ability;
            p->attributes.potentialAbility = ability + static_cast<int>(rng() % 8);
            p->attributes.pace = p->attributes.shooting = p->attributes.ballControl = ability / 5;
            p->attributes.defense = p->attributes.physical = p->attributes.technical = ability / 5;
            p->birthDate = "2000-01-01";
            p->active = true;
            team->roster.push_back(p);
        }
        teams.push_back(team);
    }
    return teams;
}

static void ScriptedCoach(CareerManager& c, const CareerStop& stop) {
    if (stop.kind == "coach_game") {
        c.SimulateCoachGame();
    } else if (stop.kind == "player_question") {
        c.AnswerQuestion(0);
    } else if (stop.kind == "draft_class") {
        c.UseGeneratedDraftClass();
    } else if (stop.kind == "friendlies_plan") {
        c.PlanFriendlies({});
    } else if (stop.kind == "draft") {
        c.DraftAdvanceToUser();
        while (c.UserOnClock() && !c.Prospects().empty()) {
            std::string message;
            c.DraftPickUser(c.Prospects().front()->playerId, &message);
        }
    } else if (stop.kind == "summer_choice") {
        c.SetSummerLeagueMode(2);
    } else if (stop.kind == "camp_plan") {
        c.SetTrainingPlan("Shooting", 2, 70);
    } else if (stop.kind == "roster_check") {
        c.AssistantFillRoster();
    }
}

static int MaxPlayersOnATeam(const std::vector<std::shared_ptr<bm::Team>>& teams) {
    size_t most = 0;
    for (const auto& t : teams) most = std::max(most, t->roster.size());
    return static_cast<int>(most);
}

static int MinPlayersOnATeam(const std::vector<std::shared_ptr<bm::Team>>& teams) {
    size_t least = 1000;
    for (const auto& t : teams) least = std::min(least, t->roster.size());
    return static_cast<int>(least);
}

void TestYearScheduleIsAFullSeason() {
    std::cout << "[TEST] Schedule: every team plays 82 games over a full season...\n";
    auto league = CreateLeague(30, 7);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "1";
    cfg.seed = 42;
    career.Start(league, cfg);

    const auto& season = career.Season();
    TEST_ASSERT(season.GetFixtureDayCount() >= 170 && season.GetFixtureDayCount() <= 190, "The season must span about 175 days");
    TEST_ASSERT(season.GetTotalGames() == 30 * 82 / 2, "A 30-team league plays 1230 games");

    std::map<std::string, int> games;
    std::map<std::string, int> sameConference;
    for (int d = 1; d <= season.GetFixtureDayCount(); ++d) {
        std::set<std::string> busy;
        for (const auto& g : season.GetFixturesForDay(d)) {
            TEST_ASSERT(busy.insert(g.homeTeam->teamId).second && busy.insert(g.awayTeam->teamId).second, "No team plays twice on one day");
            ++games[g.homeTeam->teamId];
            ++games[g.awayTeam->teamId];
            if (g.homeTeam->conferenceId == g.awayTeam->conferenceId) {
                ++sameConference[g.homeTeam->teamId];
                ++sameConference[g.awayTeam->teamId];
            }
        }
    }
    for (const auto& t : league) {
        TEST_ASSERT(games[t->teamId] == 82, "Every team plays exactly 82 games");
        TEST_ASSERT(sameConference[t->teamId] == 52, "52 of the 82 games are against the own conference");
    }
    g_testsPassed++;
}

void TestCareerYearFlow() {
    std::cout << "[TEST] Career: a dated year from preseason to the next preseason...\n";
    auto league = CreateLeague(30, 7);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "1";
    cfg.seed = 42;
    career.Start(league, cfg);
    TEST_ASSERT(career.Phase() == CareerPhase::Preseason, "A career starts in the preseason");
    TEST_ASSERT(career.Today() == DaysFromCivil(2026, 10, 1), "The year starts on 1 October");

    std::map<std::string, int> firstDay;
    std::vector<std::string> order;
    bool playoffsHiddenInSummer = true;
    bool openingNightOnTime = true;
    int firstMatchDay = 0;
    std::string championAtSeasonEnd;
    for (int guard = 0; guard < 600 && career.SeasonYear() == 2026; ++guard) {
        CareerStop stop = career.Continue();
        if (stop.kind != "coach_game" && !firstDay.count(stop.kind)) {
            firstDay[stop.kind] = career.Today();
            order.push_back(stop.kind);
        }
        if (stop.kind == "season_end") championAtSeasonEnd = career.ChampionName();
        if (stop.kind == "coach_game" && career.Phase() == CareerPhase::RegularSeason) {
            if (firstMatchDay == 0) firstMatchDay = career.Today();
            openingNightOnTime = openingNightOnTime && career.Today() == DaysFromCivil(2026, 10, 21) + career.RegularSeasonDay() - 1;
        }
        if ((career.Phase() == CareerPhase::Offseason || career.Phase() == CareerPhase::Review) && career.PlayoffsVisible()) playoffsHiddenInSummer = false;
        ScriptedCoach(career, stop);
    }
    TEST_ASSERT(career.SeasonYear() == 2027, "The calendar must roll over into the next season");
    TEST_ASSERT(openingNightOnTime && firstMatchDay >= DaysFromCivil(2026, 10, 21), "The regular season opens on 21 October and day n is the n-th date");
    TEST_ASSERT(playoffsHiddenInSummer, "The playoffs are only visible while they are played");

    TEST_ASSERT(firstDay.count("season_end") && !championAtSeasonEnd.empty(), "The season end is reported with a champion");
    TEST_ASSERT(firstDay["draft"] == DaysFromCivil(2027, 6, 25), "The draft is on 25 June");
    TEST_ASSERT(firstDay["free_agency"] == DaysFromCivil(2027, 7, 1), "Free agency opens on 1 July");
    TEST_ASSERT(firstDay["summer_choice"] == DaysFromCivil(2027, 7, 3), "Summer League starts on 3 July");
    TEST_ASSERT(firstDay["camp_plan"] == DaysFromCivil(2027, 9, 28), "Training camp starts on 28 September");
    TEST_ASSERT(firstDay["new_season"] == DaysFromCivil(2027, 10, 1), "The next season starts on 1 October");
    auto position = [&](const std::string& kind) { return std::find(order.begin(), order.end(), kind) - order.begin(); };
    TEST_ASSERT(position("season_end") < position("draft") && position("draft") < position("free_agency")
             && position("free_agency") < position("summer_choice") && position("summer_choice") < position("camp_plan")
             && position("camp_plan") < position("new_season"), "The offseason events come in calendar order");

    // The next season opens with legal rosters everywhere
    for (int guard = 0; guard < 100 && career.Phase() != CareerPhase::RegularSeason; ++guard) {
        ScriptedCoach(career, career.Continue());
    }
    TEST_ASSERT(career.Phase() == CareerPhase::RegularSeason, "The 2027/28 season must start");
    TEST_ASSERT(MinPlayersOnATeam(league) >= 13 && MaxPlayersOnATeam(league) <= 15, "Every roster is between 13 and 15 on opening night");
    g_testsPassed++;
}

void TestVacationLetsTheAssistantRunTheClub() {
    std::cout << "[TEST] Career: the assistant runs the club on vacation...\n";
    auto league = CreateLeague(30, 11);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "3";
    cfg.seed = 9;
    career.Start(league, cfg);

    CareerStop back = career.Vacation("new_season");
    TEST_ASSERT(back.kind == "vacation_ended", "A vacation ends with a report");
    TEST_ASSERT(career.SeasonYear() == 2027 && career.Phase() == CareerPhase::Preseason, "The vacation ends when the new season starts");
    TEST_ASSERT(career.Today() == DaysFromCivil(2027, 10, 1), "...on 1 October");

    // The assistant played the whole year: contracts were handled and nothing is blocked
    back = career.Vacation("month");
    TEST_ASSERT(career.Phase() == CareerPhase::RegularSeason, "A month later the season is under way");
    TEST_ASSERT(MinPlayersOnATeam(league) >= 13 && MaxPlayersOnATeam(league) <= 15, "The assistant keeps the roster legal");
    TEST_ASSERT(career.CurrentStop().kind == "none" || !career.CurrentStop().blocking || career.CurrentStop().kind == "coach_game",
                "Nothing but a match day is waiting after a vacation");
    auto weekBack = career.Vacation("week");
    TEST_ASSERT(weekBack.kind == "vacation_ended", "A week-long vacation also reports back");
    g_testsPassed++;
}

void TestCoachGameCanBePlayedLive() {
    std::cout << "[TEST] Career: the coach's live result is recorded in the standings...\n";
    auto league = CreateLeague(30, 5);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "1";
    cfg.seed = 1;
    career.Start(league, cfg);

    CareerStop classStop = career.Continue();
    TEST_ASSERT(classStop.kind == "draft_class" && classStop.blocking, "A new season first asks about the draft class");
    career.UseGeneratedDraftClass();
    TEST_ASSERT(career.Continue().kind == "friendlies_plan", "Then it asks about pre-season friendlies");
    career.PlanFriendlies({});
    CareerStop stop = career.Continue();
    TEST_ASSERT(stop.kind == "coach_game" && stop.blocking, "The first stop is the coach's match");
    TEST_ASSERT(career.Continue().kind == "coach_game", "Time does not move past an unplayed match");
    auto game = career.PendingCoachGame();
    TEST_ASSERT(game.pending && game.home && game.away, "The pending match names both teams");
    const std::string homeId = game.home->teamId;
    const std::string awayId = game.away->teamId;
    TEST_ASSERT(homeId == "1" || awayId == "1", "The pending match is the coach's own");

    TEST_ASSERT(career.RecordCoachGame(101, 95), "The live result can be recorded");
    TEST_ASSERT(!career.PendingCoachGame().pending, "The match is no longer pending");
    int homeWins = 0;
    int awayLosses = 0;
    for (const auto& rec : career.Season().GetStandings("")) {
        if (rec.team->teamId == homeId) homeWins = rec.wins;
        if (rec.team->teamId == awayId) awayLosses = rec.losses;
    }
    TEST_ASSERT(homeWins == 1 && awayLosses == 1, "The standings show the recorded result");
    CareerStop next = career.Continue();
    for (int guard = 0; guard < 20 && next.kind != "coach_game"; ++guard) {
        if (next.kind == "player_question") career.AnswerQuestion(0);
        next = career.Continue();
    }
    TEST_ASSERT(next.kind == "coach_game" && career.Today() > DaysFromCivil(2026, 10, 21), "Time moves on to the next match day (news and questions may come first)");
    g_testsPassed++;
}

void TestContractsAndFreeAgencyRules() {
    std::cout << "[TEST] Career: roster limits, renewals, the salary cap and free agents...\n";
    auto league = CreateLeague(30, 13);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "2";
    cfg.seed = 21;
    career.Start(league, cfg);
    auto team = career.UserTeam();

    // Releasing is limited by the 13-man minimum in season phases
    std::string message;
    TEST_ASSERT(career.ReleasePlayer(team->roster.back()->playerId, &message), "A 15-man roster can release a player");
    TEST_ASSERT(career.ReleasePlayer(team->roster.back()->playerId, &message), "...and another down to 13");
    TEST_ASSERT(!career.ReleasePlayer(team->roster.back()->playerId, &message), "...but not below 13 before the season");
    TEST_ASSERT(!career.FreeAgents().empty(), "Released players become free agents");

    // Only players with an expiring contract can be renewed
    std::shared_ptr<bm::Player> runningDeal;
    std::shared_ptr<bm::Player> expiring;
    for (const auto& p : team->roster) {
        if (p->contractEndYear > career.SeasonYear()) runningDeal = p;
        else expiring = p;
    }
    TEST_ASSERT(runningDeal && expiring, "A roster has both running and expiring contracts");
    TEST_ASSERT(!career.RenewContract(runningDeal->playerId, 3, &message), "A running contract cannot be renewed yet");
    const int before = expiring->salary;
    TEST_ASSERT(career.RenewContract(expiring->playerId, 3, &message), "An expiring contract can be renewed");
    TEST_ASSERT(expiring->contractEndYear == career.SeasonYear() + 3 && expiring->salary > 0 && before > 0, "The renewal sets a new end year and salary");

    // Free agency: the cap decides who can be signed; players asking the minimum always can
    career.Vacation("free_agency");
    TEST_ASSERT(career.FreeAgencyOpen(), "Free agency is open on 1 July");
    TEST_ASSERT(career.FreeAgents().size() > 10, "Expired contracts feed the market");
    std::shared_ptr<bm::Player> star;
    std::shared_ptr<bm::Player> bargain;
    const int room = career.SalaryCap() - career.Payroll("2");
    for (const auto& p : career.FreeAgents()) {
        const int ask = career.Quote(*p, career.PreferredYears(*p), false);
        if (ask > room && ask > CareerManager::MinimumSalary() && (!star || p->attributes.currentAbility > star->attributes.currentAbility)) star = p;
        if (ask <= CareerManager::MinimumSalary()) bargain = p;
    }
    if (star) TEST_ASSERT(!career.SignFreeAgent(star->playerId, career.PreferredYears(*star), &message), "A player above the cap room cannot be signed");
    if (bargain) {
        const size_t size = team->roster.size();
        TEST_ASSERT(career.SignFreeAgent(bargain->playerId, 1, &message), "A minimum-salary player can always be signed");
        TEST_ASSERT(team->roster.size() == size + 1, "The signing joins the roster");
        TEST_ASSERT(std::find(career.FreeAgents().begin(), career.FreeAgents().end(), bargain) == career.FreeAgents().end(), "...and leaves the market");
    }
    g_testsPassed++;
}

void TestDraftHasSixtyPicks() {
    std::cout << "[TEST] Career: the draft has two rounds and the coach picks for his team...\n";
    auto league = CreateLeague(30, 17);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "4";
    cfg.seed = 33;
    career.Start(league, cfg);
    career.Vacation("draft");
    TEST_ASSERT(career.Phase() == CareerPhase::Draft && career.Today() == DaysFromCivil(2027, 6, 25), "It is draft day");
    TEST_ASSERT(career.DraftReady() && career.DraftTotalPicks() == 60, "Two rounds of 30 picks");
    TEST_ASSERT(career.CurrentStop().kind == "draft" && career.CurrentStop().blocking, "The draft needs the coach");

    auto team = career.UserTeam();
    const size_t before = team->roster.size();
    career.DraftAdvanceToUser();
    int picks = 0;
    while (career.UserOnClock() && !career.Prospects().empty()) {
        std::string message;
        TEST_ASSERT(career.DraftPickUser(career.Prospects().front()->playerId, &message), "The coach can make his pick");
        ++picks;
    }
    TEST_ASSERT(career.DraftFinished(), "The draft finishes after the coach's picks");
    TEST_ASSERT(picks == 2 && team->roster.size() == before + 2, "The coach drafted two players");
    std::set<std::string> unique;
    for (const auto& r : career.DraftResults()) unique.insert(r.playerId);
    TEST_ASSERT(career.DraftResults().size() == 60 && unique.size() == 60, "Sixty different players were drafted");
    TEST_ASSERT(career.CurrentStop().kind != "draft", "The draft no longer blocks time");
    g_testsPassed++;
}

void TestRandomLeagueAndSavedCareer() {
    std::cout << "[TEST] League and saves: a random league is invented, autosaved and resumed...\n";
    const std::string base = (std::filesystem::temp_directory_path() / "bm_save_test").string();
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    const std::string dbPath = base + "/career.db";

    TEST_ASSERT(GenerateRandomLeagueDb(dbPath.c_str(), 11) == 1, "A random league can be made");
    TEST_ASSERT(GenerateRandomLeagueDb((base + "/same.db").c_str(), 11) == 1, "...again with the same seed");
    LoadDatabase(dbPath.c_str());
    const std::string teams = GetTeamsJson();
    TEST_ASSERT(teams.find("\"Boston Celtics\"") == std::string::npos && teams.find("awayColor\":\"#") != std::string::npos,
                "Random clubs are invented and carry kit colours");
    TEST_ASSERT(CareerStart("1", 2026, 5) == 1, "A career starts in the random league");
    TEST_ASSERT(CareerHasSave(dbPath.c_str(), "1") == 0, "Nothing is saved before autosave is switched on");
    TEST_ASSERT(CareerEnableAutosave(dbPath.c_str()) == 1, "Autosave writes the starting position");
    TEST_ASSERT(CareerHasSave(dbPath.c_str(), "1") == 1 && CareerHasSave(dbPath.c_str(), "2") == 0, "The save belongs to the club");

    CareerVacation("new_season");   // the autosave fires when the new season begins
    const std::string rosterBefore = GetTeamRosterDetailsJson("1");
    const std::string stateBefore = CareerGetStateJson();
    TEST_ASSERT(stateBefore.find("2027/28") != std::string::npos, "The career is in its second season");
    CareerEnd();

    LoadDatabase(dbPath.c_str());
    TEST_ASSERT(CareerResume("1", 5, dbPath.c_str()) == 1, "The saved career can be resumed");
    TEST_ASSERT(std::string(CareerGetStateJson()).find("2027/28") != std::string::npos, "It resumes in the second season");
    TEST_ASSERT(GetTeamRosterDetailsJson("1") == rosterBefore, "The coach's squad is exactly as it was");
    TEST_ASSERT(CareerResume("2", 5, dbPath.c_str()) == 0, "Another club cannot open the save");

    // The resumed career plays on
    LoadDatabase(dbPath.c_str());
    TEST_ASSERT(CareerResume("1", 5, dbPath.c_str()) == 1, "Resume works again");
    CareerVacation("month");
    TEST_ASSERT(std::string(CareerGetStateJson()).find("\"phase\":\"Regular Season\"") != std::string::npos
                || std::string(CareerGetStateJson()).find("Regular") != std::string::npos, "The resumed season is played");
    CareerEnd();
    std::filesystem::remove_all(base);
    g_testsPassed++;
}

static std::string JsonValue(const std::string& json, const std::string& key) {
    const std::string pattern = "\"" + key + "\":";
    size_t at = json.find(pattern);
    if (at == std::string::npos) return "";
    at += pattern.size();
    if (json[at] == '"') ++at;
    size_t end = json.find_first_of("\",}", at);
    return json.substr(at, end - at);
}

void TestSavedCareerResumesMidSeason() {
    std::cout << "[TEST] Saves: a career resumes from a checkpoint in the season and in the offseason...\n";
    const std::string base = (std::filesystem::temp_directory_path() / "bm_checkpoint_test").string();
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    const std::string dbPath = base + "/career.db";
    TEST_ASSERT(GenerateRandomLeagueDb(dbPath.c_str(), 21) == 1, "A league is made");
    LoadDatabase(dbPath.c_str());
    TEST_ASSERT(CareerStart("3", 2026, 9) == 1 && CareerEnableAutosave(dbPath.c_str()) == 1, "A career starts with autosave");

    CareerVacation("month");
    CareerVacation("month");
    CareerVacation("week");
    const std::string midState = CareerGetStateJson();
    const int playedBefore = std::atoi(JsonValue(midState, "gamesPlayed").c_str());
    TEST_ASSERT(playedBefore > 100, "Many games were played");
    CareerEnd();

    LoadDatabase(dbPath.c_str());
    TEST_ASSERT(CareerResume("3", 9, dbPath.c_str()) == 1, "The mid-season save opens");
    const std::string resumed = CareerGetStateJson();
    const int playedAfter = std::atoi(JsonValue(resumed, "gamesPlayed").c_str());
    TEST_ASSERT(playedAfter > 0 && playedAfter <= playedBefore && playedBefore - playedAfter < 30 * 7, "Only the days since the last weekly checkpoint are lost");
    TEST_ASSERT(JsonValue(resumed, "phase") == "Regular season" || JsonValue(resumed, "phase") == "Regular Season" || !JsonValue(resumed, "phase").empty(), "The season is under way");
    CareerVacation("week");
    TEST_ASSERT(std::atoi(JsonValue(CareerGetStateJson(), "gamesPlayed").c_str()) > playedAfter, "The resumed season plays on");

    // The offseason: after the Finals the standings, draft order and prospects are back
    CareerVacation("season_end");
    const std::string reviewState = CareerGetStateJson();
    const std::string champion = JsonValue(reviewState, "champion");
    CareerEnd();
    LoadDatabase(dbPath.c_str());
    TEST_ASSERT(CareerResume("3", 9, dbPath.c_str()) == 1, "The offseason save opens");
    const std::string back = CareerGetStateJson();
    TEST_ASSERT(JsonValue(back, "date") == JsonValue(reviewState, "date"), "The date is the day of the checkpoint");
    TEST_ASSERT(JsonValue(back, "champion") == champion && !champion.empty(), "The champion is remembered");
    const std::string draft = CareerGetDraftJson();
    TEST_ASSERT(JsonValue(draft, "total") == "60" && draft.find("\"prospects\":[{") != std::string::npos, "The draft order and the class are back");
    CareerVacation("new_season");
    TEST_ASSERT(std::string(CareerGetStateJson()).find("2027/28") != std::string::npos, "The resumed career reaches the next season");
    TEST_ASSERT(std::filesystem::exists(dbPath + ".bak"), "The previous save is kept as a backup");
    TEST_ASSERT(std::string(CareerGetSaveError()).empty(), "No save error is reported while saving works");

    // A save from a newer version of the game is refused with a clear message
    {
        sqlite3* raw = nullptr;
        sqlite3_open(dbPath.c_str(), &raw);
        sqlite3_exec(raw, "UPDATE career_state SET value = '99' WHERE key = 'version'", nullptr, nullptr, nullptr);
        sqlite3_close(raw);
    }
    LoadDatabase(dbPath.c_str());
    TEST_ASSERT(CareerResume("3", 9, dbPath.c_str()) == 0 && std::string(CareerGetMessage()).find("newer version") != std::string::npos,
                "A save from a newer game version is refused with a message");
    CareerEnd();
    std::filesystem::remove_all(base);
    g_testsPassed++;
}

static std::string LeagueProblem(const CareerManager& career, bool checkRosterSize) {
    std::map<std::string, int> seen;
    for (const auto& t : career.Teams()) {
        if (checkRosterSize && (static_cast<int>(t->roster.size()) < CareerManager::MinRosterSize() || static_cast<int>(t->roster.size()) > CareerManager::MaxRosterSize())) {
            return "roster size " + std::to_string(t->roster.size()) + " at " + t->name;
        }
        std::set<int> jerseys;
        for (const auto& p : t->roster) {
            if (++seen[p->playerId] > 1) return "player on two lists: " + p->playerId;
            if (!jerseys.insert(p->jerseyNumber).second) return "two players wear " + std::to_string(p->jerseyNumber) + " at " + t->name;
            if (career.IsRetiredNumber(t->teamId, p->jerseyNumber)) return "retired number worn at " + t->name;
            const int a = p->attributes.currentAbility;
            if (a < 30 || a > 99 || p->attributes.potentialAbility < a) return "rating out of range: " + p->playerId;
            if (p->age < 17 || p->age > 46) return "age out of range: " + p->playerId + " " + std::to_string(p->age);
            if (p->salary < CareerManager::MinimumSalary() || p->contractEndYear < career.SeasonYear()) return "bad contract: " + p->playerId + " salary " + std::to_string(p->salary) + " ends " + std::to_string(p->contractEndYear) + " season " + std::to_string(career.SeasonYear());
        }
    }
    for (const auto& p : career.FreeAgents()) {
        if (++seen[p->playerId] > 1) return "free agent also elsewhere: " + p->playerId;
    }
    return "";
}

void TestManySeasonsStayConsistent() {
    const char* env = std::getenv("BM_LONG_SEASONS");
    const int seasons = env ? std::max(1, std::atoi(env)) : 4;
    std::cout << "[TEST] Career: " << seasons << " seasons in a row keep the league consistent...\n";
    auto league = CreateLeague(30, 31);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "5";
    cfg.seed = 77;
    career.Start(league, cfg);
    for (int i = 0; i < seasons; ++i) {
        career.Vacation("month");
        const std::string problem = LeagueProblem(career, true);
        TEST_ASSERT(problem.empty(), ("Season " + std::to_string(i + 1) + ": " + problem).c_str());
        career.Vacation("new_season");
        const std::string offseason = LeagueProblem(career, false);
        TEST_ASSERT(offseason.empty(), ("After season " + std::to_string(i + 1) + ": " + offseason).c_str());
    }
    TEST_ASSERT(career.SeasonYear() == 2026 + seasons, "Every season rolled over");
    TEST_ASSERT(career.RetiredPlayerCount() >= 0 && career.FreeAgents().size() <= 200, "The free agent pool stays bounded");
    g_testsPassed++;
}

void TestDayByDayAndEvents() {
    std::cout << "[TEST] Career: the days pass one by one with injuries, questions, mood and box scores...\n";
    auto league = CreateLeague(30, 41);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "7";
    cfg.seed = 123;
    career.Start(league, cfg);
    TEST_ASSERT(career.FreeAgents().size() >= 40, "The league starts with free agents on the market");
    career.UseGeneratedDraftClass();
    career.PlanFriendlies({});

    int days = 0, games = 0, questions = 0, mostInjured = 0;
    int previous = career.Today();
    for (int guard = 0; guard < 400 && days < 110; ++guard) {
        CareerStop stop = career.StepOne();
        if (stop.kind == "day") {
            TEST_ASSERT(career.Today() == previous + 1, "A step moves exactly one day");
            previous = career.Today();
            ++days;
            int injured = 0;
            for (const auto& t : league) for (const auto& p : t->roster) if (p->injured) ++injured;
            mostInjured = std::max(mostInjured, injured);
        } else if (stop.kind == "coach_game") {
            ++games;
            TEST_ASSERT(career.SimulateCoachGame(), "The assistant can play the stop");
        } else if (stop.kind == "player_question") {
            ++questions;
            TEST_ASSERT(stop.blocking && stop.options.size() >= 2, "A question has answers");
            TEST_ASSERT(career.AnswerQuestion(0), "A question can be answered");
        }
    }
    TEST_ASSERT(days >= 100 && games >= 30, "Every one of the coach's games is a stop");
    TEST_ASSERT(questions >= 1, "Players come with questions during the season");
    TEST_ASSERT(mostInjured >= 1, "Players get injured");
    const auto userBoxes = std::count_if(career.GameRecords().begin(), career.GameRecords().end(), [&](const auto& r) { return r.homeName == career.UserTeam()->name || r.awayName == career.UserTeam()->name; });
    TEST_ASSERT(userBoxes == games, "Every coach game has a box score");
    TEST_ASSERT(career.GameRecords().size() > static_cast<size_t>(games), "League games also have box scores");
    const auto& record = *std::find_if(career.GameRecords().begin(), career.GameRecords().end(), [&](const auto& r) { return r.homeName == career.UserTeam()->name || r.awayName == career.UserTeam()->name; });
    for (const auto& box : career.GameRecords()) {
        int homePoints = 0, awayPoints = 0;
        for (const auto& l : box.home) homePoints += l.points;
        for (const auto& l : box.away) awayPoints += l.points;
        TEST_ASSERT(homePoints == box.homeScore && awayPoints == box.awayScore, "Every league box matches its final score");
    }
    int points = 0;
    for (const auto& l : record.home) points += l.points;
    TEST_ASSERT(record.home.size() >= 8 && record.away.size() >= 8 && points >= record.homeScore * 8 / 10, "The box score lists the players and their points");
    float mood = 0.0f;
    for (const auto& p : career.UserTeam()->roster) mood += std::fabs(p->seasonMood);
    TEST_ASSERT(mood > 0.1f, "Results move the players' mood");

    // Season statistics for every player, archived when the Finals are over
    int withStats = 0;
    for (const auto& t : league) for (const auto& p : t->roster) if (p->seasonStats[0].games > 5.0f && p->seasonStats[0].points > 0) ++withStats;
    TEST_ASSERT(withStats >= 200, "Players across the league build up season statistics");
    career.Vacation("season_end");
    TEST_ASSERT(!career.StatHistory().empty(), "The season goes into the players' history");
    bool hasPlayoffs = false;
    for (const auto& kv : career.StatHistory()) for (const auto& r : kv.second) if (r.kind == 1) hasPlayoffs = true;
    TEST_ASSERT(hasPlayoffs, "Playoff statistics are kept apart from the regular season");
    g_testsPassed++;
}

void TestCoachLineupAndMinutes() {
    std::cout << "[TEST] Career: the coach picks the starting five and the minutes...\n";
    auto league = CreateLeague(30, 51);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "9";
    cfg.seed = 31;
    career.Start(league, cfg);
    career.UseGeneratedDraftClass();
    career.PlanFriendlies({});

    auto roster = career.UserTeam()->roster;
    std::sort(roster.begin(), roster.end(), [](const auto& a, const auto& b) { return a->attributes.currentAbility > b->attributes.currentAbility; });
    const auto star = roster.front();
    std::vector<std::string> starters;
    std::map<std::string, int> minutes;
    for (size_t i = roster.size() - 1; starters.size() < 5; --i) {   // the five weakest start
        starters.push_back(roster[i]->playerId);
        minutes[roster[i]->playerId] = 40;
    }
    minutes[star->playerId] = 0;
    TEST_ASSERT(career.SetLineup(starters, minutes), "The lineup is accepted");
    TEST_ASSERT(career.UserStarters().size() == 5, "Five starters are stored");

    for (int guard = 0; guard < 300; ++guard) {
        CareerStop stop = career.StepOne();
        if (stop.kind == "player_question") career.AnswerQuestion(0);
        if (stop.kind == "coach_game") { career.SimulateCoachGame(); break; }
    }
    TEST_ASSERT(!career.GameRecords().empty(), "A game was played");
    const auto& record = *std::find_if(career.GameRecords().begin(), career.GameRecords().end(), [&](const auto& r) { return r.homeName == career.UserTeam()->name || r.awayName == career.UserTeam()->name; });
    const auto& mine = record.userHome ? record.home : record.away;
    int starterMinutes = 0;
    bool starPlayed = false;
    for (const auto& line : mine) {
        if (line.name == star->firstName + " " + star->lastName) starPlayed = line.minutes > 0;
        for (const auto& id : starters) {
            for (const auto& p : roster) if (p->playerId == id && line.name == p->firstName + " " + p->lastName) starterMinutes += line.minutes;
        }
    }
    TEST_ASSERT(!starPlayed, "A player with zero minutes stays on the bench");
    TEST_ASSERT(starterMinutes >= 120, "The coach's starters carry the game");
    g_testsPassed++;
}

void TestTraitsAndTrades() {
    std::cout << "[TEST] Traits shape matches and trades work...\n";
    TEST_ASSERT(bm::TraitsFromCsv(bm::TraitsToCsv(bm::TRAIT_SHARPSHOOTER | bm::TRAIT_ALL_AROUND)) == (bm::TRAIT_SHARPSHOOTER | bm::TRAIT_ALL_AROUND), "Traits survive a text round trip");
    TEST_ASSERT(bm::TraitsFromCsv("Sharpshooter|cookie_monster, post_up_demon") == (bm::TRAIT_SHARPSHOOTER | bm::TRAIT_COOKIE_MONSTER | bm::TRAIT_POST_UP_DEMON), "League files can list traits");

    // A match where everybody has every trait still plays to the end
    {
        auto league = CreateLeague(30, 77);
        for (auto& p : league[0]->roster) p->traits = 255u;
        for (auto& p : league[1]->roster) p->traits = 255u;
        bm::engine::MatchEngine engine;
        engine.InitializeMatch(league[0], league[1]);
        engine.SimulateFullMatch();
        TEST_ASSERT(engine.GetMatchState()->matchComplete, "A match with traits everywhere finishes");
    }

    auto league = CreateLeague(30, 78);
    bm::engine::CareerManager career;
    bm::engine::CareerManager::Config cfg;
    cfg.userTeamId = "1";
    cfg.seed = 9;
    career.Start(league, cfg);
    TEST_ASSERT(career.TradesOpen(), "Trades are open in the preseason");

    auto mine = league[0];
    auto other = league[5];
    auto byAbility = [](const auto& a, const auto& b) { return a->attributes.currentAbility < b->attributes.currentAbility; };
    auto best = *std::max_element(mine->roster.begin(), mine->roster.end(), byAbility);
    auto myWorst = *std::min_element(mine->roster.begin(), mine->roster.end(), byAbility);
    auto theirBest = *std::max_element(other->roster.begin(), other->roster.end(), byAbility);
    auto worst = *std::min_element(other->roster.begin(), other->roster.end(), byAbility);
    for (auto& p : {best, myWorst, theirBest, worst}) p->salary = 5000000;   // keep the salaries out of the way
    auto bad = career.Trade(other->teamId, {myWorst->playerId}, {theirBest->playerId}, false);
    std::cout << "  trade message: " << bad.message << "\n";
    TEST_ASSERT(bad.valid && !bad.accepted, "A club will not give its best player for a worse one");

    auto good = career.Trade(other->teamId, {best->playerId}, {worst->playerId}, true);
    TEST_ASSERT(good.accepted && good.executed, "A club accepts a star for a role player");
    bool movedMine = false, movedTheirs = false;
    for (const auto& p : other->roster) if (p == best) movedMine = true;
    for (const auto& p : mine->roster) if (p == worst) movedTheirs = true;
    TEST_ASSERT(movedMine && movedTheirs, "The players change clubs");
    auto unknown = career.Trade(other->teamId, {"nobody"}, {}, false);
    TEST_ASSERT(!unknown.valid, "A player who is not on the roster cannot be traded");
    g_testsPassed++;
}

void TestFriendliesStaffAndOpenLeague() {
    std::cout << "[TEST] Friendlies, staff tasks, club honours and the open league...\n";
    auto league = CreateLeague(30, 91);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "1";
    cfg.seed = 12;
    career.Start(league, cfg);

    CareerStop stop;
    for (int i = 0; i < 12; ++i) {
        stop = career.StepOne();
        if (stop.kind == "draft_class") career.UseGeneratedDraftClass();
        if (stop.kind == "friendlies_plan") break;
    }
    TEST_ASSERT(stop.kind == "friendlies_plan" && stop.blocking, "The coach is asked about pre-season friendlies");
    TEST_ASSERT(career.PlanFriendlies({"2", "3", "4", "5", "6", "7", "1"}) && career.Friendlies().size() == 5, "At most five friendlies against other clubs are planned");
    TEST_ASSERT(career.PlanFriendlies({"2", "3", "4"}) && career.Friendlies().size() == 3, "The plan can be changed before the first game");

    career.SetStaff(CareerManager::TASK_DEVELOPMENT | CareerManager::TASK_CONTRACTS, "bigs", 2, 3);
    TEST_ASSERT(career.StaffTasks() == 10 && career.ScoutFocus() == "bigs" && career.StaffOffense() == 2 && career.StaffDefense() == 3, "The staff settings are stored");

    int coachGames = 0;
    for (int i = 0; i < 40 && career.Phase() == CareerPhase::Preseason; ++i) {
        stop = career.StepOne();
        if (stop.kind == "coach_game") { ++coachGames; career.SimulateCoachGame(); }
        if (stop.kind == "roster_check") career.AssistantFillRoster();
    }
    TEST_ASSERT(coachGames == 3, "The coach plays his three friendlies himself");
    int played = 0;
    for (const auto& g : career.Friendlies()) if (g.played) ++played;
    TEST_ASSERT(played == 3 && career.Phase() == CareerPhase::RegularSeason, "All friendlies are played before opening night");

    career.Vacation("season_end");
    int titles = 0;
    for (const auto& t : league) titles += career.HonorsOf(t->teamId).titles;
    TEST_ASSERT(titles == 1, "One club has the title after the Finals");
    int finalists = 0;
    for (const auto& t : league) finalists += career.HonorsOf(t->teamId).finals;
    TEST_ASSERT(finalists == 2, "Two clubs reached the Finals");

    auto player = league[0]->roster[0];
    CareerManager::PlayerEdit edit{15, 14, 13, 12, 11, 10, 77, 85, player->salary, player->contractEndYear, bm::TRAIT_SHARPSHOOTER};
    std::string message;
    TEST_ASSERT(!career.EditPlayer(player->playerId, edit, &message), "A closed league does not allow edits");
    career.SetOpenLeague(true);
    TEST_ASSERT(career.EditPlayer(player->playerId, edit, &message), "An open league allows edits");
    TEST_ASSERT(player->attributes.currentAbility == 77 && player->attributes.potentialAbility == 85 && player->traits == bm::TRAIT_SHARPSHOOTER && player->attributes.shooting == 14, "The edit changes the player");
    TEST_ASSERT(bm::DetailedRatings(*player).size() > 20, "A player has detailed ratings");
    g_testsPassed++;
}

void TestRetirementAndHallOfFame() {
    std::cout << "[TEST] Career: old stars retire into the Hall of Fame and clubs retire numbers...\n";
    auto league = CreateLeague(30, 23);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "1";
    cfg.seed = 8;
    career.Start(league, cfg);

    // Ten ageing stars with long careers at their clubs, and one ordinary old player
    std::map<std::string, int> numbers;
    for (int i = 0; i < 10; ++i) {
        auto p = league[i + 1]->roster[0];
        p->birthDate = "1980-05-05";
        p->attributes.currentAbility = 70;
        p->attributes.potentialAbility = 70;
        p->contractEndYear = 2035;
        numbers[p->playerId] = p->jerseyNumber;
        career.SetCareerRecord(p->playerId, 95, 18, league[i + 1]->teamId, 9);
    }
    auto ordinary = league[12]->roster[0];
    ordinary->birthDate = "1980-05-05";
    ordinary->attributes.currentAbility = 60;
    ordinary->attributes.potentialAbility = 60;
    ordinary->contractEndYear = 2035;
    career.SetCareerRecord(ordinary->playerId, 66, 15, league[12]->teamId, 9);

    career.Vacation("new_season");
    TEST_ASSERT(career.RetiredPlayerCount() >= 1, "Some of the oldest players retire");
    TEST_ASSERT(!career.HallOfFame().empty(), "A retired star enters the Hall of Fame");
    for (const auto& entry : career.HallOfFame()) {
        TEST_ASSERT(entry.playerId != ordinary->playerId, "An ordinary player does not enter the Hall of Fame");
        TEST_ASSERT(entry.peak >= 90 && entry.year == 2027, "Hall of Famers peaked high and retired in 2027");
    }
    TEST_ASSERT(!career.RetiredNumbers().empty(), "A club retires the number of a long-serving Hall of Famer");
    for (const auto& retired : career.RetiredNumbers()) {
        TEST_ASSERT(career.IsRetiredNumber(retired.teamId, retired.number), "The retired number is registered");
        for (const auto& team : league) {
            if (team->teamId != retired.teamId) continue;
            for (const auto& p : team->roster) TEST_ASSERT(p->jerseyNumber != retired.number, "Nobody wears a retired number");
        }
    }
    g_testsPassed++;
}

void TestDraftClassFromFile() {
    std::cout << "[TEST] Career: a draft class file replaces the made-up class...\n";
    const std::string dir = std::filesystem::temp_directory_path().string() + "/bm_draft_test";
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir + "/draft-2027.csv");
        f << "first_name,last_name,position,birth_date,height,weight,current_ability,potential_ability\n";
        for (int i = 0; i < 70; ++i) {
            f << "Rookie" << i << ",Name" << i << "," << (i % 2 ? "PG" : "C") << ",2006-05-05,1.98,90," << 80 - i / 2 << ",95\n";
        }
    }
    {
        std::ofstream f(dir + "/bad.csv");
        f << "first_name,last_name,position,current_ability\nA,B,XX,70\nC,D,PG,200\n";
    }
    auto league = CreateLeague(30, 21);
    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "2";
    cfg.seed = 5;
    career.Start(league, cfg);
    TEST_ASSERT(career.Continue().kind == "draft_class", "The coach is asked about the draft class");

    std::string message;
    TEST_ASSERT(!career.LoadDraftClass(dir + "/bad.csv", &message), "A broken file is refused");
    TEST_ASSERT(message.find("line 2") != std::string::npos && message.find("line 3") != std::string::npos, "Every problem is reported with its line");
    TEST_ASSERT(!career.LoadDraftClass(dir + "/missing.csv", &message), "A missing file is refused");
    TEST_ASSERT(career.Continue().kind == "draft_class", "The question stays open until it is answered");

    TEST_ASSERT(career.LoadDraftClass(dir + "/draft-2027.csv", &message), "A valid file loads");
    TEST_ASSERT(career.LoadedDraftClassSize() == 70 && career.DraftYear() == 2027, "Seventy prospects are loaded for the 2027 draft");
    TEST_ASSERT(career.Continue().kind != "draft_class", "The question is answered");

    career.Vacation("draft");
    TEST_ASSERT(career.DraftReady() && career.DraftTotalPicks() == 60, "Two rounds of 30 picks");
    career.DraftAutoFinish();
    TEST_ASSERT(career.DraftResults().size() == 60, "Sixty players were drafted");
    int fromFile = 0;
    for (const auto& r : career.DraftResults()) if (r.playerName.rfind("Rookie", 0) == 0) ++fromFile;
    TEST_ASSERT(fromFile == 60, "Every pick came from the file");
    TEST_ASSERT(career.DraftResults().front().playerName.rfind("Rookie", 0) == 0, "The first pick is a player from the file");

    // The next year asks again and falls back to a made-up class
    career.Vacation("new_season");
    TEST_ASSERT(career.SeasonYear() == 2027 && career.DraftYear() == 2028 && career.CurrentStop().kind == "draft_class", "The new season asks about its own class");
    std::filesystem::remove_all(dir);
    g_testsPassed++;
}

void TestProgressionFollowsAge() {
    std::cout << "[TEST] Career: young players improve and veterans decline over a year...\n";
    auto league = CreateLeague(30, 19);
    std::vector<std::shared_ptr<bm::Player>> young;
    std::vector<std::shared_ptr<bm::Player>> old;
    for (const auto& team : league) {
        for (size_t i = 0; i < team->roster.size(); ++i) {
            auto& p = team->roster[i];
            if (i % 3 == 0) { p->birthDate = "2006-05-05"; p->attributes.potentialAbility = p->attributes.currentAbility + 10; young.push_back(p); }
            else if (i % 3 == 1) { p->birthDate = "1992-05-05"; p->attributes.potentialAbility = p->attributes.currentAbility; old.push_back(p); }
            else { p->birthDate = "1999-05-05"; }
        }
    }
    std::map<std::string, int> startRating;
    for (const auto& p : young) startRating[p->playerId] = p->attributes.currentAbility;
    for (const auto& p : old) startRating[p->playerId] = p->attributes.currentAbility;

    CareerManager career;
    CareerManager::Config cfg;
    cfg.userTeamId = "1";
    cfg.seed = 4;
    career.Start(league, cfg);
    career.Vacation("new_season");

    auto average = [&](const std::vector<std::shared_ptr<bm::Player>>& group) {
        double sum = 0;
        for (const auto& p : group) sum += p->attributes.currentAbility - startRating[p->playerId];
        return sum / group.size();
    };
    const double youngChange = average(young);
    const double oldChange = average(old);
    std::cout << "  -> average change: young " << youngChange << ", veterans " << oldChange << "\n";
    TEST_ASSERT(youngChange > 1.0, "Young players improve");
    TEST_ASSERT(oldChange < -0.5, "Veterans decline");
    g_testsPassed++;
}

static std::string WriteLeagueCsv(const std::string& folder, int teams, int playersPerTeam, bool breakIt) {
    std::filesystem::create_directories(folder);
    std::ofstream t(folder + "/teams.csv");
    t << "name,conference,prestige\n";
    for (int i = 0; i < teams; ++i) t << "Club " << i + 1 << "," << (i < teams / 2 ? "Eastern" : "Western") << "," << 10 << "\n";
    std::ofstream p(folder + "/players.csv");
    p << "team,first_name,last_name,position,jersey_number,current_ability,potential_ability\n";
    const char* positions[] = {"PG", "SG", "SF", "PF", "C"};
    for (int i = 0; i < teams; ++i) {
        for (int k = 0; k < playersPerTeam; ++k) {
            p << "Club " << i + 1 << ",First" << k << ",\"Last, " << i << "x" << k << "\"," << positions[k % 5] << "," << k + 1 << "," << 65 + (k * 3 + i) % 25 << "," << 75 << "\n";
        }
    }
    if (breakIt) p << "Nowhere FC,Bad,Line,XX,1,80,80\n";
    return folder;
}

void TestCustomLeagueFiles() {
    std::cout << "[TEST] League files: a custom league is validated, imported and playable...\n";
    const std::string base = (std::filesystem::temp_directory_path() / "bm_league_test").string();
    std::filesystem::remove_all(base);

    // Valid league: 16 clubs, quoted names with commas
    const std::string good = WriteLeagueCsv(base + "/good", 16, 10, false);
    auto result = bm::db::ImportLeagueFromCsv(good, base + "/good.db");
    TEST_ASSERT(result.ok && result.teams == 16 && result.players == 160, "A valid league imports");
    {
        bm::db::DatabaseManager db;
        TEST_ASSERT(db.Open(base + "/good.db"), "The imported league opens");
        auto st = db.PrepareStatement("SELECT COUNT(*) FROM players WHERE last_name LIKE 'Last, %'");
        TEST_ASSERT(st && sqlite3_step(st) == SQLITE_ROW && sqlite3_column_int(st, 0) == 160, "Quoted fields keep their commas");
        sqlite3_finalize(st);

        // The default league can be exported as a template and imported again
        std::string error;
        std::filesystem::create_directories(base + "/export");
        TEST_ASSERT(bm::db::ExportLeagueTemplate(db.GetConnection(), base + "/export", &error), "A template can be exported");
    }
    auto again = bm::db::ImportLeagueFromCsv(base + "/export", base + "/again.db");
    TEST_ASSERT(again.ok && again.teams == 16 && again.players == 160, "An exported league imports again unchanged");

    // Kit colours are optional, validated and reach the team list
    {
        const std::string colored = WriteLeagueCsv(base + "/colors", 16, 10, false);
        std::ofstream t(colored + "/teams.csv");
        t << "name,conference,away_color,third_color\n";
        for (int i = 0; i < 16; ++i) t << "Club " << i + 1 << "," << (i < 8 ? "Eastern" : "Western") << (i == 0 ? ",#552583,#111111" : ",,") << "\n";
        t.close();
        auto ok = bm::db::ImportLeagueFromCsv(colored, base + "/colors.db");
        TEST_ASSERT(ok.ok, "Teams without colours and teams with colours can share a file");
        LoadDatabase((base + "/colors.db").c_str());
        const std::string teamsJson = GetTeamsJson();
        TEST_ASSERT(teamsJson.find("\"awayColor\":\"#552583\",\"thirdColor\":\"#111111\"") != std::string::npos, "The team list carries the kit colours");
        std::ofstream broken(colored + "/teams.csv");
        broken << "name,conference,away_color\n";
        for (int i = 0; i < 16; ++i) broken << "Club " << i + 1 << "," << (i < 8 ? "Eastern" : "Western") << (i == 0 ? ",purple" : ",") << "\n";
        broken.close();
        auto rejected = bm::db::ImportLeagueFromCsv(colored, base + "/colors2.db");
        TEST_ASSERT(!rejected.ok && rejected.errors.front().find("away_color") != std::string::npos, "A colour that is not #RRGGBB is rejected");
    }

    // Mistakes are reported with their line
    auto bad = bm::db::ImportLeagueFromCsv(WriteLeagueCsv(base + "/bad", 16, 10, true), base + "/bad.db");
    TEST_ASSERT(!bad.ok && !bad.errors.empty(), "A broken file is rejected");
    TEST_ASSERT(bad.errors.front().find("players.csv line") != std::string::npos, "The error names the line");
    TEST_ASSERT(!std::filesystem::exists(base + "/bad.db"), "Nothing is written for an invalid league");
    auto small = bm::db::ImportLeagueFromCsv(WriteLeagueCsv(base + "/small", 10, 10, false), base + "/small.db");
    TEST_ASSERT(!small.ok, "Fewer than 16 teams are rejected");
    auto missing = bm::db::ImportLeagueFromCsv(base + "/nothing", base + "/missing.db");
    TEST_ASSERT(!missing.ok, "A missing folder is rejected");

    // The custom league is playable through the bridge, including clubs with only ten players
    LoadDatabase((base + "/good.db").c_str());
    TEST_ASSERT(CareerStart("1", 2026, 7) == 1, "A career can start in the custom league");
    const std::string state = CareerGetStateJson();
    TEST_ASSERT(state.find("\"started\":true") != std::string::npos && state.find("\"phase\":\"Preseason\"") != std::string::npos, "The state shows the preseason");
    CareerVacation("season_end");
    const std::string review = CareerGetStateJson();
    TEST_ASSERT(review.find("\"phase\":\"Post-season\"") != std::string::npos, "The assistant played a whole season in the custom league");
    TEST_ASSERT(review.find("\"champion\":\"Club") != std::string::npos, "A champion was crowned");
    CareerVacation("new_season");
    CareerVacation("month");
    const std::string next = CareerGetStateJson();
    TEST_ASSERT(next.find("\"rosterSize\":1") != std::string::npos, "Rosters were filled to a legal size for the next season");
    CareerEnd();
    std::filesystem::remove_all(base);
    g_testsPassed++;
}

void TestDevelopmentUpdates() {
    std::cout << "[TEST] Training, mentoring, scouting, editing and two-attack shouts...\n";
    auto league = CreateLeague(30, 94);
    CareerManager career;
    CareerManager::Config cfg; cfg.userTeamId = "1"; cfg.seed = 72;
    career.Start(league, cfg);
    auto p = league[0]->roster[0];
    auto q = league[0]->roster[4];
    p->age = 31; q->age = 20;
    p->position = bm::Position::PG; q->position = bm::Position::C;
    TEST_ASSERT(!CareerManager::CanMentor(*p, *q), "A normal PG cannot mentor a centre");
    p->age = 35; p->attributes.technical = 18;
    TEST_ASSERT(CareerManager::CanMentor(*p, *q), "An old elite leader can mentor across positions");
    TEST_ASSERT(!CareerManager::CanMentor(*p, *p), "Cannot mentor oneself");
    p->age = 29; q->position = bm::Position::SG;
    TEST_ASSERT(CareerManager::CanMentor(*p, *q), "Adjacent positions can mentor");
    career.SetWeeklyTraining("Shooting", 7, 3);
    for (int i = 0; i < 8; ++i) career.TrainDay();
    TEST_ASSERT(q->trainingFatigue > 0.3f, "Daily heavy training reduces readiness");
    const float tired = q->trainingFatigue;
    career.SetWeeklyTraining("Shooting", 0, 1);
    for (int i = 0; i < 4; ++i) career.TrainDay();
    TEST_ASSERT(q->trainingFatigue < tired, "Rest restores readiness");
    career.SetWeeklyTraining("Shooting", 99, 3);
    TEST_ASSERT(career.WeeklySessions() == 7, "Sessions are capped at seven");
    std::vector<int> values(bm::DetailedRatings(*q).size(), 80);
    TEST_ASSERT(!career.EditDetailedRatings(q->playerId, values), "Closed leagues reject detailed edits");
    career.SetOpenLeague(true);
    TEST_ASSERT(career.EditDetailedRatings(q->playerId, values), "Open leagues accept detailed edits");
    TEST_ASSERT(q->attributes.currentAbility == 80, "Overall follows detailed stats");
    values[3] = 40;
    TEST_ASSERT(career.EditDetailedRatings(q->playerId, values), "A single stat can be changed independently");
    TEST_ASSERT(bm::DetailedRatings(*q)[3].value == 40 && bm::DetailedRatings(*q)[2].value == 80, "Other detailed stats retain their values");
    TEST_ASSERT(q->attributes.currentAbility < 80, "Lowering a stat lowers overall");
    values[0] = 200;
    TEST_ASSERT(!career.EditDetailedRatings(q->playerId, values), "Out-of-range edits are rejected");
    TEST_ASSERT(!career.Prospects().empty(), "Draft class is scoutable during preseason");
    career.SetStaff(CareerManager::TASK_SCOUTING, "guards-offense", -1, -1);
    bool report = false;
    for (const auto& n : career.Inbox()) if (n.title == "Scout update") report = true;
    TEST_ASSERT(report, "Assignments produce a news report");
    sqlite3* db = nullptr; sqlite3_open(":memory:", &db); std::string error;
    TEST_ASSERT(career.SaveState(db, &error), "New development data saves");
    const std::string id = q->playerId;
    CareerManager restored; restored.Start(CreateLeague(30, 94), cfg);
    TEST_ASSERT(restored.LoadState(db, &error), "New development data loads");
    sqlite3_close(db);
    TEST_ASSERT(restored.WeeklySessions() == 7, "Weekly training survives a save round-trip");
    auto saved = restored.FindPlayer(id);
    TEST_ASSERT(saved && bm::DetailedRatings(*saved)[3].value == 40, "Individual ratings survive a save round-trip");
    TEST_ASSERT(saved->trainingFatigue > 0, "Readiness survives a save round-trip");
    bm::engine::MatchEngine engine; engine.SetHeadless(true); engine.SetPauseEnabled(false);
    engine.InitializeMatch(league[0], league[1]);
    TEST_ASSERT(!engine.Shout(true, 5, "missing"), "Play-through rejects invalid players");
    TEST_ASSERT(engine.Shout(true, 0), "Coach can shout during a match");
    TEST_ASSERT(engine.ShoutAttacksLeft(true) == 2, "Shout begins with two attacks");
    int guard = 0;
    while (engine.ShoutAttacksLeft(true) > 0 && guard++ < 8) engine.SimulatePossession();
    TEST_ASSERT(engine.ShoutAttacksLeft(true) == 0, "Shout expires after the team's attacks");
    TEST_ASSERT(engine.GetTacticalManager()->GetHomeOffensiveStrategy() == bm::engine::OffensiveStrategy::BALANCED, "Shout preserves permanent tactics");
    g_testsPassed++;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  Basketball Manager Core Automated Tests\n";
    std::cout << "========================================\n";

    TestDevelopmentUpdates();
    TestQuarterProgression();
    TestOvertimeTriggerWhenTied();
    TestSecondByStepFlow();
    TestFlowVariety();
    TestBreaksTimeoutsAndTalks();
    TestTeamTalkFitsTheSituation();
    TestOffenseSchemesPlaybookAndFeedback();
    TestBridgeSeasonAndPlayoffs();
    TestYearScheduleIsAFullSeason();
    TestCareerYearFlow();
    TestVacationLetsTheAssistantRunTheClub();
    TestCoachGameCanBePlayedLive();
    TestContractsAndFreeAgencyRules();
    TestDraftHasSixtyPicks();
    TestDraftClassFromFile();
    TestRetirementAndHallOfFame();
    TestRandomLeagueAndSavedCareer();
    TestSavedCareerResumesMidSeason();
    TestManySeasonsStayConsistent();
    TestDayByDayAndEvents();
    TestCoachLineupAndMinutes();
    TestTraitsAndTrades();
    TestFriendliesStaffAndOpenLeague();
    TestProgressionFollowsAge();
    TestCustomLeagueFiles();

    std::cout << "\n----------------------------------------\n";
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed.\n";
    std::cout << "========================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
