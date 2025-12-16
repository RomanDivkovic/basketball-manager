#include "player/PlayerGenerator.h"
#include <algorithm>
#include <ctime>

// Helper to clamp values
template<typename T>
T clamp(T value, T min, T max) {
    return value < min ? min : (value > max ? max : value);
}

namespace bm {
namespace player {

PlayerGenerator::PlayerGenerator() : rng(std::random_device{}()) {
    LoadNames();
}

void PlayerGenerator::LoadNames() {
    // Common American first names for basketball players
    firstNames = {
        "James", "Michael", "John", "David", "Chris", "Kevin", "Marcus", "Jamal",
        "Tyler", "Brandon", "Jordan", "Jason", "Anthony", "Justin", "Ryan", "Eric",
        "Kyle", "Matthew", "Daniel", "Cameron", "Isaiah", "Caleb", "Mason", "Noah",
        "Ethan", "Logan", "Lucas", "Jackson", "Aiden", "Carter", "Dylan", "Jayden",
        "Zion", "Trae", "Ja", "Luka", "Shai", "Devin", "Donovan", "De'Aaron",
        "Malik", "Tyrese", "Jalen", "RJ", "Jaren", "Marvin", "Wendell", "Miles",
        "Darius", "Coby", "Kendrick", "Lonzo", "Kyle", "Markelle", "Dennis",
        "Collin", "Cade", "Jaden", "Paolo", "Jabari", "Keegan", "Bennedict",
        "Johnny", "Mark", "Patrick", "Sam", "Tommy", "Willie", "Larry", "Magic"
    };
    
    // Common American last names
    lastNames = {
        "Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia", "Miller", "Davis",
        "Rodriguez", "Martinez", "Hernandez", "Lopez", "Gonzalez", "Wilson", "Anderson",
        "Thomas", "Taylor", "Moore", "Jackson", "Martin", "Lee", "Thompson", "White",
        "Harris", "Sanchez", "Clark", "Ramirez", "Lewis", "Robinson", "Walker", "Young",
        "Allen", "King", "Wright", "Scott", "Torres", "Nguyen", "Hill", "Flores",
        "Green", "Adams", "Nelson", "Baker", "Hall", "Rivera", "Campbell", "Mitchell",
        "Carter", "Roberts", "Gomez", "Phillips", "Evans", "Turner", "Diaz", "Parker",
        "Cruz", "Edwards", "Collins", "Reyes", "Stewart", "Morris", "Morales", "Murphy",
        "Cook", "Rogers", "Morgan", "Peterson", "Cooper", "Reed", "Bailey", "Bell",
        "Howard", "Ward", "Cox", "Richardson", "Wood", "Watson", "Brooks", "Bennett",
        "Gray", "James", "Reeves", "Coleman", "Price", "Perry", "Powell", "Long"
    };
}

std::string PlayerGenerator::GenerateFirstName() {
    std::uniform_int_distribution<> dist(0, firstNames.size() - 1);
    return firstNames[dist(rng)];
}

std::string PlayerGenerator::GenerateLastName() {
    std::uniform_int_distribution<> dist(0, lastNames.size() - 1);
    return lastNames[dist(rng)];
}

std::string PlayerGenerator::GenerateName() {
    return GenerateFirstName() + " " + GenerateLastName();
}

int PlayerGenerator::GenerateHeight(const std::string& position) {
    // Height in inches
    if (position == "PG") return 72 + (rng() % 6);      // 6'0" - 6'5"
    if (position == "SG") return 74 + (rng() % 6);      // 6'2" - 6'7"
    if (position == "SF") return 76 + (rng() % 6);      // 6'4" - 6'9"
    if (position == "PF") return 78 + (rng() % 6);      // 6'6" - 6'11"
    if (position == "C") return 80 + (rng() % 6);       // 6'8" - 7'1"
    return 75; // Default
}

int PlayerGenerator::GenerateWeight(int height, const std::string& position) {
    // Base weight on height and position
    int baseWeight = (height - 60) * 5 + 140;
    
    // Position adjustments
    if (position == "PG" || position == "SG") baseWeight -= 15;
    if (position == "C") baseWeight += 20;
    
    // Add variance
    int variance = -10 + (rng() % 20);
    return baseWeight + variance;
}

int PlayerGenerator::GenerateAttribute(int baseValue, int variance, int teamPrestige) {
    // Higher prestige teams get better players
    int prestigeBonus = (teamPrestige - 10) / 2; // -5 to +5 bonus
    
    std::uniform_int_distribution<> dist(-variance, variance);
    int value = baseValue + dist(rng) + prestigeBonus;
    
    // Clamp to 0-20 range
    return clamp(value, 0, 20);
}

void PlayerGenerator::GenerateAttributesForPosition(
    Player& player,
    const std::string& position,
    int teamPrestige,
    const std::string& year
) {
    // Year modifiers - older players are better
    int yearBonus = 0;
    if (year == "Sophomore") yearBonus = 1;
    else if (year == "Junior") yearBonus = 2;
    else if (year == "Senior") yearBonus = 3;
    
    // Base attributes for position (0-20 scale)
    int base = 10; // Average
    int variance = 3;
    
    // Map to the actual PlayerAttributes structure
    // We have: pace, shooting, ballControl, defense, physical, technical
    
    if (position == "PG") {
        // Point Guards: high ball control, technical, pace
        player.attributes.ballControl = GenerateAttribute(base + 3 + yearBonus, variance, teamPrestige);
        player.attributes.technical = GenerateAttribute(base + 3 + yearBonus, variance, teamPrestige); // IQ/passing
        player.attributes.pace = GenerateAttribute(base + 2 + yearBonus, variance, teamPrestige);
        player.attributes.shooting = GenerateAttribute(base + yearBonus, variance, teamPrestige);
        player.attributes.physical = GenerateAttribute(base - 2 + yearBonus, variance, teamPrestige);
        player.attributes.defense = GenerateAttribute(base + yearBonus, variance, teamPrestige);
    }
    else if (position == "SG") {
        // Shooting Guards: high shooting, pace
        player.attributes.ballControl = GenerateAttribute(base + 1 + yearBonus, variance, teamPrestige);
        player.attributes.technical = GenerateAttribute(base + yearBonus, variance, teamPrestige);
        player.attributes.pace = GenerateAttribute(base + 2 + yearBonus, variance, teamPrestige);
        player.attributes.shooting = GenerateAttribute(base + 3 + yearBonus, variance, teamPrestige);
        player.attributes.physical = GenerateAttribute(base + yearBonus, variance, teamPrestige);
        player.attributes.defense = GenerateAttribute(base + 1 + yearBonus, variance, teamPrestige);
    }
    else if (position == "SF") {
        // Small Forwards: balanced
        player.attributes.ballControl = GenerateAttribute(base + yearBonus, variance, teamPrestige);
        player.attributes.technical = GenerateAttribute(base + yearBonus, variance, teamPrestige);
        player.attributes.pace = GenerateAttribute(base + 1 + yearBonus, variance, teamPrestige);
        player.attributes.shooting = GenerateAttribute(base + 2 + yearBonus, variance, teamPrestige);
        player.attributes.physical = GenerateAttribute(base + 1 + yearBonus, variance, teamPrestige);
        player.attributes.defense = GenerateAttribute(base + 1 + yearBonus, variance, teamPrestige);
    }
    else if (position == "PF") {
        // Power Forwards: physical, defense
        player.attributes.ballControl = GenerateAttribute(base - 2 + yearBonus, variance, teamPrestige);
        player.attributes.technical = GenerateAttribute(base - 1 + yearBonus, variance, teamPrestige);
        player.attributes.pace = GenerateAttribute(base + yearBonus, variance, teamPrestige);
        player.attributes.shooting = GenerateAttribute(base + yearBonus, variance, teamPrestige);
        player.attributes.physical = GenerateAttribute(base + 3 + yearBonus, variance, teamPrestige);
        player.attributes.defense = GenerateAttribute(base + 2 + yearBonus, variance, teamPrestige);
    }
    else if (position == "C") {
        // Centers: high physical, defense
        player.attributes.ballControl = GenerateAttribute(base - 4 + yearBonus, variance, teamPrestige);
        player.attributes.technical = GenerateAttribute(base - 2 + yearBonus, variance, teamPrestige);
        player.attributes.pace = GenerateAttribute(base - 1 + yearBonus, variance, teamPrestige);
        player.attributes.shooting = GenerateAttribute(base - 1 + yearBonus, variance, teamPrestige);
        player.attributes.physical = GenerateAttribute(base + 4 + yearBonus, variance, teamPrestige);
        player.attributes.defense = GenerateAttribute(base + 3 + yearBonus, variance, teamPrestige);
    }
    
    // Calculate current and potential ability (0-100 scale)
    int avgAttribute = (player.attributes.pace + player.attributes.shooting + 
                       player.attributes.ballControl + player.attributes.defense + 
                       player.attributes.physical + player.attributes.technical) / 6;
    
    player.attributes.currentAbility = (avgAttribute * 5); // Convert 0-20 to 0-100
    
    // Potential - younger players have more potential
    int potentialBonus = 0;
    if (year == "Freshman") potentialBonus = 15;
    else if (year == "Sophomore") potentialBonus = 10;
    else if (year == "Junior") potentialBonus = 5;
    
    player.attributes.potentialAbility = clamp(player.attributes.currentAbility + potentialBonus + static_cast<int>(rng() % 10), 0, 100);
}

std::string PlayerGenerator::SelectRandomPosition(const std::vector<int>& existing) {
    // Positions: PG, SG, SF, PF, C
    std::vector<std::string> positions = {"PG", "SG", "SF", "PF", "C"};
    
    // Weighted selection to balance roster
    std::vector<int> weights = {20, 25, 20, 20, 15}; // Need more guards than centers
    
    std::discrete_distribution<> dist(weights.begin(), weights.end());
    return positions[dist(rng)];
}

std::string PlayerGenerator::SelectRandomYear() {
    // Distribution: 35% Freshman, 30% Sophomore, 20% Junior, 15% Senior
    std::vector<int> weights = {35, 30, 20, 15};
    std::vector<std::string> years = {"Freshman", "Sophomore", "Junior", "Senior"};
    
    std::discrete_distribution<> dist(weights.begin(), weights.end());
    return years[dist(rng)];
}

Player PlayerGenerator::GeneratePlayer(
    int teamId,
    const std::string& position,
    const std::string& year,
    int teamPrestige
) {
    Player player;
    
    // Generate name
    std::string fullName = GenerateName();
    size_t spacePos = fullName.find(' ');
    player.firstName = fullName.substr(0, spacePos);
    player.lastName = fullName.substr(spacePos + 1);
    player.playerId = std::to_string(teamId) + "_" + player.lastName + "_" + std::to_string(rng() % 1000);
    
    // Position enum
    if (position == "PG") player.position = Position::PG;
    else if (position == "SG") player.position = Position::SG;
    else if (position == "SF") player.position = Position::SF;
    else if (position == "PF") player.position = Position::PF;
    else if (position == "C") player.position = Position::C;
    
    // Year in school (1-4)
    player.yearInSchool = (year == "Freshman" ? 1 : year == "Sophomore" ? 2 : year == "Junior" ? 3 : 4);
    
    // Physical attributes
    int heightInches = GenerateHeight(position);
    player.height = heightInches * 0.0254f; // Convert inches to meters
    player.weight = GenerateWeight(heightInches, position) * 0.453592f; // Convert lbs to kg
    player.jerseyNumber = 1 + (rng() % 55); // Jersey numbers 1-55
    
    // Birth date (age 18-22)
    int age = 18 + player.yearInSchool - 1;
    player.birthDate = std::to_string(2025 - age) + "-01-01";
    
    // Status
    player.active = true;
    player.injured = false;
    player.suspensionMatches = 0;
    player.international = (rng() % 10 == 0); // 10% international
    player.draftEligible = (player.yearInSchool >= 2); // Eligible after freshman year
    player.nilValue = 0; // Will be calculated based on attributes
    
    // Generate attributes
    GenerateAttributesForPosition(player, position, teamPrestige, year);
    
    // NIL value based on ability
    player.nilValue = player.attributes.currentAbility * 1000; // $1k per rating point
    
    return player;
}

std::vector<Player> PlayerGenerator::GenerateRoster(
    int teamId,
    const std::string& teamName,
    int teamPrestige,
    int numPlayers
) {
    std::vector<Player> roster;
    roster.reserve(numPlayers);
    
    // Count positions to balance roster
    std::vector<int> positionCounts(5, 0); // PG, SG, SF, PF, C
    
    for (int i = 0; i < numPlayers; ++i) {
        std::string position = SelectRandomPosition(positionCounts);
        std::string year = SelectRandomYear();
        
        Player player = GeneratePlayer(teamId, position, year, teamPrestige);
        roster.push_back(player);
    }
    
    return roster;
}

} // namespace player
} // namespace bm
