#pragma once

#include "common/Types.h"
#include "engine/SeasonManager.h"
#include <deque>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace bm {
namespace engine {

// Civil calendar helpers; a day is the number of days since 1970-01-01
struct CalDate {
    int year = 1970;
    int month = 1;
    int day = 1;
};
int DaysFromCivil(int year, int month, int day);
CalDate CivilFromDays(int days);
int WeekdayOfDays(int days);                 // 0 = Sunday
std::string FormatDate(int days);            // "Wed 21 Oct 2026"

enum class CareerPhase {
    Preseason,       // Oct 1 - the day before opening night
    RegularSeason,
    PlayIn,
    Playoffs,
    Review,          // after the Finals: contract renewals, until draft day
    Draft,           // draft day
    Offseason,       // free agency, summer league
    TrainingCamp
};
const char* PhaseName(CareerPhase phase);

// What the coach must (or may) look at before time moves on
struct CareerStop {
    std::string kind;      // "none", "coach_game", "draft", "camp_plan", "summer_choice", "roster_check", or an info kind
    std::string title;
    std::string detail;
    bool blocking = false;
};

struct CareerEvent {
    int day = 0;
    std::string title;
    std::string text;
};

struct CoachGameInfo {
    bool pending = false;
    std::string kind;          // "regular", "playin", "playoff", "summer"
    std::string label;         // e.g. "Regular season, day 12", "Western First Round, game 3"
    std::string seriesId;      // playoffs
    std::shared_ptr<Team> home;
    std::shared_ptr<Team> away;
};

struct DraftPickRecord {
    int pick = 0;              // 1-based overall
    std::string teamId;
    std::string playerId;
    std::string playerName;
};

struct SummerGame {
    int day = 0;
    std::shared_ptr<Team> home;
    std::shared_ptr<Team> away;
    bool played = false;
    int homeScore = 0;
    int awayScore = 0;
};

/**
 * CareerManager - one dated year at a time: preseason, 82-game season, play-in and playoffs,
 * contract renewals, draft, free agency, summer league and training camp.
 * Time only moves through Continue()/Vacation(); the human coach's own decisions stop it.
 */
class CareerManager {
public:
    struct Config {
        int startYear = 2026;               // the season starting in October of this year
        std::string userTeamId;
        unsigned seed = 0;                  // 0 = random
        int gamesPerTeam = 82;
        bool quiet = true;                  // mute console output from simulated matches
    };

    CareerManager();

    void Start(const std::vector<std::shared_ptr<Team>>& teams, const Config& config);
    bool IsStarted() const { return started; }

    // ---- time ----
    int Today() const { return today; }
    std::string DateLabel() const { return FormatDate(today); }
    CareerPhase Phase() const { return phase; }
    int SeasonYear() const { return seasonYear; }
    int RegularSeasonDay() const;            // 1-based fixture day for today, 0 outside the regular season
    int RegularSeasonLength() const { return season.GetFixtureDayCount(); }
    int DaysToNextMilestone() const;
    std::string NextMilestoneLabel() const;

    // Moves time until something needs the coach; stops at once if something already does
    CareerStop Continue(int maxDays = 400);
    // The assistant runs the club (matches, contracts, draft, camp) until the target; target in
    // {"week","month","playoffs","season_end","draft","free_agency","camp","new_season"}
    CareerStop Vacation(const std::string& target);
    CareerStop CurrentStop();                 // what is waiting right now (kind "none" if nothing)

    // ---- the coach's match ----
    CoachGameInfo PendingCoachGame();
    bool SimulateCoachGame();                 // the assistant plays it
    bool RecordCoachGame(int homeScore, int awayScore);   // the coach played it live

