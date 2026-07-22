#include "Plugin.h"
#include "JsonRpc.h"
#include "Walnut/Core/Log.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// ============================================================================
// Helpers
// ============================================================================
static std::wstring W(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    w.resize(n - 1);
    return w;
}

static bool IsPy(const std::string& n) {
    std::string l = n;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    return l.size() >= 3 && l.substr(l.size() - 3) == ".py";
}

static bool IsExe(const std::string& n) {
    std::string l = n;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    return l.size() >= 4 && l.substr(l.size() - 4) == ".exe";
}

std::string Plugin::FindPython(const std::string& pluginDir) {
    if (!pluginDir.empty()) {
        fs::path embPy = fs::path(pluginDir) / "python" / "python.exe";
        if (fs::exists(embPy)) return fs::absolute(embPy).string();
    }
    return "python.exe";
}

// ============================================================================
Plugin::Plugin()  { m_Dir = "plugins"; }
Plugin::~Plugin() { Shutdown(); }

bool Plugin::Initialize() {
    if (m_Ok) return true;
    m_Python = FindPython();
    WL_INFO_TAG("PLUGIN", "Python: {}", m_Python);
    fs::path b = fs::path(m_Dir) / "plugin_bridge.py";
    if (!fs::exists(b)) { WL_ERROR_TAG("PLUGIN", "plugin_bridge.py not found"); return false; }
    m_Bridge = b.string();
    m_Ok = true;
    WL_INFO_TAG("PLUGIN", "Ready");
    return true;
}

void Plugin::Shutdown() { UnloadAll(); m_Ok = false; }

void Plugin::Reader(E* e, IHostServices* host) {
    std::vector<char> buf(65536); std::string p;
    while (e->running) {
        DWORD a = 0;
        if (!PeekNamedPipe(e->hOut, nullptr, 0, nullptr, &a, nullptr) || a == 0) {
            DWORD x; if (GetExitCodeProcess(e->hProc, &x) && x != STILL_ACTIVE) { e->running = false; break; }
            Sleep(10); continue;
        }
        DWORD r = 0;
        if (ReadFile(e->hOut, buf.data(), (std::min)(a, (DWORD)65535), &r, nullptr) && r) {
            buf[r] = '\0'; p += buf.data();
            size_t pos;
            while ((pos = p.find('\n')) != std::string::npos) {
                std::string line = p.substr(0, pos); p.erase(0, pos + 1);
                if (line.empty()) continue;

                // Fast string check: skip Python JSON-RPC responses (C++ Call() is fire-and-forget).
                // This avoids jsonrpcpp::Parser::do_parse() which asserts on certain response payloads.
                if (line.find("\"id\"") != std::string::npos &&
                    (line.find("\"result\"") != std::string::npos || line.find("\"error\"") != std::string::npos)) {
                    continue;
                }

                try {
                auto parsed = JsonRpc::Parse(line);
                switch (parsed.type)
                {
                case JsonRpc::ParsedMessage::Type::Request:
                    if (host) {
                        try {
                            Json result = host->HandleRequest(parsed.method, parsed.params);
                            EnqueueUrgentWrite(*e, JsonRpc::BuildResponse(parsed.id, result));
                        } catch (const std::exception& ex) {
                            EnqueueUrgentWrite(*e, JsonRpc::BuildError(parsed.id, -32603, ex.what()));
                        } catch (...) {
                            EnqueueUrgentWrite(*e, JsonRpc::BuildError(parsed.id, -32603, "Internal error"));
                        }
                    }
                    break;
                case JsonRpc::ParsedMessage::Type::Notification:
                    try {
                        // Intercept plugin_info: Python reports PLUGIN_NAME, update entry
                        // so Subscribe/Unsubscribe can match by either name.
                        if (parsed.method == "plugin_info" && parsed.params.is_object()) {
                            auto name = parsed.params.value("name", std::string(""));
                            if (!name.empty()) e->info.name = name;
                            e->info.version = parsed.params.value("version", e->info.version);
                            e->info.author  = parsed.params.value("author",  e->info.author);
                        }
                        if (host) { host->HandleNotification(parsed.method, parsed.params); }
                    } catch (const std::exception& ex) {
                        if (host) host->Log(IHostServices::LogLevel::Error, "PLUGIN", ex.what());
                    } catch (...) {
                        if (host) host->Log(IHostServices::LogLevel::Error, "PLUGIN", "Unknown notification error");
                    }
                    break;
                case JsonRpc::ParsedMessage::Type::Response:
                case JsonRpc::ParsedMessage::Type::Error:
                    // C++ Call() is fire-and-forget — ignore Python responses silently
                    break;
                default:
                    break;
                }
                } catch (...) {
                    if (host) host->Log(IHostServices::LogLevel::Warn, "PLUGIN", line.c_str());
                }
            }
        }
    }
}

