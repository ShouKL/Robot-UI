#include "PluginManager.h"
#include "../NodeGraphManager.h"
#include "../NodeGraph.h"
#include "../RobotStatus.h"
#include "../GamepadMapperManager.h"
#include "Walnut/Core/Log.h"
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <windows.h>

namespace fs = std::filesystem;

static std::string PathToUtf8(const fs::path& p)
{
    std::wstring ws = p.wstring();
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), s.data(), len, nullptr, nullptr);
    return s;
}

// ============================================================================
PluginManager::PluginManager()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    fs::path exeDir = fs::path(exePath).parent_path();
    m_DirPath = fs::absolute(exeDir / "../../../plugins");
    m_Dir     = PathToUtf8(m_DirPath);
    WL_INFO_TAG("PLUGIN", "Plugin directory: {}", m_Dir);
    if (!fs::exists(m_DirPath)) fs::create_directories(m_DirPath);
    m_Plugin.SetDir(m_Dir);
    m_Plugin.Initialize();

    // Provide robot state to plugins during on_update
    m_Plugin.SetStateProvider([this](float dt) -> Json {
        Json j;
        j["actuators"] = Json::array();
        j["sensors"]   = Json::object();
        j["gamepad"]   = Json::object();
        j["node_graph"] = Json::object({{"variables", Json::object()}, {"outputs", Json::object()}});

        // ── Actuator data (use live command, not static config) ──
        if (m_RobotStatus && m_RobotStatus->HasActiveMode())
        {
            auto cmd = m_RobotStatus->GetCurrentCommand(0);
            if (cmd) {
                for (auto& m : cmd->brushlessmotor) {
                    j["actuators"].push_back(Json::object({
                        {"type", "brushlessmotor"}, {"id", m.id}, {"name", m.name},
                        {"target_speed", m.target_speed.value}, {"target_position", m.target_position.value}
                    }));
                }
                for (auto& s : cmd->servo) {
                    j["actuators"].push_back(Json::object({
                        {"type", "servo"}, {"id", s.id}, {"name", s.name},
                        {"angle", s.angle.value}
                    }));
                }
                for (auto& mc : cmd->motions) {
                    j["actuators"].push_back(Json::object({
                        {"type", "motion"}, {"name", mc.name},
                        {"x", mc.x.value}, {"y", mc.y.value}, {"z", mc.z.value},
                        {"rx", mc.rx.value}, {"ry", mc.ry.value}, {"rz", mc.rz.value}
                    }));
                }
            }
        }

        // ── Sensor data: always populate fields so plugins see available paths ──
        if (m_RobotStatus && m_RobotStatus->IsSensorValid())
        {
            auto s = m_RobotStatus->GetCurrentSensor();
            j["sensors"]["temperature"] = s.temperature.value.value;
            j["sensors"]["humidity"]    = s.humidity.value.value;
            j["sensors"]["depth"]       = s.depth.value.value;
        }
        else
        {
            j["sensors"]["temperature"] = 0.0;
            j["sensors"]["humidity"]    = 0.0;
            j["sensors"]["depth"]       = 0.0;
        }

        // ── NodeGraph: keyValues→gamepad, globals→variables, outputs→outputs ──
        NgSnapshot snap;
        if (GetNodeGraphSnapshot(snap) != 0)
        {
            for (int i = 0; i < snap.keyCount; i++)
                j["gamepad"][std::string(snap.keyNames[i])] = snap.keyValues[i];
            for (int i = 0; i < snap.globalCount; i++)
                j["node_graph"]["variables"][std::string(snap.globalNames[i])] = snap.globalValues[i];
            for (int i = 0; i < snap.outputCount; i++)
                j["node_graph"]["outputs"][std::string(snap.outputPaths[i])] = snap.outputValues[i];
        }

        return j;
    });
}

