#pragma once

extern "C" {

// Match simulation interface
void InitializeMatch(const char* homeTeamId, const char* awayTeamId);
int SimulatePossession();
// Plays up to `seconds` game seconds (positions, ball, clock); returns the seconds actually played
int StepMatchSeconds(int seconds);

// Breaks, timeouts and team talks (the match opens with a pre-match break)
int RequestTimeout(int isHomeTeam, int full);                       // 1 = queued for the next dead ball
void EndMatchBreak();                                               // resume play after a break
int ApplyTeamTalk(int isHomeTeam, int talkType, int coachMotivation); // talkType 0..6, see TeamTalkType
const char* GetTeamTalkJson();

// Offense schemes (0 balanced, 1 inside, 2 threes, 3 pace&space, 4 isolation, 5 fast break,
// 6 pick and roll, 7 move the ball, 8 play through a player), playbook and individual feedback
int SetOffenseScheme(int isHomeTeam, int scheme, const char* targetPlayerId, const char* playId); // 0 = rejected, see GetCoachError
const char* GetCoachError();
int SetDefenseScheme(int isHomeTeam, int scheme);                  // 0..5, see DefensiveStrategy
const char* GetPlaybookJson();
int GiveFeedback(int isHomeTeam, const char* playerId, int instruction, int coachMotivation); // 1 settle down, 2 focus on defense, 3 look for your shot
const char* GetFeedbackJson();
void SimulateMatch();
const char* GetMatchResult();
const char* GetLiveCommentary();
const char* GetMatchStateJson();
int IsMatchComplete();
void SetSimulationSpeed(int speedMultiplier);
void SetPauseEnabled(int enabled);
void CallTimeout(int isHomeTeam);
void ApplyCoachingCallout(int isHomeTeam, int callout);

// Season management
void InitializeSeason(int year);
void SimulateRound(int roundNumber);
int GetCurrentSeasonDay();
int GetTotalSeasonDays();
int SimulateSeasonDay(int speedMultiplier);
const char* GetSeasonCalendarJson();
const char* GetFixturesForDayJson(int dayNumber);
const char* GetStandingsJson(const char* conference);

// Playoffs management
int GeneratePlayoffBracket();
int AdvancePlayoffRound(int speedMultiplier);
int SimulatePlayoffGame(const char* seriesId, int speedMultiplier);
const char* GetPlayoffsBracketJson();
int ArePlayoffsComplete();
const char* GetPlayoffChampionJson();

// Tactics & Lineup management
const char* GetTeamRosterDetailsJson(const char* teamId);
void SetTeamTactics(int isHomeTeam, int offensiveStrategy, int defensiveStrategy, int pace);
int SwapMatchPlayers(int isHomeTeam, const char* playerOutId, const char* playerInId);

// Database
void LoadDatabase(const char* dbPath);
void SaveDatabase(const char* dbPath);
const char* GetTeamsJson();

// AI
void InitializeAI(const char* teamId);
void GetAILineup(char* outBuffer, int bufferSize);

// Version info
const char* GetEngineVersion();

// League files: teams.csv + players.csv in a folder become a league database (same schema as the shipped one)
int ImportLeagueFolder(const char* folder, const char* outDbPath);   // 0 = invalid, see GetLeagueMessage
int GenerateRandomLeagueDb(const char* outDbPath, int seed);          // made-up 30-team league; 0 = failed, see GetLeagueMessage
int ExportLeagueTemplate(const char* folder);                         // writes teams.csv, players.csv, README.txt
const char* GetLeagueMessage();

// Career: a dated year (preseason, 82-game season, play-in, playoffs, contracts, draft, free agency, summer league, camp)
int CareerStart(const char* userTeamId, int startYear, int seed);
void CareerEnd();
int CareerHasSave(const char* leagueDbPath, const char* userTeamId);   // 1 when the league file holds a saved career for this club
int CareerResume(const char* userTeamId, int seed, const char* leagueDbPath);   // continue from the last season start
int CareerEnableAutosave(const char* leagueDbPath);                   // saves now and at every new season
const char* CareerGetStateJson();
const char* CareerContinue();                    // moves time until something needs the coach; returns the stop
const char* CareerVacation(const char* target);  // week, month, playoffs, season_end, draft, free_agency, camp, new_season
void CareerDismissStop();
const char* CareerGetScheduleJson();
int CareerBeginCoachGame();                      // sets up the live match for the coach's pending game
int CareerFinishCoachGame();                     // records the finished live match
int CareerSimCoachGame();                        // the assistant plays it
const char* CareerGetContractsJson();
int CareerRenew(const char* playerId, int years);
int CareerRelease(const char* playerId);
const char* CareerGetFreeAgentsJson();
int CareerSignFreeAgent(const char* playerId, int years);
const char* CareerGetMessage();
const char* CareerGetDraftJson();
void CareerDraftAdvance();
int CareerDraftPick(const char* playerId);
void CareerDraftAuto();
int CareerLoadDraftClass(const char* path);      // draft file for the coming draft; 0 = invalid, see CareerGetMessage
void CareerUseGeneratedDraftClass();             // the game makes up the class
const char* CareerGetHistoryJson();              // Hall of Fame and retired numbers, newest first
const char* CareerGetSummerJson();
void CareerSetSummerMode(int mode);              // 1 = the coach plays, 2 = the assistant runs it
void CareerSetTrainingPlan(const char* focus, int intensity, int coachDevelopment);
void CareerSetMentoring(const char* mentorId, const char* menteeId);
int CareerAssistantFillRoster();

} // extern "C"
