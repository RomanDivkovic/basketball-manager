using CsvHelper.Configuration.Attributes;

namespace NCAAImporter.Models;

/// <summary>
/// Represents a team's season statistics from the NCAA CSV data
/// </summary>
public class NCAATeamStats
{
    [Name("RK")]
    public int? Rank { get; set; }

    [Name("Team")]
    public string TeamName { get; set; } = string.Empty;

    [Name("CONF")]
    public string Conference { get; set; } = string.Empty;

    [Name("G")]
    public int GamesPlayed { get; set; }

    [Name("W")]
    public int Wins { get; set; }

    // Adjusted Offensive Efficiency (points per 100 possessions)
    [Name("ADJOE")]
    public double AdjustedOffense { get; set; }

    // Adjusted Defensive Efficiency (points allowed per 100 possessions)
    [Name("ADJDE")]
    public double AdjustedDefense { get; set; }

    // Power rating (probability of beating average team)
    [Name("BARTHAG")]
    public double PowerRating { get; set; }

    // Effective Field Goal % (offense)
    [Name("EFG_O")]
    public double EffectiveFGOffense { get; set; }

    // Effective Field Goal % (defense)
    [Name("EFG_D")]
    public double EffectiveFGDefense { get; set; }

    // Turnover % (offense)
    [Name("TOR")]
    public double TurnoverOffense { get; set; }

    // Turnover % forced (defense)
    [Name("TORD")]
    public double TurnoverDefense { get; set; }

    // Offensive Rebound %
    [Name("ORB")]
    public double OffensiveRebound { get; set; }

    // Defensive Rebound %
    [Name("DRB")]
    public double DefensiveRebound { get; set; }

    // Free Throw Rate (FTA/FGA)
    [Name("FTR")]
    public double FreeThrowRate { get; set; }

    // Free Throw Rate allowed
    [Name("FTRD")]
    public double FreeThrowRateDefense { get; set; }

    // 2-Point Shooting %
    [Name("2P_O")]
    public double TwoPointOffense { get; set; }

    // 2-Point Shooting % (defense)
    [Name("2P_D")]
    public double TwoPointDefense { get; set; }

    // 3-Point Shooting %
    [Name("3P_O")]
    public double ThreePointOffense { get; set; }

    // 3-Point Shooting % (defense)
    [Name("3P_D")]
    public double ThreePointDefense { get; set; }

    // 3-Point Rate (% of FGA that are 3PA)
    [Name("3PR")]
    public double? ThreePointRate { get; set; }

    // 3-Point Rate allowed
    [Name("3PRD")]
    public double? ThreePointRateDefense { get; set; }

    // Adjusted Tempo (possessions per 40 minutes)
    [Name("ADJ_T")]
    public double AdjustedTempo { get; set; }

    // Wins Above Bubble
    [Name("WAB")]
    public double WinsAboveBubble { get; set; }

    // Tournament seed (if applicable)
    [Name("SEED")]
    [Optional]
    public string? Seed { get; set; }

    [Name("YEAR")]
    [Optional]
    public int? Year { get; set; }

    // Helper properties
    public double WinPercentage => GamesPlayed > 0 ? (double)Wins / GamesPlayed : 0;
}
