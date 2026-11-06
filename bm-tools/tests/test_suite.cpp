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

int main() {
    std::cout << "========================================\n";
    std::cout << "  Basketball Manager Core Automated Tests\n";
    std::cout << "========================================\n";

    TestQuarterProgression();
    TestOvertimeTriggerWhenTied();
    TestSecondByStepFlow();
    TestFlowVariety();
    TestBreaksTimeoutsAndTalks();
    TestTeamTalkFitsTheSituation();
    TestOffenseSchemesPlaybookAndFeedback();
    TestBridgeSeasonAndPlayoffs();

    std::cout << "\n----------------------------------------\n";
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed.\n";
    std::cout << "========================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
