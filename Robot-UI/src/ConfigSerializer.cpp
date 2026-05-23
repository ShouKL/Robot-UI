#include "ConfigSerializer.h"
#include "Robot_Config.h"
#include "GamepadMapper.h"
#include "imgui_style.h"
#include "LiveStream.h"

#include <fstream>
#include <cstring>
#include <cstdlib>
#include <windows.h>
#include <commdlg.h>

bool ConfigSerializer::Save(const std::string& filepath,
                            const Robot_Config& robotConfig,
                            const GamepadMapper& gamepadMapper,
                            const ImGuiStyleManager& styleManager,
                            const std::vector<StreamConfig>& streams,
                            std::string* outError)
{
    try
    {
        YAML::Emitter out;
        out << YAML::BeginMap;

        out << YAML::Key << "robot_ui_config" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "version" << YAML::Value << 1;

        EmitRobotConfig(out, robotConfig);
        EmitGamepadMapper(out, gamepadMapper);
        EmitStyle(out, styleManager);
        EmitStreams(out, streams);

        out << YAML::EndMap;  // robot_ui_config
        out << YAML::EndMap;  // root

        if (!out.good())
        {
            if (outError) *outError = "YAML emit error: " + out.GetLastError();
            return false;
        }

        std::ofstream ofs(filepath, std::ios::out | std::ios::binary);
        if (!ofs.is_open())
        {
            if (outError) *outError = "Cannot open file for writing: " + filepath;
            return false;
        }
        ofs << out.c_str();
        ofs.close();
        return true;
    }
    catch (const YAML::Exception& e)
    {
        if (outError) *outError = std::string("Save YAML exception: ") + e.what();
        return false;
    }
    catch (const std::exception& e)
    {
        if (outError) *outError = std::string("Save exception: ") + e.what();
        return false;
    }
}

bool ConfigSerializer::Load(const std::string& filepath,
                            Robot_Config& robotConfig,
                            GamepadMapper& gamepadMapper,
                            ImGuiStyleManager& styleManager,
                            std::vector<StreamConfig>& streams,
                            std::string* outError)
{
    try
    {
        YAML::Node doc = YAML::LoadFile(filepath);
        if (!doc.IsMap())
        {
            if (outError) *outError = "Root YAML node is not a map";
            return false;
        }

        const YAML::Node& cfg = doc["robot_ui_config"];
        if (!cfg.IsDefined() || cfg.IsNull())
        {
            if (outError) *outError = "Missing top-level key: robot_ui_config";
            return false;
        }

        if (const YAML::Node& robotNode = cfg["robot"]; robotNode.IsDefined())
        {
            if (!ApplyRobotConfig(robotNode, robotConfig, outError))
                return false;
        }

        if (const YAML::Node& gamepadNode = cfg["gamepad"]; gamepadNode.IsDefined())
        {
            if (!ApplyGamepadMapper(gamepadNode, gamepadMapper, outError))
                return false;
        }

        if (const YAML::Node& styleNode = cfg["style"]; styleNode.IsDefined())
        {
            if (!ApplyStyle(styleNode, styleManager, outError))
                return false;
        }

        if (const YAML::Node& streamsNode = cfg["streams"]; streamsNode.IsDefined())
        {
            if (!ApplyStreams(streamsNode, streams, outError))
                return false;
        }

        return true;
    }
    catch (const YAML::BadFile& e)
    {
        if (outError) *outError = std::string("Cannot open file: ") + e.what();
        return false;
    }
    catch (const YAML::Exception& e)
    {
        if (outError) *outError = std::string("Load YAML exception: ") + e.what();
        return false;
    }
    catch (const std::exception& e)
    {
        if (outError) *outError = std::string("Load exception: ") + e.what();
        return false;
    }
}

// ============================================================================
//  YAML 写入（基于 yaml-cpp Emitter）
// ============================================================================