void Plugin::StartReader(E& e, IHostServices* host) { e.rd = new std::thread(Reader, &e, host); }

// ============================================================================
// Async Writer — decouples pipe writes from game loop & reader thread
// Without this, WriteFile can block the reader thread when the pipe buffer
// is full (from on_update floods), causing RPC response starvation.
// ============================================================================
void Plugin::EnqueueWrite(E& e, std::string&& data) {
    if (!e.hIn || !e.running) return;
    std::lock_guard lk(*e.writeMtx);
    if (e.writeQueue.size() >= 128) {
        if (!e.writeQueue.empty()) e.writeQueue.erase(e.writeQueue.begin());
    }
    e.writeQueue.push_back(std::move(data));
}

void Plugin::EnqueueUrgentWrite(E& e, std::string&& data) {
    if (!e.hIn || !e.running) return;
    std::lock_guard lk(*e.writeMtx);
    e.urgentQueue.push_back(std::move(data));
}

void Plugin::WriterThread(E* e) {
    std::vector<std::string> urgent, normal;
    while (!e->writerStop) {
        {
            std::lock_guard lk(*e->writeMtx);
            urgent.swap(e->urgentQueue);
            normal.swap(e->writeQueue);
        }
        // RESPONSES first: urgent (RPC replies), then normal (on_update notifications)
        for (auto& data : urgent) {
            DWORD w = 0;
            WriteFile(e->hIn, data.c_str(), (DWORD)data.size(), &w, nullptr);
        }
        urgent.clear();
        for (auto& data : normal) {
            DWORD w = 0;
            WriteFile(e->hIn, data.c_str(), (DWORD)data.size(), &w, nullptr);
        }
        normal.clear();
        Sleep(1);
    }
    // Drain remaining
    {
        std::lock_guard lk(*e->writeMtx);
        for (auto& data : e->urgentQueue) {
            DWORD w = 0;
            WriteFile(e->hIn, data.c_str(), (DWORD)data.size(), &w, nullptr);
        }
        for (auto& data : e->writeQueue) {
            DWORD w = 0;
            WriteFile(e->hIn, data.c_str(), (DWORD)data.size(), &w, nullptr);
        }
        e->writeQueue.clear(); e->urgentQueue.clear();
    }
}

// ============================================================================
// RPC
// ============================================================================
bool Plugin::Call(E& e, const char* method, const Json& params) {
    if (!e.hIn || !e.running) return false;
    EnqueueWrite(e, JsonRpc::BuildRequest(e.nextId++, method, params));
    return true;
}

bool Plugin::Notify(E& e, const char* method, const Json& params) {
    if (!e.hIn || !e.running) return false;
    EnqueueWrite(e, JsonRpc::BuildNotification(method, params));
    return true;
}

void Plugin::SendResponse(E& e, int id, const Json& result) {
    EnqueueWrite(e, JsonRpc::BuildResponse(id, result));
}

void Plugin::SendError(E& e, int id, int code, const std::string& msg) {
    EnqueueWrite(e, JsonRpc::BuildError(id, code, msg));
}

// ============================================================================
// Scan & Load
// ============================================================================
void Plugin::Scan(IHostServices* host) {
    if (!m_Ok || !fs::exists(m_Dir)) return;
    std::lock_guard l(m_Mx);
    for (auto& de : fs::directory_iterator(m_Dir)) {
        std::string fp = de.path().string();
        std::string fn = de.path().filename().string();

        // ── v2: Folder-based plugin (has plugin.json) ──
        if (de.is_directory()) {
            if (fn == "__pycache__" || fn == "data" || fn == "sdk" || fn[0] == '.') continue;
            auto manifest = de.path() / "plugin.json";
            if (!fs::exists(manifest)) continue;
        } else {
            // ── v1: File-based plugin ──
            if (!de.is_regular_file()) continue;
            if (fn == "plugin_bridge.py" || fn == "README.md" || fn == "__init__.py" || fn == "index.json") continue;
            if (!IsPy(fn) && !IsExe(fn)) continue;
        }

        bool dup = false;
        for (auto& e : m_Entries) if (e.path == fp) { dup = true; break; }
        if (dup) continue;
        Load(fp, host);
    }
    WL_INFO_TAG("PLUGIN", "Scan: {} plugins", m_Entries.size());
}

