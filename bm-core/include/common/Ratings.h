#pragma once

#include "common/Types.h"
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace bm {

struct DetailRating {
    int group;          // 0 scoring, 1 athleticism, 2 playmaking, 3 defense and rebounding, 4 mental
    const char* name;
    int value;          // 25..99
};

// The six attributes (1..20) spread into the detailed ratings shown on a player's profile
inline std::vector<DetailRating> DetailedRatings(const Player& p) {
    const double pace = p.attributes.pace, shoot = p.attributes.shooting, ball = p.attributes.ballControl;
    const double def = p.attributes.defense, phys = p.attributes.physical, tech = p.attributes.technical;
    const bool big = p.position == Position::C || p.position == Position::PF;
    const bool guard = p.position == Position::PG || p.position == Position::SG;
    const double tall = (p.height - 1.98) * 55.0;       // taller players score and defend inside better
    const double heavy = (p.weight - 95.0) * 0.2;
    const size_t seed = std::hash<std::string>()(p.playerId + p.lastName);
    int salt = 0;
    auto rate = [&](double attribute, double adjust) {
        const int jitter = static_cast<int>((seed >> ((salt++ * 3) % (sizeof(size_t) * 8))) % 7) - 3;
        return std::clamp(static_cast<int>(28.0 + attribute * 3.45 + adjust + jitter), 25, 99);
    };
    std::vector<DetailRating> out;
    auto add = [&](int group, const char* name, double attribute, double adjust) { out.push_back({group, name, rate(attribute, adjust)}); };

    add(0, "Inside scoring", shoot * 0.4 + phys * 0.4 + tech * 0.2, big ? 4 : (guard ? -4 : 0));
    add(0, "Close shot", shoot, 0);
    add(0, "Mid-range", shoot, guard ? 2 : (big ? -4 : 0));
    add(0, "Three-point", shoot, guard ? 3 : (big ? -8 : 0));
    add(0, "Free throw", shoot * 0.7 + tech * 0.3, 0);
    add(0, "Layup", shoot * 0.4 + ball * 0.3 + phys * 0.3, 0);
    add(0, "Dunk", phys, tall * 0.5 - (guard ? 6 : 0));
    add(0, "Post control", tech * 0.5 + phys * 0.5, big ? 6 : -12);

    add(1, "Speed", pace, -heavy * 0.7);
    add(1, "Acceleration", pace * 0.7 + phys * 0.3, -heavy * 0.4);
    add(1, "Strength", phys, heavy * 1.2);
    add(1, "Vertical", phys * 0.7 + pace * 0.3, -heavy * 0.5);
    add(1, "Stamina", phys * 0.8 + def * 0.2, 0);
    add(1, "Hustle", def * 0.4 + phys * 0.3 + tech * 0.3, 0);

    add(2, "Ball handling", ball, guard ? 3 : 0);
    add(2, "Passing accuracy", ball * 0.6 + tech * 0.4, 0);
    add(2, "Passing vision", tech * 0.7 + ball * 0.3, 0);
    add(2, "Speed with ball", pace * 0.5 + ball * 0.5, 0);

    add(3, "Perimeter defense", def * 0.8 + pace * 0.2, guard ? 3 : -3);
    add(3, "Interior defense", def * 0.6 + phys * 0.4, (big ? 5 : -8) + tall * 0.3);
    add(3, "Steal", def * 0.5 + pace * 0.3 + tech * 0.2, 0);
    add(3, "Block", def * 0.4 + phys * 0.4 + pace * 0.2, tall * 0.7 - (guard ? 6 : 0));
    add(3, "Offensive rebound", phys * 0.7 + pace * 0.3, tall * 0.4);
    add(3, "Defensive rebound", phys * 0.6 + def * 0.2 + tech * 0.2, tall * 0.5);

    add(4, "Basketball IQ", tech, 0);
    add(4, "Leadership", tech, std::max(0, p.age - 28) * 0.5);
    add(4, "Discipline", tech * 0.5 + def * 0.5, 0);
    add(4, "Consistency", tech * 0.6 + shoot * 0.2 + ball * 0.2, 0);
    for (auto& r : out) {
        auto it = p.detailedRatings.find(r.name);
        if (it != p.detailedRatings.end()) r.value = std::clamp(it->second, 25, 99);
    }
    return out;
}

inline const char* RatingGroupName(int group) {
    static const char* names[] = {"Scoring", "Athleticism", "Playmaking", "Defense and rebounding", "Mental"};
    return names[std::clamp(group, 0, 4)];
}

}  // namespace bm
