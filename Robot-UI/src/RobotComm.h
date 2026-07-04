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
// RobotComm — 通信节点（数据 + UI 编辑状态）
// 协议字段编辑（发送/接收）、网络配置、帧预览
// 绘制已迁移到 RobotCommManager
// ============================================================================

class RobotComponentManager;

class RobotComm : public EditDraftBase
{
public:
    // ---- Item identity (from CommNode) ----
    int  id          = 0;
    bool isLinked    = false;
    bool isSelected  = false;

    // ---- Config (from RobotCommConfig + old RobotComm) ----
    char name[64]          = "Default";
    char host_ip[64]       = "192.168.0.10";
    int  remote_port       = 8888;
    int  local_port        = 8888;
    int  transport_type    = 0;     // 0=UDP, 1=TCP, 2=Serial
    int  send_freq_hz      = 100;   // 发送频率（Hz）
    int  retry_count       = 6;     // 连接重试总次数
    int  active_component_idx = 0;  // 每个 comm 节点独立的 component 选择

    // Serial-specific config
    char com_port_str[16]  = "COM1";
    int  baud_rate         = 115200;
    int  data_bits         = 3;     // index: 0=5,1=6,2=7,3=8
    int  stop_bits         = 0;     // index: 0=1,1=1.5,2=2
    int  parity            = 0;     // index: 0=None,1=Odd,2=Even,3=Mark,4=Space

    // 独立的发送/接收协议字段（不再与 RobotComponent 共享）
    std::vector<ProtocolSendConfig>    protocol_send;
    std::vector<ProtocolReceiveConfig> protocol_receive;

    // ---- 窗口控制 ----
    void Open()            { m_Open = true; m_TabIndex = 0; }
    void Close()           { m_Open = false; }
    bool IsOpen()    const { return m_Open; }

    friend class RobotCommManager;

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
