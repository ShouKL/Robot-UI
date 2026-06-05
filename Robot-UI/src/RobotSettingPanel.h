#pragma once

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include "EditDraftBase.h"
#include "GamepadMapperManager.h"
#include "LiveStreamManager.h"
#include "NodeGraphManager.h"
#include "RobotCommManager.h"
#include "RobotComponentManager.h"
#include "LiveStream.h"
#include "Robot_API/robot_api.h"
#include <memory>
#include <vector>

class RobotStatus;

// ============================================================================
// RobotSettingPanel — 机器人设置面板
// （Component + GamepadMapper + Live Streamer + Robot Comm）
// 继承 EditDraftBase，支持快照模式（BeginEdit/ApplyEdit/CancelEdit）
// 由 Edit > Robot Setting 菜单控制打开，标签页切换子面板
// ============================================================================

class RobotSettingPanel : public EditDraftBase
{
public:
    RobotSettingPanel();
    ~RobotSettingPanel() = default;

    // ---- 子组件访问 ----
    RobotComponentManager& GetRobotComponentManager() { return *m_RobotComponentManager; }
    GamepadMapperManager&  GetGamepadMapperManager()  { return *m_GamepadMapperManager; }
    NodeGraphManager&      GetNodeGraphManager()      { return *m_NodeGraphManager; }
    NodeGraph*             GetNodeGraph()              { return m_NodeGraphManager->GetSelectedGraph(); }
    LiveStreamManager*     GetLiveStreamManager()      { return m_LiveStreamManager.get(); }
    RobotCommManager*      GetRobotCommManager()       { return m_RobotCommManager.get(); }

    // ---- 标签页切换 ----
    void SelectTab(int tabId) { m_selected_id = tabId; }

    // ---- 调试：设置 RobotStatus 指针（由 Robot_UI 注入） ----
    void SetRobotStatus(RobotStatus* rs) { m_RobotStatus = rs; }

    // ---- 子窗口开关引用（供序列化使用） ----
    bool& GetLiveStreamerOpen()  { return m_LiveStreamerOpen; }
    bool& GetRobotCommOpen()     { return m_RobotCommOpen; }

    // ---- EditDraftBase 覆盖 ----
    void BeginEdit() override;
    void ApplyEdit() override;
    void CancelEdit() override;

    // ---- UI 渲染 ----
    void Draw(bool* p_open = nullptr);

private:
    void TakeSnapshots();
    void SaveRobotStatusActive();
    void RestoreRobotStatusActive();

    // 统一策略：根据当前打开的标签页，同步五个 Active 项到 RobotStatus
    void SyncActiveItems(int tabId);

    std::unique_ptr<RobotComponentManager> m_RobotComponentManager;
    std::unique_ptr<GamepadMapperManager>  m_GamepadMapperManager;
    std::unique_ptr<NodeGraphManager>      m_NodeGraphManager;
    std::unique_ptr<LiveStreamManager>     m_LiveStreamManager;
    std::unique_ptr<RobotCommManager>      m_RobotCommManager;

    int  m_selected_id = 0;  // 0=Component, 1=GamepadMapper, 2=NodeGraph, 3=LiveStream, 4=RobotComm

    RobotStatus* m_RobotStatus = nullptr;

    bool m_LiveStreamerOpen = true;
    bool m_RobotCommOpen    = true;

    // 快照
    std::vector<RobotMode>        m_ComponentSnapshot;
    std::vector<GamepadMapper>    m_GamepadSnapshot;
    std::vector<StreamConfig>     m_StreamSnapshot;
    std::vector<RobotCommConfig>  m_CommSnapshot;
    std::vector<GraphItem>        m_NodeGraphSnapshot;

    // 调试：追踪 GamepadMapper 选中项变化
    int m_LastGamepadIndex = -1;

    // 统一策略：追踪各 Manager 选中项变化，用于跨组件同步
    int m_LastComponentIdx  = -1;
    int m_LastNodeGraphIdx  = -1;
    int m_LastLiveStreamIdx  = -1;
    int m_LastCommIdx        = -1;

    // 保存的 RobotStatus active 状态（退出面板时恢复）
    // BASE 仅保存 Status 的两个选择，Component/Gamepad/Comm 均从 NodeGraph 推导
    int m_SavedLiveStreamIdx = 0;
    int m_SavedNodeGraphIdx  = 0;
};
