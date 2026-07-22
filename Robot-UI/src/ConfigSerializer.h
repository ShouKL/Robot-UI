#pragma once

#include "core.h"
#include "imgui_style.h"
#include "RobotComponentManager.h"
#include "GamepadMapperManager.h"
#include "NodeGraph.h"
#include "LiveStream.h"
#include "RobotComm.h"
#include "Robot_API/robot_api.h"

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
    bool monitor_wall_open      = false;
    int  robot_active_mode     = 0;
    int  gamepad_active_mode   = 0;
    float node_left_side_width  = 180.0f;
    float node_right_side_width = 200.0f;

    // ---- 软件 UI 快捷键键值（.kernel 持久化） ----
    std::string software_shortcuts_yaml;

    // ---- FileManager 状态（.kernel 中持久化） ----
    std::string robot_path;
    bool robot_dirty  = false;
    bool kernel_dirty = false;
    std::vector<std::string> recent_files;

    // ---- 截图设置（.kernel 持久化） ----
    int         screenshot_scope = 0;     // 0=客户区, 1=完整窗口, 2=全屏, 3+=子面板
    std::string screenshot_path;          // 保存路径（空=exe 同级的 screenshots/）

    // ---- 连接设置（.kernel 持久化） ----
    int  conn_retry_count = 6;           // 机器人连接重试总次数（1~20）
    int  camera_retry_count = 2;         // 摄像头连接重试次数（额外次数，不含初始）

    // ---- 启用的插件列表（.kernel 持久化） ----
    std::vector<std::string> enabled_plugins;

    // ---- 通信节点配置（.kernel 持久化，记住发送帧 enable 状态等） ----
    std::vector<RobotComm> comm_configs;
};

class ConfigSerializer
{
public:
    static bool Save(const std::string& filepath,
                     const RobotComponentManager& robotMgr,
                     const GamepadMapperManager& gamepadMgr,
                     const ImGuiStyleManager& styleManager,
                     const std::vector<LiveStream>& streams,
                     const UIState& uiState,
                     const ThrustCurve* editorCurve,
                     const std::vector<RobotComm>& commConfigs = {},
                     const std::map<std::string, std::string>* graphMap = nullptr,
                     const std::vector<NodeGraph>* graphItems = nullptr,
                     std::string* outError = nullptr);

    static bool Load(const std::string& filepath,
                     RobotComponentManager& robotMgr,
                     GamepadMapperManager& gamepadMgr,
                     ImGuiStyleManager& styleManager,
                     std::vector<LiveStream>& streams,
                     UIState& uiState,
                     ThrustCurve* editorCurve,
                     std::vector<RobotComm>* commConfigs = nullptr,
                     std::map<std::string, std::string>* graphMap = nullptr,
                     std::vector<NodeGraph>* graphItems = nullptr,
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
    static void EmitStreams(YAML::Emitter& out, const std::vector<LiveStream>& configs);
    static void EmitUIState(YAML::Emitter& out, const UIState& uiState);
    static void EmitEditorCurve(YAML::Emitter& out, const ThrustCurve& curve);
    static void EmitRobotComm(YAML::Emitter& out, const std::vector<RobotComm>& configs);
    static void EmitGraphItems(YAML::Emitter& out, const std::vector<NodeGraph>& items);
    static void EmitSoftwareShortcuts(YAML::Emitter& out, const std::string& swYaml);
    static bool ApplySoftwareShortcuts(const YAML::Node& node, std::string& outYaml, std::string* outError);

    // ======================== YAML 读取（基于 yaml-cpp） ========================
    static bool ApplyRobotConfig(const YAML::Node& node, RobotComponentManager& robotMgr, std::string* outError);
    static bool ApplyGamepadMapper(const YAML::Node& node, GamepadMapperManager& gamepadMgr, std::string* outError);
    static bool ApplyStyle(const YAML::Node& node, ImGuiStyleManager& style, std::string* outError);
    static bool ApplyStreams(const YAML::Node& node, std::vector<LiveStream>& streams, std::string* outError);
    static bool ApplyUIState(const YAML::Node& node, UIState& uiState, std::string* outError);
    static bool ApplyEditorCurve(const YAML::Node& node, ThrustCurve& curve, std::string* outError);
    static bool ApplyRobotComm(const YAML::Node& node, std::vector<RobotComm>& configs, std::string* outError);
    static bool ApplyGraphItems(const YAML::Node& node, std::vector<NodeGraph>& items, std::string* outError);
};
