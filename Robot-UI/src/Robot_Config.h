#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Robot_API/robot_api.h"
#include <cstring>
#include <cstdint>
#include "GamepadMapper.h"
#include "ThrustCurveEditor.h"

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
    const ProtocolSendConfig& GetActiveProtocolConfig() const {
        static ProtocolSendConfig defaultCfg;
        return active_modes.empty() ? defaultCfg : active_modes[active_mode_index].protocol_send;
    }
    const ProtocolReceiveConfig& GetActiveProtocolReceiveConfig() const {
        static ProtocolReceiveConfig defaultCfg;
        return active_modes.empty() ? defaultCfg : active_modes[active_mode_index].protocol_receive;
    }

    // 以下返回引用时，根据编辑状态返回草稿或正式模式列表
    std::vector<RobotMode>& GetModes() {
        return m_IsEditing ? m_EditingModes : modes;
    }
    const std::vector<RobotMode>& GetModes() const {
        return modes;
    }
    std::vector<RobotMode>& GetActiveModes() { return active_modes; }
    int GetActiveModeIndex() const { return active_mode_index; }
    void SetActiveModeIndex(int idx) {
        if (idx >= 0 && idx < (int)active_modes.size()) {
            bool changed = (active_mode_index != idx);
            active_mode_index = idx;
            if (changed) m_ThrustCurveEditor.Close();
        }
    }

    void SetAvailableModeNames(const std::vector<std::string>& names) { m_AvailableModes = names; }

    bool HasTemperature() const { return active_modes.empty() ? false : active_modes[active_mode_index].has_temperature; }
    bool HasHumidity()    const { return active_modes.empty() ? false : active_modes[active_mode_index].has_humidity; }
    bool HasDepth()       const { return active_modes.empty() ? false : active_modes[active_mode_index].has_depth; }

    // 编辑索引（指向当前 UI 选中的模式）
    int& GetEditModeIndex();

    void AddMode();
    void DeleteMode(int index);

    // 协议字段配置子窗口
    void DrawProtocolFieldConfig(ProtocolSendConfig& cfg, ActuatorData& actuator, bool isSend);
    void DrawProtocolReceiveFieldConfig(ProtocolReceiveConfig& cfg, bool hasTemp, bool hasHum, bool hasDep);
    void DrawProtocolConfigWindow();    // 独立渲染子窗口（由 OnUIRender 调用）
    bool IsProtocolConfigOpen() const { return m_ProtocolConfigOpen; }

    // Thrust curve visual editor
    void OpenThrustCurveEditor(const char* motorName, ThrustCurve& curve) {
        m_ThrustCurveEditor.Open(motorName, curve);
    }
    void DrawThrustCurveEditor() { m_ThrustCurveEditor.Draw(); }

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
    int m_LastEditingModeIndex = 0;
    bool m_IsEditing = false;

    std::vector<std::string> m_AvailableModes;

    // 协议配置子窗口状态
    bool m_ProtocolConfigOpen = false;
    int m_ProtocolTabIndex = 0;

    // 推力曲线可视化编辑器
    ThrustCurveEditor m_ThrustCurveEditor;
};