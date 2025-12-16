#pragma once

#include "common/Types.h"
#include <string>
#include <vector>
#include <memory>

namespace bm {
namespace engine {

/**
 * CourtVisualizer - Creates text-based visual representation of basketball court
 * Similar to Football Manager's pitch view with player positions
 */
class CourtVisualizer {
public:
    CourtVisualizer();
    ~CourtVisualizer();
    
    // Render current court state with player positions
    std::string RenderCourt(
        const std::vector<std::shared_ptr<Player>>& homeTeam,
        const std::vector<std::shared_ptr<Player>>& awayTeam,
        const std::shared_ptr<Player>& ballHandler,
        bool homeHasBall
    );
    
    // Render with ball position highlighted
    std::string RenderCourtWithBall(
        const std::vector<std::shared_ptr<Player>>& homeTeam,
        const std::vector<std::shared_ptr<Player>>& awayTeam,
        int ballX, // 0-40 (court width)
        int ballY  // 0-20 (court height)
    );
    
    // Render simplified court for commentary
    std::string RenderMiniCourt(int homeScore, int awayScore, int quarter, int timeLeft);

private:
    // Court dimensions (text characters)
    static constexpr int COURT_WIDTH = 60;
    static constexpr int COURT_HEIGHT = 25;
    
    // Player position on court (text grid)
    struct PlayerPosition {
        int x; // 0-60
        int y; // 0-25
        std::string initial; // Player initials (e.g., "MJ")
        int jerseyNumber;
        bool hasball;
    };
    
    // Calculate player positions based on formation
    std::vector<PlayerPosition> CalculatePositions(
        const std::vector<std::shared_ptr<Player>>& team,
        bool isOffense
    );
    
    // Draw court boundaries
    void DrawCourtBoundaries(std::vector<std::string>& court);
    
    // Draw player on court
    void DrawPlayer(
        std::vector<std::string>& court,
        const PlayerPosition& pos,
        bool isHome
    );
    
    // Draw ball
    void DrawBall(std::vector<std::string>& court, int x, int y);
};

} // namespace engine
} // namespace bm
