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
struct UIState
{
    bool about_open            = false;
    bool option_open           = false;
    bool simulation_open       = false;
    bool live_streamer_open    = false;
    bool robot_status_open     = false;
    bool node_editor_open      = false;
    bool thrust_curve_editor_open = false;
    int  robot_active_mode     = 0;
    int  gamepad_active_mode   = 0;
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
                     std::string* outError = nullptr);

    static bool Load(const std::string& filepath,
                     RobotComponent& robotConfig,
                     GamepadMapper& gamepadMapper,
                     ImGuiStyleManager& styleManager,
                     std::vector<StreamConfig>& streams,
                     UIState& uiState,
                     ThrustCurve* editorCurve,
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

    // ======================== YAML 读取（基于 yaml-cpp） ========================
    static bool ApplyRobotConfig(const YAML::Node& node, RobotComponent& config, std::string* outError);
    static bool ApplyGamepadMapper(const YAML::Node& node, GamepadMapper& mapper, std::string* outError);
    static bool ApplyStyle(const YAML::Node& node, ImGuiStyleManager& style, std::string* outError);
    static bool ApplyStreams(const YAML::Node& node, std::vector<StreamConfig>& streams, std::string* outError);
    static bool ApplyUIState(const YAML::Node& node, UIState& uiState, std::string* outError);
    static bool ApplyEditorCurve(const YAML::Node& node, ThrustCurve& curve, std::string* outError);
};
