#pragma once

class LiveStreamManager;

#include "imgui.h"
#include <vector>
#include <memory>

// ============================================================================
// MonitorWall — 监控墙窗口
// 当 RobotStatus 点击 Connect 时打开，Disconnect 时关闭
// 仅展示 RobotStatus 中当前选中的那一个 LiveStream
// ============================================================================

class MonitorWall
{
public:
    MonitorWall();
    ~MonitorWall();

    void SetLiveStreamManager(LiveStreamManager* mgr) { m_LiveStreamMgr = mgr; }
    void SetActiveStreamIndex(int idx) { m_ActiveStreamIdx = idx; }

    void Draw(bool* p_open);

    // ---- 由外部（RobotStatus Connect/Disconnect）控制流开关 ----
    void ConnectStream();
    void DisconnectStream();

    // ---- 由外部注入索引，仅设置显示哪个流，不自动连接 ----
    void SetStreamIndex(int idx) { m_ActiveStreamIdx = idx; }

private:
    void StartStream();
    void StopStream();
    void UpdateStream();

    LiveStreamManager* m_LiveStreamMgr = nullptr;
    int  m_ActiveStreamIdx = -1;
    bool m_StreamRunning = false;
};
