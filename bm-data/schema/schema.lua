-- SQL Schema initialization
-- This file defines the complete database structure

local schema = {}

schema.sql_statements = {
    -- Players table with attributes
    [[
    CREATE TABLE IF NOT EXISTS players (
        player_id TEXT PRIMARY KEY,
        first_name TEXT NOT NULL,
        last_name TEXT NOT NULL,
        position TEXT DEFAULT 'G',
        height REAL,
        weight REAL,
        birth_date TEXT,
        jersey_number INTEGER,
        team_id TEXT,
        age INTEGER,
        
        -- Attributes (0-20 scale, where 20 is elite)
        pace INTEGER DEFAULT 10,
        shooting INTEGER DEFAULT 10,
        ball_control INTEGER DEFAULT 10,
        defense INTEGER DEFAULT 10,
        physical INTEGER DEFAULT 10,
        technical INTEGER DEFAULT 10,
        
        -- Developer potential
        current_ability INTEGER DEFAULT 70,
        potential_ability INTEGER DEFAULT 80,
        age_current INTEGER,
        
        -- Status
        active BOOLEAN DEFAULT 1,
        injured BOOLEAN DEFAULT 0,
        suspension_matches INTEGER DEFAULT 0,
        
        -- NCAA specific
        year_in_school INTEGER,
        international BOOLEAN DEFAULT 0,
        draft_eligible BOOLEAN DEFAULT 0,
        nil_value INTEGER DEFAULT 0,
        
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    );
    ]],
    
    -- Teams table
    [[
    CREATE TABLE IF NOT EXISTS teams (
        team_id TEXT PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        conference_id TEXT NOT NULL,
        city TEXT,
        state TEXT,
        arena TEXT,
        founded INTEGER,
        logo_path TEXT,
        primary_color TEXT,
        secondary_color TEXT,
        
        -- Reputation and prestige
        prestige INTEGER DEFAULT 50,
        home_record TEXT DEFAULT '0-0',
        away_record TEXT DEFAULT '0-0',
        
        -- Financial
        budget INTEGER DEFAULT 1000000,
        nil_budget INTEGER DEFAULT 500000,
        
        active BOOLEAN DEFAULT 1,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    );
    ]],
    
    -- Conferences table
    [[
    CREATE TABLE IF NOT EXISTS conferences (
        conference_id TEXT PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        country TEXT DEFAULT 'USA',
        division TEXT DEFAULT 'NCAA-D1',
        
        num_teams INTEGER,
        playoff_format TEXT DEFAULT 'conference_tournament',
        
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    );
    ]],
    
    -- Contracts table
    [[
    CREATE TABLE IF NOT EXISTS contracts (
        contract_id TEXT PRIMARY KEY,
        player_id TEXT NOT NULL,
        team_id TEXT NOT NULL,
        
        -- For NCAA (recruiting)
        scholarship_offer BOOLEAN DEFAULT 0,
        scholarship_percent REAL DEFAULT 1.0,
        
        -- For professional
        start_year INTEGER,
        end_year INTEGER,
        annual_salary INTEGER DEFAULT 0,
        total_guaranteed INTEGER DEFAULT 0,
        
        status TEXT DEFAULT 'active',
        
        FOREIGN KEY(player_id) REFERENCES players(player_id),
        FOREIGN KEY(team_id) REFERENCES teams(team_id)
    );
    ]],
    
    -- Statistics table
    [[
    CREATE TABLE IF NOT EXISTS statistics (
        stat_id TEXT PRIMARY KEY,
        player_id TEXT NOT NULL,
        team_id TEXT NOT NULL,
        season INTEGER,
        
        -- Game log
        games_played INTEGER DEFAULT 0,
        games_started INTEGER DEFAULT 0,
        minutes REAL DEFAULT 0,
        
        -- Scoring
        points REAL DEFAULT 0,
        fg_made INTEGER DEFAULT 0,
        fg_attempts INTEGER DEFAULT 0,
        three_made INTEGER DEFAULT 0,
        three_attempts INTEGER DEFAULT 0,
        ft_made INTEGER DEFAULT 0,
        ft_attempts INTEGER DEFAULT 0,
        
        -- Rebounds & assists
        rebounds REAL DEFAULT 0,
        assists REAL DEFAULT 0,
        
        -- Defense & other
        steals REAL DEFAULT 0,
        blocks REAL DEFAULT 0,
        turnovers REAL DEFAULT 0,
        fouls REAL DEFAULT 0,
        
        -- Advanced stats
        per REAL DEFAULT 0,
        ortg REAL DEFAULT 100,
        drtg REAL DEFAULT 100,
        
        FOREIGN KEY(player_id) REFERENCES players(player_id),
        FOREIGN KEY(team_id) REFERENCES teams(team_id)
    );
    ]],
    
    -- Matches table
    [[
    CREATE TABLE IF NOT EXISTS matches (
        match_id TEXT PRIMARY KEY,
        season INTEGER NOT NULL,
        round INTEGER,
        conference_id TEXT,
        
        home_team_id TEXT NOT NULL,
        away_team_id TEXT NOT NULL,
        
        home_score INTEGER DEFAULT 0,
        away_score INTEGER DEFAULT 0,
        
        status TEXT DEFAULT 'scheduled',
        match_date TEXT,
        match_time TEXT,
        location TEXT,
        
        -- Result details
        winning_team_id TEXT,
        margin_of_victory INTEGER,
        
        FOREIGN KEY(home_team_id) REFERENCES teams(team_id),
        FOREIGN KEY(away_team_id) REFERENCES teams(team_id)
    );
    ]],
    
    -- Seasons table
    [[
    CREATE TABLE IF NOT EXISTS seasons (
        season_id TEXT PRIMARY KEY,
        year INTEGER UNIQUE,
        status TEXT DEFAULT 'planning',
        current_round INTEGER DEFAULT 0,
        start_date TEXT,
        end_date TEXT,
        
        championship_team_id TEXT,
        
        FOREIGN KEY(championship_team_id) REFERENCES teams(team_id)
    );
    ]],
    
    -- Indices for performance
    [[CREATE INDEX IF NOT EXISTS idx_players_team ON players(team_id);]],
    [[CREATE INDEX IF NOT EXISTS idx_teams_conference ON teams(conference_id);]],
    [[CREATE INDEX IF NOT EXISTS idx_stats_player ON statistics(player_id);]],
    [[CREATE INDEX IF NOT EXISTS idx_matches_season ON matches(season);]],
}

return schema
