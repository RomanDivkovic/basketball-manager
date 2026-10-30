#pragma once

extern "C" {

// Match simulation interface
void InitializeMatch(const char* homeTeamId, const char* awayTeamId);
int SimulatePossession();
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
