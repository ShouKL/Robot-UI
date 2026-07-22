#pragma once
#include "Plugin.h"
#include "../core.h"
#include <vector>
#include <mutex>
#include <filesystem>
class RobotStatus;
// ============================================================================
// PluginManager — 统一管理子进程插件（.py + .exe）
// 每个插件是独立进程，通过 stdin/stdout JSON-RPC 通信。
// 插件崩溃不影响宿主。
// ============================================================================
class PluginManager : public IHostServices
{
public:
    PluginManager();
    ~PluginManager() = default;
    void SetPluginDirectory(const std::string& dir);
    const std::string& GetPluginDirectory() const { return m_Dir; }
    void ScanPlugins();
    void RefreshPlugins();
    bool LoadPlugin(const std::string& file);
    void UnloadPlugin(const std::string& name);
    void EnablePlugin(const std::string& name);
    void DisablePlugin(const std::string& name);
    void UnloadAll();
    bool IsPluginLoaded(const std::string& name) const;
    bool IsPluginEnabled(const std::string& name) const;
    PluginState GetPluginState(const std::string& name) const;
    const PluginInfo* GetPluginInfo(const std::string& name) const;
    size_t GetPluginCount() const;
    std::vector<std::string> GetPluginNames() const;
    std::vector<std::string> GetEnabledPluginNames() const;
    void EnablePlugins(const std::vector<std::string>& names);
    void SetNodeGraphManager(NodeGraphManager* m);
    void SetRobotCommManager(RobotCommManager* m);
    void SetRobotComponentManager(RobotComponentManager* m);
    void SetGamepadMapperManager(GamepadMapperManager* m);
    void SetLiveStreamManager(LiveStreamManager* m);
    void SetRobotStatus(RobotStatus* rs);
    void OnUpdate(float dt);
    void OnUIRender();
    void OnMenuBar();
    // IHostServices
    NodeGraphManager*       GetNodeGraphManager()       override { return m_NGM; }
    RobotCommManager*       GetRobotCommManager()       override { return m_RCM; }
    RobotComponentManager*  GetRobotComponentManager()  override { return m_CPM; }
    GamepadMapperManager*   GetGamepadMapperManager()   override { return m_GPM; }
    LiveStreamManager*      GetLiveStreamManager()      override { return m_LSM; }
    void RegisterMenuItem(const char* path, std::function<void()> cb) override;
    void RegisterWindow(const char* title, std::function<void(bool*)> render) override;
    void UnregisterWindow(const char* title) override;
    void Log(LogLevel lv, const char* tag, const char* msg) override;
    std::string GetPluginDataDir() const override;
    std::string GetExeDir() const override;
    int GetNodeGraphSnapshot(NgSnapshot& out) override;
    void* GetImGuiContext() const override;
    Json HandleRequest(const std::string& method, const Json& params) override;
    void HandleNotification(const std::string& method, const Json& params) override;
    void SetWindowOpen(const char* title, bool open);
    bool IsWindowOpen(const char* title) const;
    std::vector<std::string> GetWindowTitles() const;

    // Progress (updated by plugin progress notifications)
    std::string m_ProgressMsg;
    std::string m_ProgressStatus;
    int         m_ProgressTotal = 0;
    int         m_ProgressDone  = 0;
private:
    struct MI { std::string path, owner; std::function<void()> cb; };
    std::vector<MI> m_Menu;
    struct WI { std::string title, owner; std::function<void(bool*)> render; bool open = true; };
    std::vector<WI> m_Wins;
    NodeGraphManager*      m_NGM = nullptr;
    RobotCommManager*      m_RCM = nullptr;
    RobotComponentManager* m_CPM = nullptr;
    GamepadMapperManager*  m_GPM = nullptr;
    LiveStreamManager*     m_LSM = nullptr;
    RobotStatus*           m_RobotStatus = nullptr;
    bool                   m_NgSourceEvaluator = true;  // true=RobotStatus evaluator, false=editor graph
    std::string m_Dir;
    std::filesystem::path m_DirPath;
    mutable std::recursive_mutex m_Mx;
    Plugin m_Plugin;
};
