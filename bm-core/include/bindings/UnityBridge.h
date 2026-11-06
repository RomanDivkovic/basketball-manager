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

} // extern "C"
