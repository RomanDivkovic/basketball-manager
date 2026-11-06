#include "db/LeagueImport.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace bm {
namespace db {

namespace {

const char* kSchema =
    "CREATE TABLE teams ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, conference TEXT NOT NULL,"
    "prestige INTEGER DEFAULT 10, pace INTEGER DEFAULT 10, shooting INTEGER DEFAULT 10,"
    "defense INTEGER DEFAULT 10, rebounding INTEGER DEFAULT 10, ball_handling INTEGER DEFAULT 10,"
    "active INTEGER DEFAULT 1, created_at DATETIME DEFAULT CURRENT_TIMESTAMP);"
    "CREATE TABLE players ("
    "player_id TEXT PRIMARY KEY, first_name TEXT NOT NULL, last_name TEXT NOT NULL, team_id INTEGER,"
    "position TEXT NOT NULL, year_in_school INTEGER DEFAULT 4, height REAL DEFAULT 2.0, weight REAL DEFAULT 100.0,"
    "jersey_number INTEGER DEFAULT 0, birth_date TEXT DEFAULT '2000-01-01', pace INTEGER DEFAULT 10,"
    "shooting INTEGER DEFAULT 10, ball_control INTEGER DEFAULT 10, defense INTEGER DEFAULT 10,"
    "physical INTEGER DEFAULT 10, technical INTEGER DEFAULT 10, current_ability INTEGER DEFAULT 70,"
    "potential_ability INTEGER DEFAULT 75, active INTEGER DEFAULT 1, injured INTEGER DEFAULT 0,"
    "suspension_matches INTEGER DEFAULT 0, international INTEGER DEFAULT 0, draft_eligible INTEGER DEFAULT 0,"
    "nil_value INTEGER DEFAULT 0, games_played REAL DEFAULT 0, minutes_played REAL DEFAULT 0,"
    "points_per_game REAL DEFAULT 0, rebounds_per_game REAL DEFAULT 0, assists_per_game REAL DEFAULT 0,"
    "FOREIGN KEY (team_id) REFERENCES teams(id));"
    "CREATE INDEX idx_players_team ON players(team_id);"
    "CREATE INDEX idx_teams_conf ON teams(conference);";

struct CsvTable {
    std::vector<std::string> header;                     // lower-case column names
    std::vector<std::vector<std::string>> rows;
    std::vector<int> lines;                              // source line of each row
};

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string Trim(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Reads a CSV file with quoted fields; returns false and an error when the file cannot be read
bool ReadCsv(const std::string& path, CsvTable& table, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = path + ": file not found";
        return false;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string text = buffer.str();
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    bool any = false;
    int line = 1;
    int rowLine = 1;
    auto endRow = [&]() {
        if (any || !field.empty() || !fields.empty()) {
            fields.push_back(field);
            const bool blank = fields.size() == 1 && Trim(fields[0]).empty();
            if (!blank) {
                if (table.header.empty()) {
                    for (auto& f : fields) table.header.push_back(Lower(Trim(f)));
                } else {
                    table.rows.push_back(fields);
                    table.lines.push_back(rowLine);
                }
            }
        }
        fields.clear();
        field.clear();
        any = false;
    };
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (quoted) {
            if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; }
            else if (c == '"') quoted = false;
            else { field += c; if (c == '\n') ++line; }
        } else if (c == '"') {
            quoted = true;
            any = true;
        } else if (c == ',') {
            fields.push_back(field);
            field.clear();
            any = true;
        } else if (c == '\r') {
            continue;
        } else if (c == '\n') {
            endRow();
            ++line;
            rowLine = line;
        } else {
            field += c;
            any = true;
        }
    }
    endRow();
    if (table.header.empty()) {
        error = path + ": the file is empty";
        return false;
    }
    return true;
}

int ColumnOf(const CsvTable& t, const std::string& name) {
    for (size_t i = 0; i < t.header.size(); ++i) if (t.header[i] == name) return static_cast<int>(i);
    return -1;
}

std::string Cell(const CsvTable& t, size_t row, int column) {
    if (column < 0 || column >= static_cast<int>(t.rows[row].size())) return "";
    return Trim(t.rows[row][column]);
}

bool ParseInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

bool ParseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return *end == '\0';
}