void PluginManager::SetPluginDirectory(const std::string& dir) { m_Dir = dir; m_DirPath = dir; m_Plugin.SetDir(dir); }
void PluginManager::ScanPlugins()  { std::lock_guard lock(m_Mx); if (!fs::exists(m_DirPath)) { fs::create_directories(m_DirPath); return; } m_Plugin.Scan(this); WL_INFO_TAG("PLUGIN", "Scan done: {} plugins", m_Plugin.Count()); }
void PluginManager::RefreshPlugins() { UnloadAll(); ScanPlugins(); }
bool PluginManager::LoadPlugin(const std::string& f) { std::lock_guard lock(m_Mx); return m_Plugin.Load(f, this); }

void PluginManager::UnloadPlugin(const std::string& name)
{
    std::lock_guard lock(m_Mx);
    m_Menu.erase(std::remove_if(m_Menu.begin(), m_Menu.end(), [&](const MI& mi) { return mi.owner == name; }), m_Menu.end());
    m_Wins.erase(std::remove_if(m_Wins.begin(), m_Wins.end(), [&](const WI& wi) { return wi.owner == name; }), m_Wins.end());
    m_Plugin.Unload(name);
}

void PluginManager::EnablePlugin(const std::string& name)   { m_Plugin.Enable({name}); }
void PluginManager::DisablePlugin(const std::string& name)  { m_Plugin.Disable({name}); }
void PluginManager::UnloadAll() { std::lock_guard lock(m_Mx); m_Menu.clear(); m_Wins.clear(); m_Plugin.UnloadAll(); }

// Queries
bool PluginManager::IsPluginLoaded(const std::string& n) const  { return m_Plugin.Loaded(n); }
bool PluginManager::IsPluginEnabled(const std::string& n) const { return m_Plugin.Enabled(n); }
PluginState PluginManager::GetPluginState(const std::string& n) const { return m_Plugin.State(n); }
const PluginInfo* PluginManager::GetPluginInfo(const std::string& n) const { return m_Plugin.Info(n); }
size_t PluginManager::GetPluginCount() const { return m_Plugin.Count(); }
std::vector<std::string> PluginManager::GetPluginNames() const { return m_Plugin.Names(); }
std::vector<std::string> PluginManager::GetEnabledPluginNames() const { return m_Plugin.EnabledNames(); }
void PluginManager::EnablePlugins(const std::vector<std::string>& ns) { m_Plugin.Enable(ns); }

// Setters
void PluginManager::SetNodeGraphManager(NodeGraphManager* m)       { m_NGM = m; }
void PluginManager::SetRobotCommManager(RobotCommManager* m)        { m_RCM = m; }
void PluginManager::SetRobotComponentManager(RobotComponentManager* m) { m_CPM = m; }
void PluginManager::SetGamepadMapperManager(GamepadMapperManager* m)   { m_GPM = m; }
void PluginManager::SetLiveStreamManager(LiveStreamManager* m)      { m_LSM = m; }
void PluginManager::SetRobotStatus(RobotStatus* rs)                  { m_RobotStatus = rs; }

// Tick
void PluginManager::OnUpdate(float dt)  { std::lock_guard lock(m_Mx); m_Plugin.OnUpdate(dt); }
void PluginManager::OnUIRender()        { std::lock_guard lock(m_Mx); for (auto& w : m_Wins) if (w.open && w.render && !w.title.empty()) if (ImGui::Begin(w.title.c_str(), &w.open)) { w.render(&w.open); ImGui::End(); } m_Plugin.OnUIRender(); }
void PluginManager::OnMenuBar()         { std::lock_guard lock(m_Mx); for (auto& mi : m_Menu) if (m_Plugin.Enabled(mi.owner) && ImGui::MenuItem(mi.path.c_str()) && mi.cb) mi.cb(); m_Plugin.OnMenuBar(); }

