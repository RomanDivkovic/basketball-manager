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
    private static extern void SimulateMatch();
    
    [DllImport(PluginName)]
    private static extern System.IntPtr GetMatchResult();
    
    [DllImport(PluginName)]
    private static extern System.IntPtr GetLiveCommentary();
    
    [DllImport(PluginName)]
    private static extern void InitializeSeason(int year);
    
    [DllImport(PluginName)]
    private static extern void SimulateRound(int roundNumber);
    
    [DllImport(PluginName)]
    private static extern void LoadDatabase(string dbPath);
    
    [DllImport(PluginName)]
    private static extern void SaveDatabase(string dbPath);
    
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

    public static void StartMatch(string homeTeamId, string awayTeamId) {
        InitializeMatch(homeTeamId, awayTeamId);
        SimulateMatch();
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
