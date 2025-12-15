using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using CsvHelper;
using CsvHelper.Configuration;
using Microsoft.Data.Sqlite;
using NCAAImporter.Models;

namespace NCAAImporter;

class Program
{
    static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;
        Console.WriteLine("🏀 Basketball Manager - NCAA Data Importer");
        Console.WriteLine("==========================================\n");

        string csvPath = args.Length > 0 
            ? args[0] 
            : "../../bm-data/ncaa-source/cbb25.csv";

        string dbPath = args.Length > 1 
            ? args[1] 
            : "../../bm-data/basketball_manager.db";

        if (!File.Exists(csvPath))
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"❌ Error: CSV file not found: {csvPath}");
            Console.ResetColor();
            return;
        }

        try
        {
            Console.WriteLine($"📂 Reading CSV: {Path.GetFileName(csvPath)}");
            var teams = ReadNCAAData(csvPath);
            Console.WriteLine($"✅ Loaded {teams.Count} teams\n");

            Console.WriteLine($"💾 Database: {Path.GetFileName(dbPath)}");
            InitializeDatabase(dbPath);
            ImportTeams(dbPath, teams);

            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine("\n✨ Import completed successfully!");
            Console.ResetColor();

            // Print summary statistics
            PrintSummary(teams);
        }
        catch (Exception ex)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"\n❌ Error: {ex.Message}");
            Console.WriteLine(ex.StackTrace);
            Console.ResetColor();
        }
    }

    static List<NCAATeamStats> ReadNCAAData(string csvPath)
    {
        var config = new CsvConfiguration(CultureInfo.InvariantCulture)
        {
            HasHeaderRecord = true,
            MissingFieldFound = null,
            BadDataFound = null
        };

        using var reader = new StreamReader(csvPath);
        using var csv = new CsvReader(reader, config);
        
        return csv.GetRecords<NCAATeamStats>().ToList();
    }

    static void InitializeDatabase(string dbPath)
    {
        // Ensure directory exists
        var dbDir = Path.GetDirectoryName(dbPath);
        if (!string.IsNullOrEmpty(dbDir) && !Directory.Exists(dbDir))
        {
            Directory.CreateDirectory(dbDir);
        }

        // Create database file if it doesn't exist
        if (!File.Exists(dbPath))
        {
            Console.WriteLine("Creating new database...");
            File.Create(dbPath).Dispose();
        }

        using var connection = new SqliteConnection($"Data Source={dbPath}");
        connection.Open();

        var command = connection.CreateCommand();
        command.CommandText = @"
            CREATE TABLE IF NOT EXISTS teams (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL UNIQUE,
                conference TEXT NOT NULL,
                prestige INTEGER DEFAULT 10,
                pace INTEGER DEFAULT 10,
                shooting INTEGER DEFAULT 10,
                defense INTEGER DEFAULT 10,
                rebounding INTEGER DEFAULT 10,
                ball_handling INTEGER DEFAULT 10,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );

            CREATE INDEX IF NOT EXISTS idx_teams_conference ON teams(conference);
            CREATE INDEX IF NOT EXISTS idx_teams_name ON teams(name);
        ";
        command.ExecuteNonQuery();

        Console.WriteLine("✅ Database schema initialized");
    }

    static void ImportTeams(string dbPath, List<NCAATeamStats> teams)
    {
        using var connection = new SqliteConnection($"Data Source={dbPath}");
        connection.Open();

        // Clear existing data
        var clearCommand = connection.CreateCommand();
        clearCommand.CommandText = "DELETE FROM teams";
        clearCommand.ExecuteNonQuery();
        Console.WriteLine("🗑️  Cleared existing team data");

        // Prepare insert statement
        var insertCommand = connection.CreateCommand();
        insertCommand.CommandText = @"
            INSERT INTO teams (name, conference, prestige, pace, shooting, defense, rebounding, ball_handling)
            VALUES (@name, @conference, @prestige, @pace, @shooting, @defense, @rebounding, @ballHandling)
        ";

        // Add parameters
        insertCommand.Parameters.Add("@name", SqliteType.Text);
        insertCommand.Parameters.Add("@conference", SqliteType.Text);
        insertCommand.Parameters.Add("@prestige", SqliteType.Integer);
        insertCommand.Parameters.Add("@pace", SqliteType.Integer);
        insertCommand.Parameters.Add("@shooting", SqliteType.Integer);
        insertCommand.Parameters.Add("@defense", SqliteType.Integer);
        insertCommand.Parameters.Add("@rebounding", SqliteType.Integer);
        insertCommand.Parameters.Add("@ballHandling", SqliteType.Integer);

        int imported = 0;
        int skipped = 0;

        Console.WriteLine("\n📊 Importing teams...");
        Console.WriteLine("─────────────────────────────────────────────────────────────");

        foreach (var team in teams)
        {
            try
            {
                var attributes = StatConverter.ConvertToTeamAttributes(team);
                var normalizedConf = StatConverter.NormalizeConference(team.Conference);

                insertCommand.Parameters["@name"].Value = team.TeamName;
                insertCommand.Parameters["@conference"].Value = normalizedConf;
                insertCommand.Parameters["@prestige"].Value = attributes.prestige;
                insertCommand.Parameters["@pace"].Value = attributes.pace;
                insertCommand.Parameters["@shooting"].Value = attributes.shooting;
                insertCommand.Parameters["@defense"].Value = attributes.defense;
                insertCommand.Parameters["@rebounding"].Value = attributes.rebounding;
                insertCommand.Parameters["@ballHandling"].Value = attributes.ballHandling;

                insertCommand.ExecuteNonQuery();
                imported++;

                // Print every 50th team
                if (imported % 50 == 0)
                {
                    Console.WriteLine($"  ✓ {imported} teams imported...");
                }
            }
            catch (SqliteException ex) when (ex.Message.Contains("UNIQUE"))
            {
                skipped++;
                Console.WriteLine($"  ⚠️  Skipped duplicate: {team.TeamName}");
            }
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine($"  ⚠️  Failed to import {team.TeamName}: {ex.Message}");
                Console.ResetColor();
                skipped++;
            }
        }

        Console.WriteLine("─────────────────────────────────────────────────────────────");
        Console.WriteLine($"✅ Imported: {imported} teams");
        if (skipped > 0)
        {
            Console.WriteLine($"⚠️  Skipped: {skipped} teams");
        }
    }

    static void PrintSummary(List<NCAATeamStats> teams)
    {
        Console.WriteLine("\n📈 Summary Statistics");
        Console.WriteLine("═════════════════════════════════════════════════════════════");

        var conferences = teams.GroupBy(t => t.Conference)
            .OrderByDescending(g => g.Count())
            .Take(10);

        Console.WriteLine("\n🏆 Top Conferences by Team Count:");
        foreach (var conf in conferences)
        {
            Console.WriteLine($"  {conf.Key,-15} {conf.Count(),3} teams");
        }

        var topTeams = teams.OrderByDescending(t => t.PowerRating).Take(10);
        Console.WriteLine("\n⭐ Top 10 Teams by Power Rating:");
        int rank = 1;
        foreach (var team in topTeams)
        {
            var attrs = StatConverter.ConvertToTeamAttributes(team);
            Console.WriteLine($"  {rank,2}. {team.TeamName,-25} ({team.Conference,-6}) " +
                            $"OFF:{attrs.shooting,2} DEF:{attrs.defense,2} PRE:{attrs.prestige,2}");
            rank++;
        }

        Console.WriteLine("\n═════════════════════════════════════════════════════════════");
    }
}