void ConfigSerializer::EmitRobotConfig(YAML::Emitter& out, const Robot_Config& config)
{
    out << YAML::Key << "robot" << YAML::Value << YAML::BeginMap;

    const auto& modes = config.GetModes();

    out << YAML::Key << "modes" << YAML::Value << YAML::BeginSeq;
    for (const auto& mode : modes)
    {
        out << YAML::BeginMap;

        out << YAML::Key << "name" << YAML::Value << mode.name;
        out << YAML::Key << "gamepad_mapping" << YAML::Value << mode.gamepad_mapping_Mode;
        out << YAML::Key << "host_ip" << YAML::Value << mode.host_ip;
        out << YAML::Key << "remote_port" << YAML::Value << mode.remote_port;
        out << YAML::Key << "local_port" << YAML::Value << mode.local_port;
        out << YAML::Key << "protocol_type" << YAML::Value << mode.protocol_type;
        out << YAML::Key << "data_format" << YAML::Value << mode.data_format;
        out << YAML::Key << "callback_file" << YAML::Value << mode.callback_file;
        out << YAML::Key << "has_temperature" << YAML::Value << mode.has_temperature;
        out << YAML::Key << "has_humidity" << YAML::Value << mode.has_humidity;
        out << YAML::Key << "has_depth" << YAML::Value << mode.has_depth;

        out << YAML::Key << "brushless_motors" << YAML::Value << YAML::BeginSeq;
        for (const auto& pair : mode.actuator_config.brushlessmotor)
        {
            const auto& m = pair.second;
            out << YAML::BeginMap;
            out << YAML::Key << "id" << YAML::Value << m.id;
            out << YAML::Key << "target_speed" << YAML::Value << m.target_speed;
            out << YAML::Key << "np_mid" << YAML::Value << m.curve.np_mid;
            out << YAML::Key << "np_ini" << YAML::Value << m.curve.np_ini;
            out << YAML::Key << "pp_ini" << YAML::Value << m.curve.pp_ini;
            out << YAML::Key << "pp_mid" << YAML::Value << m.curve.pp_mid;
            out << YAML::Key << "nt_end" << YAML::Value << m.curve.nt_end;
            out << YAML::Key << "nt_mid" << YAML::Value << m.curve.nt_mid;
            out << YAML::Key << "pt_mid" << YAML::Value << m.curve.pt_mid;
            out << YAML::Key << "pt_end" << YAML::Value << m.curve.pt_end;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "servos" << YAML::Value << YAML::BeginSeq;
        for (const auto& pair : mode.actuator_config.servo)
        {
            const auto& s = pair.second;
            out << YAML::BeginMap;
            out << YAML::Key << "id" << YAML::Value << s.id;
            out << YAML::Key << "angle" << YAML::Value << s.angle;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;  // servos

        // 节点图数据
        if (!mode.node_graph.empty())
            out << YAML::Key << "node_graph" << YAML::Value << YAML::Literal << mode.node_graph;

        // 参数映射表
        if (!mode.parameter_mapping.empty())
        {
            out << YAML::Key << "parameter_mapping" << YAML::Value << YAML::BeginMap;
            for (const auto& [k, v] : mode.parameter_mapping)
                out << YAML::Key << k << YAML::Value << v;
            out << YAML::EndMap;
        }

        out << YAML::EndMap;  // mode
    }
    out << YAML::EndSeq;  // modes

    out << YAML::Key << "active_mode" << YAML::Value << config.GetActiveModeIndex();
    out << YAML::EndMap;
}

void ConfigSerializer::EmitGamepadMapper(YAML::Emitter& out, const GamepadMapper& mapper)
{
    out << YAML::Key << "gamepad" << YAML::Value << YAML::BeginMap;

    const auto& modes = mapper.GetModes();

    out << YAML::Key << "modes" << YAML::Value << YAML::BeginSeq;
    for (const auto& mode : modes)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "name" << YAML::Value << mode.name;
        out << YAML::Key << "gamepad_type" << YAML::Value << static_cast<int>(mode.gamepad_type);

        // 保存自定义键位定义（keys）
        out << YAML::Key << "keys" << YAML::Value << YAML::BeginSeq;
        for (const auto& k : mode.keys)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "key_id" << YAML::Value << k.id;
            out << YAML::Key << "key_name" << YAML::Value << k.name;
            out << YAML::Key << "analog" << YAML::Value << k.is_analog;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "mappings" << YAML::Value << YAML::BeginSeq;
        for (const auto& m : mode.mappings)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "key_id" << YAML::Value << m.key_id;
            out << YAML::Key << "key_name" << YAML::Value << m.key_name;
            out << YAML::Key << "key" << YAML::Value << m.gamepad_key;
            out << YAML::Key << "analog" << YAML::Value << m.is_analog;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "active_mode" << YAML::Value << mapper.GetActiveModeIndex();
    out << YAML::EndMap;
}

void ConfigSerializer::EmitStyle(YAML::Emitter& out, const ImGuiStyleManager& style)
{
    out << YAML::Key << "style" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "theme" << YAML::Value << static_cast<int>(style.GetTheme());
    out << YAML::Key << "invert" << YAML::Value << style.GetInvert();
    out << YAML::Key << "alpha" << YAML::Value << style.GetAlpha();
    out << YAML::EndMap;
}

void ConfigSerializer::EmitStreams(YAML::Emitter& out, const std::vector<StreamConfig>& configs)
{
    out << YAML::Key << "streams" << YAML::Value << YAML::BeginSeq;
    for (const auto& c : configs)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "name" << YAML::Value << c.name;
        out << YAML::Key << "ip" << YAML::Value << c.ip;
        out << YAML::Key << "user" << YAML::Value << c.user;
        out << YAML::Key << "pass" << YAML::Value << c.pass;
        out << YAML::Key << "port" << YAML::Value << c.port;
        out << YAML::Key << "brand" << YAML::Value << static_cast<int>(c.brand);
        out << YAML::Key << "channel" << YAML::Value << c.channel;
        out << YAML::Key << "codec" << YAML::Value << static_cast<int>(c.codec);
        out << YAML::Key << "stream_type" << YAML::Value << static_cast<int>(c.streamType);
        out << YAML::Key << "protocol" << YAML::Value << static_cast<int>(c.protocol);
        out << YAML::Key << "custom_path" << YAML::Value << c.customPath;
        out << YAML::Key << "latency" << YAML::Value << c.latency;
        out << YAML::Key << "udp_buffer_size" << YAML::Value << c.udpBufferSize;
        out << YAML::Key << "timeout" << YAML::Value << c.timeout;
        out << YAML::Key << "drop_on_latency" << YAML::Value << c.dropOnLatency;
        out << YAML::Key << "ntp_sync" << YAML::Value << c.ntpSync;
        out << YAML::Key << "buffer_mode" << YAML::Value << static_cast<int>(c.bufferMode);
        out << YAML::Key << "decoder" << YAML::Value << static_cast<int>(c.decoder);
        out << YAML::Key << "cpu_threads" << YAML::Value << c.cpuThreads;
        out << YAML::Key << "sync_to_clock" << YAML::Value << c.syncToClock;
        out << YAML::Key << "max_buffers" << YAML::Value << c.maxBuffers;
        out << YAML::Key << "low_latency_mode" << YAML::Value << c.lowLatencyMode;
        out << YAML::Key << "use_bgra" << YAML::Value << c.useBGRA;
        out << YAML::Key << "auto_hardware_fallback" << YAML::Value << c.autoHardwareFallback;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
}

// ============================================================================
//  YAML 读取（基于 yaml-cpp Node）
// ============================================================================

static void SafeStrCpy(char* dst, size_t dstSize, const std::string& src)
{
    if (dstSize == 0) return;
    strncpy(dst, src.c_str(), dstSize - 1);
    dst[dstSize - 1] = '\0';
}

bool ConfigSerializer::ApplyRobotConfig(const YAML::Node& robotNode, Robot_Config& config, std::string* outError)
{
    const YAML::Node& modesNode = robotNode["modes"];
    if (!modesNode.IsDefined() || !modesNode.IsSequence())
    {
        if (outError) *outError = "robot section missing 'modes'";
        return false;
    }
    if (modesNode.size() == 0)
    {
        if (outError) *outError = "robot modes list is empty";
        return false;
    }

    if (config.IsEditing())
        config.CancelEdit();
    config.BeginEdit();

    auto& editModes = config.GetModes();

    std::vector<RobotMode> loadedModes;

    for (const auto& item : modesNode)
    {
        RobotMode mode;

        auto readStr = [&](const char* key, char* dst, size_t dstSize) {
            const YAML::Node& n = item[key];
            if (n.IsDefined() && n.IsScalar())
                SafeStrCpy(dst, dstSize, n.as<std::string>());
        };
        auto readStdStr = [&](const char* key, std::string& out) {
            const YAML::Node& n = item[key];
            if (n.IsDefined() && n.IsScalar())
                out = n.as<std::string>();
        };

        readStr("name", mode.name, sizeof(mode.name));
        readStdStr("gamepad_mapping", mode.gamepad_mapping_Mode);
        readStdStr("host_ip", mode.host_ip);

        if (const YAML::Node& n = item["remote_port"]; n.IsDefined()) mode.remote_port = n.as<int>();
        if (const YAML::Node& n = item["local_port"]; n.IsDefined())  mode.local_port = n.as<int>();
        if (const YAML::Node& n = item["protocol_type"]; n.IsDefined()) mode.protocol_type = n.as<int>();
        if (const YAML::Node& n = item["data_format"]; n.IsDefined())  mode.data_format = n.as<int>();

        readStr("callback_file", mode.callback_file, sizeof(mode.callback_file));

        if (const YAML::Node& n = item["has_temperature"]; n.IsDefined()) mode.has_temperature = n.as<bool>();
        if (const YAML::Node& n = item["has_humidity"]; n.IsDefined())    mode.has_humidity = n.as<bool>();
        if (const YAML::Node& n = item["has_depth"]; n.IsDefined())       mode.has_depth = n.as<bool>();

        // 读取节点图
        if (const YAML::Node& n = item["node_graph"]; n.IsDefined() && n.IsScalar())
            mode.node_graph = n.as<std::string>();

        // 读取参数映射表
        const YAML::Node& pm = item["parameter_mapping"];
        if (pm.IsDefined() && pm.IsMap())
        {
            mode.parameter_mapping.clear();
            for (auto it = pm.begin(); it != pm.end(); ++it)
                mode.parameter_mapping[it->first.as<std::string>()] = it->second.as<std::string>();
        }

        mode.actuator_config.brushlessmotor.clear();
        const YAML::Node& motors = item["brushless_motors"];
        if (motors.IsDefined() && motors.IsSequence())
        {
            for (const auto& mItem : motors)
            {
                BrushlessMotor bm;
                if (const YAML::Node& n = mItem["id"]; n.IsDefined())           bm.id = n.as<int>();
                if (const YAML::Node& n = mItem["target_speed"]; n.IsDefined())  bm.target_speed = n.as<double>();
                if (const YAML::Node& n = mItem["np_mid"]; n.IsDefined())  bm.curve.np_mid = n.as<double>();
                if (const YAML::Node& n = mItem["np_ini"]; n.IsDefined())  bm.curve.np_ini = n.as<double>();
                if (const YAML::Node& n = mItem["pp_ini"]; n.IsDefined())  bm.curve.pp_ini = n.as<double>();
                if (const YAML::Node& n = mItem["pp_mid"]; n.IsDefined())  bm.curve.pp_mid = n.as<double>();
                if (const YAML::Node& n = mItem["nt_end"]; n.IsDefined())  bm.curve.nt_end = n.as<double>();
                if (const YAML::Node& n = mItem["nt_mid"]; n.IsDefined())  bm.curve.nt_mid = n.as<double>();
                if (const YAML::Node& n = mItem["pt_mid"]; n.IsDefined())  bm.curve.pt_mid = n.as<double>();
                if (const YAML::Node& n = mItem["pt_end"]; n.IsDefined())  bm.curve.pt_end = n.as<double>();
                mode.actuator_config.brushlessmotor[bm.id] = bm;
            }
        }

        mode.actuator_config.servo.clear();
        const YAML::Node& servos = item["servos"];
        if (servos.IsDefined() && servos.IsSequence())
        {
            for (const auto& sItem : servos)
            {
                Servo sv;
                if (const YAML::Node& n = sItem["id"]; n.IsDefined())    sv.id = n.as<int>();
                if (const YAML::Node& n = sItem["angle"]; n.IsDefined()) sv.angle = n.as<double>();
                mode.actuator_config.servo[sv.id] = sv;
            }
        }

        loadedModes.push_back(mode);
    }

    while (static_cast<int>(editModes.size()) > 1)
        config.DeleteMode(0);

    if (!loadedModes.empty())
    {
        editModes[0] = loadedModes[0];
        for (size_t i = 1; i < loadedModes.size(); ++i)
        {
            config.AddMode();
            editModes.back() = loadedModes[i];
        }
    }

    if (const YAML::Node& n = robotNode["active_mode"]; n.IsDefined())
    {
        int idx = n.as<int>();
        if (idx >= 0 && idx < static_cast<int>(editModes.size()))
            config.GetEditModeIndex() = idx;
    }

    config.ApplyEdit();
    return true;
}

bool ConfigSerializer::ApplyGamepadMapper(const YAML::Node& gamepadNode, GamepadMapper& mapper, std::string* outError)
{
    // 读取每个模式自己的 gamepad_type（旧版全局字段仅作 fallback，新版在 mode 内）
    GamepadType fallbackType = GamepadType::Xbox;
    if (const YAML::Node& n = gamepadNode["gamepad_type"]; n.IsDefined())
        fallbackType = static_cast<GamepadType>(n.as<int>());

    const YAML::Node& modesNode = gamepadNode["modes"];
    if (!modesNode.IsDefined() || !modesNode.IsSequence())
    {
        if (outError) *outError = "gamepad section missing or invalid 'modes'";
        return false;
    }

    if (mapper.IsEditing())
        mapper.CancelEdit();

    std::vector<GamepadMode> loadedModes;
    for (const auto& item : modesNode)
    {
        GamepadMode mode;

        if (const YAML::Node& n = item["name"]; n.IsDefined() && n.IsScalar())
        {
            SafeStrCpy(mode.name, sizeof(mode.name), n.as<std::string>());
        }
        else
        {
            SafeStrCpy(mode.name, sizeof(mode.name), "Default");
        }

        // 读取每个模式的手柄类型（优先 mode 内字段，fallback 到全局字段）
        if (const YAML::Node& n = item["gamepad_type"]; n.IsDefined())
            mode.gamepad_type = static_cast<GamepadType>(n.as<int>());
        else
            mode.gamepad_type = fallbackType;

        mode.mappings.clear();
        mode.keys.clear();

        // 读取自定义键位定义（keys）
        int maxKeyId = 0;
        const YAML::Node& keysNode = item["keys"];
        if (keysNode.IsDefined() && keysNode.IsSequence())
        {
            for (const auto& kItem : keysNode)
            {
                GamepadKey gk;
                if (const YAML::Node& n = kItem["key_id"]; n.IsDefined())   gk.id = n.as<int>();
                if (const YAML::Node& n = kItem["key_name"]; n.IsDefined()) gk.name = n.as<std::string>();
                if (const YAML::Node& n = kItem["analog"]; n.IsDefined())   gk.is_analog = n.as<bool>();
                mode.keys.push_back(gk);
                if (gk.id > maxKeyId) maxKeyId = gk.id;
            }
        }

        const YAML::Node& mappings = item["mappings"];
        if (mappings.IsDefined() && mappings.IsSequence())
        {
            for (const auto& mItem : mappings)
            {
                KeyMapping km;
                km.is_bound = false;

                // 读取键位名（兼容旧版 "action" 字段）
                if (const YAML::Node& n = mItem["key_name"]; n.IsDefined())
                    km.key_name = n.as<std::string>();
                else if (const YAML::Node& n = mItem["action"]; n.IsDefined())
                    km.key_name = n.as<std::string>();
                if (const YAML::Node& n = mItem["key_id"]; n.IsDefined())
                    km.key_id = n.as<int>();
                if (const YAML::Node& n = mItem["key"]; n.IsDefined())
                {
                    km.gamepad_key = n.as<std::string>();
                    if (!km.gamepad_key.empty())
                        km.is_bound = true;
                }
                if (const YAML::Node& n = mItem["analog"]; n.IsDefined()) km.is_analog = n.as<bool>();

                km.key_pos = ImVec2();
                mode.mappings.push_back(km);
                if (km.key_id > maxKeyId) maxKeyId = km.key_id;
            }
        }

        // 如果没有 keys 数据（旧格式），从 mappings 重建 keys
        if (mode.keys.empty() && !mode.mappings.empty())
        {
            for (const auto& m : mode.mappings)
            {
                GamepadKey gk;
                gk.id = m.key_id;
                gk.name = m.key_name;
                gk.is_analog = m.is_analog;
                mode.keys.push_back(gk);
            }
        }

        loadedModes.push_back(mode);
    }

    if (loadedModes.empty())
    {
        if (outError) *outError = "gamepad modes list is empty";
        return false;
    }

    // 直接替换全部模式（不经过草稿机制，保持编辑状态不变）
    {
        if (mapper.IsEditing())
            mapper.CancelEdit();
        // 确保模式数量匹配
        auto& modes = mapper.GetModes();
        while ((int)modes.size() > (int)loadedModes.size())
            mapper.DeleteMode((int)modes.size() - 1);
        while ((int)modes.size() < (int)loadedModes.size())
            mapper.AddMode();

        for (size_t i = 0; i < loadedModes.size(); ++i)
            modes[i] = loadedModes[i];
    }

    if (const YAML::Node& n = gamepadNode["active_mode"]; n.IsDefined())
    {
        int idx = n.as<int>();
        if (idx >= 0 && idx < static_cast<int>(loadedModes.size()))
            mapper.SetActiveModeByIndex(idx);
    }

    mapper.UpdateNextKeyID();

    return true;
}

bool ConfigSerializer::ApplyStyle(const YAML::Node& styleNode, ImGuiStyleManager& style, std::string* outError)
{
    int theme = 0;
    bool invert = false;
    float alpha = 1.0f;

    if (const YAML::Node& n = styleNode["theme"]; n.IsDefined())  theme = n.as<int>();
    if (const YAML::Node& n = styleNode["invert"]; n.IsDefined()) invert = n.as<bool>();
    if (const YAML::Node& n = styleNode["alpha"]; n.IsDefined())  alpha = n.as<float>();

    style.ApplyImGuiStyle(static_cast<ImGuiTheme>(theme), invert, alpha);
    return true;
}

bool ConfigSerializer::ApplyStreams(const YAML::Node& streamsNode, std::vector<StreamConfig>& streams, std::string* outError)
{
    if (!streamsNode.IsSequence())
    {
        if (outError) *outError = "streams section is not a list";
        return false;
    }

    streams.clear();
    for (const auto& item : streamsNode)
    {
        StreamConfig cfg;

        auto readStr = [&](const char* key, char* dst, size_t dstSize) {
            const YAML::Node& n = item[key];
            if (n.IsDefined() && n.IsScalar())
                SafeStrCpy(dst, dstSize, n.as<std::string>());
        };

        readStr("name", cfg.name, sizeof(cfg.name));
        readStr("ip", cfg.ip, sizeof(cfg.ip));
        readStr("user", cfg.user, sizeof(cfg.user));
        readStr("pass", cfg.pass, sizeof(cfg.pass));

        if (const YAML::Node& n = item["port"]; n.IsDefined())      cfg.port = n.as<int>();
        if (const YAML::Node& n = item["brand"]; n.IsDefined())     cfg.brand = static_cast<CameraBrand>(n.as<int>());
        if (const YAML::Node& n = item["channel"]; n.IsDefined())   cfg.channel = n.as<int>();
        if (const YAML::Node& n = item["codec"]; n.IsDefined())     cfg.codec = static_cast<CodecType>(n.as<int>());
        if (const YAML::Node& n = item["stream_type"]; n.IsDefined()) cfg.streamType = static_cast<StreamType>(n.as<int>());
        if (const YAML::Node& n = item["protocol"]; n.IsDefined())  cfg.protocol = static_cast<TransportProto>(n.as<int>());
        readStr("custom_path", cfg.customPath, sizeof(cfg.customPath));

        if (const YAML::Node& n = item["latency"]; n.IsDefined())             cfg.latency = n.as<int>();
        if (const YAML::Node& n = item["udp_buffer_size"]; n.IsDefined())     cfg.udpBufferSize = n.as<int>();
        if (const YAML::Node& n = item["timeout"]; n.IsDefined())              cfg.timeout = n.as<int>();
        if (const YAML::Node& n = item["drop_on_latency"]; n.IsDefined())      cfg.dropOnLatency = n.as<bool>();
        if (const YAML::Node& n = item["ntp_sync"]; n.IsDefined())             cfg.ntpSync = n.as<bool>();
        if (const YAML::Node& n = item["buffer_mode"]; n.IsDefined())          cfg.bufferMode = static_cast<BufferMode>(n.as<int>());
        if (const YAML::Node& n = item["decoder"]; n.IsDefined())              cfg.decoder = static_cast<DecoderType>(n.as<int>());
        if (const YAML::Node& n = item["cpu_threads"]; n.IsDefined())          cfg.cpuThreads = n.as<int>();
        if (const YAML::Node& n = item["sync_to_clock"]; n.IsDefined())        cfg.syncToClock = n.as<bool>();
        if (const YAML::Node& n = item["max_buffers"]; n.IsDefined())          cfg.maxBuffers = n.as<int>();
        if (const YAML::Node& n = item["low_latency_mode"]; n.IsDefined())     cfg.lowLatencyMode = n.as<bool>();
        if (const YAML::Node& n = item["use_bgra"]; n.IsDefined())             cfg.useBGRA = n.as<bool>();
        if (const YAML::Node& n = item["auto_hardware_fallback"]; n.IsDefined()) cfg.autoHardwareFallback = n.as<bool>();

        streams.push_back(cfg);
    }

    return true;
}

// ============================================================================
//  节点图持久化辅助方法
// ============================================================================

std::string ConfigSerializer::EmitNodeGraphYaml(const std::string& nodeGraph)
{
    // node_graph is already a YAML string produced by NodeEditor::GetGraphYaml().
    // We pass it through — it gets stored as YAML::Literal inside EmitRobotConfig.
    return nodeGraph;
}

bool ConfigSerializer::ApplyNodeGraphYaml(const YAML::Node& node, std::string& nodeGraph)
{
    if (node.IsDefined() && node.IsScalar())
    {
        nodeGraph = node.as<std::string>();
        return true;
    }
    return false;
}

// ============================================================================
//  文件对话框（Win32 封装）
// ============================================================================

std::string ConfigSerializer::OpenFileDialog(const char* filter)
{
    HWND hwnd = GetActiveWindow();
    char filePath[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = sizeof(filePath);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn))
        return filePath;
    return "";
}

std::string ConfigSerializer::SaveFileDialog(const char* filter, const char* defaultExt)
{
    HWND hwnd = GetActiveWindow();
    char filePath[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = sizeof(filePath);
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn))
        return filePath;
    return "";
}
