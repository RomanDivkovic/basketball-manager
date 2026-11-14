#pragma once

#include <sstream>
#include <string>

namespace bm {

// Player traits (badges): rare, earned through attributes and several seasons of production
enum Trait : unsigned {
    TRAIT_SHARPSHOOTER = 1u << 0,
    TRAIT_FLYING_HIGH = 1u << 1,
    TRAIT_MID_RANGE_DEMON = 1u << 2,
    TRAIT_COOKIE_MONSTER = 1u << 3,
    TRAIT_NOT_IN_MY_HOUSE = 1u << 4,
    TRAIT_POST_UP_DEMON = 1u << 5,
    TRAIT_ALL_AROUND = 1u << 6,
    TRAIT_FLOOR_GENERAL = 1u << 7
};

struct TraitInfo {
    Trait bit;
    const char* id;
    const char* name;
    const char* text;
};

inline const TraitInfo* AllTraits(int* count) {
    static const TraitInfo list[] = {
        {TRAIT_SHARPSHOOTER, "sharpshooter", "Sharpshooter", "Hits two threes in a row while nobody else scores and he heats up from deep."},
        {TRAIT_FLYING_HIGH, "flying_high", "Flying High", "Two dunks in a row give a boost on dunks and layups."},
        {TRAIT_MID_RANGE_DEMON, "mid_range_demon", "Mid-Range Demon", "Better mid-range, free throws and inside scoring."},
        {TRAIT_COOKIE_MONSTER, "cookie_monster", "Cookie Monster", "Better on-ball defense and more steals."},
        {TRAIT_NOT_IN_MY_HOUSE, "not_in_my_house", "Not In My House", "Two blocks in a row give better inside defense and more blocks."},
        {TRAIT_POST_UP_DEMON, "post_up_demon", "Post-Up Demon", "Two post scores in a row boost post offense, mid-range, inside and layups."},
        {TRAIT_ALL_AROUND, "all_around", "All-Around", "An assist, a rebound and a basket in a row: better shooting, rebounding and passing."},
        {TRAIT_FLOOR_GENERAL, "floor_general", "Floor General", "Three assists in a row: better passing, ball handling and speed."}
    };
    *count = static_cast<int>(sizeof(list) / sizeof(list[0]));
    return list;
}

inline bool HasTrait(unsigned traits, Trait t) { return (traits & t) != 0; }

inline std::string TraitsToCsv(unsigned traits) {
    int n = 0;
    const TraitInfo* all = AllTraits(&n);
    std::string out;
    for (int i = 0; i < n; ++i) {
        if (!(traits & all[i].bit)) continue;
        if (!out.empty()) out += "|";
        out += all[i].id;
    }
    return out;
}

// "sharpshooter|all_around" (also accepts commas and semicolons)
inline unsigned TraitsFromCsv(const std::string& text) {
    int n = 0;
    const TraitInfo* all = AllTraits(&n);
    unsigned bits = 0;
    std::string token;
    auto flush = [&]() {
        for (int i = 0; i < n; ++i) if (token == all[i].id) bits |= all[i].bit;
        token.clear();
    };
    for (char c : text) {
        if (c == '|' || c == ',' || c == ';' || c == ' ') flush();
        else token += static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
    flush();
    return bits;
}

inline std::string TraitNames(unsigned traits) {
    int n = 0;
    const TraitInfo* all = AllTraits(&n);
    std::string out;
    for (int i = 0; i < n; ++i) {
        if (!(traits & all[i].bit)) continue;
        if (!out.empty()) out += ", ";
        out += all[i].name;
    }
    return out;
}

}  // namespace bm