// IHostServices
void PluginManager::RegisterMenuItem(const char* p, std::function<void()> cb) { std::lock_guard lock(m_Mx); m_Menu.push_back({p, "", std::move(cb)}); }
void PluginManager::RegisterWindow(const char* t, std::function<void(bool*)> r) { std::lock_guard lock(m_Mx); m_Wins.push_back({t, "", std::move(r), true}); }
void PluginManager::UnregisterWindow(const char* t) { std::lock_guard lock(m_Mx); m_Wins.erase(std::remove_if(m_Wins.begin(), m_Wins.end(), [&](const WI& w){ return w.title == t; }), m_Wins.end()); }
void PluginManager::Log(LogLevel lv, const char* tag, const char* msg) {
    switch (lv) {
    case LogLevel::Trace: WL_TRACE_TAG(tag, "{}", msg); break;
    case LogLevel::Info:  WL_INFO_TAG(tag,  "{}", msg); break;
    case LogLevel::Warn:  WL_WARN_TAG(tag,  "{}", msg); break;
    case LogLevel::Error: WL_ERROR_TAG(tag, "{}", msg); break;
    }
}
std::string PluginManager::GetPluginDataDir() const { fs::path d = m_DirPath / "data"; if (!fs::exists(d)) fs::create_directories(d); return d.string(); }
std::string PluginManager::GetExeDir() const { wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); return PathToUtf8(fs::path(p).parent_path()); }

int PluginManager::GetNodeGraphSnapshot(NgSnapshot& out)
{
    memset(&out, 0, sizeof(out));
    auto* editorGraph = m_NGM ? m_NGM->GetSelectedGraph() : nullptr;
    auto* evaluator = m_RobotStatus ? m_RobotStatus->GetGraphEvaluator() : nullptr;
    // m_NgSourceEvaluator: true = RobotStatus evaluator, false = editor graph
    auto* primary = m_NgSourceEvaluator ? evaluator : editorGraph;
    if (!primary) primary = m_NgSourceEvaluator ? editorGraph : evaluator; // fallback
    out.isRunning = (primary ? 1 : 0);
    out.nodeCount = primary ? primary->GetNodeCount() : 0;
    if (primary) for (auto& kv : primary->GetKeyValuesSnapshot()) { if (out.keyCount >= NG_SNAP_MAX_KEYS) break; strncpy_s(out.keyNames[out.keyCount], kv.first.c_str(), NG_SNAP_MAX_NAME-1); out.keyValues[out.keyCount] = kv.second; out.keyCount++; }
    auto cO = [&](NodeGraph* s) { if (!s) return; for (auto& kv : s->GetLastOutputsSnapshot()) { if (out.outputCount >= NG_SNAP_MAX_OUTPUTS) break; strncpy_s(out.outputPaths[out.outputCount], kv.first.c_str(), NG_SNAP_MAX_PATH-1); out.outputValues[out.outputCount]=kv.second; out.outputCount++; } };
    if (primary) cO(primary);
    auto cG = [&](NodeGraph* s) { if (!s) return; for (size_t i=0,n=s->GetGlobals().size();i<n;++i) { if (out.globalCount >= NG_SNAP_MAX_GLOBALS) break; const auto& gv=s->GetGlobals()[i]; strncpy_s(out.globalNames[out.globalCount],gv.name.c_str(),NG_SNAP_MAX_NAME-1); out.globalValues[out.globalCount]=gv.value; out.globalCount++; } };
    if (primary) cG(primary);
    return primary ? 1 : 0;
}
void* PluginManager::GetImGuiContext() const { return (void*)ImGui::GetCurrentContext(); }
void PluginManager::SetWindowOpen(const char* t, bool o) { std::lock_guard lock(m_Mx); for (auto& w : m_Wins) if (w.title == t) { w.open = o; return; } }
bool PluginManager::IsWindowOpen(const char* t) const { std::lock_guard lock(m_Mx); for (auto& w : m_Wins) if (w.title == t) return w.open; return false; }
std::vector<std::string> PluginManager::GetWindowTitles() const { std::lock_guard lock(m_Mx); std::vector<std::string> r; for (auto& w : m_Wins) r.push_back(w.title); return r; }