bool Plugin::Load(const std::string& path, IHostServices* host) {
    fs::path pp(path);
    if (!fs::exists(pp)) { fs::path alt = fs::path(m_Dir) / path; if (fs::exists(alt)) pp = alt; }
    if (!fs::exists(pp)) { WL_ERROR_TAG("PLUGIN", "Not found: {}", path); return false; }

    std::lock_guard l(m_Mx);
    std::string name = pp.stem().string();
    std::string ext  = pp.extension().string();

    // ── v2: Read plugin.json for folder-based plugins ──
    bool isFolder = fs::is_directory(pp);
    if (isFolder) {
        auto manifestPath = pp / "plugin.json";
        if (fs::exists(manifestPath)) {
            std::ifstream mf(manifestPath);
            std::stringstream buf; buf << mf.rdbuf();
            try {
                auto m = Json::parse(buf.str());
                if (m.contains("name")) name = m["name"].get<std::string>();
                // "type": "native" → run .exe directly
                if (m.contains("type") && m["type"].get<std::string>() == "native") {
                    isFolder = false;
                    ext = ".exe";
                    pp = pp / m.value("main", "");
                }
            } catch (...) {
                name = pp.filename().string();
            }
        } else {
            name = pp.filename().string();
        }
    }

    // Build command — per-plugin Python for folder-based plugins
    std::string cmd;
    if (isFolder) {
        cmd = "\"" + FindPython(pp.string()) + "\" -u \"" + m_Bridge + "\" \"" + pp.string() + "\"";
    }
    else if (IsPy(ext))
        cmd = "\"" + FindPython(pp.parent_path().string()) + "\" -u \"" + m_Bridge + "\" \"" + pp.string() + "\"";
    else if (IsExe(ext))
        cmd = "\"" + pp.string() + "\"";
    else {
        WL_ERROR_TAG("PLUGIN", "Unknown type: '{}' (file: {})", ext, pp.filename().string());
        return false;
    }

    // Pipes — use 64KB buffer (VS Code style) to avoid stalling
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE r0 = nullptr, w0 = nullptr, r1 = nullptr, w1 = nullptr;
    if (!CreatePipe(&r0, &w0, &sa, 65536) || !CreatePipe(&r1, &w1, &sa, 65536)) return false;
    SetHandleInformation(w0, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(r1, HANDLE_FLAG_INHERIT, 0);
    // Note: do NOT set PIPE_NOWAIT on w0 — it causes WriteFile to silently
    // drop JSON-RPC responses, which breaks call_host() in the Python bridge.

    PROCESS_INFORMATION pi = {};
    STARTUPINFOW si = {}; si.cb = sizeof(si);
    si.hStdInput = r0; si.hStdOutput = w1;
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);  // 不重定向 stderr，Tkinter 需要原生控制台
    si.dwFlags = STARTF_USESTDHANDLES;

    if (!CreateProcessW(nullptr, W(cmd).data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WL_ERROR_TAG("PLUGIN", "Process failed: {} (err {})", name, GetLastError());
        CloseHandle(r0); CloseHandle(w0); CloseHandle(r1); CloseHandle(w1);
        return false;
    }
    CloseHandle(r0); CloseHandle(w1); CloseHandle(pi.hThread);

    // emplace_back to avoid unique_ptr move, keeping Reader pointer stable
    m_Entries.emplace_back();
    E& re = m_Entries.back();
    re.name = name; re.path = pp.string();
    re.hProc = pi.hProcess; re.hIn = w0; re.hOut = r1;
    re.info.name = name; re.info.version = "1.0.0"; re.info.author = "plugin";
    re.info.description = pp.filename().string();

    StartReader(re, host);
    re.writerStop = false;
    re.writer = new std::thread(WriterThread, &re);
    Call(re, "on_load");
    re.state = PluginState::Loaded;
    WL_INFO_TAG("PLUGIN", "Loaded: {}", name);
    return true;
}

// ============================================================================
// Unload / Enable / Disable
// ============================================================================
void Plugin::Unload(const std::string& name) {
    std::lock_guard l(m_Mx);
    for (auto it = m_Entries.begin(); it != m_Entries.end(); ++it) {
        if (it->name != name) continue;
        WL_INFO_TAG("PLUGIN", "Unloading: {}", name);
        Call(*it, "on_unload");
        it->running = false;
        it->writerStop = true;
        // 先关 pipe handles，解除 WriteFile/ReadFile 阻塞
        if (it->hIn)  { CloseHandle(it->hIn);  it->hIn  = nullptr; }
        if (it->hOut) { CloseHandle(it->hOut); it->hOut = nullptr; }
        if (it->writer) { it->writer->join(); delete it->writer; it->writer = nullptr; }
        if (it->rd) { it->rd->join(); delete it->rd; it->rd = nullptr; }
        if (it->hProc){ TerminateProcess(it->hProc, 0); CloseHandle(it->hProc); it->hProc = nullptr; }
        m_Entries.erase(it);
        return;
    }
}

void Plugin::UnloadAll() {
    std::lock_guard l(m_Mx);
    for (auto& e : m_Entries) {
        Call(e, "on_unload");
        e.running = false;
        e.writerStop = true;
        if (e.hIn)  { CloseHandle(e.hIn);  e.hIn  = nullptr; }
        if (e.hOut) { CloseHandle(e.hOut); e.hOut = nullptr; }
        if (e.writer) { e.writer->join(); delete e.writer; e.writer = nullptr; }
        if (e.rd) { e.rd->join(); delete e.rd; e.rd = nullptr; }
        if (e.hProc){ TerminateProcess(e.hProc, 0); CloseHandle(e.hProc); e.hProc = nullptr; }
    }
    m_Entries.clear();
}

void Plugin::Enable(const std::vector<std::string>& names) {
    std::lock_guard l(m_Mx);
    for (auto& n : names) {
        auto* e = Find(n);
        if (!e) for (auto& x : m_Entries) if (x.info.name == n) { e = &x; break; }
        if (e && e->state == PluginState::Loaded) {
            WL_INFO_TAG("PLUGIN", "Enabling: {}", n);
            Call(*e, "on_enable");
            e->state = PluginState::Active;
            WL_INFO_TAG("PLUGIN", "Enabled: {}", n);
        } else if (e && e->state == PluginState::Active) {
            WL_INFO_TAG("PLUGIN", "Already active: {}", n);
        } else if (!e) {
            WL_WARN_TAG("PLUGIN", "Enable failed, not found: {}", n);
        } else {
            WL_WARN_TAG("PLUGIN", "Enable failed, not loaded: {}", n);
        }
    }
}
void Plugin::Disable(const std::vector<std::string>& names) {
    std::lock_guard l(m_Mx);
    for (auto& n : names) {
        auto* e = Find(n);
        if (!e) for (auto& x : m_Entries) if (x.info.name == n) { e = &x; break; }
        if (e && e->state == PluginState::Active) {
            WL_INFO_TAG("PLUGIN", "Disabling: {}", n);
            Call(*e, "on_disable");
            e->state = PluginState::Loaded;
            e->subscribed = false;  // auto-unsubscribe on disable
            e->subPaths = Json::array();
            WL_INFO_TAG("PLUGIN", "Disabled: {}", n);
        } else if (e && e->state == PluginState::Loaded) {
            WL_INFO_TAG("PLUGIN", "Already disabled: {}", n);
        } else if (!e) {
            WL_WARN_TAG("PLUGIN", "Disable failed, not found: {}", n);
        }
    }
}

void Plugin::Subscribe(const std::string& name, const Json& paths, double intervalSec) {
    std::lock_guard l(m_Mx);
    auto* e = Find(name);
    // Also try matching by info.name (Python plugins report PLUGIN_NAME)
    if (!e) for (auto& x : m_Entries) if (x.info.name == name) { e = &x; break; }
    if (!e) return;
    e->subscribed = true;
    e->subPaths = paths;
    e->subIntervalSec = (intervalSec > 0.01) ? intervalSec : 0.1;
    e->subLastPush = 0.0;
    WL_INFO_TAG("PLUGIN", "Subscribe: {} (paths={}, interval={}s)", e->name, paths.size(), e->subIntervalSec);
}

void Plugin::Unsubscribe(const std::string& name) {
    std::lock_guard l(m_Mx);
    auto* e = Find(name);
    if (!e) for (auto& x : m_Entries) if (x.info.name == name) { e = &x; break; }
    if (!e) return;
    e->subscribed = false;
    e->subPaths = Json::array();
    WL_INFO_TAG("PLUGIN", "Unsubscribe: {}", e->name);
}

// ============================================================================
// Tick — only push state to subscribed plugins, or send minimal dt-only update
// ============================================================================
void Plugin::OnUpdate(float dt) {
    std::lock_guard l(m_Mx);

    Json hostState = m_StateProvider ? m_StateProvider(dt) : Json::object();

    for (auto& e : m_Entries) {
        if (e.state != PluginState::Active) continue;
        if (!e.running) { e.state = PluginState::Error; WL_ERROR_TAG("PLUGIN", "Crashed: {}", e.name);
            // Clean up handles so .venv files get unlocked
            e.writerStop = true;
            if (e.hIn)  { CloseHandle(e.hIn);  e.hIn  = nullptr; }
            if (e.hOut) { CloseHandle(e.hOut); e.hOut = nullptr; }
            if (e.hProc){ TerminateProcess(e.hProc, 0); CloseHandle(e.hProc); e.hProc = nullptr; }
            continue; }

        if (e.subscribed && !e.subPaths.empty()) {
            // Subscription mode: push state at configured interval
            e.subLastPush += dt;
            if (e.subLastPush >= e.subIntervalSec) {
                e.subLastPush = 0.0;
                Json filtered = FilterState(hostState, e.subPaths);
                filtered["dt"] = dt;
                Notify(e, "on_update", filtered);
            }
        }
        // Unsubscribed: no data — plugin must subscribe first
    }
}

// ── FilterState: reduce full state to only paths in subPaths ──
Json Plugin::FilterState(const Json& full, const Json& paths) {
    if (!paths.is_array() || paths.empty()) return full;

    Json out;
    for (auto& p : paths) {
        if (!p.is_string()) continue;
        std::string path = p.get<std::string>();

        // Path format: "actuators/motor1/target_speed" or "sensors/temperature"
        size_t s1 = path.find('/');
        if (s1 == std::string::npos) continue;
        std::string section = path.substr(0, s1);
        std::string rest = path.substr(s1 + 1);

        if (!full.contains(section)) continue;

        if (section == "sensors" || section == "node_graph") {
            if (!full[section].is_object()) continue;
            // Simple key lookup
            size_t s2 = rest.find('/');
            std::string sub = (s2 != std::string::npos) ? rest.substr(0, s2) : rest;
            std::string key = (s2 != std::string::npos) ? rest.substr(s2 + 1) : "";
            if (key.empty()) {
                if (!full[section].contains(sub)) continue;
                if (!out.contains(section)) out[section] = Json::object();
                out[section][sub] = full[section][sub];
            } else {
                if (!full[section].contains(sub) || !full[section][sub].is_object() || !full[section][sub].contains(key)) continue;
                if (!out.contains(section)) out[section] = Json::object();
                if (!out[section].contains(sub)) out[section][sub] = Json::object();
                out[section][sub][key] = full[section][sub][key];
            }
        } else if (section == "actuators") {
            // Array: find by name
            if (!out.contains("actuators")) out["actuators"] = Json::array();
            for (auto& act : full["actuators"]) {
                if (act.contains("name") && rest.find(act["name"].get<std::string>()) == 0) {
                    out["actuators"].push_back(act);
                }
            }
        } else if (section == "gamepad") {
            // Flat key-value (keyValues from node graph input)
            if (!full["gamepad"].is_object()) continue;
            if (!out.contains("gamepad")) out["gamepad"] = Json::object();
            if (full["gamepad"].contains(rest))
                out["gamepad"][rest] = full["gamepad"][rest];
        }
    }
    return out;
}

void Plugin::OnUIRender() {
    std::lock_guard l(m_Mx);
    for (auto& e : m_Entries) {
        if (e.state != PluginState::Active || !e.running) continue;
        e.uiCmds.clear(); e.uiDone = false;
        Call(e, "on_ui_render");
        e.uiDone = true;
    }
}

void Plugin::OnMenuBar() {}

// ============================================================================
// Queries
// ============================================================================
Plugin::E* Plugin::Find(const std::string& n) { for (auto& e : m_Entries) if (e.name == n) return &e; return nullptr; }
const Plugin::E* Plugin::Find(const std::string& n) const { for (auto& e : m_Entries) if (e.name == n) return &e; return nullptr; }
bool Plugin::Loaded(const std::string& n) const { auto* e = Find(n); return e && e->state != PluginState::NotLoaded; }
bool Plugin::Enabled(const std::string& n) const { auto* e = Find(n); return e && e->state == PluginState::Active; }
PluginState Plugin::State(const std::string& n) const { auto* e = Find(n); return e ? e->state : PluginState::NotLoaded; }
const PluginInfo* Plugin::Info(const std::string& n) const { auto* e = Find(n); return e ? &e->info : nullptr; }
size_t Plugin::Count() const { std::lock_guard l(m_Mx); return m_Entries.size(); }
std::vector<std::string> Plugin::Names() const { std::lock_guard l(m_Mx); std::vector<std::string> r; for (auto& e : m_Entries) r.push_back(e.name); return r; }
std::vector<std::string> Plugin::EnabledNames() const { std::lock_guard l(m_Mx); std::vector<std::string> r; for (auto& e : m_Entries) if (e.state == PluginState::Active) r.push_back(e.name); return r; }
