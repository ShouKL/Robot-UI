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
#include "LiveStream.h"
#include "StreamManager.h"
#include "ConfigSerializer.h"
#include "NodeEditor.h"
#include "ThrustCurveEditor.h"
#include "RobotCommManager.h"
#include "RobotStatus.h"
#include "Robot_API/robot_api.h"
#include "imgui.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

// ============================================================================
// Robot_UI_Layer — 主入口层（Walnut Layer）
// 协调所有子系统，管理菜单栏和窗口生命周期
// ============================================================================

class Robot_UI_Layer : public Walnut::Layer
{
public:
    Robot_UI_Layer();
    ~Robot_UI_Layer();

    // ---- Walnut 生命周期 ----
    virtual void OnUIRender() override;

    // ---- 窗口显示控制 ----
    void ShowOption()            { m_OptionOpen = true; }
    void ShowAbout()             { m_AboutOpen = true; }
    void ShowRobotStatus()       { m_RobotStatusOpen = true; }
    void ShowNodeEditor();
    void ShowThrustCurveEditor();
    void ShowRobotCommConfig();

    // ---- 窗口状态访问 ----
    bool& GetLiveStreamerOpen()  { return m_LiveStreamerOpen; }
    bool& GetShowRobotComm()     { return m_RobotCommOpen; }
    bool& GetShowRobotStatus()   { return m_RobotStatusOpen; }

    // ---- 文件操作 ----
    void FileOpen();
    void FileSave();
    void FileSaveAs();

private:
    void LoadConfigFile(const std::string& path);
    void ApplyUIState(const UIState& st);
    void GamepadRoutine();

    // 窗口开关状态
    bool m_AboutOpen;
    bool m_OptionOpen;
    bool m_LiveStreamerOpen;
    bool m_RobotStatusOpen;
    bool m_RobotCommOpen            = true;
    bool m_NodeEditorOpen;
    bool m_ThrustCurveEditorOpen    = false;

    // 子系统
    std::unique_ptr<OptionPanel>      m_OptionPanel;
    std::unique_ptr<StreamManager>    m_StreamManager;
    std::unique_ptr<NodeEditor>       m_NodeEditor;
    std::unique_ptr<ThrustCurveEditor> m_ThrustCurveEditor;
    std::unique_ptr<RobotCommManager> m_RobotCommManager;
    std::unique_ptr<RobotStatus>      m_RobotStatus;

    // 文件状态
    std::string m_CurrentSavePath;

    // 手柄线程
    std::thread m_GamepadThread;
    std::atomic<bool> m_Running;
    std::atomic<std::shared_ptr<const ActuatorConfig>> m_CurrentCommand;
};