    // ---- league ----
    SeasonManager& Season() { return season; }
    const SeasonManager& Season() const { return season; }
    const std::string& UserTeamId() const { return userTeamId; }
    std::shared_ptr<Team> FindTeam(const std::string& teamId) const;
    const std::vector<std::shared_ptr<Team>>& Teams() const { return teams; }
    const std::deque<CareerEvent>& Inbox() const { return inbox; }
    const std::string& ChampionName() const { return championName; }
    std::string FixtureDateLabel(int dayNumber) const { return FormatDate(regularStartDay + dayNumber - 1); }
    int RegularSeasonStart() const { return regularStartDay; }
    std::shared_ptr<Team> UserTeam() const { return FindTeam(userTeamId); }
    const std::vector<std::string>& DraftOrder() const { return draftOrder; }
    const std::map<std::string, int>& SummerWins() const { return summerWins; }
    std::string CampFocus() const { return campFocus; }
    int CampIntensity() const { return campIntensity; }
    // Totals over finished seasons plus the season in progress (counted once the playoffs are over)
    int CompletedWins() const { return completedWins; }
    int CompletedLosses() const { return completedLosses; }
    int TitlesWon() const { return titlesWon; }
    bool SeasonCounted() const { return seasonCounted; }
    bool PlayoffsVisible() const;             // true from the play-in until the Finals end

    // ---- contracts and free agency ----
    static int MinimumSalary() { return 1500000; }
    int SalaryCap() const { return salaryCap; }
    int LuxuryLine() const { return luxuryLine; }
    int Payroll(const std::string& teamId) const;
    std::vector<std::shared_ptr<Player>> ExpiringPlayers() const;       // user team, contract ends this season
    int MarketValue(const Player& player) const;
    int PreferredYears(const Player& player) const;
    int Quote(const Player& player, int years, bool ownPlayer) const;   // salary he asks for that length
    bool RenewContract(const std::string& playerId, int years, std::string* message);
    bool ReleasePlayer(const std::string& playerId, std::string* message);
    const std::vector<std::shared_ptr<Player>>& FreeAgents() const { return freeAgents; }
    bool SignFreeAgent(const std::string& playerId, int years, std::string* message);
    bool FreeAgencyOpen() const;

    // ---- draft ----
    bool DraftReady() const { return !draftOrder.empty(); }
    bool DraftFinished() const { return draftDone; }
    const std::vector<std::shared_ptr<Player>>& Prospects() const { return prospects; }
    const std::vector<DraftPickRecord>& DraftResults() const { return draftResults; }
    int DraftCursor() const { return static_cast<int>(draftResults.size()); }
    int DraftTotalPicks() const { return static_cast<int>(draftOrder.size()); }
    std::string DraftTeamOnClock() const;
    bool UserOnClock() const;
    bool DraftPickUser(const std::string& playerId, std::string* message);
    void DraftAdvanceToUser();                // computer picks until the coach is on the clock or the draft ends
    void DraftAutoFinish();                   // the assistant makes every remaining pick for the coach too

    // ---- summer league and camp ----
    // mode: 0 = undecided, 1 = the coach plays his team's games, 2 = the assistant runs them
    void SetSummerLeagueMode(int mode) { summerMode = mode; }
    int SummerLeagueMode() const { return summerMode; }
    bool InSummerLeague() const;
    const std::vector<SummerGame>& SummerGames() const { return summerGames; }
    std::vector<std::shared_ptr<Player>> SummerRoster(const std::string& teamId) const;
    void SetTrainingPlan(const std::string& focus, int intensity, int coachDevelopment);
    bool TrainingPlanSet() const { return campPlanned; }
    void SetMentoring(const std::string& mentorId, const std::string& menteeId) { mentor = mentorId; mentee = menteeId; }

    // ---- roster limits ----
    static int MinRosterSize() { return 13; }
    static int MaxRosterSize() { return 15; }
    bool AssistantFillRoster();               // trims or fills the coach's roster to the legal size

    // For tests and the bridge
    unsigned Hash(const std::string& text) const;

private:
    SeasonManager season;
    std::vector<std::shared_ptr<Team>> teams;
    std::unordered_map<std::string, std::shared_ptr<Team>> teamById;
    std::string userTeamId;
    Config config;
    bool started = false;
    std::mt19937 rng;

