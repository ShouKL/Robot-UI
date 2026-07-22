#pragma once

#include "PluginManager.h"
#include "../core.h"
#include <functional>
#include <chrono>
#include <vector>
#include <filesystem>

class PluginManager;

// ============================================================================
// PluginPanel - Plugin manager with left list + right detail
//
// Maintains its own discovery list (scan folders, NOT auto-load).
// State machine:
//   Not Installed -> Download (create .venv + pip install)
//   Ready         -> Activate (load+enable via PluginManager)
//   Active        -> Deactivate (disable+unload via PluginManager)
//   Ready/Active  -> Uninstall (delete .venv, stays in list)
// ============================================================================

class PluginPanel
{
public:
    PluginPanel();
    ~PluginPanel() = default;

    void SetPluginManager(PluginManager* mgr) { m_PluginMgr = mgr; }
    void SetPluginsDir(const std::string& dir) { m_PluginsDir = dir; ScanPluginsDir(); }
    void Draw(bool* p_open = nullptr);

private:
    enum class PanelState {
        NotInstalled,  // no .venv, not loaded
        Ready,         // has .venv, not loaded
        Active,        // loaded + enabled (via PluginManager)
        Downloading    // install in progress
    };

    struct Entry {
        std::string   name;
        std::string   folderPath;
        std::string   version;
        std::string   author;
        std::string   description;
        std::string   website;
        std::vector<std::string> dependencies;
        bool          hasVenv = false;
        bool          isNative = false;       // "type": "native" — precompiled .exe, no Python needed
        std::string   nativeExe;             // "main" field for native plugins

        // per-entry download state
        bool          downloading = false;
        std::string   dlStatus;
        std::string   dlOutput;     // captured stdout from pip
        int           dlOutputLines = 0;
        std::chrono::steady_clock::time_point dlStartTime;
        void*         dlProcess = nullptr;   // HANDLE to cmd.exe
        void*         dlPipeRead = nullptr;  // HANDLE to stdout pipe
        int           dlPhase = 0;           // 0=running, 1=done-hold
        std::chrono::steady_clock::time_point dlPhaseTime;
    };

    void ScanPluginsDir();
    PanelState ComputeState(const Entry& e) const;
    static const char* StateToString(PanelState s);
    static ImVec4 StateToColor(PanelState s);

    void DrawPluginList();
    void DrawPluginDetail();

    void StartDownload(int index);
    void TickDownloads();
    void RemoveVenv(const std::string& folderPath);

    PluginManager* m_PluginMgr = nullptr;
    std::string    m_PluginsDir;
    std::vector<Entry> m_Entries;
    int            m_SelectedIndex = -1;
    char           m_SearchBuf[128] = {};

    // ── Confirm dialog ──
    bool        m_ConfirmOpen = false;
    std::string m_ConfirmTitle;
    std::string m_ConfirmMsg;
    std::function<void()> m_ConfirmAction;
};
