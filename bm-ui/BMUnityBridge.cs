using UnityEngine;
using System.Runtime.InteropServices;

/**
 * UnityBridge - C# interop layer for C++ Backend
 * Loads native plugin and provides game interface
 */
public class BMUnityBridge : MonoBehaviour {
    
#if UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX
    private const string PluginName = "BMCore";
#else
    private const string PluginName = "BMCore";
#endif

    // C++ function declarations
    [DllImport(PluginName)]
    private static extern void InitializeMatch(string homeTeamId, string awayTeamId);

    [DllImport(PluginName)]
    private static extern int SimulatePossession();
    
    [DllImport(PluginName)]
    private static extern void SimulateMatch();
    
    [DllImport(PluginName)]
    private static extern System.IntPtr GetMatchResult();
    
    [DllImport(PluginName)]
    private static extern System.IntPtr GetLiveCommentary();

    [DllImport(PluginName)]
    private static extern System.IntPtr GetMatchStateJson();

    [DllImport(PluginName)]
    private static extern int IsMatchComplete();

    [DllImport(PluginName)]
    private static extern void SetSimulationSpeed(int speedMultiplier);

    [DllImport(PluginName)]
    private static extern void SetPauseEnabled(int enabled);

    [DllImport(PluginName)]
    private static extern void CallTimeout(int isHomeTeam);

    [DllImport(PluginName)]
    private static extern void ApplyCoachingCallout(int isHomeTeam, int callout);
    
    [DllImport(PluginName)]
    private static extern void InitializeSeason(int year);
    
    [DllImport(PluginName)]
    private static extern void SimulateRound(int roundNumber);
    
    [DllImport(PluginName)]
    private static extern void LoadDatabase(string dbPath);
    
    [DllImport(PluginName)]
    private static extern void SaveDatabase(string dbPath);

    [DllImport(PluginName)]
    private static extern System.IntPtr GetTeamsJson();
    
    [DllImport(PluginName)]
    private static extern void InitializeAI(string teamId);
    
    [DllImport(PluginName)]
    private static extern void GetAILineup(System.IntPtr buffer, int bufferSize);
    
    [DllImport(PluginName)]
    private static extern System.IntPtr GetEngineVersion();

    private static BMUnityBridge instance;

    void Awake() {
        if (instance == null) {
            instance = this;
            DontDestroyOnLoad(gameObject);
        } else {
            Destroy(gameObject);
        }
    }

    void Start() {
        Debug.Log("[BMUnityBridge] Initializing Basketball Manager");
        
        string dataPath = Application.persistentDataPath;
        LoadDatabase(dataPath);
        
        string version = Marshal.PtrToStringAnsi(GetEngineVersion());
        Debug.Log("[BMUnityBridge] Engine version: " + version);
    }

    public static void LoadDb(string dbPath) {
        LoadDatabase(dbPath);
    }

    public static string GetAvailableTeamsJson() {
        return Marshal.PtrToStringAnsi(GetTeamsJson()) ?? "{\"teams\":[]}";
    }

    public static void StartMatch(string homeTeamId, string awayTeamId) {
        InitializeMatch(homeTeamId, awayTeamId);
    }

    public static int AdvancePossession() => SimulatePossession();

    public static bool IsComplete() => IsMatchComplete() != 0;

    public static string GetMatchState() {
        return Marshal.PtrToStringAnsi(GetMatchStateJson()) ?? "{}";
    }

    public static void SetSpeed(int speedMultiplier) {
        SetSimulationSpeed(speedMultiplier);
    }

    public static void SetInteractive(bool enabled) {
        SetPauseEnabled(enabled ? 1 : 0);
    }

    public static void Timeout(bool homeTeam) {
        CallTimeout(homeTeam ? 1 : 0);
    }

    public static void CoachCallout(bool homeTeam, int callout) {
        ApplyCoachingCallout(homeTeam ? 1 : 0, callout);
    }

    public static string GetMatchResultText() {
        return Marshal.PtrToStringAnsi(GetMatchResult());
    }

    public static string GetCommentary() {
        return Marshal.PtrToStringAnsi(GetLiveCommentary());
    }

    public static void StartSeason(int year) {
        InitializeSeason(year);
    }

    public static void SimulateGameRound(int roundNum) {
        SimulateRound(roundNum);
    }
}
