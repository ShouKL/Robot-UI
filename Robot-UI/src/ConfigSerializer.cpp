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
                            const UIState& uiState,
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
        EmitUIState(out, uiState);

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
                            UIState& uiState,
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

        if (const YAML::Node& uiNode = cfg["ui_state"]; uiNode.IsDefined())
        {
            if (!ApplyUIState(uiNode, uiState, outError))
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

        // Motion 组件
        out << YAML::Key << "has_motion" << YAML::Value << mode.actuator_config.has_motion;

        // Sensor 组件
        out << YAML::Key << "has_temperature" << YAML::Value << mode.has_temperature;
        out << YAML::Key << "has_humidity" << YAML::Value << mode.has_humidity;
        out << YAML::Key << "has_depth" << YAML::Value << mode.has_depth;
        out << YAML::Key << "temp_encoding"  << YAML::Value << static_cast<int>(mode.temp_encoding);
        out << YAML::Key << "hum_encoding"   << YAML::Value << static_cast<int>(mode.hum_encoding);
        out << YAML::Key << "depth_encoding" << YAML::Value << static_cast<int>(mode.depth_encoding);

        out << YAML::Key << "brushless_motors" << YAML::Value << YAML::BeginSeq;
        for (const auto& pair : mode.actuator_config.brushlessmotor)
        {
            const auto& m = pair.second;
            out << YAML::BeginMap;
            out << YAML::Key << "id" << YAML::Value << m.id;
            if (!m.name.empty())
                out << YAML::Key << "name" << YAML::Value << m.name;
            out << YAML::Key << "target_speed" << YAML::Value << m.target_speed;
            out << YAML::Key << "target_speed_enc" << YAML::Value << static_cast<int>(m.target_speed.encoding);
            out << YAML::Key << "np_mid" << YAML::Value << m.curve.np_mid;
            out << YAML::Key << "np_mid_enc" << YAML::Value << static_cast<int>(m.curve.np_mid.encoding);
            out << YAML::Key << "np_ini" << YAML::Value << m.curve.np_ini;
            out << YAML::Key << "np_ini_enc" << YAML::Value << static_cast<int>(m.curve.np_ini.encoding);
            out << YAML::Key << "pp_ini" << YAML::Value << m.curve.pp_ini;
            out << YAML::Key << "pp_ini_enc" << YAML::Value << static_cast<int>(m.curve.pp_ini.encoding);
            out << YAML::Key << "pp_mid" << YAML::Value << m.curve.pp_mid;
            out << YAML::Key << "pp_mid_enc" << YAML::Value << static_cast<int>(m.curve.pp_mid.encoding);
            out << YAML::Key << "nt_end" << YAML::Value << m.curve.nt_end;
            out << YAML::Key << "nt_end_enc" << YAML::Value << static_cast<int>(m.curve.nt_end.encoding);
            out << YAML::Key << "nt_mid" << YAML::Value << m.curve.nt_mid;
            out << YAML::Key << "nt_mid_enc" << YAML::Value << static_cast<int>(m.curve.nt_mid.encoding);
            out << YAML::Key << "pt_mid" << YAML::Value << m.curve.pt_mid;
            out << YAML::Key << "pt_mid_enc" << YAML::Value << static_cast<int>(m.curve.pt_mid.encoding);
            out << YAML::Key << "pt_end" << YAML::Value << m.curve.pt_end;
            out << YAML::Key << "pt_end_enc" << YAML::Value << static_cast<int>(m.curve.pt_end.encoding);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "servos" << YAML::Value << YAML::BeginSeq;
        for (const auto& pair : mode.actuator_config.servo)
        {
            const auto& s = pair.second;
            out << YAML::BeginMap;
            out << YAML::Key << "id" << YAML::Value << s.id;
            if (!s.name.empty())
                out << YAML::Key << "name" << YAML::Value << s.name;
            out << YAML::Key << "angle" << YAML::Value << s.angle;
            out << YAML::Key << "angle_enc" << YAML::Value << static_cast<int>(s.angle.encoding);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;  // servos

        // Motion 数据
        if (mode.actuator_config.has_motion) {
            const auto& mc = mode.actuator_config.motion;
            out << YAML::Key << "motion" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "x"  << YAML::Value << mc.x;  out << YAML::Key << "x_enc"  << YAML::Value << static_cast<int>(mc.x.encoding);
            out << YAML::Key << "y"  << YAML::Value << mc.y;  out << YAML::Key << "y_enc"  << YAML::Value << static_cast<int>(mc.y.encoding);
            out << YAML::Key << "z"  << YAML::Value << mc.z;  out << YAML::Key << "z_enc"  << YAML::Value << static_cast<int>(mc.z.encoding);
            out << YAML::Key << "rx" << YAML::Value << mc.rx; out << YAML::Key << "rx_enc" << YAML::Value << static_cast<int>(mc.rx.encoding);
            out << YAML::Key << "ry" << YAML::Value << mc.ry; out << YAML::Key << "ry_enc" << YAML::Value << static_cast<int>(mc.ry.encoding);
            out << YAML::Key << "rz" << YAML::Value << mc.rz; out << YAML::Key << "rz_enc" << YAML::Value << static_cast<int>(mc.rz.encoding);
            out << YAML::EndMap;
        }

        // 节点图数据
        if (!mode.node_graph.empty())
            out << YAML::Key << "node_graph" << YAML::Value << YAML::Literal << mode.node_graph;

        // 协议发送配置
        {
            const auto& p = mode.protocol_send;
            out << YAML::Key << "protocol_send" << YAML::Value << YAML::BeginMap;

            out << YAML::Key << "include_length" << YAML::Value << p.include_length;
            out << YAML::Key << "checksum" << YAML::Value << static_cast<int>(p.checksum);

            auto emitBytes = [&](const char* key, const std::vector<uint8_t>& bytes) {
                out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
                for (auto b : bytes) out << static_cast<int>(b);
                out << YAML::EndSeq;
            };
            emitBytes("header", p.header);
            emitBytes("tail", p.tail);

            out << YAML::Key << "fields" << YAML::Value << YAML::BeginSeq;
            for (const auto& f : p.fields) {
                out << YAML::BeginMap;
                out << YAML::Key << "name" << YAML::Value << f.name;
                out << YAML::Key << "path" << YAML::Value << f.field_path;
                out << YAML::Key << "encoding" << YAML::Value << static_cast<int>(f.encoding);
                out << YAML::Key << "group" << YAML::Value << f.group;
                out << YAML::Key << "visible" << YAML::Value << f.visible;
                out << YAML::Key << "fix" << YAML::Value << f.fix;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;  // fields

            out << YAML::EndMap;  // protocol_send
        }

        // 协议接收配置
        {
            const auto& pr = mode.protocol_receive;
            out << YAML::Key << "protocol_receive" << YAML::Value << YAML::BeginMap;

            out << YAML::Key << "include_length" << YAML::Value << pr.include_length;
            out << YAML::Key << "checksum" << YAML::Value << static_cast<int>(pr.checksum);
            out << YAML::Key << "msg_type" << YAML::Value << static_cast<int>(pr.msg_type);

            auto emitBytes = [&](const char* key, const std::vector<uint8_t>& bytes) {
                out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
                for (auto b : bytes) out << static_cast<int>(b);
                out << YAML::EndSeq;
            };
            emitBytes("header", pr.header);
            emitBytes("tail", pr.tail);

            out << YAML::Key << "fields" << YAML::Value << YAML::BeginSeq;
            for (const auto& f : pr.fields) {
                out << YAML::BeginMap;
                out << YAML::Key << "name" << YAML::Value << f.name;
                out << YAML::Key << "path" << YAML::Value << f.field_path;
                out << YAML::Key << "encoding" << YAML::Value << static_cast<int>(f.encoding);
                out << YAML::Key << "group" << YAML::Value << f.group;
                out << YAML::Key << "visible" << YAML::Value << f.visible;
                out << YAML::Key << "fix" << YAML::Value << f.fix;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;  // fields

            out << YAML::EndMap;  // protocol_receive
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

        // Motion 组件
        if (const YAML::Node& n = item["has_motion"]; n.IsDefined())      mode.actuator_config.has_motion = n.as<bool>();

        // Sensor 组件
        if (const YAML::Node& n = item["has_temperature"]; n.IsDefined()) mode.has_temperature = n.as<bool>();
        if (const YAML::Node& n = item["has_humidity"]; n.IsDefined())    mode.has_humidity = n.as<bool>();
        if (const YAML::Node& n = item["has_depth"]; n.IsDefined())       mode.has_depth = n.as<bool>();
        if (const YAML::Node& n = item["temp_encoding"]; n.IsDefined())   mode.temp_encoding  = static_cast<DataEncoding>(n.as<int>());
        if (const YAML::Node& n = item["hum_encoding"]; n.IsDefined())    mode.hum_encoding   = static_cast<DataEncoding>(n.as<int>());
        if (const YAML::Node& n = item["depth_encoding"]; n.IsDefined())  mode.depth_encoding = static_cast<DataEncoding>(n.as<int>());

        // 读取节点图
        if (const YAML::Node& n = item["node_graph"]; n.IsDefined() && n.IsScalar())
            mode.node_graph = n.as<std::string>();

        // 读取协议发送配置
        const YAML::Node& ps = item["protocol_send"];
        if (ps.IsDefined() && ps.IsMap())
        {
            auto& p = mode.protocol_send;

            auto readBytes = [](const YAML::Node& seq) -> std::vector<uint8_t> {
                std::vector<uint8_t> bytes;
                if (seq.IsDefined() && seq.IsSequence())
                    for (const auto& v : seq)
                        bytes.push_back(static_cast<uint8_t>(v.as<int>()));
                return bytes;
            };

            if (const YAML::Node& n = ps["include_length"]; n.IsDefined())
                p.include_length = n.as<bool>();
            if (const YAML::Node& n = ps["checksum"]; n.IsDefined())
                p.checksum = static_cast<ChecksumType>(n.as<int>());
            p.header = readBytes(ps["header"]);
            p.tail   = readBytes(ps["tail"]);

            const YAML::Node& fieldsNode = ps["fields"];
            if (fieldsNode.IsDefined() && fieldsNode.IsSequence())
            {
                p.fields.clear();
                for (const auto& fItem : fieldsNode)
                {
                    SendField sf;
                    if (const YAML::Node& n = fItem["name"]; n.IsDefined())
                        sf.name = n.as<std::string>();
                    if (const YAML::Node& n = fItem["path"]; n.IsDefined())
                        sf.field_path = n.as<std::string>();
                    if (const YAML::Node& n = fItem["encoding"]; n.IsDefined())
                        sf.encoding = static_cast<DataEncoding>(n.as<int>());
                    if (const YAML::Node& n = fItem["group"]; n.IsDefined())
                        sf.group = n.as<std::string>();
                    if (const YAML::Node& n = fItem["visible"]; n.IsDefined())
                        sf.visible = n.as<bool>();
                    if (const YAML::Node& n = fItem["fix"]; n.IsDefined())
                        sf.fix = n.as<bool>();
                    p.fields.push_back(sf);
                }
            }
        }

        // 读取协议接收配置
        const YAML::Node& pr = item["protocol_receive"];
        if (pr.IsDefined() && pr.IsMap())
        {
            auto& rc = mode.protocol_receive;

            auto readBytes = [](const YAML::Node& seq) -> std::vector<uint8_t> {
                std::vector<uint8_t> bytes;
                if (seq.IsDefined() && seq.IsSequence())
                    for (const auto& v : seq)
                        bytes.push_back(static_cast<uint8_t>(v.as<int>()));
                return bytes;
            };

            if (const YAML::Node& n = pr["include_length"]; n.IsDefined())
                rc.include_length = n.as<bool>();
            if (const YAML::Node& n = pr["checksum"]; n.IsDefined())
                rc.checksum = static_cast<ChecksumType>(n.as<int>());
            if (const YAML::Node& n = pr["msg_type"]; n.IsDefined())
                rc.msg_type = static_cast<uint8_t>(n.as<int>());
            rc.header = readBytes(pr["header"]);
            rc.tail   = readBytes(pr["tail"]);

            const YAML::Node& rfieldsNode = pr["fields"];
            if (rfieldsNode.IsDefined() && rfieldsNode.IsSequence())
            {
                rc.fields.clear();
                for (const auto& fItem : rfieldsNode)
                {
                    ReceiveField rf;
                    if (const YAML::Node& n = fItem["name"]; n.IsDefined())
                        rf.name = n.as<std::string>();
                    if (const YAML::Node& n = fItem["path"]; n.IsDefined())
                        rf.field_path = n.as<std::string>();
                    if (const YAML::Node& n = fItem["encoding"]; n.IsDefined())
                        rf.encoding = static_cast<DataEncoding>(n.as<int>());
                    if (const YAML::Node& n = fItem["group"]; n.IsDefined())
                        rf.group = n.as<std::string>();
                    if (const YAML::Node& n = fItem["visible"]; n.IsDefined())
                        rf.visible = n.as<bool>();
                    if (const YAML::Node& n = fItem["fix"]; n.IsDefined())
                        rf.fix = n.as<bool>();
                    rc.fields.push_back(rf);
                }
            }
        }

        mode.actuator_config.brushlessmotor.clear();
        const YAML::Node& motors = item["brushless_motors"];
        if (motors.IsDefined() && motors.IsSequence())
        {
            for (const auto& mItem : motors)
            {
                BrushlessMotor bm;
                if (const YAML::Node& n = mItem["id"]; n.IsDefined())           bm.id = n.as<int>();
                if (const YAML::Node& n = mItem["name"]; n.IsDefined())         bm.name = n.as<std::string>();
                if (const YAML::Node& n = mItem["target_speed"]; n.IsDefined())  bm.target_speed = n.as<double>();
                if (const YAML::Node& n = mItem["target_speed_enc"]; n.IsDefined()) bm.target_speed.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["np_mid"]; n.IsDefined())  bm.curve.np_mid = n.as<double>();
                if (const YAML::Node& n = mItem["np_mid_enc"]; n.IsDefined()) bm.curve.np_mid.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["np_ini"]; n.IsDefined())  bm.curve.np_ini = n.as<double>();
                if (const YAML::Node& n = mItem["np_ini_enc"]; n.IsDefined()) bm.curve.np_ini.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["pp_ini"]; n.IsDefined())  bm.curve.pp_ini = n.as<double>();
                if (const YAML::Node& n = mItem["pp_ini_enc"]; n.IsDefined()) bm.curve.pp_ini.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["pp_mid"]; n.IsDefined())  bm.curve.pp_mid = n.as<double>();
                if (const YAML::Node& n = mItem["pp_mid_enc"]; n.IsDefined()) bm.curve.pp_mid.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["nt_end"]; n.IsDefined())  bm.curve.nt_end = n.as<double>();
                if (const YAML::Node& n = mItem["nt_end_enc"]; n.IsDefined()) bm.curve.nt_end.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["nt_mid"]; n.IsDefined())  bm.curve.nt_mid = n.as<double>();
                if (const YAML::Node& n = mItem["nt_mid_enc"]; n.IsDefined()) bm.curve.nt_mid.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["pt_mid"]; n.IsDefined())  bm.curve.pt_mid = n.as<double>();
                if (const YAML::Node& n = mItem["pt_mid_enc"]; n.IsDefined()) bm.curve.pt_mid.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["pt_end"]; n.IsDefined())  bm.curve.pt_end = n.as<double>();
                if (const YAML::Node& n = mItem["pt_end_enc"]; n.IsDefined()) bm.curve.pt_end.encoding = static_cast<DataEncoding>(n.as<int>());
                // Backward compat: old single "encoding" key
                if (const YAML::Node& n = mItem["encoding"]; n.IsDefined()) {
                    DataEncoding oldEnc = static_cast<DataEncoding>(n.as<int>());
                    bm.target_speed.encoding = oldEnc;
                    bm.curve.np_mid.encoding = oldEnc;
                    bm.curve.np_ini.encoding = oldEnc;
                    bm.curve.pp_ini.encoding = oldEnc;
                    bm.curve.pp_mid.encoding = oldEnc;
                    bm.curve.nt_end.encoding = oldEnc;
                    bm.curve.nt_mid.encoding = oldEnc;
                    bm.curve.pt_mid.encoding = oldEnc;
                    bm.curve.pt_end.encoding = oldEnc;
                }
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
                if (const YAML::Node& n = sItem["id"]; n.IsDefined())      sv.id = n.as<int>();
                if (const YAML::Node& n = sItem["name"]; n.IsDefined())    sv.name = n.as<std::string>();
                if (const YAML::Node& n = sItem["angle"]; n.IsDefined())   sv.angle = n.as<double>();
                if (const YAML::Node& n = sItem["angle_enc"]; n.IsDefined()) sv.angle.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = sItem["encoding"]; n.IsDefined()) sv.angle.encoding = static_cast<DataEncoding>(n.as<int>());
                mode.actuator_config.servo[sv.id] = sv;
            }
        }

        // 读取 Motion 数据
        const YAML::Node& motionNode = item["motion"];
        if (motionNode.IsDefined() && motionNode.IsMap()) {
            auto& mc = mode.actuator_config.motion;
            if (const YAML::Node& n = motionNode["x"]; n.IsDefined())  mc.x = n.as<double>();
            if (const YAML::Node& n = motionNode["x_enc"]; n.IsDefined())  mc.x.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = motionNode["y"]; n.IsDefined())  mc.y = n.as<double>();
            if (const YAML::Node& n = motionNode["y_enc"]; n.IsDefined())  mc.y.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = motionNode["z"]; n.IsDefined())  mc.z = n.as<double>();
            if (const YAML::Node& n = motionNode["z_enc"]; n.IsDefined())  mc.z.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = motionNode["rx"]; n.IsDefined()) mc.rx = n.as<double>();
            if (const YAML::Node& n = motionNode["rx_enc"]; n.IsDefined()) mc.rx.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = motionNode["ry"]; n.IsDefined()) mc.ry = n.as<double>();
            if (const YAML::Node& n = motionNode["ry_enc"]; n.IsDefined()) mc.ry.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = motionNode["rz"]; n.IsDefined()) mc.rz = n.as<double>();
            if (const YAML::Node& n = motionNode["rz_enc"]; n.IsDefined()) mc.rz.encoding = static_cast<DataEncoding>(n.as<int>());
        }

        loadedModes.push_back(mode);
    }

    // 清理旧模式（保留一个位置给 loadedModes[0]）
    while (static_cast<int>(editModes.size()) > 1)
        config.DeleteMode(0);

    if (!loadedModes.empty())
    {
        // 如果编辑列表为空（首次启动加载），先添加一个空模式占位
        if (editModes.empty())
            config.AddMode();

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
//  UI 状态持久化辅助方法
// ============================================================================

void ConfigSerializer::EmitUIState(YAML::Emitter& out, const UIState& uiState)
{
    out << YAML::Key << "ui_state" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "about_open"               << YAML::Value << uiState.about_open;
    out << YAML::Key << "option_open"              << YAML::Value << uiState.option_open;
    out << YAML::Key << "simulation_open"          << YAML::Value << uiState.simulation_open;
    out << YAML::Key << "live_streamer_open"       << YAML::Value << uiState.live_streamer_open;
    out << YAML::Key << "robot_status_open"        << YAML::Value << uiState.robot_status_open;
    out << YAML::Key << "node_editor_open"         << YAML::Value << uiState.node_editor_open;
    out << YAML::Key << "thrust_curve_editor_open" << YAML::Value << uiState.thrust_curve_editor_open;
    out << YAML::Key << "robot_active_mode"        << YAML::Value << uiState.robot_active_mode;
    out << YAML::Key << "gamepad_active_mode"      << YAML::Value << uiState.gamepad_active_mode;
    out << YAML::EndMap;  // ui_state
}

bool ConfigSerializer::ApplyUIState(const YAML::Node& node, UIState& uiState, std::string* outError)
{
    (void)outError;
    if (const YAML::Node& n = node["about_open"];               n.IsDefined()) uiState.about_open               = n.as<bool>();
    if (const YAML::Node& n = node["option_open"];              n.IsDefined()) uiState.option_open              = n.as<bool>();
    if (const YAML::Node& n = node["simulation_open"];          n.IsDefined()) uiState.simulation_open          = n.as<bool>();
    if (const YAML::Node& n = node["live_streamer_open"];       n.IsDefined()) uiState.live_streamer_open       = n.as<bool>();
    if (const YAML::Node& n = node["robot_status_open"];        n.IsDefined()) uiState.robot_status_open        = n.as<bool>();
    if (const YAML::Node& n = node["node_editor_open"];         n.IsDefined()) uiState.node_editor_open         = n.as<bool>();
    if (const YAML::Node& n = node["thrust_curve_editor_open"]; n.IsDefined()) uiState.thrust_curve_editor_open = n.as<bool>();
    if (const YAML::Node& n = node["robot_active_mode"];        n.IsDefined()) uiState.robot_active_mode        = n.as<int>();
    if (const YAML::Node& n = node["gamepad_active_mode"];      n.IsDefined()) uiState.gamepad_active_mode      = n.as<int>();
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
