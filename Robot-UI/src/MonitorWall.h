#pragma once

#include "LiveStreamManager.h"

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

    // ---- Flip & Rotation (controlled from Option panel) ----
    bool  GetFlipH() const { return m_FlipH; }
    bool  GetFlipV() const { return m_FlipV; }
    float GetRotationAngle() const { return m_RotationAngle; }
    void  SetFlipH(bool v) { m_FlipH = v; }
    void  SetFlipV(bool v) { m_FlipV = v; }
    void  SetRotationAngle(float v) { m_RotationAngle = v; }

private:
    void StartStream();
    void StopStream();
    void UpdateStream();

    LiveStreamManager* m_LiveStreamMgr = nullptr;
    int  m_ActiveStreamIdx = -1;
    bool m_StreamRunning = false;

    // ---- Flip & Rotation ----
    bool m_FlipH = false;
    bool m_FlipV = false;
    float m_RotationAngle = 0.0f;   // degrees
};
