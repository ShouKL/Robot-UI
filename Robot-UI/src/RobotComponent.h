#pragma once

#include "GamepadMapper.h"
#include "Robot_API/robot_api.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// RobotComponent — 机器人模式配置编辑器
// 持有 modes 向量，是所有机器人配置的数据源
// ============================================================================

class RobotComponent
{
public:
    RobotComponent();
    ~RobotComponent();

    // ---- 模式管理 ----
    void AddMode();
    void DeleteMode(int index);
    void NextActiveMode() {
        if (!modes.empty())
            active_mode_index = (active_mode_index + 1) % modes.size();
    }

    // ---- 模式数据访问 ----
    std::vector<RobotMode>&       GetModes()          { return modes; }
    const std::vector<RobotMode>& GetModes()          const { return modes; }
    std::vector<RobotMode>&       GetActiveModes()    { return modes; }

    int  GetActiveModeIndex() const { return active_mode_index; }
    void SetActiveModeIndex(int idx) {
        if (idx >= 0 && idx < (int)modes.size())
            active_mode_index = idx;
    }
    int& GetEditModeIndex()       { return edit_mode_index; }
    int  GetEditModeIndex() const { return edit_mode_index; }

    // ---- 活跃模式配置快捷访问 ----
    ActuatorConfig GetActiveActuatorConfig() {
        return modes.empty() ? ActuatorConfig() : modes[active_mode_index].actuator_config;
    }
    const ProtocolSendConfig& GetActiveProtocolSendConfig() const {
        static ProtocolSendConfig defaultCfg;
        return modes.empty() ? defaultCfg : modes[active_mode_index].protocol_send;
    }
    const ProtocolReceiveConfig& GetActiveProtocolReceiveConfig() const {
        static ProtocolReceiveConfig defaultCfg;
        return modes.empty() ? defaultCfg : modes[active_mode_index].protocol_receive;
    }
    bool HasTemperature() const { return modes.empty() ? false : modes[active_mode_index].sensor_config.has_temperature; }
    bool HasHumidity()    const { return modes.empty() ? false : modes[active_mode_index].sensor_config.has_humidity; }
    bool HasDepth()       const { return modes.empty() ? false : modes[active_mode_index].sensor_config.has_depth; }

    // ---- UI ----
    void DrawRobotConfigPanel();

private:
    std::vector<RobotMode> modes;
    int edit_mode_index   = 0;
    int active_mode_index = 0;
};
