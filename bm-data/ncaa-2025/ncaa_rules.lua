-- NCAA Basketball Rules Engine
-- Handles conference structure, schedule generation, playoffs

ncaa_rules = {}

-- NCAA Conferences (Division I)
ncaa_rules.conferences = {
    ACC = {name = "Atlantic Coast Conference", teams = 15},
    Big10 = {name = "Big Ten Conference", teams = 14},
    Big12 = {name = "Big 12 Conference", teams = 16},
    Pac12 = {name = "Pac-12 Conference", teams = 12},
    SEC = {name = "Southeastern Conference", teams = 16},
    AAC = {name = "American Athletic Conference", teams = 11},
    MWC = {name = "Mountain West Conference", teams = 12},
    MAC = {name = "Mid-American Conference", teams = 14},
    WCC = {name = "West Coast Conference", teams = 11},
    A10 = {name = "Atlantic 10 Conference", teams = 14},
}

-- Season structure
ncaa_rules.season = {
    regular_season_start = "November 1",
    regular_season_end = "March 15",
    conference_tournament_start = "March 10",
    conference_tournament_end = "March 17",
    ncaa_tournament_start = "March 20",
    ncaa_tournament_end = "April 8",
}

-- Regular season format
ncaa_rules.conference_schedule = {
    games_per_team = 18, -- Some conferences vary (16-20)
    home_away_split = true,
}

-- Playoff structure
ncaa_rules.ncaa_tournament = {
    format = "single_elimination",
    rounds = 6,
    first_round_byes = 16,
    total_teams = 68,
}

-- Generate conference schedule
function ncaa_rules:generateConferenceSchedule(conference, teams)
    local schedule = {}
    local num_games = self.conference_schedule.games_per_team
    
    -- Round-robin style schedule generation
    for i = 1, num_games do
        for j = 1, #teams do
            table.insert(schedule, {
                round = i,
                home_team = teams[j],
                away_team = teams[(j % #teams) + 1]
            })
        end
    end
    
    return schedule
end

-- Generate full season schedule
function ncaa_rules:generateSchedule(season)
    print("[NCAA Rules] Generating season " .. season .. " schedule")
    
    local schedule = {}
    
    -- Generate conference schedules
    for conf_name, conf_data in pairs(self.conferences) do
        local conf_schedule = self:generateConferenceSchedule(conf_name, {})
        schedule[conf_name] = conf_schedule
    end
    
    return schedule
end

-- Generate NCAA tournament bracket
function ncaa_rules:generatePlayoffs(season)
    print("[NCAA Rules] Generating NCAA Tournament bracket for season " .. season)
    
    local bracket = {}
    
    -- 4 rounds of single elimination
    -- First round: 64 -> 32
    -- Sweet 16: 32 -> 16
    -- Elite 8: 16 -> 8
    -- Final Four: 8 -> 4
    -- Championship: 2 -> 1
    
    return bracket
end

-- Player eligibility rules
function ncaa_rules:checkPlayerEligibility(player, season)
    -- Freshmen eligibility
    if player.year == 1 then
        return true, "Freshman eligible"
    end
    
    -- Transfer portal rules
    if player.transfer_status == "portal" then
        return true, "Transfer portal eligible"
    end
    
    -- Foreign player rules
    if player.international then
        return true, "International player eligible"
    end
    
    return true, "Eligible"
end

-- NIL (Name, Image, Likeness) rules
function ncaa_rules:calculateNILBudget(team, season)
    -- Simplified NIL budget calculation
    -- Based on team revenue, market size, etc.
    
    local base_budget = 500000 -- Base pool
    local market_multiplier = 1.5 -- Market size factor
    
    return base_budget * market_multiplier
end

-- Recruiting rules
function ncaa_rules:validateRecruit(player)
    -- Must be high school senior or international
    if player.grade == 12 or player.international then
        return true, "Valid recruit"
    end
    
    return false, "Invalid recruit status"
end

-- Violation checking
function ncaa_rules:checkViolations(action)
    print("[NCAA Rules] Checking violations for: " .. action)
    return true -- Simplified for now
end

return ncaa_rules
