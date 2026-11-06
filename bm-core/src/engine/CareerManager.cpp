#include "engine/CareerManager.h"
#include "player/PlayerGenerator.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <numeric>
#include <sstream>

namespace bm {
namespace engine {

// ---------------------------------------------------------------------------------------------
// Calendar helpers (proleptic Gregorian, days since 1970-01-01)
// ---------------------------------------------------------------------------------------------

int DaysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

CalDate CivilFromDays(int z) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = static_cast<int>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;
    CalDate out;
    out.year = y + (m <= 2 ? 1 : 0);
    out.month = static_cast<int>(m);
    out.day = static_cast<int>(d);
    return out;
}

int WeekdayOfDays(int z) {
    return z >= -4 ? (z + 4) % 7 : (z + 5) % 7 + 6;
}

std::string FormatDate(int days) {
    static const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const CalDate d = CivilFromDays(days);
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%s %d %s %d", weekdays[WeekdayOfDays(days)], d.day, months[d.month - 1], d.year);
    return buffer;
}

const char* PhaseName(CareerPhase phase) {
    switch (phase) {
        case CareerPhase::Preseason: return "Preseason";
        case CareerPhase::RegularSeason: return "Regular season";
        case CareerPhase::PlayIn: return "Play-In";
        case CareerPhase::Playoffs: return "Playoffs";
        case CareerPhase::Review: return "Post-season";
        case CareerPhase::Draft: return "Draft day";
        case CareerPhase::Offseason: return "Offseason";
        case CareerPhase::TrainingCamp: return "Training camp";
    }
    return "";
}

namespace {

std::string Money(int dollars) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "$%.1fM", dollars / 1000000.0);
    return buffer;
}

std::string FullName(const Player& p) { return p.firstName + " " + p.lastName; }