// ============================================================================
// HandleRequest / HandleNotification — process incoming JSON-RPC from plugins
// ============================================================================

Json PluginManager::HandleRequest(const std::string& method, const Json& params)
{
    try {
    if (method == "get_state")
    {
        // params: {"paths": ["actuators/motor1/target_speed", "sensors/temperature", ...]}
        // Returns: matching state snapshot
        Json paths = params.is_object() && params.contains("paths") ? params["paths"] : Json::array();

        // Build full state (same as StateProvider)
        Json full;
        full["actuators"] = Json::array();
        full["sensors"]   = Json::object();
        full["gamepad"]   = Json::object();
        full["node_graph"] = Json::object({{"variables", Json::object()}, {"outputs", Json::object()}});

        if (m_RobotStatus && m_RobotStatus->HasActiveMode())
        {
            auto cmd = m_RobotStatus->GetCurrentCommand(0);
            if (cmd) {
                for (auto& m : cmd->brushlessmotor) {
                    full["actuators"].push_back(Json::object({
                        {"type", "brushlessmotor"}, {"id", m.id}, {"name", m.name},
                        {"target_speed", m.target_speed.value}, {"target_position", m.target_position.value}
                    }));
                }
                for (auto& s : cmd->servo) {
                    full["actuators"].push_back(Json::object({
                        {"type", "servo"}, {"id", s.id}, {"name", s.name}, {"angle", s.angle.value}
                    }));
                }
                for (auto& mc : cmd->motions) {
                    full["actuators"].push_back(Json::object({
                        {"type", "motion"}, {"name", mc.name},
                        {"x", mc.x.value}, {"y", mc.y.value}, {"z", mc.z.value},
                        {"rx", mc.rx.value}, {"ry", mc.ry.value}, {"rz", mc.rz.value}
                    }));
                }
            }
        }
        if (m_RobotStatus && m_RobotStatus->IsSensorValid())
        {
            auto s = m_RobotStatus->GetCurrentSensor();
            full["sensors"]["temperature"] = s.temperature.value.value;
            full["sensors"]["humidity"]    = s.humidity.value.value;
            full["sensors"]["depth"]       = s.depth.value.value;
        }
        else
        {
            full["sensors"]["temperature"] = 0.0;
            full["sensors"]["humidity"]    = 0.0;
            full["sensors"]["depth"]       = 0.0;
        }
        NgSnapshot snap;
        if (GetNodeGraphSnapshot(snap) != 0)
        {
            for (int i = 0; i < snap.keyCount; i++)
                full["gamepad"][std::string(snap.keyNames[i])] = snap.keyValues[i];
            for (int i = 0; i < snap.globalCount; i++)
                full["node_graph"]["variables"][std::string(snap.globalNames[i])] = snap.globalValues[i];
            for (int i = 0; i < snap.outputCount; i++)
                full["node_graph"]["outputs"][std::string(snap.outputPaths[i])] = snap.outputValues[i];
        }

        // Filter to requested paths
        if (paths.is_array() && !paths.empty())
            return Plugin::FilterState(full, paths);
        return full;
    }
    else if (method == "subscribe")
    {
        // params: {"name": "plugin_name", "paths": [...], "interval_sec": 0.1}
        if (!params.is_object()) return "err: invalid params";
        std::string name = params.value("name", "");
        Json paths = params.contains("paths") ? params["paths"] : Json::array();
        double interval = params.value("interval_sec", 0.1);
        m_Plugin.Subscribe(name, paths, interval);
        return "ok";
    }
    else if (method == "unsubscribe")
    {
        if (!params.is_object()) return "err: invalid params";
        std::string name = params.value("name", "");
        m_Plugin.Unsubscribe(name);
        return "ok";
    }
    else if (method == "list_paths")
    {
        // Return only concrete (non-wildcard) variable paths
        Json paths = Json::array();

        // Dynamic: add actual actuator names and node graph names
        if (m_RobotStatus && m_RobotStatus->HasActiveMode()) {
            auto& act = m_RobotStatus->GetAppliedActuator();
            for (auto& m : act.brushlessmotor) {
                paths.push_back("actuators/" + m.name + "/target_speed");
                paths.push_back("actuators/" + m.name + "/target_position");
            }
            for (auto& s : act.servo)
                paths.push_back("actuators/" + s.name + "/angle");
        }
        NgSnapshot snap;
        if (GetNodeGraphSnapshot(snap) != 0) {
            for (int i = 0; i < snap.keyCount; i++)
                paths.push_back("gamepad/" + std::string(snap.keyNames[i]));
            for (int i = 0; i < snap.globalCount; i++)
                paths.push_back("node_graph/variables/" + std::string(snap.globalNames[i]));
            for (int i = 0; i < snap.outputCount; i++)
                paths.push_back("node_graph/outputs/" + std::string(snap.outputPaths[i]));
        }
        // Always include sensors (keys always present)
        paths.push_back("sensors/temperature");
        paths.push_back("sensors/humidity");
        paths.push_back("sensors/depth");
        return paths;
    }
    else if (method == "get_option")
    {
        if (!params.is_object()) return nullptr;
        std::string key = params.value("key", "");
        if (key == "ng_source")
            return m_NgSourceEvaluator ? "evaluator" : "editor";
        return nullptr;
    }
    else if (method == "set_option")
    {
        if (!params.is_object()) return nullptr;
        std::string key = params.value("key", "");
        std::string val = params.value("value", "");
        if (key == "ng_source")
        {
            m_NgSourceEvaluator = (val != "editor");
            return m_NgSourceEvaluator ? "evaluator" : "editor";
        }
        return nullptr;
    }
    else if (method == "refresh_plugins")
    {
        RefreshPlugins();
        return "ok";
    }
    else if (method == "request_disable")
    {
        // Plugin 请求自行停用（如 UI 窗口被用户关闭）
        if (params.is_object() && params.contains("name"))
            DisablePlugin(params["name"].get<std::string>());
        return "ok";
    }
    else
    {
        throw std::runtime_error("Unknown method: " + method);
    }
    } catch (const std::exception& ex) {
        WL_ERROR_TAG("PLUGIN", "HandleRequest exception: {}", ex.what());
        throw;
    } catch (...) {
        WL_ERROR_TAG("PLUGIN", "HandleRequest unknown exception");
        throw std::runtime_error("Internal error");
    }
}

