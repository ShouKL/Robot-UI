#pragma once

#include "EditDraftBase.h"
#include "Robot_API/robot_api.h"
#include "Walnut/Core/Log.h"
#include <imgui.h>
#include <cctype>
#include <cstdint>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// ============================================================================
// RobotCommConfig — 通信节点配置
// ============================================================================

struct RobotCommConfig
{
    char name[64]          = "Default";
    char host_ip[64]       = "192.168.0.10";
    int  remote_port       = 8888;
    int  local_port        = 8888;
    int  transport_type    = 0;     // 0=UDP, 1=TCP, 2=Serial
    int  send_freq_hz      = 100;   // 发送频率（Hz）
    int  active_component_idx = 0;  // 每个 comm 节点独立的 component 选择

    // 独立的发送/接收协议字段（不再与 RobotComponent 共享）
    std::vector<ProtocolSendConfig>    protocol_send;
    std::vector<ProtocolReceiveConfig> protocol_receive;
};

// ============================================================================
// RobotComm — 通信面板 UI
// 协议字段编辑（发送/接收）、网络配置、帧预览
// ============================================================================

class RobotComponentManager;

class RobotComm : public EditDraftBase
{
public:
    // ---- 窗口控制 ----
    void Open()            { m_Open = true; m_TabIndex = 0; }
    void Close()           { m_Open = false; }
    bool IsOpen()    const { return m_Open; }

    // ---- UI 绘制 ----
    void DrawSendFieldConfig(ProtocolSendConfig& cfg, ActuatorConfig& actuator);
    void DrawReceiveFieldConfig(ProtocolReceiveConfig& cfg, const SensorConfig& sensor);
    void DrawControlPanel(RobotCommConfig& cfg,
                          RobotComponentManager* robotMgr);

private:
    bool m_Open     = false;
    int  m_TabIndex = 0;
    int  m_ActiveSendCfgIdx    = 0;  // 当前编辑的发送帧索引
    int  m_ActiveRecvCfgIdx    = 0;  // 当前编辑的接收帧索引
    int  m_EditingSendName     = -1; // 正在重命名的发送帧索引（-1=无）
    int  m_EditingRecvName     = -1; // 正在重命名的接收帧索引（-1=无）
    int  m_EditingSendField    = -1; // 正在双击改名的字段索引
    int  m_EditingRecvField    = -1; // 正在双击改名的字段索引
};

// ============================================================================
// CommNode — 纯数据（不含连接逻辑），用于序列化/传递
// ============================================================================
struct CommNode
{
    int              id          = 0;
    bool             isLinked = false;
    bool             isSelected  = false;
    RobotCommConfig  component;
};