int BirthYearOf(const Player& p) {
    if (p.birthDate.size() < 4) return 0;
    try { return std::stoi(p.birthDate.substr(0, 4)); } catch (...) { return 0; }
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------

CareerManager::CareerManager() : rng(std::random_device{}()) {}

unsigned CareerManager::Hash(const std::string& text) const {
    unsigned h = 2166136261u;
    for (unsigned char c : text) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

void CareerManager::Start(const std::vector<std::shared_ptr<Team>>& teamList, const Config& cfg) {
    config = cfg;
    teams = teamList;
    teamById.clear();
    for (const auto& t : teams) teamById[t->teamId] = t;
    userTeamId = cfg.userTeamId;
    rng.seed(cfg.seed != 0 ? cfg.seed : std::random_device{}());

    seasonYear = cfg.startYear;
    today = DateOf(seasonYear, 10, 1);
    regularStartDay = DateOf(seasonYear, 10, 21);
    phase = CareerPhase::Preseason;
    stageStartDay = today;
    lastRound = -1;
    lastSeriesCount = 0;
    championName.clear();
    inbox.clear();
    acknowledged.clear();
    completedWins = completedLosses = titlesWon = 0;
    seasonCounted = false;
    draftOrder.clear();
    prospects.clear();
    draftResults.clear();
    draftDone = false;
    freeAgents.clear();
    summerMode = 0;
    summerGames.clear();
    summerTeams.clear();
    summerPlayers.clear();
    summerWins.clear();
    campPlanned = false;
    nextProspectId = 1;
    assistantActive = false;

    AssignInitialContracts();

    // Cap and tax line follow the league's own payrolls (custom leagues come with other salary levels)
    {
        std::vector<int> payrolls;
        for (const auto& t : teams) payrolls.push_back(Payroll(t->teamId));
        std::sort(payrolls.begin(), payrolls.end());
        if (!payrolls.empty() && payrolls.back() > 0) {
            const int median = payrolls[payrolls.size() / 2];
            salaryCap = std::max(20000000, static_cast<int>(std::round(median * 0.95 / 1000000.0)) * 1000000);
            luxuryLine = std::max(static_cast<int>(salaryCap * 1.2), static_cast<int>(payrolls.back() * 1.05));
            luxuryLine = static_cast<int>(std::round(luxuryLine / 1000000.0)) * 1000000;
        }
    }

    season.SetQuiet(cfg.quiet);
    talentAnchor = TopTenAverage();
    season.SetGamesPerTeam(cfg.gamesPerTeam);
    if (cfg.seed != 0) season.SetScheduleSeed(cfg.seed);
    season.InitializeSeason(seasonYear, "NBA");
    season.LoadTeams(teams);
    season.GenerateSchedule();

    started = true;
    Acknowledge("newseason" + std::to_string(seasonYear));
    Post("Welcome", "Your career starts on " + FormatDate(today) + ". Opening night is " + FormatDate(regularStartDay) + ".");
}

void CareerManager::Post(const std::string& title, const std::string& text) {
    CareerEvent ev;
    ev.day = today;
    ev.title = title;
    ev.text = text;
    inbox.push_front(ev);
    while (inbox.size() > 60) inbox.pop_back();
}

bool CareerManager::Acknowledge(const std::string& key) {
    return acknowledged.insert(key).second;
}

std::shared_ptr<Team> CareerManager::FindTeam(const std::string& teamId) const {
    auto it = teamById.find(teamId);
    return it == teamById.end() ? nullptr : it->second;
}

int CareerManager::RegularSeasonDay() const {
    if (phase != CareerPhase::RegularSeason) return 0;
    return today - regularStartDay + 1;
}

bool CareerManager::PlayoffsVisible() const {
    return phase == CareerPhase::PlayIn || phase == CareerPhase::Playoffs;
}

bool CareerManager::FreeAgencyOpen() const {
    return phase == CareerPhase::Offseason || phase == CareerPhase::TrainingCamp
        || phase == CareerPhase::Preseason || phase == CareerPhase::RegularSeason;
}

bool CareerManager::InSummerLeague() const {
    if (phase != CareerPhase::Offseason) return false;
    return today >= SummerStart() && today <= SummerStart() + 9;
}

int CareerManager::Payroll(const std::string& teamId) const {
    auto team = FindTeam(teamId);
    if (!team) return 0;
    int total = 0;
    for (const auto& p : team->roster) total += p->salary;
    return total;
}

// ---------------------------------------------------------------------------------------------
// Stepping
// ---------------------------------------------------------------------------------------------

CareerStop CareerManager::Continue(int maxDays) {
    assistantActive = false;
    for (int i = 0; i < maxDays; ++i) {
        CareerStop stop = FindStop(true);
        if (stop.kind != "none") return stop;
        StepDay();
    }
    CareerStop limit;
    limit.kind = "limit";
    limit.title = "Paused";
    return limit;
}

CareerStop CareerManager::CurrentStop() {
    const bool savedAssistant = assistantActive;
    assistantActive = false;
    CareerStop stop = FindStop(false);
    assistantActive = savedAssistant;
    return stop;
}

CareerStop CareerManager::Vacation(const std::string& target) {
    assistantActive = true;
    const int startDay = today;
    const int startYear = seasonYear;
    auto reached = [&]() {
        if (target == "week") return today >= startDay + 7;
        if (target == "month") return today >= startDay + 30;
        if (target == "playoffs") return phase == CareerPhase::PlayIn || phase == CareerPhase::Playoffs;
        if (target == "season_end") return phase == CareerPhase::Review;
        if (target == "draft") return phase == CareerPhase::Draft;
        if (target == "free_agency") return phase == CareerPhase::Offseason;
        if (target == "camp") return phase == CareerPhase::TrainingCamp;
        if (target == "new_season") return seasonYear > startYear;
        return true;
    };

    CareerStop result;
    result.kind = "vacation_ended";
    result.title = "Back from vacation";
    for (int i = 0; i < 700; ++i) {
        // A target already satisfied when the vacation starts only counts after time has moved
        if (i > 0 && reached()) break;
        CareerStop stop = FindStop(true);
        int guard = 0;
        while (stop.kind != "none" && guard++ < 8) {
            ResolveStopWithAssistant(stop);
            stop = FindStop(true);
        }
        StepDay();
    }
    assistantActive = false;
    result.detail = "It is " + FormatDate(today) + ". The assistant handled matches, contracts and the draft while you were away.";
    Post("Back from vacation", result.detail);
    return result;
}

CoachGameInfo CareerManager::PendingCoachGame() {
    CoachGameInfo info;
    if (!started || userTeamId.empty()) return info;

    if (phase == CareerPhase::RegularSeason) {
        const int day = RegularSeasonDay();
        std::shared_ptr<Team> home;
        std::shared_ptr<Team> away;
        if (day >= 1 && season.HasUnplayedGame(day, userTeamId, &home, &away)) {
            info.pending = true;
            info.kind = "regular";
            info.label = "Regular season - day " + std::to_string(day) + " of " + std::to_string(season.GetFixtureDayCount());
            info.home = home;
            info.away = away;
        }
        return info;
    }

    if (phase == CareerPhase::PlayIn || phase == CareerPhase::Playoffs) {
        for (const auto& s : season.GetPlayoffBracket()) {
            if (s.roundIndex != season.GetCurrentPlayoffRound() || s.complete || !s.team1 || !s.team2) continue;
            if (s.team1->teamId != userTeamId && s.team2->teamId != userTeamId) continue;
            const int offset = today - stageStartDay;
            const int played = s.team1Wins + s.team2Wins;
            if (offset < 0 || offset % 2 != 0 || played > offset / 2) continue;
            std::shared_ptr<Team> home;
            std::shared_ptr<Team> away;
            if (!season.NextPlayoffGameTeams(s.seriesId, &home, &away)) continue;
            info.pending = true;
            info.kind = (phase == CareerPhase::PlayIn) ? "playin" : "playoff";
            info.seriesId = s.seriesId;
            info.label = s.roundName + " - game " + std::to_string(played + 1);
            info.home = home;
            info.away = away;
            return info;
        }
        return info;
    }

    if (phase == CareerPhase::Offseason && summerMode == 1) {
        for (const auto& g : summerGames) {
            if (g.day != today || g.played) continue;
            if (g.home->teamId != userTeamId && g.away->teamId != userTeamId) continue;
            info.pending = true;
            info.kind = "summer";
            info.label = "Summer League";
            info.home = g.home;
            info.away = g.away;
            return info;
        }
    }
    return info;
}

bool CareerManager::SimulateCoachGame() {
    CoachGameInfo game = PendingCoachGame();
    if (!game.pending) return false;
    if (game.kind == "regular") {
        const bool ok = season.SimulateHeldGame(RegularSeasonDay(), userTeamId);
        return ok;
    }
    if (game.kind == "playin" || game.kind == "playoff") {
        const bool ok = season.SimulatePlayoffGame(game.seriesId, 6);
        // a finished round starts the next stage two days later
        const size_t count = season.GetPlayoffBracket().size();
        if (season.GetCurrentPlayoffRound() != lastRound || count != lastSeriesCount) {
            stageStartDay = today + 2;
            lastRound = season.GetCurrentPlayoffRound();
            lastSeriesCount = count;
        }
        return ok;
    }
    for (auto& g : summerGames) {
        if (g.day == today && !g.played && g.home == game.home) {
            auto score = season.PlayStandaloneGame(g.home, g.away);
            g.homeScore = score.first;
            g.awayScore = score.second;
            g.played = true;
            summerWins[score.first > score.second ? g.home->teamId : g.away->teamId]++;
            return true;
        }
    }
    return false;
}

bool CareerManager::RecordCoachGame(int homeScore, int awayScore) {
    CoachGameInfo game = PendingCoachGame();
    if (!game.pending) return false;
    if (game.kind == "regular") {
        return season.RecordResult(RegularSeasonDay(), game.home->teamId, game.away->teamId, homeScore, awayScore);
    }
    if (game.kind == "playin" || game.kind == "playoff") {
        const bool ok = season.RecordPlayoffGame(game.seriesId, homeScore, awayScore);
        const size_t count = season.GetPlayoffBracket().size();
        if (season.GetCurrentPlayoffRound() != lastRound || count != lastSeriesCount) {
            stageStartDay = today + 2;
            lastRound = season.GetCurrentPlayoffRound();
            lastSeriesCount = count;
        }
        return ok;
    }
    for (auto& g : summerGames) {
        if (g.day == today && !g.played && g.home == game.home) {
            g.homeScore = homeScore;
            g.awayScore = awayScore;
            g.played = true;
            summerWins[homeScore > awayScore ? g.home->teamId : g.away->teamId]++;
            return true;
        }
    }
    return false;
}

// What needs the coach today? Blocking stops must be resolved; info stops are shown once.
CareerStop CareerManager::FindStop(bool consume) {
    CareerStop none;
    none.kind = "none";
    if (!started) return none;

    // Everyone else plays first so the day's results are visible while the coach plays his game
    if (phase == CareerPhase::RegularSeason && RegularSeasonDay() >= 1) {
        season.PlayDayExcept(RegularSeasonDay(), userTeamId);
    } else if (phase == CareerPhase::PlayIn || phase == CareerPhase::Playoffs) {
        StepPlayoffDay(true);
    }

    CoachGameInfo game = PendingCoachGame();
    if (game.pending) {
        CareerStop stop;
        stop.kind = "coach_game";
        stop.blocking = true;
        stop.title = "Match day";
        stop.detail = game.home->name + " vs " + game.away->name + " (" + game.label + ")";
        return stop;
    }

    auto info = [&](const std::string& key, const std::string& kind, const std::string& title, const std::string& text) {
        CareerStop stop;
        stop.kind = kind;
        stop.title = title;
        stop.detail = text;
        if (consume && Acknowledge(key)) {
            Post(title, text);
            return stop;
        }
        stop.kind = "none";
        return stop;
    };
    const std::string yr = std::to_string(seasonYear);
    auto team = FindTeam(userTeamId);
    const int rosterSize = team ? static_cast<int>(team->roster.size()) : 0;

    switch (phase) {
        case CareerPhase::Preseason: {
            if (today == DateOf(seasonYear, 10, 1)) {
                CareerStop s = info("newseason" + yr, "new_season", "A new season",
                                    "The " + yr + "/" + std::to_string((seasonYear + 1) % 100) + " season starts " + FormatDate(regularStartDay)
                                    + ". Rosters must be 13 to 15 players by " + FormatDate(RosterDeadline()) + ".");
                if (s.kind != "none") return s;
            }
            if (today >= RosterDeadline() && (rosterSize < MinRosterSize() || rosterSize > MaxRosterSize())) {
                CareerStop s;
                s.kind = "roster_check";
                s.blocking = true;
                s.title = "Roster check";
                s.detail = "Your roster has " + std::to_string(rosterSize) + " players. It must be " + std::to_string(MinRosterSize())
                         + " to " + std::to_string(MaxRosterSize()) + " before the season.";
                return s;
            }
            break;
        }
        case CareerPhase::RegularSeason: {
            if (rosterSize < MinRosterSize() || rosterSize > MaxRosterSize()) {
                CareerStop s;
                s.kind = "roster_check";
                s.blocking = true;
                s.title = "Roster check";
                s.detail = "Your roster has " + std::to_string(rosterSize) + " players. It must be " + std::to_string(MinRosterSize())
                         + " to " + std::to_string(MaxRosterSize()) + ".";
                return s;
            }
            break;
        }
        case CareerPhase::PlayIn:
        case CareerPhase::Playoffs: {
            bool userIn = false;
            for (const auto& s : season.GetPlayoffBracket()) {
                if ((s.team1 && s.team1->teamId == userTeamId) || (s.team2 && s.team2->teamId == userTeamId)) userIn = true;
            }
            if (userIn) {
                CareerStop s = info("playoffs" + yr, "playoffs_start", "Postseason", "Your team is in the postseason. Games are played every other day.");
                if (s.kind != "none") return s;
            }
            break;
        }
        case CareerPhase::Review: {
            std::string text = championName.empty() ? std::string("The season is over.") : ("Champions: " + championName + ".");
            if (!seasonResult.empty()) text += " " + seasonResult;
            const int expiring = ExpiringCount(userTeamId);
            text += " The draft is " + FormatDate(DraftDay()) + ".";
            if (expiring > 0) text += " " + std::to_string(expiring) + " of your players have contracts that end " + FormatDate(DateOf(seasonYear + 1, 6, 30)) + ".";
            CareerStop s = info("seasonend" + yr, "season_end", "Season over", text);
            if (s.kind != "none") return s;
            if (today == DateOf(seasonYear + 1, 6, 29) && expiring > 0) {
                CareerStop d = info("deadline" + yr, "contract_deadline", "Contract deadline",
                                    "Contracts end tomorrow. " + std::to_string(expiring) + " players will leave as free agents unless you renew them.");
                if (d.kind != "none") return d;
            }
            break;
        }
        case CareerPhase::Draft: {
            if (!draftDone) {
                CareerStop s;
                s.kind = "draft";
                s.blocking = true;
                s.title = "Draft day";
                s.detail = "The draft is under way. Make your picks or let the assistant draft for you.";
                return s;
            }
            break;
        }
        case CareerPhase::Offseason: {
            if (today == FreeAgencyStart()) {
                CareerStop s = info("fa" + yr, "free_agency", "Free agency opens",
                                    "Players with expired contracts are on the market. Summer League starts " + FormatDate(SummerStart()) + ".");
                if (s.kind != "none") return s;
            }
            if (today == SummerStart() && summerMode == 0) {
                CareerStop s;
                s.kind = "summer_choice";
                s.blocking = true;
                s.title = "Summer League";
                s.detail = "Coach the Summer League games yourself, or let the assistant run it.";
                return s;
            }
            break;
        }
        case CareerPhase::TrainingCamp: {
            if (!campPlanned) {
                CareerStop s;
                s.kind = "camp_plan";
                s.blocking = true;
                s.title = "Training camp";
                s.detail = "Training camp starts. Set the focus and intensity.";
                return s;
            }
            break;
        }
    }
    return none;
}

void CareerManager::ResolveStopWithAssistant(const CareerStop& stop) {
    if (stop.kind == "coach_game") {
        SimulateCoachGame();
    } else if (stop.kind == "draft") {
        DraftAutoFinish();
    } else if (stop.kind == "summer_choice") {
        summerMode = 2;
    } else if (stop.kind == "camp_plan") {
        SetTrainingPlan(campFocus, campIntensity, coachDevelopment);
    } else if (stop.kind == "roster_check") {
        AssistantFillRoster();
    } else if (stop.kind == "contract_deadline" || stop.kind == "season_end") {
        // The assistant renews the players the club wants to keep
        auto team = FindTeam(userTeamId);
        if (team) {
            auto expiring = ExpiringPlayers();
            std::sort(expiring.begin(), expiring.end(), [](const auto& a, const auto& b) {
                return a->attributes.currentAbility > b->attributes.currentAbility;
            });
            std::vector<std::shared_ptr<Player>> ranked = team->roster;
            std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
                return a->attributes.currentAbility > b->attributes.currentAbility;
            });
            for (const auto& p : expiring) {
                const int rank = static_cast<int>(std::find(ranked.begin(), ranked.end(), p) - ranked.begin());
                if (rank <= 9) {
                    std::string ignored;
                    RenewContract(p->playerId, PreferredYears(*p), &ignored);
                }
            }
        }
    }
}

