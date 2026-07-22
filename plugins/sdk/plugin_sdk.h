// plugin_sdk.h — Robot-UI C/C++ Plugin SDK (header-only)
// =========================================================
// Include this single header in your C/C++ plugin .cpp file.
// The plugin runs as a standalone .exe spawned by Robot-UI.
// Communication is via stdin/stdout JSON-RPC.
//
// Build (MSVC):
//   cl /EHsc /O2 /Fe:hello.exe hello.cpp /link /SUBSYSTEM:CONSOLE
//
// Build (MinGW):
//   g++ -O2 -o hello.exe hello.cpp -static
//
// Plugin example (hello.cpp):
//   #include "plugin_sdk.h"
//   PLUGIN_INFO("HelloCpp", "1.0.0", "dev")
//   PLUGIN_FUNC(on_load)  { LOG_INFO("Hello from C++!"); return 0; }
//   PLUGIN_FUNC(on_enable){ return 0; }
//   // ... etc
//   REGISTER_PLUGIN()  // must be last line
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <functional>
#include <iostream>
#include <sstream>
#include <cstring>

// ======================== JSON helpers ========================
namespace plugin {

inline std::string json_escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:   r += c;
        }
    }
    return r;
}

inline void send(const char* method, const std::string& params = "{}") {
    std::string msg = "{\"jsonrpc\":\"2.0\",\"method\":\"";
    msg += method;
    msg += "\",\"params\":";
    msg += params;
    msg += "}\n";
    std::cout << msg << std::flush;
}

inline void send_result(int id, const std::string& result = "true") {
    std::string msg = "{\"jsonrpc\":\"2.0\",\"result\":";
    msg += result;
    msg += ",\"id\":";
    msg += std::to_string(id);
    msg += "}\n";
    std::cout << msg << std::flush;
}

inline void send_error(int id, int code, const std::string& msg) {
    std::cout << "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":" << code
              << ",\"message\":\"" << json_escape(msg) << "\"},\"id\":" << id << "}\n" << std::flush;
}

#define LOG_TRACE(msg) plugin::send("log", "{\"level\":0,\"msg\":\"" + plugin::json_escape(msg) + "\"}")
#define LOG_INFO(msg)  plugin::send("log", "{\"level\":1,\"msg\":\"" + plugin::json_escape(msg) + "\"}")
#define LOG_WARN(msg)  plugin::send("log", "{\"level\":2,\"msg\":\"" + plugin::json_escape(msg) + "\"}")
#define LOG_ERROR(msg) plugin::send("log", "{\"level\":3,\"msg\":\"" + plugin::json_escape(msg) + "\"}")

// Parse simple JSON-RPC request (只解析 method + id, 不引入完整 JSON 解析器)
inline bool parse_request(const std::string& line, std::string& method, int& id) {
    method.clear(); id = 0;
    auto mpos = line.find("\"method\":\"");
    if (mpos == std::string::npos) return false;
    mpos += 10;
    auto mend = line.find("\"", mpos);
    if (mend == std::string::npos) return false;
    method = line.substr(mpos, mend - mpos);

    auto ipos = line.find("\"id\":");
    if (ipos != std::string::npos) {
        ipos += 5;
        id = 0;
        while (ipos < line.size() && line[ipos] >= '0' && line[ipos] <= '9') {
            id = id * 10 + (line[ipos] - '0');
            ipos++;
        }
    }
    return true;
}

// Parse params value for a specific key
inline std::string parse_param(const std::string& line, const std::string& key) {
    std::string q = "\"" + key + "\":";
    auto pos = line.find(q);
    if (pos == std::string::npos) return "";
    pos += q.size();
    while (pos < line.size() && line[pos] == ' ') pos++;
    if (pos >= line.size()) return "";
    if (line[pos] == '"') {
        pos++;
        auto end = line.find("\"", pos);
        if (end == std::string::npos) return "";
        return line.substr(pos, end - pos);
    }
    // Number or bool
    auto end = line.find_first_of(",}", pos);
    if (end == std::string::npos) return "";
    return line.substr(pos, end - pos);
}

} // namespace plugin

// ======================== Macro API ========================
#define PLUGIN_INFO(name, ver, author) \
    const char* PLUGIN_NAME = name; \
    const char* PLUGIN_VERSION = ver; \
    const char* PLUGIN_AUTHOR = author;

// ---- Declare lifecycle stubs (user must implement at least on_load) ----
#ifndef PLUGIN_DECLARE
#define PLUGIN_DECLARE() \
    int on_load(); \
    int on_enable(); \
    int on_disable(); \
    int on_unload(); \
    int on_update(); \
    int on_ui_render(); \
    int on_menu_bar();
#endif

// Plugin dispatch table
struct PluginEntry { const char* name; int (*fn)(); };

#define REGISTER_PLUGIN() \
    PLUGIN_DECLARE() \
    PluginEntry g_plugin_table[] = { \
        {"on_load",      on_load}, \
        {"on_enable",    on_enable}, \
        {"on_disable",   on_disable}, \
        {"on_unload",    on_unload}, \
        {"on_update",    on_update}, \
        {"on_ui_render", on_ui_render}, \
        {"on_menu_bar",  on_menu_bar}, \
        {nullptr, nullptr} \
    }; \
    int main() { \
        using namespace plugin; \
        send("plugin_info", "{\"name\":\"" + json_escape(PLUGIN_NAME) + \
               "\",\"version\":\"" + json_escape(PLUGIN_VERSION) + \
               "\",\"author\":\"" + json_escape(PLUGIN_AUTHOR) + "\"}"); \
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE); \
        char buf[65536]; std::string pending; \
        for (;;) { \
            DWORD avail = 0; \
            if (!PeekNamedPipe(hIn, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) { \
                Sleep(1); continue; \
            } \
            DWORD r = 0; \
            DWORD toRead = (avail < (DWORD)(sizeof(buf) - 1)) ? avail : (DWORD)(sizeof(buf) - 1); \
            if (ReadFile(hIn, buf, toRead, &r, nullptr) && r > 0) { \
                buf[r] = '\\0'; pending += buf; \
                size_t pos; \
                while ((pos = pending.find('\\n')) != std::string::npos) { \
                    std::string line = pending.substr(0, pos); \
                    pending.erase(0, pos + 1); \
                    if (line.empty()) continue; \
                    std::string method; int id = 0; \
                    if (!parse_request(line, method, id)) { send_error(id, -32700, "Parse error"); continue; } \
                    bool handled = false; \
                    for (auto* p = g_plugin_table; p->name; p++) { \
                        if (method == p->name && p->fn) { \
                            int rc = p->fn(); \
                            send_result(id, rc == 0 ? "true" : "false"); \
                            handled = true; break; \
                        } \
                    } \
                    if (!handled) send_error(id, -32601, "Method not found: " + method); \
                } \
            } \
        } \
        return 0; \
    }