void PluginManager::HandleNotification(const std::string& method, const Json& params)
{
    if (method == "log")
    {
        int level = params.value("level", 1);
        std::string msg = params.value("msg", params.dump());
        LogLevel lv = LogLevel::Info;
        if (level == 0) lv = LogLevel::Trace;
        else if (level == 2) lv = LogLevel::Warn;
        else if (level >= 3) lv = LogLevel::Error;
        Log(lv, "PLUGIN", msg.c_str());
    }
    else if (method == "request_disable")
    {
        std::string name = params.value("name", "");
        if (!name.empty())
        {
            WL_INFO_TAG("PLUGIN", "Plugin '{}' requested disable (UI closed)", name);
            DisablePlugin(name);
        }
    }
    else if (method == "progress")
    {
        std::string status = params.value("status", "");
        std::string msg    = params.value("msg", "");
        int total = params.value("total", 0);
        int done  = params.value("done", 0);

        m_ProgressMsg    = msg;
        m_ProgressStatus = status;
        m_ProgressTotal  = total;
        m_ProgressDone   = done;

        if (!msg.empty())
        {
            if (status == "error")      WL_ERROR_TAG("PLUGIN", "{}", msg);
            else if (status == "warn")  WL_WARN_TAG("PLUGIN",  "{}", msg);
            else                        WL_INFO_TAG("PLUGIN",  "[{}] {}", status, msg);
        }
    }
}
