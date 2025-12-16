#include "engine/CourtVisualizer.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace bm {
namespace engine {

CourtVisualizer::CourtVisualizer() {
}

CourtVisualizer::~CourtVisualizer() {
}

std::string CourtVisualizer::RenderCourt(
    const std::vector<std::shared_ptr<Player>>& homeTeam,
    const std::vector<std::shared_ptr<Player>>& awayTeam,
    const std::shared_ptr<Player>& ballHandler,
    bool homeHasBall
) {
    std::vector<std::string> court(COURT_HEIGHT, std::string(COURT_WIDTH, ' '));
    
    // Draw boundaries
    DrawCourtBoundaries(court);
    
    // Calculate positions (take first 5 players as "on court")
    auto homePositions = CalculatePositions(
        std::vector<std::shared_ptr<Player>>(homeTeam.begin(), homeTeam.begin() + std::min(5, (int)homeTeam.size())),
        homeHasBall
    );
    
    auto awayPositions = CalculatePositions(
        std::vector<std::shared_ptr<Player>>(awayTeam.begin(), awayTeam.begin() + std::min(5, (int)awayTeam.size())),
        !homeHasBall
    );
    
    // Draw players
    for (const auto& pos : homePositions) {
        DrawPlayer(court, pos, true);
    }
    for (const auto& pos : awayPositions) {
        DrawPlayer(court, pos, false);
    }
    
    // Draw ball on ball handler
    if (ballHandler) {
        for (const auto& pos : (homeHasBall ? homePositions : awayPositions)) {
            if (pos.hasball) {
                DrawBall(court, pos.x, pos.y);
                break;
            }
        }
    }
    
    // Build output
    std::stringstream ss;
    ss << "+" << std::string(COURT_WIDTH, '=') << "+\n";
    for (const auto& line : court) {
        ss << "|" << line << "|\n";
    }
    ss << "+" << std::string(COURT_WIDTH, '=') << "+\n";
    
    return ss.str();
}

std::string CourtVisualizer::RenderCourtWithBall(
    const std::vector<std::shared_ptr<Player>>& homeTeam,
    const std::vector<std::shared_ptr<Player>>& awayTeam,
    int ballX,
    int ballY
) {
    // Similar to RenderCourt but with explicit ball position
    return RenderCourt(homeTeam, awayTeam, nullptr, true);
}

std::string CourtVisualizer::RenderMiniCourt(int homeScore, int awayScore, int quarter, int timeLeft) {
    std::stringstream ss;
    ss << "┌──────────────────────────────┐\n";
    ss << "│  Q" << quarter << "  " << std::setw(3) << homeScore << " - " 
       << std::setw(3) << awayScore << "  (" << (timeLeft / 60) << ":" 
       << std::setfill('0') << std::setw(2) << (timeLeft % 60) << ") │\n";
    ss << "└──────────────────────────────┘";
    return ss.str();
}

void CourtVisualizer::DrawCourtBoundaries(std::vector<std::string>& court) {
    // Top and bottom boundaries
    for (int x = 0; x < COURT_WIDTH; ++x) {
        court[0][x] = '-';
        court[COURT_HEIGHT - 1][x] = '-';
    }
    
    // Left and right boundaries
    for (int y = 0; y < COURT_HEIGHT; ++y) {
        court[y][0] = '|';
        court[y][COURT_WIDTH - 1] = '|';
    }
    
    // Corners
    court[0][0] = '+';
    court[0][COURT_WIDTH - 1] = '+';
    court[COURT_HEIGHT - 1][0] = '+';
    court[COURT_HEIGHT - 1][COURT_WIDTH - 1] = '+';
    
    // Center line
    int centerX = COURT_WIDTH / 2;
    for (int y = 1; y < COURT_HEIGHT - 1; ++y) {
        court[y][centerX] = '|';
    }
    
    // Three-point arcs (simplified)
    int leftArc = 8;
    int rightArc = COURT_WIDTH - 9;
    for (int y = 5; y < COURT_HEIGHT - 5; ++y) {
        if (court[y][leftArc] == ' ') court[y][leftArc] = '(';
        if (court[y][rightArc] == ' ') court[y][rightArc] = ')';
    }
    
    // Paint areas (use # for simplicity)
    for (int y = COURT_HEIGHT / 2 - 3; y <= COURT_HEIGHT / 2 + 3; ++y) {
        for (int x = 3; x <= 6; ++x) {
            if (court[y][x] == ' ') court[y][x] = '#';
        }
        for (int x = COURT_WIDTH - 7; x < COURT_WIDTH - 3; ++x) {
            if (court[y][x] == ' ') court[y][x] = '#';
        }
    }
}

std::vector<CourtVisualizer::PlayerPosition> CourtVisualizer::CalculatePositions(
    const std::vector<std::shared_ptr<Player>>& team,
    bool isOffense
) {
    std::vector<PlayerPosition> positions;
    
    if (team.empty()) return positions;
    
    // Offensive formation: 1-2-2 (PG at top, wings, posts)
    // Defensive formation: Man-to-man spread
    
    int side = isOffense ? 15 : 45; // Left side for offense, right for defense
    int courtCenter = COURT_HEIGHT / 2;
    
    for (size_t i = 0; i < team.size() && i < 5; ++i) {
        PlayerPosition pos;
        auto player = team[i];
        
        // Get initials
        pos.initial = "";
        if (!player->firstName.empty()) pos.initial += player->firstName[0];
        if (!player->lastName.empty()) pos.initial += player->lastName[0];
        pos.jerseyNumber = player->jerseyNumber;
        pos.hasball = (i == 0); // First player has ball for simplicity
        
        // Position based on role
        switch (player->position) {
            case Position::PG: // Point guard - top of key
                pos.x = side;
                pos.y = courtCenter;
                break;
            case Position::SG: // Shooting guard - wing right
                pos.x = side + (isOffense ? 5 : -5);
                pos.y = courtCenter - 4;
                break;
            case Position::SF: // Small forward - wing left
                pos.x = side + (isOffense ? 5 : -5);
                pos.y = courtCenter + 4;
                break;
            case Position::PF: // Power forward - low post right
                pos.x = side + (isOffense ? 8 : -8);
                pos.y = courtCenter - 6;
                break;
            case Position::C: // Center - low post left
                pos.x = side + (isOffense ? 8 : -8);
                pos.y = courtCenter + 6;
                break;
        }
        
        // Clamp to court boundaries
        pos.x = std::max(2, std::min(COURT_WIDTH - 3, pos.x));
        pos.y = std::max(2, std::min(COURT_HEIGHT - 3, pos.y));
        
        positions.push_back(pos);
    }
    
    return positions;
}

void CourtVisualizer::DrawPlayer(
    std::vector<std::string>& court,
    const PlayerPosition& pos,
    bool isHome
) {
    // Draw player with jersey number
    char marker = isHome ? 'H' : 'A';
    
    // Clear space for player (3 chars wide)
    if (pos.x - 1 >= 0 && pos.x + 1 < COURT_WIDTH) {
        std::string playerStr = marker + std::to_string(pos.jerseyNumber % 100);
        if (playerStr.length() > 3) playerStr = playerStr.substr(0, 3);
        
        int startX = pos.x - 1;
        for (size_t i = 0; i < playerStr.length() && startX + i < COURT_WIDTH; ++i) {
            court[pos.y][startX + i] = playerStr[i];
        }
    }
}

void CourtVisualizer::DrawBall(std::vector<std::string>& court, int x, int y) {
    // Draw ball as O near player
    if (y > 0 && x + 3 < COURT_WIDTH) {
        court[y - 1][x + 3] = 'O';
    }
}

} // namespace engine
} // namespace bm
