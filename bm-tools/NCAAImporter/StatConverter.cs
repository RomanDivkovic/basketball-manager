using System;
using NCAAImporter.Models;

namespace NCAAImporter;

/// <summary>
/// Converts NCAA real-world statistics to Basketball Manager's 0-20 attribute scale
/// </summary>
public static class StatConverter
{
    // NCAA stat ranges (approximate from analyzing top/bottom teams)
    private const double MIN_ADJOE = 85.0;
    private const double MAX_ADJOE = 130.0;
    private const double MIN_ADJDE = 85.0;
    private const double MAX_ADJDE = 110.0;
    private const double MIN_TEMPO = 58.0;
    private const double MAX_TEMPO = 78.0;
    private const double MIN_EFG = 40.0;
    private const double MAX_EFG = 62.0;
    private const double MIN_3P = 28.0;
    private const double MAX_3P = 42.0;
    private const double MIN_REBOUND = 20.0;
    private const double MAX_REBOUND = 45.0;

    /// <summary>
    /// Converts NCAA stats to BM team attributes (0-20 scale)
    /// </summary>
    public static (int pace, int shooting, int defense, int rebounding, int ballHandling, int prestige) 
        ConvertToTeamAttributes(NCAATeamStats stats)
    {
        // PACE: Based on adjusted tempo
        int pace = MapToScale(stats.AdjustedTempo, MIN_TEMPO, MAX_TEMPO);

        // SHOOTING: Average of offensive efficiency and effective FG%
        double shootingScore = (
            MapToScaleDouble(stats.AdjustedOffense, MIN_ADJOE, MAX_ADJOE) +
            MapToScaleDouble(stats.EffectiveFGOffense, MIN_EFG, MAX_EFG) +
            MapToScaleDouble(stats.ThreePointOffense, MIN_3P, MAX_3P)
        ) / 3.0;
        int shooting = (int)Math.Round(shootingScore);

        // DEFENSE: Inverse of defensive efficiency (lower is better)
        int defense = MapToScale(MAX_ADJDE - stats.AdjustedDefense + MIN_ADJDE, MIN_ADJDE, MAX_ADJDE);

        // REBOUNDING: Average of offensive and defensive rebound %
        double reboundScore = (
            MapToScaleDouble(stats.OffensiveRebound, MIN_REBOUND, MAX_REBOUND) +
            MapToScaleDouble(stats.DefensiveRebound, MIN_REBOUND, MAX_REBOUND)
        ) / 2.0;
        int rebounding = (int)Math.Round(reboundScore);

        // BALL HANDLING: Inverse of turnover rate (lower is better)
        double ballHandlingScore = MapToScaleDouble(25.0 - stats.TurnoverOffense, 8.0, 18.0);
        int ballHandling = (int)Math.Round(ballHandlingScore);

        // PRESTIGE: Based on power rating, wins above bubble, and recent success
        double prestigeScore = (
            MapToScaleDouble(stats.PowerRating * 100, 10, 98) * 0.5 +
            MapToScaleDouble(stats.WinsAboveBubble, -5, 12) * 0.3 +
            MapToScaleDouble(stats.WinPercentage * 100, 20, 95) * 0.2
        );
        int prestige = (int)Math.Round(prestigeScore);

        // Clamp all values to 0-20 range
        return (
            Clamp(pace, 0, 20),
            Clamp(shooting, 0, 20),
            Clamp(defense, 0, 20),
            Clamp(rebounding, 0, 20),
            Clamp(ballHandling, 0, 20),
            Clamp(prestige, 0, 20)
        );
    }

    private static int MapToScale(double value, double min, double max)
    {
        return (int)Math.Round(MapToScaleDouble(value, min, max));
    }

    private static double MapToScaleDouble(double value, double min, double max)
    {
        if (max <= min) return 10.0;
        
        double normalized = (value - min) / (max - min);
        normalized = Math.Max(0, Math.Min(1, normalized)); // Clamp to 0-1
        
        // Map to 0-20 scale with slight curve (better teams get higher scores)
        return normalized * 20.0;
    }

    private static int Clamp(int value, int min, int max)
    {
        return Math.Max(min, Math.Min(max, value));
    }

    /// <summary>
    /// Gets a simplified conference abbreviation for the database
    /// </summary>
    public static string NormalizeConference(string conf)
    {
        return conf switch
        {
            "ACC" => "ACC",
            "B10" => "Big Ten",
            "B12" => "Big 12",
            "BE" => "Big East",
            "SEC" => "SEC",
            "P12" => "Pac-12",
            "WCC" => "WCC",
            "A10" => "Atlantic 10",
            "MWC" => "Mountain West",
            "Amer" or "Amer." => "American",
            "MVC" => "Missouri Valley",
            "CUSA" => "Conference USA",
            "MAC" => "MAC",
            "WAC" => "WAC",
            "Ivy" => "Ivy League",
            "MAAC" => "MAAC",
            "SB" => "Sun Belt",
            "CAA" => "CAA",
            _ => conf
        };
    }
}
