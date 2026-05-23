#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

// 前向声明：避免在头文件中引入所有依赖
class Robot_Config;
class GamepadMapper;
class ImGuiStyleManager;
class NodeEditor;
struct StreamConfig;

class ConfigSerializer
{
public:
    // 保存全部配置到 .rbt 文件（YAML 格式），成功返回 true
    static bool Save(const std::string& filepath,
                     const Robot_Config& robotConfig,
                     const GamepadMapper& gamepadMapper,
                     const ImGuiStyleManager& styleManager,
                     const std::vector<StreamConfig>& streams,
                     std::string* outError = nullptr);

    // 从 .rbt 文件加载全部配置，直接恢复到传入的对象中，成功返回 true
    static bool Load(const std::string& filepath,
                     Robot_Config& robotConfig,
                     GamepadMapper& gamepadMapper,
                     ImGuiStyleManager& styleManager,
                     std::vector<StreamConfig>& streams,
                     std::string* outError = nullptr);

    // 默认扩展名
    static const char* DefaultExtension() { return ".rbt"; }

    // 文件对话框（Win32 封装）
    static std::string OpenFileDialog(const char* filter);
    static std::string SaveFileDialog(const char* filter, const char* defaultExt);

private:
    // ======================== YAML 写入（基于 yaml-cpp Emitter） ========================
    static void EmitRobotConfig(YAML::Emitter& out, const Robot_Config& config);
    static void EmitGamepadMapper(YAML::Emitter& out, const GamepadMapper& mapper);
    static void EmitStyle(YAML::Emitter& out, const ImGuiStyleManager& style);
    static void EmitStreams(YAML::Emitter& out, const std::vector<StreamConfig>& configs);

    // ======================== YAML 读取（基于 yaml-cpp） ========================
    static bool ApplyRobotConfig(const YAML::Node& node, Robot_Config& config, std::string* outError);
    static bool ApplyGamepadMapper(const YAML::Node& node, GamepadMapper& mapper, std::string* outError);
    static bool ApplyStyle(const YAML::Node& node, ImGuiStyleManager& style, std::string* outError);
    static bool ApplyStreams(const YAML::Node& node, std::vector<StreamConfig>& streams, std::string* outError);

    // ======================== 节点图持久化 ========================
    static std::string EmitNodeGraphYaml(const std::string& nodeGraph);
    static bool        ApplyNodeGraphYaml(const YAML::Node& node, std::string& nodeGraph);
};
