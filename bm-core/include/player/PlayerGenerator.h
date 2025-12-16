#pragma once

#include "common/Types.h"
#include <string>
#include <vector>
#include <random>
#include <memory>

namespace bm {
namespace player {

/**
 * PlayerGenerator - Generates realistic NCAA basketball players
 * Creates players with attributes based on team prestige and position
 */
class PlayerGenerator {
public:
    PlayerGenerator();
    
    /**
     * Generate a complete roster for a team
     * @param teamId Database ID of the team
     * @param teamName Name of the team
     * @param teamPrestige Team prestige (0-20 scale)
     * @param numPlayers Number of players to generate (default 13)
     * @return Vector of generated players
     */
    std::vector<Player> GenerateRoster(
        int teamId,
        const std::string& teamName,
        int teamPrestige,
        int numPlayers = 13
    );
    
    /**
     * Generate a single player
     * @param teamId Team database ID
     * @param position Player position (PG, SG, SF, PF, C)
     * @param year Academic year (Freshman, Sophomore, Junior, Senior)
     * @param teamPrestige Team prestige affects player quality
     * @return Generated player
     */
    Player GeneratePlayer(
        int teamId,
        const std::string& position,
        const std::string& year,
        int teamPrestige
    );

private:
    std::mt19937 rng;
    std::vector<std::string> firstNames;
    std::vector<std::string> lastNames;
    
    // Internal generation methods
    std::string GenerateFirstName();
    std::string GenerateLastName();
    std::string GenerateName();
    int GenerateHeight(const std::string& position);
    int GenerateWeight(int height, const std::string& position);
    
    // Attribute generation based on position and prestige
    int GenerateAttribute(int baseValue, int variance, int teamPrestige);
    void GenerateAttributesForPosition(
        Player& player,
        const std::string& position,
        int teamPrestige,
        const std::string& year
    );
    
    // Position distribution
    std::string SelectRandomPosition(const std::vector<int>& existing);
    
    // Year/class distribution
    std::string SelectRandomYear();
    
    // Load name banks
    void LoadNames();
};

} // namespace player
} // namespace bm
