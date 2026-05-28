#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Robot_API/robot_api.h"
#include <cstring>
#include <cstdint>
#include "GamepadMapper.h"

struct RobotMode {
    char name[64] = "";
    std::string gamepad_mapping_Mode;
    std::string node_graph;
    ActuatorData actuator_config;
    ProtocolSendConfig protocol_send;
    ProtocolReceiveConfig protocol_receive;
    std::string host_ip;
    int remote_port = 0;
    int local_port = 0;
    int protocol_type = 0;
    bool has_temperature = false;
    bool has_humidity = false;
    bool has_depth = false;
    DataEncoding temp_encoding  = DataEncoding::Float32;
    DataEncoding hum_encoding   = DataEncoding::Float32;
    DataEncoding depth_encoding = DataEncoding::Float32;
};

class RobotComponent {
public:
    RobotComponent();
    ~RobotComponent();

    void DrawRobotConfigPanel();

    void NextActiveMode() {
        if (!modes.empty())
            active_mode_index = (active_mode_index + 1) % modes.size();
    }
    ActuatorData GetActiveActuatorConfig() {
        return modes.empty() ? ActuatorData() : modes[active_mode_index].actuator_config;
    }
    const ProtocolSendConfig& GetActiveProtocolSendConfig() const {
        static ProtocolSendConfig defaultCfg;
        return modes.empty() ? defaultCfg : modes[active_mode_index].protocol_send;
    }
    const ProtocolReceiveConfig& GetActiveProtocolReceiveConfig() const {
        static ProtocolReceiveConfig defaultCfg;
        return modes.empty() ? defaultCfg : modes[active_mode_index].protocol_receive;
    }

    std::vector<RobotMode>& GetModes() { return modes; }
    const std::vector<RobotMode>& GetModes() const { return modes; }
    std::vector<RobotMode>& GetActiveModes() { return modes; }
    int GetActiveModeIndex() const { return active_mode_index; }
    void SetActiveModeIndex(int idx) {
        if (idx >= 0 && idx < (int)modes.size())
            active_mode_index = idx;
    }

    bool HasTemperature() const { return modes.empty() ? false : modes[active_mode_index].has_temperature; }
    bool HasHumidity()    const { return modes.empty() ? false : modes[active_mode_index].has_humidity; }
    bool HasDepth()       const { return modes.empty() ? false : modes[active_mode_index].has_depth; }

    int& GetEditModeIndex() { return edit_mode_index; }

    void AddMode();
    void DeleteMode(int index);

private:
    std::vector<RobotMode> modes;
    int edit_mode_index = 0;
    int active_mode_index = 0;
};