void CareerManager::StepDay() {
    switch (phase) {
        case CareerPhase::Preseason:
            if (today == RosterDeadline()) AiRosterFill();
            break;
        case CareerPhase::RegularSeason:
            StepRegularSeasonDay();
            break;
        case CareerPhase::PlayIn:
        case CareerPhase::Playoffs:
            StepPlayoffDay(false);
            break;
        case CareerPhase::Review:
            if (today == DateOf(seasonYear + 1, 6, 29)) AiRenewals();
            if (today == DateOf(seasonYear + 1, 6, 30)) ExpireContracts();
            break;
        case CareerPhase::Draft:
            if (!draftDone) DraftAutoFinish();
            break;
        case CareerPhase::Offseason:
            if (today == SummerStart() && summerGames.empty()) PlanSummerLeague();
            if (InSummerLeague()) StepSummerLeagueDay();
            AiFreeAgencyDay();
            break;
        case CareerPhase::TrainingCamp:
            AiFreeAgencyDay();
            if (today == NextSeasonStart() - 1) ApplyProgression();
            break;
    }
    ++today;
    RefreshPhase();
}

void CareerManager::StepRegularSeasonDay() {
    const int day = RegularSeasonDay();
    if (day < 1) return;
    // The coach's own game is resolved before the day moves on (live or by the assistant)
    season.PlayDayExcept(day, "");
}

void CareerManager::StepPlayoffDay(bool holdUser) {
    std::vector<std::string> ids;
    for (const auto& s : season.GetPlayoffBracket()) {
        if (s.roundIndex == season.GetCurrentPlayoffRound() && !s.complete) ids.push_back(s.seriesId);
    }
    for (const auto& id : ids) {
        const SeasonManager::PlayoffSeries* series = nullptr;
        for (const auto& s : season.GetPlayoffBracket()) {
            if (s.seriesId == id) { series = &s; break; }
        }
        if (!series || series->complete || !series->team1 || !series->team2) continue;
        const int offset = today - stageStartDay;
        const int played = series->team1Wins + series->team2Wins;
        if (offset < 0 || offset % 2 != 0 || played > offset / 2) continue;
        const bool involvesUser = series->team1->teamId == userTeamId || series->team2->teamId == userTeamId;
        if (involvesUser && holdUser) continue;

        season.SimulatePlayoffGame(id, 6);
        const size_t count = season.GetPlayoffBracket().size();
        if (season.GetCurrentPlayoffRound() != lastRound || count != lastSeriesCount) {
            stageStartDay = today + 2;
            lastRound = season.GetCurrentPlayoffRound();
            lastSeriesCount = count;
        }
    }
}

void CareerManager::StartPlayoffs() {
    season.GeneratePlayoffBracket();
    phase = season.GetCurrentPlayoffRound() == 0 ? CareerPhase::PlayIn : CareerPhase::Playoffs;
    stageStartDay = today;
    lastRound = season.GetCurrentPlayoffRound();
    lastSeriesCount = season.GetPlayoffBracket().size();
}

