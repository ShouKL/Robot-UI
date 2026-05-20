#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Robot_API/robot_api.h"
#include <cstring>
#include <cstdint>
#include "GamepadMapper.h"

struct RobotMode {
    char name[64] = "Default Mode";
    std::string gamepad_mapping_Mode = "Default";
    ActuatorData actuator_config;
    std::string host_ip = "192.168.0.10";
    int remote_port = 8888;
    int local_port = 8888;
    int protocol_type = 0;
    int data_format = 0;
    char callback_file[256] = "protocol_callback.cpp";
    bool has_temperature = true;
    bool has_humidity = true;
    bool has_depth = true;


};

class Robot_Config {
public:
    Robot_Config();
    ~Robot_Config();

    // ---------- UI 绘制（操作草稿） ----------
    void DrawRobotConfigPanel();

    // ---------- 草稿机制 ----------
    void BeginEdit();               // 从当前保存的模式（modes）创建编辑草稿
    void ApplyEdit();               // 提交草稿到 modes，并同步到 active_modes
    void CancelEdit();              // 丢弃草稿
    bool IsEditing() const { return m_IsEditing; }

    // ---------- 运行时接口 ----------
    void NextActiveMode() {
        if (!active_modes.empty())
            active_mode_index = (active_mode_index + 1) % active_modes.size();
    }
    std::string GetActiveModeName() {
        return active_modes.empty() ? "None" : active_modes[active_mode_index].name;
    }
    ActuatorData GetActiveActuatorConfig() {
        return active_modes.empty() ? ActuatorData() : active_modes[active_mode_index].actuator_config;
    }
    std::string GetActiveHostIP() {
        return active_modes.empty() ? "192.168.0.10" : active_modes[active_mode_index].host_ip;
    }
    int GetActiveRemotePort() {
        return active_modes.empty() ? 8888 : active_modes[active_mode_index].remote_port;
    }
    int GetActiveLocalPort() {
        return active_modes.empty() ? 8888 : active_modes[active_mode_index].local_port;
    }
    bool HasTemperature() {
        return active_modes.empty() ? true : active_modes[active_mode_index].has_temperature;
    }
    bool HasHumidity() {
        return active_modes.empty() ? true : active_modes[active_mode_index].has_humidity;
    }
    bool HasDepth() {
        return active_modes.empty() ? true : active_modes[active_mode_index].has_depth;
    }

    // 以下返回引用时，根据编辑状态返回草稿或正式模式列表
    std::vector<RobotMode>& GetModes() {
        return m_IsEditing ? m_EditingModes : modes;
    }
    std::vector<RobotMode>& GetActiveModes() { return active_modes; }
    int GetActiveModeIndex() { return active_mode_index; }

    void SetAvailableModeNames(const std::vector<std::string>& names) { m_AvailableModes = names; }

    // 编辑索引（指向当前 UI 选中的模式）
    int& GetEditModeIndex();

    void AddMode();
    void DeleteMode(int index);

private:
    // 保存的配置（上一次 Apply 固化后的状态）
    std::vector<RobotMode> modes;
    int edit_mode_index = 0;                // 仅用于未编辑时的回退

    // 实际运行的配置
    std::vector<RobotMode> active_modes;
    int active_mode_index = 0;

    // 编辑草稿
    std::vector<RobotMode> m_EditingModes;
    int m_EditingModeIndex = 0;
    bool m_IsEditing = false;

    std::vector<std::string> m_AvailableModes;
};