std::string Quote(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    return out + "\"";
}

std::string Text(sqlite3_stmt* st, int col) {
    const unsigned char* t = sqlite3_column_text(st, col);
    return t ? reinterpret_cast<const char*>(t) : "";
}

} // namespace

LeagueImportResult ImportLeagueFromCsv(const std::string& folder, const std::string& outDbPath) {
    LeagueImportResult result;
    auto fail = [&](const std::string& message) {
        if (result.errors.size() < 12) result.errors.push_back(message);
    };

    CsvTable teams;
    CsvTable players;
    std::string error;
    const std::string sep = (!folder.empty() && folder.back() == '/') ? "" : "/";
    const std::string teamsPath = folder + sep + "teams.csv";
    const std::string playersPath = folder + sep + "players.csv";
    if (!ReadCsv(teamsPath, teams, error)) { fail(error); return result; }
    if (!ReadCsv(playersPath, players, error)) { fail(error); return result; }

    const int tName = ColumnOf(teams, "name");
    const int tConf = ColumnOf(teams, "conference");
    if (tName < 0 || tConf < 0) {
        fail("teams.csv needs the columns: name, conference");
        return result;
    }
    const int pTeam = ColumnOf(players, "team");
    const int pFirst = ColumnOf(players, "first_name");
    const int pLast = ColumnOf(players, "last_name");
    const int pPos = ColumnOf(players, "position");
    const int pAbility = ColumnOf(players, "current_ability");
    if (pTeam < 0 || pFirst < 0 || pLast < 0 || pPos < 0 || pAbility < 0) {
        fail("players.csv needs the columns: team, first_name, last_name, position, current_ability");
        return result;
    }

    struct TeamRow { std::string name, conference; int prestige = 10, pace = 10, shooting = 10, defense = 10, rebounding = 10, ballHandling = 10; };
    std::vector<TeamRow> teamRows;
    std::map<std::string, int> teamIndex;
    std::map<std::string, int> conferenceCount;
    auto teamInt = [&](size_t r, const char* column, int fallback, int lo, int hi, int& out) {
        const int c = ColumnOf(teams, column);
        const std::string s = Cell(teams, r, c);
        out = fallback;
        if (c < 0 || s.empty()) return;
        int v;
        if (!ParseInt(s, v) || v < lo || v > hi) {
            fail("teams.csv line " + std::to_string(teams.lines[r]) + ": " + column + " must be a number from " + std::to_string(lo) + " to " + std::to_string(hi));
            return;
        }
        out = v;
    };
    for (size_t r = 0; r < teams.rows.size(); ++r) {
        TeamRow t;
        t.name = Cell(teams, r, tName);
        std::string conf = Lower(Cell(teams, r, tConf));
        if (t.name.empty()) { fail("teams.csv line " + std::to_string(teams.lines[r]) + ": the team name is empty"); continue; }
        if (conf == "eastern" || conf == "east") t.conference = "Eastern";
        else if (conf == "western" || conf == "west") t.conference = "Western";
        else { fail("teams.csv line " + std::to_string(teams.lines[r]) + ": conference must be Eastern or Western"); continue; }
        if (teamIndex.count(Lower(t.name))) { fail("teams.csv line " + std::to_string(teams.lines[r]) + ": duplicate team " + t.name); continue; }
        teamInt(r, "prestige", 10, 0, 20, t.prestige);
        teamInt(r, "pace", 10, 0, 20, t.pace);
        teamInt(r, "shooting", 10, 0, 20, t.shooting);
        teamInt(r, "defense", 10, 0, 20, t.defense);
        teamInt(r, "rebounding", 10, 0, 20, t.rebounding);
        teamInt(r, "ball_handling", 10, 0, 20, t.ballHandling);
        teamIndex[Lower(t.name)] = static_cast<int>(teamRows.size());
        ++conferenceCount[t.conference];
        teamRows.push_back(t);
    }
    if (teamRows.size() < 16 || teamRows.size() > 40) {
        fail("The league needs 16 to 40 teams (found " + std::to_string(teamRows.size()) + ")");
    }
    for (const char* conf : {"Eastern", "Western"}) {
        if (conferenceCount[conf] < 8) fail(std::string("The ") + conf + " conference needs at least 8 teams (found " + std::to_string(conferenceCount[conf]) + ")");
    }

    struct PlayerRow {
        int team = 0;
        std::string first, last, position, birth = "2000-01-01";
        int jersey = 0, ability = 70, potential = 75;
        double height = 2.0, weight = 100.0;
        int pace = 0, shooting = 0, ballControl = 0, defense = 0, physical = 0, technical = 0;
    };
    std::vector<PlayerRow> playerRows;
    std::vector<int> perTeam(teamRows.size(), 0);
    std::set<std::string> seen;
    static const std::set<std::string> kPositions = {"PG", "SG", "SF", "PF", "C"};
    for (size_t r = 0; r < players.rows.size(); ++r) {
        const std::string where = "players.csv line " + std::to_string(players.lines[r]) + ": ";
        PlayerRow p;
        const std::string teamName = Cell(players, r, pTeam);
        auto it = teamIndex.find(Lower(teamName));
        if (it == teamIndex.end()) { fail(where + "unknown team \"" + teamName + "\" (it must match a name in teams.csv)"); continue; }
        p.team = it->second;
        p.first = Cell(players, r, pFirst);
        p.last = Cell(players, r, pLast);
        if (p.first.empty() || p.last.empty()) { fail(where + "first_name and last_name are required"); continue; }
        std::string position = Cell(players, r, pPos);
        std::transform(position.begin(), position.end(), position.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (!kPositions.count(position)) { fail(where + "position must be PG, SG, SF, PF or C"); continue; }
        p.position = position;
        if (!ParseInt(Cell(players, r, pAbility), p.ability) || p.ability < 40 || p.ability > 99) {
            fail(where + "current_ability must be a number from 40 to 99");
            continue;
        }
        const std::string key = Lower(teamName + "|" + p.first + " " + p.last);
        if (!seen.insert(key).second) { fail(where + "duplicate player " + p.first + " " + p.last + " on " + teamName); continue; }

        bool bad = false;
        auto optionalInt = [&](const char* column, int fallback, int lo, int hi, int& out) {
            const std::string s = Cell(players, r, ColumnOf(players, column));
            out = fallback;
            if (s.empty()) return;
            int v;
            if (!ParseInt(s, v) || v < lo || v > hi) {
                fail(where + column + " must be a number from " + std::to_string(lo) + " to " + std::to_string(hi));
                bad = true;
                return;
            }
            out = v;
        };
        optionalInt("potential_ability", std::min(99, p.ability + 5), 40, 99, p.potential);
        p.potential = std::max(p.potential, p.ability);
        optionalInt("jersey_number", 0, 0, 99, p.jersey);
        const int base = std::clamp(static_cast<int>(p.ability / 5.0 + 0.5), 1, 20);
        optionalInt("pace", base, 1, 20, p.pace);
        optionalInt("shooting", base, 1, 20, p.shooting);
        optionalInt("ball_control", base, 1, 20, p.ballControl);
        optionalInt("defense", base, 1, 20, p.defense);
        optionalInt("physical", base, 1, 20, p.physical);
        optionalInt("technical", base, 1, 20, p.technical);
        const std::string birth = Cell(players, r, ColumnOf(players, "birth_date"));
        if (!birth.empty()) {
            int y = 0;
            if (birth.size() < 4 || !ParseInt(birth.substr(0, 4), y) || y < 1970 || y > 2012) {
                fail(where + "birth_date must start with a year from 1970 to 2012 (YYYY-MM-DD)");
                bad = true;
            } else {
                p.birth = birth;
            }
        }
        const std::string h = Cell(players, r, ColumnOf(players, "height"));
        if (!h.empty() && (!ParseDouble(h, p.height) || p.height < 1.6 || p.height > 2.4)) { fail(where + "height is in metres, 1.6 to 2.4"); bad = true; }
        const std::string w = Cell(players, r, ColumnOf(players, "weight"));
        if (!w.empty() && (!ParseDouble(w, p.weight) || p.weight < 60 || p.weight > 180)) { fail(where + "weight is in kg, 60 to 180"); bad = true; }
        if (bad) continue;
        ++perTeam[p.team];
        playerRows.push_back(p);
    }
    for (size_t t = 0; t < teamRows.size(); ++t) {
        if (perTeam[t] < 8 || perTeam[t] > 20) {
            fail("Team " + teamRows[t].name + " has " + std::to_string(perTeam[t]) + " players; each team needs 8 to 20");
        }
    }
    if (!result.errors.empty()) return result;

    std::remove(outDbPath.c_str());
    sqlite3* db = nullptr;
    if (sqlite3_open(outDbPath.c_str(), &db) != SQLITE_OK) {
        fail("Could not create " + outDbPath);
        if (db) sqlite3_close(db);
        return result;
    }
    char* message = nullptr;
    bool ok = sqlite3_exec(db, "BEGIN", nullptr, nullptr, &message) == SQLITE_OK
           && sqlite3_exec(db, kSchema, nullptr, nullptr, &message) == SQLITE_OK;
    if (ok) {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db, "INSERT INTO teams (name, conference, prestige, pace, shooting, defense, rebounding, ball_handling) VALUES (?,?,?,?,?,?,?,?)", -1, &st, nullptr);
        for (const auto& t : teamRows) {
            sqlite3_reset(st);
            sqlite3_bind_text(st, 1, t.name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, t.conference.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 3, t.prestige);
            sqlite3_bind_int(st, 4, t.pace);
            sqlite3_bind_int(st, 5, t.shooting);
            sqlite3_bind_int(st, 6, t.defense);
            sqlite3_bind_int(st, 7, t.rebounding);
            sqlite3_bind_int(st, 8, t.ballHandling);
            if (sqlite3_step(st) != SQLITE_DONE) ok = false;
        }
        sqlite3_finalize(st);
    }
    if (ok) {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db,
            "INSERT INTO players (player_id, first_name, last_name, team_id, position, height, weight, jersey_number, birth_date,"
            " pace, shooting, ball_control, defense, physical, technical, current_ability, potential_ability)"
            " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", -1, &st, nullptr);
        std::vector<int> counter(teamRows.size(), 0);
        for (const auto& p : playerRows) {
            sqlite3_reset(st);
            const int teamId = p.team + 1;   // AUTOINCREMENT follows the file order
            const std::string id = std::to_string(teamId) + "_" + std::to_string(++counter[p.team]);
            sqlite3_bind_text(st, 1, id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, p.first.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 3, p.last.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 4, teamId);
            sqlite3_bind_text(st, 5, p.position.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(st, 6, p.height);
            sqlite3_bind_double(st, 7, p.weight);
            sqlite3_bind_int(st, 8, p.jersey);
            sqlite3_bind_text(st, 9, p.birth.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 10, p.pace);
            sqlite3_bind_int(st, 11, p.shooting);
            sqlite3_bind_int(st, 12, p.ballControl);
            sqlite3_bind_int(st, 13, p.defense);
            sqlite3_bind_int(st, 14, p.physical);
            sqlite3_bind_int(st, 15, p.technical);
            sqlite3_bind_int(st, 16, p.ability);
            sqlite3_bind_int(st, 17, p.potential);
            if (sqlite3_step(st) != SQLITE_DONE) ok = false;
        }
        sqlite3_finalize(st);
    }
    if (ok) ok = sqlite3_exec(db, "COMMIT", nullptr, nullptr, &message) == SQLITE_OK;
    if (!ok) {
        fail(std::string("Could not write the league database: ") + (message ? message : sqlite3_errmsg(db)));
        sqlite3_free(message);
        sqlite3_close(db);
        std::remove(outDbPath.c_str());
        return result;
    }
    sqlite3_close(db);
    result.ok = true;
    result.teams = static_cast<int>(teamRows.size());
    result.players = static_cast<int>(playerRows.size());
    return result;
}

