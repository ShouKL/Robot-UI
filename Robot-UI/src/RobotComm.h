#pragma once
#include "EditDraftBase.h"
#include "Robot_API/robot_api.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <functional>

#include "RobotComponent.h"
#include "Walnut/Core/Log.h"
#include <cstdio>

struct RobotCommConfig {
    char name[64] = "Default";
    char host_ip[64] = "192.168.0.10";
    int  remote_port = 8888;
    int  local_port  = 8888;
    int  transport_type = 0;  // 0=UDP, 1=TCP, 2=Serial
};

class RobotComm : public EditDraftBase {
public:
    void Open()     { m_Open = true; m_TabIndex = 0; }
    void Close()    { m_Open = false; }
    bool IsOpen() const { return m_Open; }

    // 主绘制入口
    void DrawWindow(ProtocolSendConfig& sendCfg, ProtocolReceiveConfig& recvCfg,
                    ActuatorData& actuator, bool hasTemp, bool hasHum, bool hasDep);

    // 分项绘制
    void DrawSendFieldConfig(ProtocolSendConfig& cfg, ActuatorData& actuator);
    void DrawReceiveFieldConfig(ProtocolReceiveConfig& cfg, bool hasTemp, bool hasHum, bool hasDep);

    // 控制面板（Robot Mode / Network / Transport / Connect 按钮）
    void DrawControlPanel(RobotCommConfig& cfg, bool isConnected, int nodeId,
                          RobotComponent* robotCfg,
                          std::function<void(int)> onConnect,
                          std::function<void()>   onDisconnect,
                          std::function<void(int)> onActiveModeChanged = {});

private:
    bool m_Open = false;
    int  m_TabIndex = 0;
};
