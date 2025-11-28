#include "bindings/UnityBridge.h"
#include <cstring>
#include <iostream>

extern "C" {

static const char* ENGINE_VERSION = "1.0.0-alpha";

void InitializeMatch(const char* homeTeamId, const char* awayTeamId) {
    std::cout << "[UnityBridge] InitializeMatch: " << homeTeamId << " vs " << awayTeamId << "\n";
}

void SimulateMatch() {
    std::cout << "[UnityBridge] SimulateMatch called\n";
}

const char* GetMatchResult() {
    return "MATCH RESULT";
}

const char* GetLiveCommentary() {
    return "Commentary text...";
}

void InitializeSeason(int year) {
    std::cout << "[UnityBridge] InitializeSeason: " << year << "\n";
}

void SimulateRound(int roundNumber) {
    std::cout << "[UnityBridge] SimulateRound: " << roundNumber << "\n";
}

void LoadDatabase(const char* dbPath) {
    std::cout << "[UnityBridge] LoadDatabase: " << dbPath << "\n";
}

void SaveDatabase(const char* dbPath) {
    std::cout << "[UnityBridge] SaveDatabase: " << dbPath << "\n";
}

void InitializeAI(const char* teamId) {
    std::cout << "[UnityBridge] InitializeAI: " << teamId << "\n";
}

void GetAILineup(char* outBuffer, int bufferSize) {
    std::string lineup = "Player1,Player2,Player3,Player4,Player5";
    std::strncpy(outBuffer, lineup.c_str(), bufferSize - 1);
    outBuffer[bufferSize - 1] = '\0';
}

const char* GetEngineVersion() {
    return ENGINE_VERSION;
}

} // extern "C"
