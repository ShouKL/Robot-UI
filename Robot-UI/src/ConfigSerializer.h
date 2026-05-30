#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

class RobotComponent;
class GamepadMapper;
class ImGuiStyleManager;
class NodeEditor;
struct StreamConfig;
struct ThrustCurve;
struct RobotCommConfig;
struct UIState
{
    bool about_open            = false;
    bool option_open           = false;
    bool simulation_open       = false;
    bool live_streamer_open    = false;
    bool robot_status_open     = false;
    bool node_editor_open      = false;
    bool thrust_curve_editor_open = false;
    bool robot_comm_open       = true;
    int  robot_active_mode     = 0;
    int  gamepad_active_mode   = 0;
    float node_left_side_width  = 180.0f;
    float node_right_side_width = 200.0f;
};

class ConfigSerializer
{
public:
    static bool Save(const std::string& filepath,
                     const RobotComponent& robotConfig,
                     const GamepadMapper& gamepadMapper,
                     const ImGuiStyleManager& styleManager,
                     const std::vector<StreamConfig>& streams,
                     const UIState& uiState,
                     const ThrustCurve* editorCurve,
                     const std::vector<RobotCommConfig>& commConfigs = {},
                     int commActiveId = -1,
                     const std::map<std::string, std::string>* graphMap = nullptr,
                     std::string* outError = nullptr);

    static bool Load(const std::string& filepath,
                     RobotComponent& robotConfig,
                     GamepadMapper& gamepadMapper,
                     ImGuiStyleManager& styleManager,
                     std::vector<StreamConfig>& streams,
                     UIState& uiState,
                     ThrustCurve* editorCurve,
                     std::vector<RobotCommConfig>* commConfigs = nullptr,
                     int* commActiveId = nullptr,
                     std::map<std::string, std::string>* graphMap = nullptr,
                     std::string* outError = nullptr);

    // 默认扩展名
    static const char* DefaultExtension() { return ".rbt"; }

private:
    static void EmitRobotConfig(YAML::Emitter& out, const RobotComponent& config);
    static void EmitGamepadMapper(YAML::Emitter& out, const GamepadMapper& mapper);
    static void EmitStyle(YAML::Emitter& out, const ImGuiStyleManager& style);
    static void EmitStreams(YAML::Emitter& out, const std::vector<StreamConfig>& configs);
    static void EmitUIState(YAML::Emitter& out, const UIState& uiState);
    static void EmitEditorCurve(YAML::Emitter& out, const ThrustCurve& curve);
    static void EmitRobotComm(YAML::Emitter& out, const std::vector<RobotCommConfig>& configs, int activeId);

    // ======================== YAML 读取（基于 yaml-cpp） ========================
    static bool ApplyRobotConfig(const YAML::Node& node, RobotComponent& config, std::string* outError);
    static bool ApplyGamepadMapper(const YAML::Node& node, GamepadMapper& mapper, std::string* outError);
    static bool ApplyStyle(const YAML::Node& node, ImGuiStyleManager& style, std::string* outError);
    static bool ApplyStreams(const YAML::Node& node, std::vector<StreamConfig>& streams, std::string* outError);
    static bool ApplyUIState(const YAML::Node& node, UIState& uiState, std::string* outError);
    static bool ApplyEditorCurve(const YAML::Node& node, ThrustCurve& curve, std::string* outError);
    static bool ApplyRobotComm(const YAML::Node& node, std::vector<RobotCommConfig>& configs, int& activeId, std::string* outError);
};