void CareerManager::EndPlayoffs() {
    phase = CareerPhase::Review;
    championName = season.GetChampion() ? season.GetChampion()->name : std::string();

    // How the coach's season ended
    std::string result = "Your team missed the postseason.";
    const SeasonManager::PlayoffSeries* last = nullptr;
    for (const auto& s : season.GetPlayoffBracket()) {
        if ((s.team1 && s.team1->teamId == userTeamId) || (s.team2 && s.team2->teamId == userTeamId)) {
            if (!last || s.roundIndex >= last->roundIndex) last = &s;
        }
    }
    if (last) {
        if (last->complete && last->winner && last->winner->teamId == userTeamId && last->roundName == "NBA Finals") result = "Your team won the title.";
        else result = "Your team's postseason ended in: " + last->roundName + ".";
    }
    seasonResult = result;
    for (const auto& rec : season.GetStandings("")) {
        if (rec.team->teamId == userTeamId) {
            completedWins += rec.wins;
            completedLosses += rec.losses;
        }
    }
    seasonCounted = true;
    if (season.GetChampion() && season.GetChampion()->teamId == userTeamId) ++titlesWon;
    PrepareDraft();
}

void CareerManager::RefreshPhase() {
    for (int guard = 0; guard < 8; ++guard) {
        const CareerPhase before = phase;
        switch (phase) {
            case CareerPhase::Preseason:
                if (today >= regularStartDay) {
                    phase = CareerPhase::RegularSeason;
                    Post("Opening night", "The regular season has started.");
                }
                break;
            case CareerPhase::RegularSeason:
                if (RegularSeasonDay() > season.GetFixtureDayCount()) {
                    StartPlayoffs();
                    Post("Regular season over", "The postseason starts: Play-In, then the playoffs.");
                }
                break;
            case CareerPhase::PlayIn:
            case CareerPhase::Playoffs:
                if (season.ArePlayoffsComplete()) EndPlayoffs();
                else phase = season.GetCurrentPlayoffRound() == 0 ? CareerPhase::PlayIn : CareerPhase::Playoffs;
                break;
            case CareerPhase::Review:
                if (!draftDone && today >= DraftDay()) phase = CareerPhase::Draft;
                else if (draftDone && today >= FreeAgencyStart()) phase = CareerPhase::Offseason;
                break;
            case CareerPhase::Draft:
                if (draftDone && today > DraftDay()) phase = CareerPhase::Review;
                break;
            case CareerPhase::Offseason:
                if (today >= CampStart()) phase = CareerPhase::TrainingCamp;
                break;
            case CareerPhase::TrainingCamp:
                if (today >= NextSeasonStart()) RollOverSeason();
                break;
        }
        if (phase == before) break;
    }
}

void CareerManager::RollOverSeason() {
    ++seasonYear;
    today = DateOf(seasonYear, 10, 1);
    regularStartDay = DateOf(seasonYear, 10, 21);
    phase = CareerPhase::Preseason;
    draftOrder.clear();
    prospects.clear();
    draftResults.clear();
    draftDone = false;
    summerMode = 0;
    summerGames.clear();
    summerTeams.clear();
    summerPlayers.clear();
    summerWins.clear();
    campPlanned = false;
    championName.clear();
    stageStartDay = today;
    lastRound = -1;
    lastSeriesCount = 0;
    seasonCounted = false;

    for (const auto& t : teams) {
        for (const auto& p : t->roster) {
            p->age = seasonYear - BirthYearOf(*p);
            p->morale = 0.0f;
            p->fatigueLevel = 0.0f;
        }
    }
    for (const auto& p : freeAgents) p->age = seasonYear - BirthYearOf(*p);
    PruneFreeAgents();

    if (config.seed != 0) season.SetScheduleSeed(config.seed + static_cast<unsigned>(seasonYear));
    season.InitializeSeason(seasonYear, "NBA");
    season.LoadTeams(teams);
    season.GenerateSchedule();
}

int CareerManager::DaysToNextMilestone() const {
    switch (phase) {
        case CareerPhase::Preseason: return regularStartDay - today;
        case CareerPhase::RegularSeason: return std::max(0, season.GetFixtureDayCount() - RegularSeasonDay() + 1);
        case CareerPhase::PlayIn:
        case CareerPhase::Playoffs: return 0;
        case CareerPhase::Review: return std::max(0, DraftDay() - today);
        case CareerPhase::Draft: return 0;
        case CareerPhase::Offseason: return std::max(0, CampStart() - today);
        case CareerPhase::TrainingCamp: return std::max(0, NextSeasonStart() - today);
    }
    return 0;
}

std::string CareerManager::NextMilestoneLabel() const {
    switch (phase) {
        case CareerPhase::Preseason: return "Opening night";
        case CareerPhase::RegularSeason: return "Play-In and playoffs";
        case CareerPhase::PlayIn:
        case CareerPhase::Playoffs: return "Champion crowned";
        case CareerPhase::Review: return "Draft";
        case CareerPhase::Draft: return "Free agency";
        case CareerPhase::Offseason: return "Training camp";
        case CareerPhase::TrainingCamp: return "New season";
    }
    return "";
}

// ---------------------------------------------------------------------------------------------
// Contracts, free agency and roster limits
// ---------------------------------------------------------------------------------------------

int CareerManager::EstimateAge(const Player& p) const {
    const int born = BirthYearOf(p);
    if (born > 1900 && p.birthDate != "2000-01-01") return seasonYear - born;
    // The database only carries a placeholder birth date: young high-potential players, older finished ones
    const int gap = std::max(0, p.attributes.potentialAbility - p.attributes.currentAbility);
    const int age = 31 - static_cast<int>(std::round(1.4 * gap)) + static_cast<int>(Hash(p.playerId) % 5) - 2;
    return std::clamp(age, 19, 36);
}

int CareerManager::ContractStartSeason() const {
    switch (phase) {
        case CareerPhase::Review:
        case CareerPhase::Draft:
        case CareerPhase::Offseason:
        case CareerPhase::TrainingCamp:
            return seasonYear + 1;
        default:
            return seasonYear;
    }
}

