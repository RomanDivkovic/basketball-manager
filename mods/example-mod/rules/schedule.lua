-- Example custom schedule generation
-- This demonstrates how mods can override game behavior

custom_schedule = {}

function custom_schedule:generateConferenceSchedule(teams)
    print("[CustomSchedule] Generating custom schedule")
    
    local schedule = {}
    
    -- 2-1-1 format: Play each team twice, with crossover
    for i = 1, #teams do
        for j = 1, #teams do
            if i ~= j then
                -- First meeting
                table.insert(schedule, {
                    round = (i-1) * (#teams - 1) + (j-1),
                    home_team = teams[i].id,
                    away_team = teams[j].id,
                    stage = "round_robin"
                })
            end
        end
    end
    
    return schedule
end

return custom_schedule