bool ExportLeagueTemplate(sqlite3* source, const std::string& folder, std::string* error) {
    auto say = [&](const std::string& text) { if (error) *error = text; };
    if (!source) { say("No league is loaded"); return false; }
    const std::string sep = (!folder.empty() && folder.back() == '/') ? "" : "/";

    std::ofstream teamsOut(folder + sep + "teams.csv", std::ios::binary);
    std::ofstream playersOut(folder + sep + "players.csv", std::ios::binary);
    std::ofstream readme(folder + sep + "README.txt", std::ios::binary);
    if (!teamsOut || !playersOut || !readme) { say("Could not write into " + folder); return false; }

    teamsOut << "name,conference,prestige,pace,shooting,defense,rebounding,ball_handling\n";
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(source, "SELECT name, conference, prestige, pace, shooting, defense, rebounding, ball_handling FROM teams ORDER BY id", -1, &st, nullptr);
    while (st && sqlite3_step(st) == SQLITE_ROW) {
        teamsOut << Quote(Text(st, 0)) << "," << Text(st, 1);
        for (int c = 2; c <= 7; ++c) teamsOut << "," << sqlite3_column_int(st, c);
        teamsOut << "\n";
    }
    sqlite3_finalize(st);

    playersOut << "team,first_name,last_name,position,jersey_number,birth_date,height,weight,pace,shooting,ball_control,defense,physical,technical,current_ability,potential_ability\n";
    st = nullptr;
    sqlite3_prepare_v2(source,
        "SELECT t.name, p.first_name, p.last_name, p.position, p.jersey_number, p.birth_date, p.height, p.weight,"
        " p.pace, p.shooting, p.ball_control, p.defense, p.physical, p.technical, p.current_ability, p.potential_ability"
        " FROM players p JOIN teams t ON t.id = p.team_id ORDER BY t.id, p.jersey_number, p.player_id", -1, &st, nullptr);
    while (st && sqlite3_step(st) == SQLITE_ROW) {
        playersOut << Quote(Text(st, 0)) << "," << Quote(Text(st, 1)) << "," << Quote(Text(st, 2)) << "," << Text(st, 3)
                   << "," << sqlite3_column_int(st, 4) << "," << Text(st, 5) << "," << sqlite3_column_double(st, 6)
                   << "," << sqlite3_column_double(st, 7);
        for (int c = 8; c <= 15; ++c) playersOut << "," << sqlite3_column_int(st, c);
        playersOut << "\n";
    }
    sqlite3_finalize(st);

    readme <<
        "Custom league files\n"
        "===================\n"
        "Edit teams.csv and players.csv, then pick this folder when you create a new career.\n\n"
        "teams.csv (required: name, conference)\n"
        "  name           unique team name\n"
        "  conference     Eastern or Western (at least 8 teams in each, 16 to 40 teams in total)\n"
        "  prestige, pace, shooting, defense, rebounding, ball_handling   0 to 20 (optional, default 10)\n\n"
        "players.csv (required: team, first_name, last_name, position, current_ability)\n"
        "  team                  must match a name in teams.csv (8 to 20 players per team)\n"
        "  position              PG, SG, SF, PF or C\n"
        "  current_ability       40 to 99\n"
        "  potential_ability     40 to 99 (optional, default ability + 5)\n"
        "  jersey_number         0 to 99 (optional)\n"
        "  birth_date            YYYY-MM-DD (optional; players with the placeholder 2000-01-01 get an age from their potential)\n"
        "  height (m), weight (kg), pace, shooting, ball_control, defense, physical, technical (1 to 20)   optional\n\n"
        "Fields containing commas must be wrapped in double quotes.\n";
    return true;
}

} // namespace db
} // namespace bm