double CareerManager::TopTenAverage() const {
    double sum = 0.0;
    int count = 0;
    for (const auto& t : teams) {
        std::vector<int> abilities;
        for (const auto& p : t->roster) abilities.push_back(p->attributes.currentAbility);
        std::sort(abilities.rbegin(), abilities.rend());
        for (size_t i = 0; i < abilities.size() && i < 10; ++i) {
            sum += abilities[i];
            ++count;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

// Unsigned players drift out of the game so the pool does not grow forever
void CareerManager::PruneFreeAgents() {
    freeAgents.erase(std::remove_if(freeAgents.begin(), freeAgents.end(), [&](const std::shared_ptr<Player>& p) {
        return p->attributes.currentAbility < 64 || p->age >= 36;
    }), freeAgents.end());
    if (freeAgents.size() > 120) {
        std::sort(freeAgents.begin(), freeAgents.end(), [&](const auto& a, const auto& b) { return ValueScore(*a) > ValueScore(*b); });
        freeAgents.resize(120);
    }
}

void CareerManager::AssignInitialContracts() {
    for (const auto& t : teams) {
        for (const auto& p : t->roster) {
            p->age = EstimateAge(*p);
            p->birthDate = std::to_string(seasonYear - p->age) + "-07-01";
            const unsigned h = Hash(p->playerId);
            const double variance = 0.92 + (h % 17) / 100.0;
            p->salary = static_cast<int>(std::round(MarketValue(*p) * variance / 10000.0)) * 10000;
            p->contractEndYear = seasonYear + static_cast<int>((h >> 3) % 4);
        }
    }
}

int CareerManager::MarketValue(const Player& p) const {
    double eff = p.attributes.currentAbility;
    if (p.age <= 24) eff += std::max(0, p.attributes.potentialAbility - p.attributes.currentAbility) * 0.35;
    if (p.age >= 33) eff -= (p.age - 32) * 1.6;
    eff = std::max(eff, 60.0);
    const double millions = 1.2 + 0.0295 * (eff - 60.0) * (eff - 60.0);
    const double value = std::min(millions * 1000000.0, 0.35 * salaryCap);
    return std::max(MinimumSalary(), static_cast<int>(std::round(value / 10000.0)) * 10000);
}

int CareerManager::PreferredYears(const Player& p) const {
    if (p.age <= 24) return 4;
    if (p.age <= 29) return 3;
    if (p.age <= 32) return 2;
    return 1;
}

double CareerManager::AskFactor() const {
    switch (phase) {
        case CareerPhase::Offseason:
        case CareerPhase::TrainingCamp:
            return std::max(0.55, 1.0 - 0.0055 * std::max(0, today - FreeAgencyStart()));
        case CareerPhase::Preseason:
        case CareerPhase::RegularSeason:
            return 0.55;
        default:
            return 1.0;
    }
}

int CareerManager::Quote(const Player& p, int years, bool ownPlayer) const {
    years = std::clamp(years, 1, 5);
    const double jitter = 0.97 + (Hash(p.playerId) % 9) / 100.0;
    const int diff = std::abs(years - PreferredYears(p));
    const double length = 1.0 + 0.04 * std::max(0, diff - 1);
    double price = MarketValue(p) * jitter * length * (ownPlayer ? 0.95 : AskFactor());
    int rounded = static_cast<int>(std::round(price / 10000.0)) * 10000;
    rounded = std::min(rounded, static_cast<int>(0.35 * salaryCap));
    return std::max(MinimumSalary(), rounded);
}

int CareerManager::CurrentAsk(const Player& p) const {
    return Quote(p, PreferredYears(p), false);
}

double CareerManager::ValueScore(const Player& p) const {
    double score = p.attributes.currentAbility;
    if (p.age <= 24) score += 0.3 * std::max(0, p.attributes.potentialAbility - p.attributes.currentAbility);
    if (p.age >= 33) score -= (p.age - 32) * 1.2;
    return score;
}

std::vector<std::shared_ptr<Player>> CareerManager::ExpiringPlayers() const {
    std::vector<std::shared_ptr<Player>> out;
    auto team = FindTeam(userTeamId);
    if (!team) return out;
    for (const auto& p : team->roster) {
        if (p->contractEndYear <= seasonYear) out.push_back(p);
    }
    return out;
}

int CareerManager::ExpiringCount(const std::string& teamId) const {
    auto team = FindTeam(teamId);
    if (!team) return 0;
    int n = 0;
    for (const auto& p : team->roster) {
        if (p->contractEndYear <= seasonYear) ++n;
    }
    return n;
}

int CareerManager::FreeJersey(const Team& team, int preferred) const {
    auto taken = [&](int n) {
        for (const auto& p : team.roster) if (p->jerseyNumber == n) return true;
        return false;
    };
    if (preferred >= 0 && preferred <= 99 && !taken(preferred)) return preferred;
    for (int n = 1; n < 100; ++n) {
        if (!taken(n)) return n;
    }
    return 0;
}

void CareerManager::RemoveFromTeam(const std::shared_ptr<Team>& team, const std::string& playerId) {
    team->roster.erase(std::remove_if(team->roster.begin(), team->roster.end(),
        [&](const std::shared_ptr<Player>& p) { return p->playerId == playerId; }), team->roster.end());
}

void CareerManager::SignPlayer(const std::shared_ptr<Team>& team, const std::shared_ptr<Player>& player, int salary, int years) {
    freeAgents.erase(std::remove(freeAgents.begin(), freeAgents.end(), player), freeAgents.end());
    player->salary = salary;
    player->contractEndYear = ContractStartSeason() + std::max(1, years) - 1;
    player->jerseyNumber = FreeJersey(*team, player->jerseyNumber);
    player->onCourt = false;
    team->roster.push_back(player);
}

bool CareerManager::RenewContract(const std::string& playerId, int years, std::string* message) {
    auto say = [&](const std::string& text) { if (message) *message = text; };
    auto team = FindTeam(userTeamId);
    if (!team) { say("No team"); return false; }
    std::shared_ptr<Player> player;
    for (const auto& p : team->roster) if (p->playerId == playerId) player = p;
    if (!player) { say("That player is not on your roster"); return false; }
    if (player->contractEndYear > seasonYear) {
        say(FullName(*player) + " is under contract until " + std::to_string(player->contractEndYear) + "/" + std::to_string((player->contractEndYear + 1) % 100));
        return false;
    }
    years = std::clamp(years, 1, 5);
    const int price = Quote(*player, years, true);
    if (Payroll(userTeamId) - player->salary + price > luxuryLine) {
        say("That would take the payroll over the luxury line (" + Money(luxuryLine) + ")");
        return false;
    }
    player->salary = price;
    player->contractEndYear = seasonYear + years;
    say(FullName(*player) + " signs for " + std::to_string(years) + (years == 1 ? " year" : " years") + " at " + Money(price) + " a season");
    return true;
}

bool CareerManager::ReleasePlayer(const std::string& playerId, std::string* message) {
    auto say = [&](const std::string& text) { if (message) *message = text; };
    auto team = FindTeam(userTeamId);
    if (!team) { say("No team"); return false; }
    std::shared_ptr<Player> player;
    for (const auto& p : team->roster) if (p->playerId == playerId) player = p;
    if (!player) { say("That player is not on your roster"); return false; }
    const bool inSeason = phase == CareerPhase::Preseason || phase == CareerPhase::RegularSeason
                       || phase == CareerPhase::PlayIn || phase == CareerPhase::Playoffs;
    if (inSeason && static_cast<int>(team->roster.size()) <= MinRosterSize()) {
        say("You need at least " + std::to_string(MinRosterSize()) + " players");
        return false;
    }
    RemoveFromTeam(team, playerId);
    player->salary = 0;
    player->contractEndYear = 0;
    freeAgents.push_back(player);
    say(FullName(*player) + " was released");
    return true;
}

bool CareerManager::SignFreeAgent(const std::string& playerId, int years, std::string* message) {
    auto say = [&](const std::string& text) { if (message) *message = text; };
    auto team = FindTeam(userTeamId);
    if (!team) { say("No team"); return false; }
    if (!FreeAgencyOpen()) { say("Free agency opens " + FormatDate(FreeAgencyStart())); return false; }
    std::shared_ptr<Player> player;
    for (const auto& p : freeAgents) if (p->playerId == playerId) player = p;
    if (!player) { say("He is no longer available"); return false; }
    const int limit = (phase == CareerPhase::RegularSeason) ? MaxRosterSize() : 20;
    if (static_cast<int>(team->roster.size()) >= limit) {
        say("Your roster is full (" + std::to_string(limit) + " players)");
        return false;
    }
    years = std::clamp(years, 1, 5);
    const int price = Quote(*player, years, false);
    const int room = salaryCap - Payroll(userTeamId);
    if (price > room && price > MinimumSalary()) {
        say(FullName(*player) + " wants " + Money(price) + " but you only have " + Money(std::max(0, room)) + " under the cap. Players asking the minimum can always be signed.");
        return false;
    }
    SignPlayer(team, player, price, years);
    say(FullName(*player) + " signs for " + std::to_string(years) + (years == 1 ? " year" : " years") + " at " + Money(price) + " a season");
    return true;
}

void CareerManager::AiRenewals() {
    for (const auto& team : teams) {
        if (team->teamId == userTeamId) continue;
        std::vector<std::shared_ptr<Player>> ranked = team->roster;
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            return a->attributes.currentAbility > b->attributes.currentAbility;
        });
        std::uniform_real_distribution<double> roll(0.0, 1.0);
        for (const auto& p : team->roster) {
            if (p->contractEndYear > seasonYear) continue;
            const int rank = static_cast<int>(std::find(ranked.begin(), ranked.end(), p) - ranked.begin());
            const double keep = rank <= 8 ? 0.85 : (rank <= 11 ? 0.4 : 0.1);
            if (roll(rng) > keep) continue;
            const int years = PreferredYears(*p);
            const int price = Quote(*p, years, true);
            if (Payroll(team->teamId) - p->salary + price > luxuryLine) continue;
            p->salary = price;
            p->contractEndYear = seasonYear + years;
        }
    }
}

void CareerManager::ExpireContracts() {
    std::vector<std::string> userLeft;
    int total = 0;
    for (const auto& team : teams) {
        std::vector<std::shared_ptr<Player>> leaving;
        for (const auto& p : team->roster) {
            if (p->contractEndYear <= seasonYear) leaving.push_back(p);
        }
        for (const auto& p : leaving) {
            RemoveFromTeam(team, p->playerId);
            if (team->teamId == userTeamId) userLeft.push_back(FullName(*p));
            p->salary = 0;
            p->contractEndYear = 0;
            freeAgents.push_back(p);
            ++total;
        }
    }
    std::string text = std::to_string(total) + " players around the league became free agents.";
    if (!userLeft.empty()) {
        text += " From your team: ";
        for (size_t i = 0; i < userLeft.size(); ++i) text += (i ? ", " : "") + userLeft[i];
        text += ".";
    }
    Post("Contracts expired", text);
}

void CareerManager::AiFreeAgencyDay() {
    if (freeAgents.empty()) return;
    const int daysSince = today - FreeAgencyStart();
    if (daysSince < 0) return;
    const double prob = daysSince < 14 ? 0.22 : (daysSince < 60 ? 0.09 : 0.14);

    std::vector<std::shared_ptr<Team>> order = teams;
    std::shuffle(order.begin(), order.end(), rng);
    std::uniform_real_distribution<double> roll(0.0, 1.0);
    for (const auto& team : order) {
        if (team->teamId == userTeamId || freeAgents.empty()) continue;
        const int size = static_cast<int>(team->roster.size());
        const bool needs = size < MaxRosterSize();
        const bool upgrade = !needs && size < 17;
        if (!needs && !upgrade) continue;
        if (roll(rng) > (needs ? prob : prob * 0.25)) continue;

        std::sort(freeAgents.begin(), freeAgents.end(), [&](const auto& a, const auto& b) {
            return ValueScore(*a) > ValueScore(*b);
        });
        const int room = salaryCap - Payroll(team->teamId);
        int worst = 100;
        for (const auto& p : team->roster) worst = std::min(worst, p->attributes.currentAbility);
        for (const auto& p : freeAgents) {
            const int years = PreferredYears(*p);
            int price = Quote(*p, years, false);
            if (price > room && price > MinimumSalary()) {
                if (price > 2 * MinimumSalary()) continue;
                price = MinimumSalary();   // veteran-minimum deal
            }
            if (upgrade && p->attributes.currentAbility < worst + 4) break;
            SignPlayer(team, p, price, years);
            break;
        }
    }
}

void CareerManager::FixRoster(const std::shared_ptr<Team>& team) {
    auto weakest = [&]() {
        return *std::min_element(team->roster.begin(), team->roster.end(), [&](const auto& a, const auto& b) {
            return ValueScore(*a) < ValueScore(*b);
        });
    };
    while (static_cast<int>(team->roster.size()) > MaxRosterSize()) {
        auto cut = weakest();
        RemoveFromTeam(team, cut->playerId);
        cut->salary = 0;
        cut->contractEndYear = 0;
        freeAgents.push_back(cut);
    }
    while (static_cast<int>(team->roster.size()) < MinRosterSize()) {
        std::sort(freeAgents.begin(), freeAgents.end(), [&](const auto& a, const auto& b) {
            return ValueScore(*a) > ValueScore(*b);
        });
        const int room = salaryCap - Payroll(team->teamId);
        std::shared_ptr<Player> chosen;
        int price = MinimumSalary();
        for (const auto& p : freeAgents) {
            const int quote = Quote(*p, 1, false);
            if (quote <= room || quote <= 2 * MinimumSalary()) {
                chosen = p;
                price = std::min(quote, std::max(room, MinimumSalary()));
                break;
            }
        }
        if (!chosen) {
            chosen = MakePlayer(62 + static_cast<int>(rng() % 5), 66, 22 + static_cast<int>(rng() % 6), "F");
            freeAgents.push_back(chosen);
        }
        SignPlayer(team, chosen, std::max(price, MinimumSalary()), 1);
    }
}

void CareerManager::AiRosterFill() {
    for (const auto& team : teams) {
        if (team->teamId == userTeamId) continue;
        FixRoster(team);
    }
}

bool CareerManager::AssistantFillRoster() {
    auto team = FindTeam(userTeamId);
    if (!team) return false;
    FixRoster(team);
    return true;
}

// ---------------------------------------------------------------------------------------------
// Generated players (draft class, replacement-level fillers)
// ---------------------------------------------------------------------------------------------

std::shared_ptr<Player> CareerManager::MakePlayer(int ability, int potential, int age, const std::string& idPrefix) {
    static bm::player::PlayerGenerator generator;
    static const char* positions[] = {"PG", "SG", "SF", "PF", "C"};
    std::uniform_int_distribution<int> positionRoll(0, 4);
    Player base = generator.GeneratePlayer(0, positions[positionRoll(rng)], "Freshman", 10);
    auto p = std::make_shared<Player>(base);
    p->playerId = idPrefix + std::to_string(seasonYear + 1) + "_" + std::to_string(nextProspectId++);

    // Keep the position's shape but move the average to the wanted ability (attributes are 0-20)
    auto& a = p->attributes;
    const double average = (a.pace + a.shooting + a.ballControl + a.defense + a.physical + a.technical) / 6.0;
    const double shift = ability / 5.0 - average;
    int* fields[] = {&a.pace, &a.shooting, &a.ballControl, &a.defense, &a.physical, &a.technical};
    std::uniform_real_distribution<double> jitter(-0.6, 0.6);
    for (int* f : fields) *f = std::clamp(static_cast<int>(std::lround(*f + shift + jitter(rng))), 3, 20);
    a.currentAbility = ability;
    a.potentialAbility = std::max(ability, potential);

    p->age = age;
    p->birthDate = std::to_string(seasonYear + 1 - age) + "-06-01";
    p->active = true;
    p->injured = false;
    p->salary = 0;
    p->contractEndYear = 0;
    p->morale = 0.0f;
    p->fatigueLevel = 0.0f;
    return p;
}

// ---------------------------------------------------------------------------------------------
// Draft
// ---------------------------------------------------------------------------------------------

void CareerManager::GenerateProspects() {
    prospects.clear();
    const int picks = 2 * static_cast<int>(teams.size());
    const int total = picks + 15;
    std::normal_distribution<double> noise(0.0, 2.0);
    for (int i = 0; i < total; ++i) {
        const double t = static_cast<double>(i) / std::max(1, picks - 1);
        int ability = static_cast<int>(std::lround(74.0 - 12.0 * std::min(1.0, t) + noise(rng)));
        if (i >= picks) ability = 58 + static_cast<int>(rng() % 5);
        const double upside = (1.0 - std::min(1.0, t)) * 14.0;
        int potential = std::min(99, static_cast<int>(std::lround(ability + 6.0 + upside + noise(rng) * 1.5)));
        const int age = (i < 12 ? 19 : 19 + static_cast<int>(rng() % 4));
        prospects.push_back(MakePlayer(std::clamp(ability, 50, 85), potential, age, "D"));
    }
    std::shuffle(prospects.begin(), prospects.end(), rng);
}

void CareerManager::PrepareDraft() {
    draftOrder.clear();
    draftResults.clear();
    draftDone = false;
    rookies.clear();
    GenerateProspects();

    std::vector<std::string> worstFirst;
    auto standings = season.GetStandings("");
    for (auto it = standings.rbegin(); it != standings.rend(); ++it) worstFirst.push_back(it->team->teamId);

    std::set<std::string> playoffTeams;
    for (const auto& s : season.GetPlayoffBracket()) {
        if (s.roundIndex == 1) {
            if (s.team1) playoffTeams.insert(s.team1->teamId);
            if (s.team2) playoffTeams.insert(s.team2->teamId);
        }
    }
    std::vector<std::string> lottery;
    std::vector<std::string> playoff;
    for (const auto& id : worstFirst) (playoffTeams.count(id) ? playoff : lottery).push_back(id);

    // Weighted lottery for the first four picks (worse record, better odds)
    static const int odds[14] = {140, 140, 140, 125, 105, 90, 75, 60, 45, 30, 20, 15, 10, 5};
    std::vector<std::string> pool = lottery;
    std::vector<std::string> top;
    for (int pick = 0; pick < 4 && !pool.empty(); ++pick) {
        std::vector<int> weights;
        for (size_t i = 0; i < pool.size(); ++i) weights.push_back(i < 14 ? odds[i] : 1);
        std::discrete_distribution<int> draw(weights.begin(), weights.end());
        const int chosen = draw(rng);
        top.push_back(pool[chosen]);
        pool.erase(pool.begin() + chosen);
    }
    for (const auto& id : top) draftOrder.push_back(id);
    for (const auto& id : pool) draftOrder.push_back(id);
    for (const auto& id : playoff) draftOrder.push_back(id);
    for (const auto& id : worstFirst) draftOrder.push_back(id);   // second round: record order
}

std::string CareerManager::DraftTeamOnClock() const {
    const size_t cursor = draftResults.size();
    return cursor < draftOrder.size() ? draftOrder[cursor] : std::string();
}

bool CareerManager::UserOnClock() const {
    return !draftDone && DraftTeamOnClock() == userTeamId;
}

std::shared_ptr<Player> CareerManager::BestProspect() {
    std::shared_ptr<Player> best;
    double bestScore = -1.0e9;
    std::uniform_real_distribution<double> noise(-3.0, 3.0);
    for (const auto& p : prospects) {
        const double score = 0.5 * p->attributes.potentialAbility + 0.5 * p->attributes.currentAbility + noise(rng);
        if (score > bestScore) {
            bestScore = score;
            best = p;
        }
    }
    return best;
}

void CareerManager::MakePick(const std::string& teamId, const std::shared_ptr<Player>& prospect) {
    auto team = FindTeam(teamId);
    if (!team || !prospect) return;
    prospects.erase(std::remove(prospects.begin(), prospects.end(), prospect), prospects.end());

    const int pick = static_cast<int>(draftResults.size()) + 1;
    const int n = static_cast<int>(teams.size());
    int salary;
    int years;
    if (pick <= n) {
        const double t = static_cast<double>(pick - 1) / std::max(1, n - 1);
        salary = MinimumSalary() + static_cast<int>((12000000 - MinimumSalary()) * std::pow(1.0 - t, 1.6));
        years = 3;
    } else {
        salary = MinimumSalary() + 100000;
        years = 2;
    }
    salary = static_cast<int>(std::round(salary / 10000.0)) * 10000;
    prospect->salary = salary;
    prospect->contractEndYear = seasonYear + years;
    prospect->jerseyNumber = FreeJersey(*team, prospect->jerseyNumber);
    team->roster.push_back(prospect);
    rookies.insert(prospect->playerId);

    DraftPickRecord record;
    record.pick = pick;
    record.teamId = teamId;
    record.playerId = prospect->playerId;
    record.playerName = FullName(*prospect);
    draftResults.push_back(record);
    if (teamId == userTeamId) {
        Post("Draft pick", "You selected " + record.playerName + " with pick " + std::to_string(pick) + ".");
    }
}

void CareerManager::FinishDraft() {
    for (const auto& p : prospects) {
        p->salary = 0;
        p->contractEndYear = 0;
        freeAgents.push_back(p);
    }
    prospects.clear();
    draftDone = true;
    Post("Draft complete", "All " + std::to_string(draftResults.size()) + " picks have been made. Undrafted players are free agents.");
}

void CareerManager::DraftAdvanceToUser() {
    while (!draftDone && draftResults.size() < draftOrder.size()) {
        if (draftOrder[draftResults.size()] == userTeamId) return;
        auto best = BestProspect();
        if (!best) break;
        MakePick(draftOrder[draftResults.size()], best);
    }
    if (!draftDone) FinishDraft();
}

bool CareerManager::DraftPickUser(const std::string& playerId, std::string* message) {
    auto say = [&](const std::string& text) { if (message) *message = text; };
    if (!UserOnClock()) { say("You are not on the clock"); return false; }
    std::shared_ptr<Player> chosen;
    for (const auto& p : prospects) if (p->playerId == playerId) chosen = p;
    if (!chosen) { say("That prospect is no longer available"); return false; }
    MakePick(userTeamId, chosen);
    say("You selected " + FullName(*chosen));
    DraftAdvanceToUser();
    return true;
}

void CareerManager::DraftAutoFinish() {
    while (!draftDone && draftResults.size() < draftOrder.size()) {
        auto best = BestProspect();
        if (!best) break;
        MakePick(draftOrder[draftResults.size()], best);
    }
    if (!draftDone) FinishDraft();
}

// ---------------------------------------------------------------------------------------------
// Summer league and training camp
// ---------------------------------------------------------------------------------------------

std::vector<std::shared_ptr<Player>> CareerManager::SummerRoster(const std::string& teamId) const {
    auto it = summerTeams.find(teamId);
    return it == summerTeams.end() ? std::vector<std::shared_ptr<Player>>() : it->second->roster;
}

void CareerManager::PlanSummerLeague() {
    summerGames.clear();
    summerTeams.clear();
    summerWins.clear();
    summerPlayers.clear();

    for (const auto& team : teams) {
        std::vector<std::shared_ptr<Player>> squad;
        std::vector<std::shared_ptr<Player>> others;
        for (const auto& p : team->roster) (rookies.count(p->playerId) ? squad : others).push_back(p);
        std::sort(others.begin(), others.end(), [](const auto& a, const auto& b) {
            return a->attributes.currentAbility < b->attributes.currentAbility;
        });
        for (const auto& p : others) {
            if (squad.size() >= 9) break;
            squad.push_back(p);
        }
        auto copy = std::make_shared<Team>(*team);
        copy->roster = squad;
        summerTeams[team->teamId] = copy;
        for (const auto& p : squad) {
            if (p->age <= 24) summerPlayers.insert(p->playerId);
        }
    }

    std::vector<std::string> ids;
    for (const auto& team : teams) ids.push_back(team->teamId);
    for (int k = 0; k < 5; ++k) {
        std::shuffle(ids.begin(), ids.end(), rng);
        for (size_t i = 0; i + 1 < ids.size(); i += 2) {
            SummerGame g;
            g.day = SummerStart() + 2 * k;
            g.home = summerTeams[ids[i]];
            g.away = summerTeams[ids[i + 1]];
            summerGames.push_back(g);
        }
    }
}

void CareerManager::StepSummerLeagueDay() {
    for (auto& g : summerGames) {
        if (g.day != today || g.played) continue;
        auto score = season.PlayStandaloneGame(g.home, g.away);
        g.homeScore = score.first;
        g.awayScore = score.second;
        g.played = true;
        summerWins[score.first > score.second ? g.home->teamId : g.away->teamId]++;
    }
}

void CareerManager::SetTrainingPlan(const std::string& focus, int intensity, int development) {
    campFocus = focus.empty() ? "Shooting" : focus;
    campIntensity = std::clamp(intensity, 1, 3);
    coachDevelopment = std::clamp(development, 0, 100);
    campPlanned = true;
}

int CareerManager::RollProgression(const Player& p, bool isUserTeam, bool summer) {
    const int age = seasonYear + 1 - BirthYearOf(p);
    double mean;
    if (age <= 21) mean = 3.0;
    else if (age <= 24) mean = 1.8;
    else if (age <= 27) mean = 0.4;
    else if (age <= 30) mean = -0.4;
    else if (age <= 33) mean = -1.7;
    else mean = -2.9;

    const int room = p.attributes.potentialAbility - p.attributes.currentAbility;
    if (age < 26) {
        mean += std::min(1.5, room * 0.08);
        if (room <= 0) mean = std::min(mean, 0.0);
    }
    if (summer && age <= 24) mean += 0.8;
    if (isUserTeam) {
        const double dev = (coachDevelopment - 50) / 40.0;
        mean += (mean > 0 ? 0.9 : 0.4) * dev;
        if (campIntensity == 3 && age <= 25) mean += 0.3;
        if (campIntensity == 3 && age >= 30) mean -= 0.3;
    }
    std::normal_distribution<double> draw(mean, 1.3);
    return static_cast<int>(std::lround(draw(rng)));
}

void CareerManager::ApplyDelta(Player& p, int delta, bool isUserTeam) {
    auto& a = p.attributes;
    a.currentAbility = std::clamp(a.currentAbility + delta, 40, 99);
    a.potentialAbility = std::max(a.potentialAbility, a.currentAbility);

    int* fields[] = {&a.pace, &a.shooting, &a.ballControl, &a.defense, &a.physical, &a.technical};
    std::uniform_real_distribution<double> roll(0.0, 1.0);
    const double step = delta * 0.2;
    for (int* f : fields) {
        double move = step;
        int whole = static_cast<int>(std::floor(std::fabs(move)));
        double frac = std::fabs(move) - whole;
        int change = whole + (roll(rng) < frac ? 1 : 0);
        *f = std::clamp(*f + (move >= 0 ? change : -change), 1, 20);
    }

    if (isUserTeam && p.age <= 28) {
        int* focus[2] = {nullptr, nullptr};
        if (campFocus == "Shooting") focus[0] = &a.shooting;
        else if (campFocus == "Defense") focus[0] = &a.defense;
        else if (campFocus == "Playmaking") { focus[0] = &a.ballControl; focus[1] = &a.technical; }
        else if (campFocus == "Physical") { focus[0] = &a.physical; focus[1] = &a.pace; }
        else if (campFocus == "Stamina") focus[0] = &a.physical;
        for (int* f : focus) {
            if (f && roll(rng) < 0.18 * campIntensity) *f = std::min(20, *f + 1);
        }
    }
}

void CareerManager::ApplyProgression() {
    std::vector<std::string> gains;
    std::vector<std::pair<int, std::string>> userChanges;
    std::vector<std::string> retired;

    struct Change {
        std::shared_ptr<Player> player;
        bool user;
        double delta;
    };
    std::vector<Change> changes;
    for (const auto& team : teams) {
        const bool user = team->teamId == userTeamId;
        for (const auto& p : team->roster) {
            changes.push_back({p, user, static_cast<double>(RollProgression(*p, user, summerPlayers.count(p->playerId) > 0))});
        }
    }
    for (const auto& p : freeAgents) changes.push_back({p, false, static_cast<double>(RollProgression(*p, false, false))});

    // Keep the league's talent level steady from year to year: shift every change by the same amount
    double offset = 0.0;
    if (talentAnchor > 0.0) {
        std::map<const Player*, double> delta;
        for (const auto& c : changes) delta[c.player.get()] = c.delta;
        double sum = 0.0;
        int count = 0;
        for (const auto& team : teams) {
            std::vector<double> predicted;
            for (const auto& p : team->roster) predicted.push_back(p->attributes.currentAbility + delta[p.get()]);
            std::sort(predicted.rbegin(), predicted.rend());
            for (size_t i = 0; i < predicted.size() && i < 10; ++i) {
                sum += predicted[i];
                ++count;
            }
        }
        if (count > 0) offset = talentAnchor - sum / count;
    }
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    bool mentorPresent = false;
    if (auto userTeam = FindTeam(userTeamId)) {
        for (const auto& p : userTeam->roster) if (!mentor.empty() && p->playerId == mentor) mentorPresent = true;
    }
    for (const auto& c : changes) {
        const double mentoring = (c.user && mentorPresent && c.player->playerId == mentee) ? 0.8 : 0.0;
        const double wanted = c.delta + offset + mentoring;
        const double whole = std::floor(wanted);
        const int applied = static_cast<int>(whole) + (unit(rng) < wanted - whole ? 1 : 0);
        ApplyDelta(*c.player, applied, c.user);
        if (c.user) userChanges.emplace_back(applied, FullName(*c.player));
    }

    // Veterans retire
    std::uniform_real_distribution<double> roll(0.0, 1.0);
    for (const auto& team : teams) {
        std::vector<std::shared_ptr<Player>> leaving;
        for (const auto& p : team->roster) {
            const int age = seasonYear + 1 - BirthYearOf(*p);
            if (age >= 37 && p->attributes.currentAbility < 76 && roll(rng) < 0.6) leaving.push_back(p);
        }
        for (const auto& p : leaving) {
            RemoveFromTeam(team, p->playerId);
            if (team->teamId == userTeamId) retired.push_back(FullName(*p));
        }
    }
    freeAgents.erase(std::remove_if(freeAgents.begin(), freeAgents.end(), [&](const std::shared_ptr<Player>& p) {
        const int age = seasonYear + 1 - BirthYearOf(*p);
        return age >= 37 && p->attributes.currentAbility < 76;
    }), freeAgents.end());

    std::sort(userChanges.begin(), userChanges.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::string text = "Camp focus: " + campFocus + ", intensity " + std::to_string(campIntensity) + ".";
    if (!userChanges.empty()) {
        text += " Best progress: " + userChanges.front().second + " (" + (userChanges.front().first >= 0 ? "+" : "") + std::to_string(userChanges.front().first) + ").";
        text += " Biggest drop: " + userChanges.back().second + " (" + (userChanges.back().first >= 0 ? "+" : "") + std::to_string(userChanges.back().first) + ").";
    }
    if (!retired.empty()) {
        text += " Retired: ";
        for (size_t i = 0; i < retired.size(); ++i) text += (i ? ", " : "") + retired[i];
        text += ".";
    }
    Post("Training camp results", text);
}

} // namespace engine
} // namespace bm
