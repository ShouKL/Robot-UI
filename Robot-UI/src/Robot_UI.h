#pragma once

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include "Walnut/Application.h"
#include "Walnut/Layer.h"
#include "OptionPanel.h"
#include "ConfigSerializer.h"
#include "FileManager.h"
#include "ThrustCurveEditor.h"
#include "RobotSettingPanel.h"
#include "RobotStatus.h"
#include "TerminalPanel.h"
#include "Robot_API/robot_api.h"
#include "imgui.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

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
    TerminalPanel*     GetTerminalPanel()     { return m_TerminalPanel.get(); }

    // ---- 文件管理器访问 ----
    FileManager* GetFileManager() { return m_FileManager.get(); }

    // ---- 文件操作（供菜单调用） ----
    void FileOpen();
    void FileSave();
    void FileSaveAs();
    void LoadRobotFile(const std::string& path);   // 菜单中直接加载最近文件

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
    bool m_ThrustCurveEditorOpen    = false;
    bool m_TerminalOpen             = false;   // shared: Terminal + Output in one panel

    // 子系统
    std::unique_ptr<OptionPanel>       m_OptionPanel;
    std::unique_ptr<RobotSettingPanel> m_RobotSettingPanel;
    std::unique_ptr<ThrustCurveEditor> m_ThrustCurveEditor;
    std::unique_ptr<RobotStatus>       m_RobotStatus;
    std::unique_ptr<FileManager>       m_FileManager;
    std::unique_ptr<TerminalPanel>     m_TerminalPanel;

    // 手柄线程
    std::thread m_GamepadThread;
    std::atomic<bool> m_Running;
    std::atomic<std::shared_ptr<const ActuatorConfig>> m_CurrentCommand;
};