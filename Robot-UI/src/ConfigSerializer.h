#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

class RobotComponentManager;
class GamepadMapperManager;
class ImGuiStyleManager;
class NodeGraphManager;
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
    bool notification_open      = false;
    bool terminal_open          = false;
    int  robot_active_mode     = 0;
    int  gamepad_active_mode   = 0;
    float node_left_side_width  = 180.0f;
    float node_right_side_width = 200.0f;

    // ---- FileManager 状态（.kernel 中持久化） ----
    std::string robot_path;
    bool robot_dirty  = false;
    bool kernel_dirty = false;
    std::vector<std::string> recent_files;
};

class ConfigSerializer
{
public:
    static bool Save(const std::string& filepath,
                     const RobotComponentManager& robotMgr,
                     const GamepadMapperManager& gamepadMgr,
                     const ImGuiStyleManager& styleManager,
                     const std::vector<StreamConfig>& streams,
                     const UIState& uiState,
                     const ThrustCurve* editorCurve,
                     const std::vector<RobotCommConfig>& commConfigs = {},
                     const std::map<std::string, std::string>* graphMap = nullptr,
                     std::string* outError = nullptr);

    static bool Load(const std::string& filepath,
                     RobotComponentManager& robotMgr,
                     GamepadMapperManager& gamepadMgr,
                     ImGuiStyleManager& styleManager,
                     std::vector<StreamConfig>& streams,
                     UIState& uiState,
                     ThrustCurve* editorCurve,
                     std::vector<RobotCommConfig>* commConfigs = nullptr,
                     std::map<std::string, std::string>* graphMap = nullptr,
                     std::string* outError = nullptr);

    // ---- Kernel 文件（.kernel） — 仅样式 + UI 状态 ----
    static bool SaveKernel(const std::string& filepath,
                           const ImGuiStyleManager& styleManager,
                           const UIState& uiState,
                           std::string* outError = nullptr);

    static bool LoadKernel(const std::string& filepath,
                           ImGuiStyleManager& styleManager,
                           UIState& uiState,
                           std::string* outError = nullptr);

    // 默认扩展名
    static const char* DefaultExtension()        { return ".rbt"; }
    static const char* KernelDefaultExtension()  { return ".kernel"; }

private:
    static void EmitRobotConfig(YAML::Emitter& out, const RobotComponentManager& robotMgr);
    static void EmitGamepadMapper(YAML::Emitter& out, const GamepadMapperManager& gamepadMgr);
    static void EmitStyle(YAML::Emitter& out, const ImGuiStyleManager& style);
    static void EmitStreams(YAML::Emitter& out, const std::vector<StreamConfig>& configs);
    static void EmitUIState(YAML::Emitter& out, const UIState& uiState);
    static void EmitEditorCurve(YAML::Emitter& out, const ThrustCurve& curve);
    static void EmitRobotComm(YAML::Emitter& out, const std::vector<RobotCommConfig>& configs);

    // ======================== YAML 读取（基于 yaml-cpp） ========================
    static bool ApplyRobotConfig(const YAML::Node& node, RobotComponentManager& robotMgr, std::string* outError);
    static bool ApplyGamepadMapper(const YAML::Node& node, GamepadMapperManager& gamepadMgr, std::string* outError);
    static bool ApplyStyle(const YAML::Node& node, ImGuiStyleManager& style, std::string* outError);
    static bool ApplyStreams(const YAML::Node& node, std::vector<StreamConfig>& streams, std::string* outError);
    static bool ApplyUIState(const YAML::Node& node, UIState& uiState, std::string* outError);
    static bool ApplyEditorCurve(const YAML::Node& node, ThrustCurve& curve, std::string* outError);
    static bool ApplyRobotComm(const YAML::Node& node, std::vector<RobotCommConfig>& configs, std::string* outError);
};
