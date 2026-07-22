#pragma once
// ============================================================================
// Plugin.h — Robot-UI Plugin API + Subprocess Host (single header)
// Language-agnostic: .py, .exe, .js etc all run as independent subprocesses
// communicating via stdin/stdout JSON-RPC.
// ============================================================================
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "jsonrpcpp.hpp"

// ---- NgSnapshot -----------------------------------------------------------
#define NG_SNAP_MAX_KEYS    64
#define NG_SNAP_MAX_OUTPUTS 64
#define NG_SNAP_MAX_GLOBALS 32
#define NG_SNAP_MAX_NAME    64
#define NG_SNAP_MAX_PATH    128

#pragma pack(push, 1)
struct NgSnapshot
{
    int  nodeCount = 0, isRunning = 0, keyCount = 0, outputCount = 0, globalCount = 0;
    char keyNames[NG_SNAP_MAX_KEYS][NG_SNAP_MAX_NAME];
    float keyValues[NG_SNAP_MAX_KEYS];
    char outputPaths[NG_SNAP_MAX_OUTPUTS][NG_SNAP_MAX_PATH];
    float outputValues[NG_SNAP_MAX_OUTPUTS];
    char globalNames[NG_SNAP_MAX_GLOBALS][NG_SNAP_MAX_NAME];
    float globalValues[NG_SNAP_MAX_GLOBALS];
};
#pragma pack(pop)

// ---- IHostServices --------------------------------------------------------
class NodeGraphManager;
class RobotCommManager;
class RobotComponentManager;
class GamepadMapperManager;
class LiveStreamManager;

class IHostServices
{
public:
    virtual ~IHostServices() = default;
    virtual NodeGraphManager*       GetNodeGraphManager()       = 0;
    virtual RobotCommManager*       GetRobotCommManager()       = 0;
    virtual RobotComponentManager*  GetRobotComponentManager()  = 0;
    virtual GamepadMapperManager*   GetGamepadMapperManager()   = 0;
    virtual LiveStreamManager*      GetLiveStreamManager()      = 0;
    virtual void RegisterMenuItem(const char* p, std::function<void()> cb) = 0;
    virtual void RegisterWindow(const char* t, std::function<void(bool*)> r) = 0;
    virtual void UnregisterWindow(const char* t) = 0;
    enum class LogLevel { Trace, Info, Warn, Error };
    virtual void Log(LogLevel lv, const char* tag, const char* msg) = 0;
    virtual std::string GetPluginDataDir() const = 0;
    virtual std::string GetExeDir() const = 0;
    virtual int  GetNodeGraphSnapshot(NgSnapshot& out) = 0;
    virtual void* GetImGuiContext() const = 0;
    virtual Json  HandleRequest(const std::string& method, const Json& params) = 0;
    virtual void  HandleNotification(const std::string& method, const Json& params) = 0;
};

// ---- PluginInfo / PluginState ---------------------------------------------
struct PluginInfo
{
    std::string name, version, author, description, website;
    uint32_t apiVersionMajor = 0, apiVersionMinor = 0;
};

enum class PluginState { NotLoaded, Loaded, Active, Error };

// ---- Plugin ---------------------------------------------------------------
class Plugin
{
public:
    Plugin();
    ~Plugin();

    bool Initialize();
    void Shutdown();
    bool Ok() const { return m_Ok; }

    void SetDir(const std::string& dir) { m_Dir = dir; }
    const std::string& Dir() const { return m_Dir; }

    // Callback: host provides robot state JSON to merge into on_update params
    // Signature: Json(float dt) → returns extra Json to merge into params
    void SetStateProvider(std::function<Json(float dt)> provider) { m_StateProvider = std::move(provider); }

    void   Scan(IHostServices* host);
    bool   Load(const std::string& path, IHostServices* host);
    void   Unload(const std::string& name);
    void   UnloadAll();
    void   Enable(const std::vector<std::string>& names);
    void   Disable(const std::vector<std::string>& names);

    // Subscription management (called from HandleRequest)
    void   Subscribe(const std::string& name, const Json& paths, double intervalSec);
    void   Unsubscribe(const std::string& name);

    // Filter full state JSON to only requested paths
    static Json FilterState(const Json& full, const Json& paths);

    // Find Python executable for a plugin
    static std::string FindPython(const std::string& pluginDir = "");

    void OnUpdate(float dt);
    void OnUIRender();
    void OnMenuBar();

    bool        Loaded(const std::string& n) const;
    bool        Enabled(const std::string& n) const;
    PluginState State(const std::string& n) const;
    const PluginInfo* Info(const std::string& n) const;
    size_t      Count() const;
    std::vector<std::string> Names() const;
    std::vector<std::string> EnabledNames() const;

private:
    struct E
    {
        std::string   name, path;
        PluginInfo    info;
        PluginState   state = PluginState::NotLoaded;
        HANDLE        hProc = nullptr, hIn = nullptr, hOut = nullptr;
        std::thread*  rd    = nullptr;
        std::unique_ptr<std::mutex> mtx = std::make_unique<std::mutex>();
        std::vector<std::string> uiCmds;
        bool          uiDone  = false;
        bool          running = true;
        int           nextId  = 1;
        // Subscription
        bool          subscribed = false;
        double        subIntervalSec = 0.1;
        double        subLastPush = 0.0;
        Json          subPaths = Json::array();
        // Async writer — decouples pipe writes from game loop & reader thread
        std::vector<std::string> writeQueue;     // on_update NOTIFICATIONS (low priority)
        std::vector<std::string> urgentQueue;    // RESPONSES (high priority, written first)
        std::unique_ptr<std::mutex> writeMtx = std::make_unique<std::mutex>();
        bool                     writerStop = false;
        std::thread*             writer = nullptr;
    };
    E*       Find(const std::string& n);
    const E* Find(const std::string& n) const;
    bool     Call(E& e, const char* method, const Json& params = nullptr);
    bool     Notify(E& e, const char* method, const Json& params = nullptr);
    void     SendResponse(E& e, int id, const Json& result);
    void     SendError(E& e, int id, int code, const std::string& msg);
    static void EnqueueWrite(E& e, std::string&& data);
    static void EnqueueUrgentWrite(E& e, std::string&& data);
    static void WriterThread(E* e);
    void     StartReader(E& e, IHostServices* host);
    static void Reader(E* e, IHostServices* host);

    std::string m_Dir, m_Bridge, m_Python;
    std::deque<E> m_Entries;  // deque never invalidates pointers on push_back
    mutable std::recursive_mutex m_Mx;
    bool m_Ok = false;
    std::function<Json(float dt)> m_StateProvider;
};
