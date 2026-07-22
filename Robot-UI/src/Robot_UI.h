#pragma once

#include "core.h"
#include "Walnut/Application.h"
#include "Walnut/Layer.h"
#include "OptionPanel.h"
#include "ConfigSerializer.h"
#include "FileManager.h"
#include "ThrustCurveEditor.h"
#include "RobotSettingPanel.h"
#include "RobotStatus.h"
#include "TerminalPanel.h"
#include "MonitorWall.h"
#include "ShortcutManager.h"
#include "Robot_API/robot_api.h"
#include "plugin/PluginManager.h"
#include "plugin/PluginPanel.h"

class Robot_UI_Layer : public Walnut::Layer
{
public:
    Robot_UI_Layer();
    ~Robot_UI_Layer();

    // ---- Walnut 生命周期 ----
    virtual void OnUIRender() override;

    // ---- 窗口显示控制 ----
    void ShowOption()            { m_OptionOpen = true; }
    void ShowRobotSetting()      { m_RobotSettingOpen = true; }
    void ShowAbout()             { m_AboutOpen = true; }
    void ShowRobotStatus()       { m_RobotStatusOpen = true; }
    void ShowThrustCurveEditor();
    void ShowTerminal()          { m_TerminalOpen     = true; }

    // ---- 窗口状态访问 ----
    bool& GetShowRobotStatus()   { return m_RobotStatusOpen; }
    bool& GetShowTerminal()      { return m_TerminalOpen; }
    bool& GetShowMonitorWall()   { return m_MonitorWallOpen; }
    bool& GetShowPluginManager() { return m_PluginManagerOpen; }
    TerminalPanel*     GetTerminalPanel()     { return m_TerminalPanel.get(); }
    PluginManager*     GetPluginManager()     { return m_PluginMgr.get(); }

    // ---- 文件管理器访问 ----
    FileManager* GetFileManager() { return m_FileManager.get(); }

    // ---- 快捷键管理 ----
    ShortcutManager& GetShortcutManager() { return m_ShortcutManager; }

    // ---- 文件操作（供菜单调用） ----
    void FileNew();
    void FileOpen();
    void FileSave();
    void FileSaveAs();
    void LoadRobotFile(const std::string& path);   // 菜单中直接加载最近文件
    void TakeScreenshot();                           // 截图功能

private:
    void SaveRobotFile(const std::string& path);
    void LoadKernelFile(const std::string& path);
    void SaveKernelFile(const std::string& path);
    void ApplyUIState(const UIState& st);
    void GamepadRoutine();

    // 窗口开关状态
    bool m_AboutOpen;
    bool m_OptionOpen;
    bool m_RobotSettingOpen;
    bool m_RobotStatusOpen;
    bool m_MonitorWallOpen        = false;
    bool m_ThrustCurveEditorOpen  = false;
    bool m_TerminalOpen           = false;   // shared: Terminal + Output in one panel
    bool m_PluginManagerOpen      = false;   // Plugin Manager window

    // 子系统
    std::unique_ptr<OptionPanel>       m_OptionPanel;
    std::unique_ptr<RobotSettingPanel> m_RobotSettingPanel;
    std::unique_ptr<ThrustCurveEditor> m_ThrustCurveEditor;
    std::unique_ptr<RobotStatus>       m_RobotStatus;
    std::unique_ptr<MonitorWall>       m_MonitorWall;
    std::unique_ptr<FileManager>       m_FileManager;
    std::unique_ptr<TerminalPanel>     m_TerminalPanel;
    std::unique_ptr<PluginManager>     m_PluginMgr;
    std::unique_ptr<PluginPanel>       m_PluginPanel;
    ShortcutManager                     m_ShortcutManager;

    bool m_NeedNodeGraphWarmup = false; // init 时标记，OnUIRender 预热首帧

    // 手柄线程
    std::thread m_GamepadThread;
    std::atomic<bool> m_Running{false};
    std::atomic<std::shared_ptr<const ActuatorConfig>> m_CurrentCommand;
};