    int today = 0;
    int seasonYear = 2026;
    CareerPhase phase = CareerPhase::Preseason;
    int regularStartDay = 0;
    int stageStartDay = 0;
    int lastRound = -1;
    size_t lastSeriesCount = 0;
    std::string championName;
    std::string seasonResult;
    int completedWins = 0;
    int completedLosses = 0;
    int titlesWon = 0;
    bool seasonCounted = false;
    int salaryCap = 180000000;
    int luxuryLine = 215000000;
    std::deque<CareerEvent> inbox;
    std::set<std::string> acknowledged;
    bool assistantActive = false;
    int vacationEnd = -1;

    // offseason state
    std::vector<std::string> draftOrder;
    std::vector<std::shared_ptr<Player>> prospects;
    std::vector<DraftPickRecord> draftResults;
    std::set<std::string> rookies;
    bool draftDone = false;
    std::vector<std::shared_ptr<Player>> freeAgents;
    int summerMode = 0;
    std::vector<SummerGame> summerGames;
    std::map<std::string, std::shared_ptr<Team>> summerTeams;
    std::set<std::string> summerPlayers;
    std::map<std::string, int> summerWins;
    std::string campFocus = "Shooting";
    int campIntensity = 2;
    int coachDevelopment = 70;
    std::string mentor;
    std::string mentee;
    bool campPlanned = false;
    int nextProspectId = 1;
    double talentAnchor = 0.0;      // average of every team's ten best players when the career began

    // calendar anchors
    int DateOf(int year, int month, int day) const { return DaysFromCivil(year, month, day); }
    int DraftDay() const { return DateOf(seasonYear + 1, 6, 25); }
    int FreeAgencyStart() const { return DateOf(seasonYear + 1, 7, 1); }
    int SummerStart() const { return DateOf(seasonYear + 1, 7, 3); }
    int CampStart() const { return DateOf(seasonYear + 1, 9, 28); }
    int NextSeasonStart() const { return DateOf(seasonYear + 1, 10, 1); }
    int RosterDeadline() const { return DateOf(seasonYear, 10, 15); }

    // stepping
    CareerStop FindStop(bool consume = true);
    void ResolveStopWithAssistant(const CareerStop& stop);
    void StepDay();
    void RefreshPhase();
    void StepRegularSeasonDay();
    void StepPlayoffDay(bool holdUser);
    void StartPlayoffs();
    void EndPlayoffs();
    void RollOverSeason();
    void Post(const std::string& title, const std::string& text);
    bool Acknowledge(const std::string& key);      // true the first time

    // contracts and rosters
    void AssignInitialContracts();
    double TopTenAverage() const;
    void PruneFreeAgents();
    int EstimateAge(const Player& player) const;
    int ExpiringCount(const std::string& teamId) const;
    int ContractStartSeason() const;
    double ValueScore(const Player& player) const;
    void ExpireContracts();
    void AiRenewals();
    void AiFreeAgencyDay();
    void AiRosterFill();
    void FixRoster(const std::shared_ptr<Team>& team);
    void SignPlayer(const std::shared_ptr<Team>& team, const std::shared_ptr<Player>& player, int salary, int years);
    void RemoveFromTeam(const std::shared_ptr<Team>& team, const std::string& playerId);
    int FreeJersey(const Team& team, int preferred) const;
    double AskFactor() const;
    int CurrentAsk(const Player& player) const;
    std::shared_ptr<Player> MakePlayer(int ability, int potential, int age, const std::string& idPrefix);

    // draft
    void PrepareDraft();
    std::shared_ptr<Player> BestProspect();
    void MakePick(const std::string& teamId, const std::shared_ptr<Player>& prospect);
    void GenerateProspects();
    void FinishDraft();

    // summer league and camp
    void PlanSummerLeague();
    void StepSummerLeagueDay();
    void ApplyProgression();
    int RollProgression(const Player& player, bool isUserTeam, bool summer);
    void ApplyDelta(Player& player, int delta, bool isUserTeam);
};

} // namespace engine
} // namespace bm
