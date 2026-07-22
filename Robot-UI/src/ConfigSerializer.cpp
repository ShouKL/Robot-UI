#include "ConfigSerializer.h"

bool ConfigSerializer::Save(const std::string& filepath,
                            const RobotComponentManager& robotMgr,
                            const GamepadMapperManager& gamepadMgr,
                            const ImGuiStyleManager& styleManager,
                            const std::vector<LiveStream>& streams,
                            const UIState& uiState,
                            const ThrustCurve* editorCurve,
                            const std::vector<RobotComm>& commConfigs,
                            const std::map<std::string, std::string>* graphMap,
                            const std::vector<NodeGraph>* graphItems,
                            std::string* outError)
{
    try
    {
        WL_INFO_TAG("component", "Saving component: {}", filepath);

        // graphMap 已由 graph_items 独立管理，不再同步到 RobotComponent

        YAML::Emitter out;
        out << YAML::BeginMap;

        out << YAML::Key << "robot_ui_config" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "version" << YAML::Value << 1;

        EmitRobotConfig(out, robotMgr);
        EmitGamepadMapper(out, gamepadMgr);
        EmitStreams(out, streams);
        if (editorCurve) EmitEditorCurve(out, *editorCurve);
        EmitRobotComm(out, commConfigs);
        if (graphItems) EmitGraphItems(out, *graphItems);

        out << YAML::EndMap;  // robot_ui_config
        out << YAML::EndMap;  // root

        if (!out.good())
        {
            if (outError) *outError = "YAML emit error: " + out.GetLastError();
            WL_ERROR_TAG("component", "YAML emit error: {}", out.GetLastError());
            return false;
        }

        std::ofstream ofs(filepath, std::ios::out | std::ios::binary);
        if (!ofs.is_open())
        {
            if (outError) *outError = "Cannot open file for writing: " + filepath;
            WL_ERROR_TAG("component", "Cannot open file for writing: {}", filepath);
            return false;
        }
        ofs << out.c_str();
        ofs.close();
        return true;
    }
    catch (const YAML::Exception& e)
    {
        if (outError) *outError = std::string("Save YAML exception: ") + e.what();
        WL_ERROR_TAG("component", "Save YAML exception: {}", e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        if (outError) *outError = std::string("Save exception: ") + e.what();
        WL_ERROR_TAG("component", "Save exception: {}", e.what());
        return false;
    }
}

// ============================================================================
//  Kernel 文件（.kernel） — 样式 + UI 状态（自动保存）
// ============================================================================

bool ConfigSerializer::SaveKernel(const std::string& filepath,
                                  const ImGuiStyleManager& styleManager,
                                  const UIState& uiState,
                                  std::string* outError)
{
    try
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "robot_ui_kernel" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "version" << YAML::Value << 1;

        EmitStyle(out, styleManager);
        EmitUIState(out, uiState);
        if (!uiState.software_shortcuts_yaml.empty())
            EmitSoftwareShortcuts(out, uiState.software_shortcuts_yaml);

        out << YAML::EndMap;  // robot_ui_kernel
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
        if (outError) *outError = std::string("SaveKernel YAML exception: ") + e.what();
        return false;
    }
    catch (const std::exception& e)
    {
        if (outError) *outError = std::string("SaveKernel exception: ") + e.what();
        return false;
    }
}

bool ConfigSerializer::LoadKernel(const std::string& filepath,
                                  ImGuiStyleManager& styleManager,
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

        const YAML::Node& cfg = doc["robot_ui_kernel"];
        if (!cfg.IsDefined() || cfg.IsNull())
        {
            if (outError) *outError = "Missing top-level key: robot_ui_kernel";
            return false;
        }

        if (const YAML::Node& styleNode = cfg["style"]; styleNode.IsDefined())
        {
            if (!ApplyStyle(styleNode, styleManager, outError))
                return false;
        }

        if (const YAML::Node& uiNode = cfg["ui_state"]; uiNode.IsDefined())
        {
            if (!ApplyUIState(uiNode, uiState, outError))
                return false;
        }

        if (const YAML::Node& swNode = cfg["software_shortcuts"]; swNode.IsDefined())
            ApplySoftwareShortcuts(swNode, uiState.software_shortcuts_yaml, outError);

        return true;
    }
    catch (const YAML::BadFile& e)
    {
        if (outError) *outError = std::string("Cannot open kernel file: ") + e.what();
        return false;
    }
    catch (const YAML::Exception& e)
    {
        if (outError) *outError = std::string("LoadKernel YAML exception: ") + e.what();
        return false;
    }
    catch (const std::exception& e)
    {
        if (outError) *outError = std::string("LoadKernel exception: ") + e.what();
        return false;
    }
}

bool ConfigSerializer::Load(const std::string& filepath,
                            RobotComponentManager& robotMgr,
                            GamepadMapperManager& gamepadMgr,
                            ImGuiStyleManager& styleManager,
                            std::vector<LiveStream>& streams,
                            UIState& uiState,
                            ThrustCurve* editorCurve,
                            std::vector<RobotComm>* commConfigs,
                            std::map<std::string, std::string>* graphMap,
                            std::vector<NodeGraph>* graphItems,
                            std::string* outError)
{
    try
    {
        WL_INFO_TAG("component", "Loading component: {}", filepath);
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
            if (!ApplyRobotConfig(robotNode, robotMgr, outError))
                return false;
        }

        if (const YAML::Node& gamepadNode = cfg["gamepad"]; gamepadNode.IsDefined())
        {
            if (!ApplyGamepadMapper(gamepadNode, gamepadMgr, outError))
                return false;
        }

        if (const YAML::Node& streamsNode = cfg["streams"]; streamsNode.IsDefined())
        {
            if (!ApplyStreams(streamsNode, streams, outError))
                return false;
        }

        if (editorCurve)
        {
            if (const YAML::Node& curveNode = cfg["thrust_curve_editor"]; curveNode.IsDefined())
            {
                if (!ApplyEditorCurve(curveNode, *editorCurve, outError))
                    return false;
            }
        }

        if (commConfigs)
        {
            if (const YAML::Node& commNode = cfg["robot_comm"]; commNode.IsDefined())
            {
                if (!ApplyRobotComm(commNode, *commConfigs, outError))
                    return false;
            }
        }

        // graphMap 已由 NodeGraphManager 独立管理（graph_items），不再从 RobotComponent 构建
        if (graphItems)
        {
            if (const YAML::Node& giNode = cfg["graph_items"]; giNode.IsDefined())
            {
                if (!ApplyGraphItems(giNode, *graphItems, outError))
                    return false;
            }
        }

        return true;
    }
    catch (const YAML::BadFile& e)
    {
        if (outError) *outError = std::string("Cannot open file: ") + e.what();
        WL_ERROR_TAG("component", "Cannot open component file: {}", e.what());
        return false;
    }
    catch (const YAML::Exception& e)
    {
        if (outError) *outError = std::string("Load YAML exception: ") + e.what();
        WL_ERROR_TAG("component", "Load YAML exception: {}", e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        if (outError) *outError = std::string("Load exception: ") + e.what();
        WL_ERROR_TAG("component", "Load exception: {}", e.what());
        return false;
    }
}

// ============================================================================
//  YAML 写入（基于 yaml-cpp Emitter）
// ============================================================================

void ConfigSerializer::EmitRobotConfig(YAML::Emitter& out, const RobotComponentManager& robotMgr)
{
    out << YAML::Key << "robot" << YAML::Value << YAML::BeginMap;

    const auto& comps = robotMgr.GetComponents();

    out << YAML::Key << "items" << YAML::Value << YAML::BeginSeq;
    for (const auto& comp : comps)
    {
        out << YAML::BeginMap;

        out << YAML::Key << "name" << YAML::Value << comp.name;
        // gamepad_mapping 已移至 GamepadMapper 独立管理，不在 Component 中存储
        // host_ip / port / protocol_type 已移至 RobotCommConfig

        // Motion 组件 — now in motions vector above
        (void)comp;

        // Sensor 组件
        out << YAML::Key << "has_temperature" << YAML::Value << comp.sensor_config.has_temperature;
        out << YAML::Key << "has_humidity" << YAML::Value << comp.sensor_config.has_humidity;
        out << YAML::Key << "has_depth" << YAML::Value << comp.sensor_config.has_depth;

        out << YAML::Key << "brushless_motors" << YAML::Value << YAML::BeginSeq;
        for (const auto& m : comp.actuator_config.brushlessmotor)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "id" << YAML::Value << m.id;
            if (!m.name.empty())
                out << YAML::Key << "name" << YAML::Value << m.name;
            out << YAML::Key << "target_speed" << YAML::Value << m.target_speed;
            out << YAML::Key << "target_speed_enc" << YAML::Value << static_cast<int>(m.target_speed.encoding);
            out << YAML::Key << "target_position" << YAML::Value << m.target_position;
            out << YAML::Key << "target_position_enc" << YAML::Value << static_cast<int>(m.target_position.encoding);
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
            out << YAML::Key << "pwm_min" << YAML::Value << m.curve.pwm_min;
            out << YAML::Key << "pwm_max" << YAML::Value << m.curve.pwm_max;
            // Raw user points
            if (!m.curve.raw_thrust.empty()) {
                out << YAML::Key << "raw_thrust" << YAML::Value << YAML::Flow << m.curve.raw_thrust;
                out << YAML::Key << "raw_pwm" << YAML::Value << YAML::Flow << m.curve.raw_pwm;
            }
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "servos" << YAML::Value << YAML::BeginSeq;
        for (const auto& s : comp.actuator_config.servo)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "id" << YAML::Value << s.id;
            if (!s.name.empty())
                out << YAML::Key << "name" << YAML::Value << s.name;
            out << YAML::Key << "angle" << YAML::Value << s.angle;
            out << YAML::Key << "angle_enc" << YAML::Value << static_cast<int>(s.angle.encoding);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;  // servos

        // GPIO Pins
        out << YAML::Key << "gpio_pins" << YAML::Value << YAML::BeginSeq;
        for (const auto& gpio : comp.actuator_config.gpio_pins)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "id" << YAML::Value << gpio.id;
            if (!gpio.name.empty())
                out << YAML::Key << "name" << YAML::Value << gpio.name;
            out << YAML::Key << "pin_number" << YAML::Value << gpio.pin_number;
            out << YAML::Key << "value" << YAML::Value << gpio.value;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;  // gpio_pins

        // Motion 组件（向量）
        out << YAML::Key << "motions" << YAML::Value << YAML::BeginSeq;
        for (const auto& mc : comp.actuator_config.motions)
        {
            out << YAML::BeginMap;
            if (!mc.name.empty()) out << YAML::Key << "name" << YAML::Value << mc.name;
            out << YAML::Key << "x"  << YAML::Value << mc.x;  out << YAML::Key << "x_enc"  << YAML::Value << static_cast<int>(mc.x.encoding);
            out << YAML::Key << "y"  << YAML::Value << mc.y;  out << YAML::Key << "y_enc"  << YAML::Value << static_cast<int>(mc.y.encoding);
            out << YAML::Key << "z"  << YAML::Value << mc.z;  out << YAML::Key << "z_enc"  << YAML::Value << static_cast<int>(mc.z.encoding);
            out << YAML::Key << "rx" << YAML::Value << mc.rx; out << YAML::Key << "rx_enc" << YAML::Value << static_cast<int>(mc.rx.encoding);
            out << YAML::Key << "ry" << YAML::Value << mc.ry; out << YAML::Key << "ry_enc" << YAML::Value << static_cast<int>(mc.ry.encoding);
            out << YAML::Key << "rz" << YAML::Value << mc.rz; out << YAML::Key << "rz_enc" << YAML::Value << static_cast<int>(mc.rz.encoding);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        // 协议发送/接收配置已移至 RobotCommConfig（独立序列化）

        out << YAML::EndMap;  // item
    }
    out << YAML::EndSeq;  // items

    out << YAML::Key << "active_mode" << YAML::Value << robotMgr.GetSelectedIndex();
    out << YAML::EndMap;
}

void ConfigSerializer::EmitGamepadMapper(YAML::Emitter& out, const GamepadMapperManager& gamepadMgr)
{
    out << YAML::Key << "gamepad" << YAML::Value << YAML::BeginMap;

    auto mappers = gamepadMgr.GetAllItems();

    out << YAML::Key << "items" << YAML::Value << YAML::BeginSeq;
    for (const auto& item : mappers)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "name" << YAML::Value << item.name;
        out << YAML::Key << "gamepad_type" << YAML::Value << static_cast<int>(item.gamepad_type);

        // 保存自定义键位定义（keys）
        out << YAML::Key << "keys" << YAML::Value << YAML::BeginSeq;
        for (const auto& k : item.keys)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "key_id" << YAML::Value << k.id;
            out << YAML::Key << "key_name" << YAML::Value << k.name;
            out << YAML::Key << "analog" << YAML::Value << k.is_analog;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "mappings" << YAML::Value << YAML::BeginSeq;
        for (const auto& m : item.mappings)
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

    out << YAML::Key << "active_mode" << YAML::Value << gamepadMgr.GetSelectedIndex();
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

void ConfigSerializer::EmitStreams(YAML::Emitter& out, const std::vector<LiveStream>& configs)
{
    out << YAML::Key << "streams" << YAML::Value << YAML::BeginSeq;
    for (const auto& c : configs)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "id"                 << YAML::Value << c.id;
        out << YAML::Key << "is_streaming"       << YAML::Value << c.isStreaming;
        out << YAML::Key << "is_selected"        << YAML::Value << c.isSelected;
        out << YAML::Key << "name"               << YAML::Value << c.name;
        out << YAML::Key << "ip"                 << YAML::Value << c.ip;
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
    strncpy_s(dst, dstSize, src.c_str(), dstSize - 1);
}

bool ConfigSerializer::ApplyRobotConfig(const YAML::Node& robotNode, RobotComponentManager& robotMgr, std::string* outError)
{
    const YAML::Node& modesNode = robotNode["items"];
    if (!modesNode.IsDefined() || !modesNode.IsSequence())
    {
        if (outError) *outError = "robot section missing 'items'";
        return false;
    }
    if (modesNode.size() == 0)
    {
        if (outError) *outError = "robot items list is empty";
        return false;
    }

    std::vector<RobotMode> loadedModes;

    for (const auto& modeNode : modesNode)
    {
        RobotMode mode;

        auto readStr = [&](const char* key, char* dst, size_t dstSize) {
            const YAML::Node& n = modeNode[key];
            if (n.IsDefined() && n.IsScalar())
                SafeStrCpy(dst, dstSize, n.as<std::string>());
        };
        auto readStdStr = [&](const char* key, std::string& out) {
            const YAML::Node& n = modeNode[key];
            if (n.IsDefined() && n.IsScalar())
                out = n.as<std::string>();
        };

        readStr("name", mode.name, sizeof(mode.name));
        readStdStr("gamepad_mapping", mode.gamepad_mapping_Mode);
        readStdStr("host_ip", mode.host_ip);

        if (const YAML::Node& n = modeNode["remote_port"]; n.IsDefined()) mode.remote_port = n.as<int>();
        if (const YAML::Node& n = modeNode["local_port"]; n.IsDefined())  mode.local_port = n.as<int>();
        if (const YAML::Node& n = modeNode["protocol_type"]; n.IsDefined()) mode.protocol_type = n.as<int>();

        // Motion 组件flag
        if (const YAML::Node& n = modeNode["has_motion"]; n.IsDefined() && n.as<bool>() && mode.actuator_config.motions.empty()) {
            MotionControl mc;
            mode.actuator_config.motions.push_back(mc);
        }
        if (const YAML::Node& n = modeNode["has_temperature"]; n.IsDefined()) mode.sensor_config.has_temperature = n.as<bool>();
        if (const YAML::Node& n = modeNode["has_humidity"]; n.IsDefined())    mode.sensor_config.has_humidity = n.as<bool>();
        if (const YAML::Node& n = modeNode["has_depth"]; n.IsDefined())       mode.sensor_config.has_depth = n.as<bool>();

        // 读取节点图
        if (const YAML::Node& n = modeNode["node_graph"]; n.IsDefined() && n.IsScalar())
            mode.node_graph = n.as<std::string>();

        // 读取节点图组合数据 (gamepadModeName → graph yaml)
        if (const YAML::Node& pairs = modeNode["node_graph_pairs"]; pairs.IsDefined() && pairs.IsMap()) {
            for (auto it = pairs.begin(); it != pairs.end(); ++it) {
                std::string gpName = it->first.as<std::string>();
                std::string graph = it->second.as<std::string>();
                mode.node_graph_pairs[gpName] = graph;
            }
        }
        // 兼容旧版：如果没有 node_graph_pairs，用 node_graph 填充
        if (mode.node_graph_pairs.empty() && !mode.node_graph.empty()) {
            mode.node_graph_pairs[mode.gamepad_mapping_Mode] = mode.node_graph;
        }

        // 读取协议发送配置（支持多帧新格式和单帧旧格式）
        const YAML::Node& ps = modeNode["protocol_send"];
        if (ps.IsDefined())
        {
            mode.protocol_send.clear();
            auto readSendCfg = [](const YAML::Node& cfgNode, ProtocolSendConfig& p) {
                auto readBytes = [](const YAML::Node& seq) -> std::vector<uint8_t> {
                    std::vector<uint8_t> bytes;
                    if (seq.IsDefined() && seq.IsSequence())
                        for (const auto& v : seq) bytes.push_back(static_cast<uint8_t>(v.as<int>()));
                    return bytes;
                };
                if (const YAML::Node& n = cfgNode["name"]; n.IsDefined()) { std::string nm = n.as<std::string>(); strncpy_s(p.name, nm.c_str(), sizeof(p.name) - 1); }
                p.command_bytes = readBytes(cfgNode["command_byte"]);
                if (p.command_bytes.empty() && cfgNode["command_byte"].IsDefined() && !cfgNode["command_byte"].IsSequence())
                    p.command_bytes.push_back(static_cast<uint8_t>(cfgNode["command_byte"].as<int>()));
                if (const YAML::Node& n = cfgNode["enabled"]; n.IsDefined()) p.enabled = n.as<bool>();
                if (const YAML::Node& n = cfgNode["include_length"]; n.IsDefined()) p.include_length = n.as<bool>();
                if (const YAML::Node& n = cfgNode["checksum"]; n.IsDefined()) p.checksum = static_cast<ChecksumType>(n.as<int>());
                if (const YAML::Node& n = cfgNode["big_endian"]; n.IsDefined()) p.big_endian = n.as<bool>();
                if (const YAML::Node& n = cfgNode["checksum_range"]; n.IsDefined()) p.checksum_range = static_cast<ChecksumRange>(n.as<int>());
                p.header = readBytes(cfgNode["header"]);
                p.tail   = readBytes(cfgNode["tail"]);
                const YAML::Node& fieldsNode = cfgNode["fields"];
                if (fieldsNode.IsDefined() && fieldsNode.IsSequence()) {
                    p.fields.clear();
                    for (const auto& fItem : fieldsNode) {
                        SendField sf;
                        if (const YAML::Node& n = fItem["name"]; n.IsDefined()) sf.name = n.as<std::string>();
                        if (const YAML::Node& n = fItem["path"]; n.IsDefined()) sf.field_path = n.as<std::string>();
                        if (const YAML::Node& n = fItem["encoding"]; n.IsDefined()) sf.encoding = static_cast<DataEncoding>(n.as<int>());
                        if (const YAML::Node& n = fItem["group"]; n.IsDefined()) sf.group = n.as<std::string>();
                        if (const YAML::Node& n = fItem["visible"]; n.IsDefined()) sf.visible = n.as<bool>();
                        if (const YAML::Node& n = fItem["fix"]; n.IsDefined()) sf.fix = n.as<bool>();
                        if (const YAML::Node& n = fItem["fix_value"]; n.IsDefined()) sf.fix_value = n.as<double>();
                        if (const YAML::Node& n = fItem["raw_data"]; n.IsDefined() && n.IsSequence()) {
                            for (const auto& v : n) sf.raw_data.push_back(static_cast<uint8_t>(v.as<int>()));
                        }
                        p.fields.push_back(sf);
                    }
                }
            };
            if (ps.IsSequence()) { for (const auto& cfgItem : ps) { ProtocolSendConfig p; readSendCfg(cfgItem, p); mode.protocol_send.push_back(std::move(p)); } }
            else if (ps.IsMap()) { ProtocolSendConfig p; readSendCfg(ps, p); mode.protocol_send.push_back(std::move(p)); }
        }

        // 读取协议接收配置
        const YAML::Node& pr = modeNode["protocol_receive"];
        if (pr.IsDefined())
        {
            mode.protocol_receive.clear();
            auto readRecvCfg = [](const YAML::Node& cfgNode, ProtocolReceiveConfig& rc) {
                auto readBytes = [](const YAML::Node& seq) -> std::vector<uint8_t> {
                    std::vector<uint8_t> bytes;
                    if (seq.IsDefined() && seq.IsSequence())
                        for (const auto& v : seq) bytes.push_back(static_cast<uint8_t>(v.as<int>()));
                    return bytes;
                };
                if (const YAML::Node& n = cfgNode["name"]; n.IsDefined()) { std::string nm = n.as<std::string>(); strncpy_s(rc.name, nm.c_str(), sizeof(rc.name) - 1); }
                rc.command_bytes = readBytes(cfgNode["command_byte"]);
                if (rc.command_bytes.empty()) {
                    if (const YAML::Node& n = cfgNode["command_byte"]; n.IsDefined() && !n.IsSequence()) rc.command_bytes.push_back(static_cast<uint8_t>(n.as<int>()));
                    else if (const YAML::Node& n = cfgNode["msg_type"]; n.IsDefined()) rc.command_bytes.push_back(static_cast<uint8_t>(n.as<int>()));
                }
                if (const YAML::Node& n = cfgNode["include_length"]; n.IsDefined()) rc.include_length = n.as<bool>();
                if (const YAML::Node& n = cfgNode["checksum"]; n.IsDefined()) rc.checksum = static_cast<ChecksumType>(n.as<int>());
                if (const YAML::Node& n = cfgNode["big_endian"]; n.IsDefined()) rc.big_endian = n.as<bool>();
                if (const YAML::Node& n = cfgNode["checksum_range"]; n.IsDefined()) rc.checksum_range = static_cast<ChecksumRange>(n.as<int>());
                rc.header = readBytes(cfgNode["header"]);
                rc.tail   = readBytes(cfgNode["tail"]);
                const YAML::Node& fieldsNode = cfgNode["fields"];
                if (fieldsNode.IsDefined() && fieldsNode.IsSequence()) {
                    rc.fields.clear();
                    for (const auto& fItem : fieldsNode) {
                        ReceiveField rf;
                        if (const YAML::Node& n = fItem["name"]; n.IsDefined()) rf.name = n.as<std::string>();
                        if (const YAML::Node& n = fItem["path"]; n.IsDefined()) rf.field_path = n.as<std::string>();
                        if (const YAML::Node& n = fItem["encoding"]; n.IsDefined()) rf.encoding = static_cast<DataEncoding>(n.as<int>());
                        if (const YAML::Node& n = fItem["group"]; n.IsDefined()) rf.group = n.as<std::string>();
                        if (const YAML::Node& n = fItem["visible"]; n.IsDefined()) rf.visible = n.as<bool>();
                        if (const YAML::Node& n = fItem["fix"]; n.IsDefined()) rf.fix = n.as<bool>();
                        rc.fields.push_back(rf);
                    }
                }
            };
            if (pr.IsSequence()) { for (const auto& cfgItem : pr) { ProtocolReceiveConfig rc; readRecvCfg(cfgItem, rc); mode.protocol_receive.push_back(std::move(rc)); } }
            else if (pr.IsMap()) { ProtocolReceiveConfig rc; readRecvCfg(pr, rc); mode.protocol_receive.push_back(std::move(rc)); }
        }

        // 读取传感器配置
        const YAML::Node& motors = modeNode["brushless_motors"];
        if (motors.IsDefined() && motors.IsSequence())
        {
            for (const auto& mItem : motors)
            {
                BrushlessMotor bm;
                if (const YAML::Node& n = mItem["id"]; n.IsDefined())           bm.id = n.as<int>();
                if (const YAML::Node& n = mItem["name"]; n.IsDefined())         bm.name = n.as<std::string>();
                if (const YAML::Node& n = mItem["target_speed"]; n.IsDefined())  bm.target_speed = n.as<double>();
                if (const YAML::Node& n = mItem["target_speed_enc"]; n.IsDefined()) bm.target_speed.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["target_position"]; n.IsDefined())  bm.target_position = n.as<double>();
                if (const YAML::Node& n = mItem["target_position_enc"]; n.IsDefined()) bm.target_position.encoding = static_cast<DataEncoding>(n.as<int>());
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
                if (const YAML::Node& n = mItem["pwm_min"]; n.IsDefined())  bm.curve.pwm_min = n.as<float>();
                if (const YAML::Node& n = mItem["pwm_max"]; n.IsDefined())  bm.curve.pwm_max = n.as<float>();
                // Raw user points
                if (const YAML::Node& rt = mItem["raw_thrust"]; rt.IsDefined() && rt.IsSequence()) {
                    for (const auto& v : rt) bm.curve.raw_thrust.push_back(v.as<double>());
                }
                if (const YAML::Node& rp = mItem["raw_pwm"]; rp.IsDefined() && rp.IsSequence()) {
                    for (const auto& v : rp) bm.curve.raw_pwm.push_back(v.as<float>());
                }
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
                mode.actuator_config.brushlessmotor.push_back(bm);
            }
        }

        mode.actuator_config.servo.clear();
        const YAML::Node& servos = modeNode["servos"];
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
                mode.actuator_config.servo.push_back(sv);
            }
        }

        // 读取 GPIO Pins
        mode.actuator_config.gpio_pins.clear();
        const YAML::Node& gpios = modeNode["gpio_pins"];
        if (gpios.IsDefined() && gpios.IsSequence())
        {
            for (const auto& gItem : gpios)
            {
                GpioPin gpio;
                if (const YAML::Node& n = gItem["id"]; n.IsDefined())         gpio.id = n.as<int>();
                if (const YAML::Node& n = gItem["name"]; n.IsDefined())       gpio.name = n.as<std::string>();
                if (const YAML::Node& n = gItem["pin_number"]; n.IsDefined()) gpio.pin_number = n.as<int>();
                if (const YAML::Node& n = gItem["value"]; n.IsDefined())      gpio.value = n.as<int>();
                mode.actuator_config.gpio_pins.push_back(gpio);
            }
        }

        // Motion 组件（向量）
        if (const YAML::Node& motionsNode = modeNode["motions"]; motionsNode.IsDefined() && motionsNode.IsSequence()) {
            mode.actuator_config.motions.clear();
            for (const auto& mItem : motionsNode) {
                MotionControl mc;
                if (const YAML::Node& n = mItem["name"]; n.IsDefined()) mc.name = n.as<std::string>();
                if (const YAML::Node& n = mItem["x"]; n.IsDefined())  mc.x = n.as<double>();
                if (const YAML::Node& n = mItem["x_enc"]; n.IsDefined())  mc.x.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["y"]; n.IsDefined())  mc.y = n.as<double>();
                if (const YAML::Node& n = mItem["y_enc"]; n.IsDefined())  mc.y.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["z"]; n.IsDefined())  mc.z = n.as<double>();
                if (const YAML::Node& n = mItem["z_enc"]; n.IsDefined())  mc.z.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["rx"]; n.IsDefined()) mc.rx = n.as<double>();
                if (const YAML::Node& n = mItem["rx_enc"]; n.IsDefined()) mc.rx.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["ry"]; n.IsDefined()) mc.ry = n.as<double>();
                if (const YAML::Node& n = mItem["ry_enc"]; n.IsDefined()) mc.ry.encoding = static_cast<DataEncoding>(n.as<int>());
                if (const YAML::Node& n = mItem["rz"]; n.IsDefined()) mc.rz = n.as<double>();
                if (const YAML::Node& n = mItem["rz_enc"]; n.IsDefined()) mc.rz.encoding = static_cast<DataEncoding>(n.as<int>());
                mode.actuator_config.motions.push_back(mc);
            }
        }
        // 向后兼容：旧格式 motion（单对象）
        else if (const YAML::Node& motionNode = modeNode["motion"]; motionNode.IsDefined() && motionNode.IsMap()) {
            MotionControl mc;
            auto& mn = motionNode;
            if (const YAML::Node& n = mn["name"]; n.IsDefined()) mc.name = n.as<std::string>();
            if (const YAML::Node& n = mn["x"]; n.IsDefined())  mc.x = n.as<double>();
            if (const YAML::Node& n = mn["x_enc"]; n.IsDefined())  mc.x.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = mn["y"]; n.IsDefined())  mc.y = n.as<double>();
            if (const YAML::Node& n = mn["y_enc"]; n.IsDefined())  mc.y.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = mn["z"]; n.IsDefined())  mc.z = n.as<double>();
            if (const YAML::Node& n = mn["z_enc"]; n.IsDefined())  mc.z.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = mn["rx"]; n.IsDefined()) mc.rx = n.as<double>();
            if (const YAML::Node& n = mn["rx_enc"]; n.IsDefined()) mc.rx.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = mn["ry"]; n.IsDefined()) mc.ry = n.as<double>();
            if (const YAML::Node& n = mn["ry_enc"]; n.IsDefined()) mc.ry.encoding = static_cast<DataEncoding>(n.as<int>());
            if (const YAML::Node& n = mn["rz"]; n.IsDefined()) mc.rz = n.as<double>();
            if (const YAML::Node& n = mn["rz_enc"]; n.IsDefined()) mc.rz.encoding = static_cast<DataEncoding>(n.as<int>());
            mode.actuator_config.motions.push_back(mc);
        }
        // 读取 Motion 组件flag

        loadedModes.push_back(mode);
    }

    // 使用 LoadComponents 重建组件列表
    int activeIdx = robotMgr.GetSelectedIndex();
    if (const YAML::Node& n = robotNode["active_mode"]; n.IsDefined())
    {
        activeIdx = n.as<int>();
        if (activeIdx < 0 || activeIdx >= static_cast<int>(loadedModes.size()))
            activeIdx = 0;
    }
    robotMgr.LoadComponents(loadedModes, activeIdx);
    return true;
}

bool ConfigSerializer::ApplyGamepadMapper(const YAML::Node& gamepadNode, GamepadMapperManager& gamepadMgr, std::string* outError)
{
    // 读取每个模式自己的 gamepad_type（旧版全局字段仅作 fallback，新版在 item 内）
    GamepadType fallbackType = GamepadType::Xbox;
    if (const YAML::Node& n = gamepadNode["gamepad_type"]; n.IsDefined())
        fallbackType = static_cast<GamepadType>(n.as<int>());

    const YAML::Node& modesNode = gamepadNode["items"];
    if (!modesNode.IsDefined() || !modesNode.IsSequence())
    {
        if (outError) *outError = "gamepad section missing or invalid 'items'";
        return false;
    }

    std::vector<GamepadMapper> loadedModes;
    for (const auto& modeNode : modesNode)
    {
        GamepadMapper mapper;

        if (const YAML::Node& n = modeNode["name"]; n.IsDefined() && n.IsScalar())
        {
            SafeStrCpy(mapper.name, sizeof(mapper.name), n.as<std::string>());
        }
        else
        {
            SafeStrCpy(mapper.name, sizeof(mapper.name), "Default");
        }

        // 读取每个模式的手柄类型（优先 item 内字段，fallback 到全局字段）
        if (const YAML::Node& n = modeNode["gamepad_type"]; n.IsDefined())
            mapper.gamepad_type = static_cast<GamepadType>(n.as<int>());
        else
            mapper.gamepad_type = fallbackType;

        mapper.mappings.clear();
        mapper.keys.clear();

        // 读取自定义键位定义（keys）
        int maxKeyId = 0;
        const YAML::Node& keysNode = modeNode["keys"];
        if (keysNode.IsDefined() && keysNode.IsSequence())
        {
            for (const auto& kItem : keysNode)
            {
                GamepadKey gk;
                if (const YAML::Node& n = kItem["key_id"]; n.IsDefined())   gk.id = n.as<int>();
                if (const YAML::Node& n = kItem["key_name"]; n.IsDefined()) gk.name = n.as<std::string>();
                if (const YAML::Node& n = kItem["analog"]; n.IsDefined())   gk.is_analog = n.as<bool>();
                mapper.keys.push_back(gk);
                if (gk.id > maxKeyId) maxKeyId = gk.id;
            }
        }

        const YAML::Node& mappings = modeNode["mappings"];
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
                mapper.mappings.push_back(km);
                if (km.key_id > maxKeyId) maxKeyId = km.key_id;
            }
        }

        // 如果没有 keys 数据（旧格式），从 mappings 重建 keys
        if (mapper.keys.empty() && !mapper.mappings.empty())
        {
            for (const auto& m : mapper.mappings)
            {
                GamepadKey gk;
                gk.id = m.key_id;
                gk.name = m.key_name;
                gk.is_analog = m.is_analog;
                mapper.keys.push_back(gk);
            }
        }

        loadedModes.push_back(mapper);
    }

    if (loadedModes.empty())
    {
        if (outError) *outError = "gamepad items list is empty";
        return false;
    }

    // 使用 LoadMappers 重建映射列表
    int activeIdx = gamepadMgr.GetSelectedIndex();
    if (const YAML::Node& n = gamepadNode["active_mode"]; n.IsDefined())
    {
        activeIdx = n.as<int>();
        if (activeIdx < 0 || activeIdx >= static_cast<int>(loadedModes.size()))
            activeIdx = 0;
    }
    gamepadMgr.LoadMappers(loadedModes, activeIdx);

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

bool ConfigSerializer::ApplyStreams(const YAML::Node& streamsNode, std::vector<LiveStream>& streams, std::string* outError)
{
    if (!streamsNode.IsSequence())
    {
        if (outError) *outError = "streams section is not a list";
        return false;
    }

    streams.clear();
    for (const auto& item : streamsNode)
    {
        LiveStream cfg;

        auto readStr = [&](const char* key, char* dst, size_t dstSize) {
            const YAML::Node& n = item[key];
            if (n.IsDefined() && n.IsScalar())
                SafeStrCpy(dst, dstSize, n.as<std::string>());
        };

        if (const YAML::Node& n = item["id"]; n.IsDefined())           cfg.id          = n.as<int>();
        if (const YAML::Node& n = item["is_streaming"]; n.IsDefined()) cfg.isStreaming = n.as<bool>();
        if (const YAML::Node& n = item["is_selected"]; n.IsDefined())  cfg.isSelected  = n.as<bool>();
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
    out << YAML::Key << "robot_comm_open"          << YAML::Value << uiState.robot_comm_open;
    out << YAML::Key << "notification_open"       << YAML::Value << uiState.notification_open;
    out << YAML::Key << "terminal_open"           << YAML::Value << uiState.terminal_open;
    out << YAML::Key << "monitor_wall_open"       << YAML::Value << uiState.monitor_wall_open;
    out << YAML::Key << "robot_active_mode"        << YAML::Value << uiState.robot_active_mode;
    out << YAML::Key << "gamepad_active_mode"      << YAML::Value << uiState.gamepad_active_mode;
    out << YAML::Key << "node_left_side_width"     << YAML::Value << uiState.node_left_side_width;
    out << YAML::Key << "node_right_side_width"    << YAML::Value << uiState.node_right_side_width;

    // FileManager 状态
    out << YAML::Key << "robot_path"  << YAML::Value << uiState.robot_path;
    out << YAML::Key << "robot_dirty"  << YAML::Value << uiState.robot_dirty;

    out << YAML::Key << "recent_files" << YAML::Value << YAML::BeginSeq;
    for (const auto& f : uiState.recent_files)
        out << f;
    out << YAML::EndSeq;

    // 截图设置
    out << YAML::Key << "screenshot_scope" << YAML::Value << uiState.screenshot_scope;
    out << YAML::Key << "screenshot_path" << YAML::Value << uiState.screenshot_path;

    // 连接设置
    out << YAML::Key << "conn_retry_count" << YAML::Value << uiState.conn_retry_count;
    out << YAML::Key << "camera_retry_count" << YAML::Value << uiState.camera_retry_count;

    // 启用的插件列表
    out << YAML::Key << "enabled_plugins" << YAML::Value << YAML::BeginSeq;
    for (const auto& name : uiState.enabled_plugins)
        out << name;
    out << YAML::EndSeq;

    // 通信节点配置（含发送帧 enable 状态）
    if (!uiState.comm_configs.empty()) {
        EmitRobotComm(out, uiState.comm_configs);
    }

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
    if (const YAML::Node& n = node["robot_comm_open"];          n.IsDefined()) uiState.robot_comm_open          = n.as<bool>();
    if (const YAML::Node& n = node["notification_open"];        n.IsDefined()) uiState.notification_open        = n.as<bool>();
    if (const YAML::Node& n = node["terminal_open"];            n.IsDefined()) uiState.terminal_open            = n.as<bool>();
    if (const YAML::Node& n = node["monitor_wall_open"];        n.IsDefined()) uiState.monitor_wall_open        = n.as<bool>();
    if (const YAML::Node& n = node["robot_active_mode"];        n.IsDefined()) uiState.robot_active_mode        = n.as<int>();
    if (const YAML::Node& n = node["gamepad_active_mode"];      n.IsDefined()) uiState.gamepad_active_mode      = n.as<int>();
    if (const YAML::Node& n = node["node_left_side_width"];     n.IsDefined()) uiState.node_left_side_width     = n.as<float>();
    if (const YAML::Node& n = node["node_right_side_width"];    n.IsDefined()) uiState.node_right_side_width    = n.as<float>();

    // FileManager 状态
    if (const YAML::Node& n = node["robot_path"];  n.IsDefined()) uiState.robot_path  = n.as<std::string>();
    if (const YAML::Node& n = node["robot_dirty"]; n.IsDefined()) uiState.robot_dirty = n.as<bool>();
    if (const YAML::Node& n = node["recent_files"]; n.IsDefined() && n.IsSequence())
    {
        uiState.recent_files.clear();
        for (const auto& item : n)
            uiState.recent_files.push_back(item.as<std::string>());
    }

    // 截图设置
    if (const YAML::Node& n = node["screenshot_scope"]; n.IsDefined()) uiState.screenshot_scope = n.as<int>();
    if (const YAML::Node& n = node["screenshot_path"]; n.IsDefined()) uiState.screenshot_path = n.as<std::string>();

    // 连接设置
    if (const YAML::Node& n = node["conn_retry_count"]; n.IsDefined()) uiState.conn_retry_count = n.as<int>();
    if (const YAML::Node& n = node["camera_retry_count"]; n.IsDefined()) uiState.camera_retry_count = n.as<int>();

    // 启用的插件列表
    if (const YAML::Node& n = node["enabled_plugins"]; n.IsDefined() && n.IsSequence())
    {
        uiState.enabled_plugins.clear();
        for (const auto& item : n)
            uiState.enabled_plugins.push_back(item.as<std::string>());
    }

    // 通信节点配置
    if (const YAML::Node& commNode = node["robot_comm"]; commNode.IsDefined()) {
        std::string commError;
        ApplyRobotComm(commNode, uiState.comm_configs, &commError);
    }

    return true;
}
// ============================================================================
//  Thrust Curve Editor 独立曲线持久化
// ============================================================================

void ConfigSerializer::EmitEditorCurve(YAML::Emitter& out, const ThrustCurve& curve)
{
    out << YAML::Key << "thrust_curve_editor" << YAML::Value << YAML::BeginMap;

    out << YAML::Key << "pwm_min" << YAML::Value << curve.pwm_min;
    out << YAML::Key << "pwm_max" << YAML::Value << curve.pwm_max;
    out << YAML::Key << "default_pwm" << YAML::Value << curve.default_pwm;
    out << YAML::Key << "np_mid" << YAML::Value << curve.np_mid;
    out << YAML::Key << "np_ini" << YAML::Value << curve.np_ini;
    out << YAML::Key << "pp_ini" << YAML::Value << curve.pp_ini;
    out << YAML::Key << "pp_mid" << YAML::Value << curve.pp_mid;
    out << YAML::Key << "nt_end" << YAML::Value << curve.nt_end;
    out << YAML::Key << "nt_mid" << YAML::Value << curve.nt_mid;
    out << YAML::Key << "pt_mid" << YAML::Value << curve.pt_mid;
    out << YAML::Key << "pt_end" << YAML::Value << curve.pt_end;

    if (!curve.raw_thrust.empty()) {
        out << YAML::Key << "raw_thrust" << YAML::Value << YAML::Flow << curve.raw_thrust;
        out << YAML::Key << "raw_pwm" << YAML::Value << YAML::Flow << curve.raw_pwm;
    }

    out << YAML::EndMap;
}

bool ConfigSerializer::ApplyEditorCurve(const YAML::Node& node, ThrustCurve& curve, std::string* outError)
{
    (void)outError;

    if (const YAML::Node& n = node["pwm_min"]; n.IsDefined()) curve.pwm_min = n.as<float>();
    if (const YAML::Node& n = node["pwm_max"]; n.IsDefined()) curve.pwm_max = n.as<float>();
    if (const YAML::Node& n = node["default_pwm"]; n.IsDefined()) curve.default_pwm = n.as<float>();
    if (const YAML::Node& n = node["np_mid"]; n.IsDefined()) curve.np_mid.value = n.as<double>();
    if (const YAML::Node& n = node["np_ini"]; n.IsDefined()) curve.np_ini.value = n.as<double>();
    if (const YAML::Node& n = node["pp_ini"]; n.IsDefined()) curve.pp_ini.value = n.as<double>();
    if (const YAML::Node& n = node["pp_mid"]; n.IsDefined()) curve.pp_mid.value = n.as<double>();
    if (const YAML::Node& n = node["nt_end"]; n.IsDefined()) curve.nt_end.value = n.as<double>();
    if (const YAML::Node& n = node["nt_mid"]; n.IsDefined()) curve.nt_mid.value = n.as<double>();
    if (const YAML::Node& n = node["pt_mid"]; n.IsDefined()) curve.pt_mid.value = n.as<double>();
    if (const YAML::Node& n = node["pt_end"]; n.IsDefined()) curve.pt_end.value = n.as<double>();

    curve.raw_thrust.clear(); curve.raw_pwm.clear();
    if (const YAML::Node& rt = node["raw_thrust"]; rt.IsDefined() && rt.IsSequence()) {
        for (const auto& v : rt) curve.raw_thrust.push_back(v.as<double>());
    }
    if (const YAML::Node& rp = node["raw_pwm"]; rp.IsDefined() && rp.IsSequence()) {
        for (const auto& v : rp) curve.raw_pwm.push_back(v.as<float>());
    }

    return true;
}

// ============================================================================
//  Robot Comm 配置持久化
// ============================================================================

void ConfigSerializer::EmitRobotComm(YAML::Emitter& out,
                                     const std::vector<RobotComm>& configs)
{
    out << YAML::Key << "robot_comm" << YAML::Value << YAML::BeginMap;

    out << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
    for (const auto& cfg : configs)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "id"                << YAML::Value << cfg.id;
        out << YAML::Key << "is_selected"       << YAML::Value << cfg.isSelected;
        out << YAML::Key << "is_linked"         << YAML::Value << cfg.isLinked;
        out << YAML::Key << "name"              << YAML::Value << cfg.name;
        out << YAML::Key << "host_ip"       << YAML::Value << cfg.host_ip;
        out << YAML::Key << "remote_port"   << YAML::Value << cfg.remote_port;
        out << YAML::Key << "local_port"    << YAML::Value << cfg.local_port;
        out << YAML::Key << "transport_type" << YAML::Value << cfg.transport_type;
        out << YAML::Key << "send_freq_hz"   << YAML::Value << cfg.send_freq_hz;
        out << YAML::Key << "retry_count"    << YAML::Value << cfg.retry_count;
        out << YAML::Key << "active_component_idx" << YAML::Value << cfg.active_component_idx;

        // Serial-specific config
        out << YAML::Key << "com_port"   << YAML::Value << cfg.com_port_str;
        out << YAML::Key << "baud_rate"  << YAML::Value << cfg.baud_rate;
        out << YAML::Key << "data_bits"  << YAML::Value << cfg.data_bits;
        out << YAML::Key << "stop_bits"  << YAML::Value << cfg.stop_bits;
        out << YAML::Key << "parity"     << YAML::Value << cfg.parity;

        // 协议发送配置（多帧）
        out << YAML::Key << "protocol_send" << YAML::Value << YAML::BeginSeq;
        for (const auto& p : cfg.protocol_send) {
            out << YAML::BeginMap;
            out << YAML::Key << "name" << YAML::Value << p.name;
            auto emitBytes = [&](const char* key, const std::vector<uint8_t>& bytes) {
                out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
                for (auto b : bytes) out << static_cast<int>(b);
                out << YAML::EndSeq;
            };
            emitBytes("command_byte", p.command_bytes);
            out << YAML::Key << "enabled" << YAML::Value << p.enabled;
            out << YAML::Key << "include_length" << YAML::Value << p.include_length;
            out << YAML::Key << "checksum" << YAML::Value << static_cast<int>(p.checksum);
            out << YAML::Key << "big_endian" << YAML::Value << p.big_endian;
            out << YAML::Key << "checksum_range" << YAML::Value << static_cast<int>(p.checksum_range);
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
                out << YAML::Key << "fix_value" << YAML::Value << f.fix_value;
                if (!f.raw_data.empty()) {
                    out << YAML::Key << "raw_data" << YAML::Value << YAML::BeginSeq;
                    for (auto b : f.raw_data) out << static_cast<int>(b);
                    out << YAML::EndSeq;
                }
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;  // fields
            out << YAML::EndMap;  // each send config
        }
        out << YAML::EndSeq;  // protocol_send

        // 协议接收配置（多帧）
        out << YAML::Key << "protocol_receive" << YAML::Value << YAML::BeginSeq;
        for (const auto& pr : cfg.protocol_receive) {
            out << YAML::BeginMap;
            out << YAML::Key << "name" << YAML::Value << pr.name;
            auto emitBytes2 = [&](const char* key, const std::vector<uint8_t>& bytes) {
                out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
                for (auto b : bytes) out << static_cast<int>(b);
                out << YAML::EndSeq;
            };
            emitBytes2("command_byte", pr.command_bytes);
            out << YAML::Key << "include_length" << YAML::Value << pr.include_length;
            out << YAML::Key << "checksum" << YAML::Value << static_cast<int>(pr.checksum);
            out << YAML::Key << "big_endian" << YAML::Value << pr.big_endian;
            out << YAML::Key << "checksum_range" << YAML::Value << static_cast<int>(pr.checksum_range);
            emitBytes2("header", pr.header);
            emitBytes2("tail", pr.tail);
            out << YAML::Key << "fields" << YAML::Value << YAML::BeginSeq;
            for (const auto& f : pr.fields) {
                out << YAML::BeginMap;
                out << YAML::Key << "name" << YAML::Value << f.name;
                out << YAML::Key << "path" << YAML::Value << f.field_path;
                out << YAML::Key << "encoding" << YAML::Value << static_cast<int>(f.encoding);
                out << YAML::Key << "group" << YAML::Value << f.group;
                out << YAML::Key << "visible" << YAML::Value << f.visible;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;  // fields
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;  // protocol_receive

        out << YAML::EndMap;
    }
    out << YAML::EndSeq;  // nodes

    out << YAML::EndMap;  // robot_comm
}

bool ConfigSerializer::ApplyRobotComm(const YAML::Node& node,
                                      std::vector<RobotComm>& configs,
                                      std::string* outError)
{
    (void)outError;

    const YAML::Node& nodesNode = node["nodes"];
    if (nodesNode.IsDefined() && nodesNode.IsSequence())
    {
        configs.clear();
        for (const auto& item : nodesNode)
        {
            RobotComm cfg;

            auto readStr = [&](const char* key, char* dst, size_t dstSize) {
                const YAML::Node& n = item[key];
                if (n.IsDefined() && n.IsScalar())
                    SafeStrCpy(dst, dstSize, n.as<std::string>());
            };

            readStr("name",    cfg.name,    sizeof(cfg.name));
            readStr("host_ip", cfg.host_ip, sizeof(cfg.host_ip));

            if (const YAML::Node& n = item["remote_port"];    n.IsDefined()) cfg.remote_port    = n.as<int>();
            if (const YAML::Node& n = item["local_port"];     n.IsDefined()) cfg.local_port     = n.as<int>();
            if (const YAML::Node& n = item["transport_type"]; n.IsDefined()) cfg.transport_type = n.as<int>();
            if (const YAML::Node& n = item["send_freq_hz"];   n.IsDefined()) cfg.send_freq_hz   = n.as<int>();
            if (const YAML::Node& n = item["retry_count"];    n.IsDefined()) cfg.retry_count    = n.as<int>();
            if (const YAML::Node& n = item["active_component_idx"]; n.IsDefined()) cfg.active_component_idx = n.as<int>();

            // Serial-specific config
            readStr("com_port",  cfg.com_port_str, sizeof(cfg.com_port_str));
            if (const YAML::Node& n = item["baud_rate"];  n.IsDefined()) cfg.baud_rate  = n.as<int>();
            if (const YAML::Node& n = item["data_bits"];  n.IsDefined()) cfg.data_bits  = n.as<int>();
            if (const YAML::Node& n = item["stop_bits"];  n.IsDefined()) cfg.stop_bits  = n.as<int>();
            if (const YAML::Node& n = item["parity"];     n.IsDefined()) cfg.parity     = n.as<int>();

            // 协议发送配置
            const YAML::Node& ps = item["protocol_send"];
            if (ps.IsDefined()) {
                auto readBytes = [](const YAML::Node& seq) -> std::vector<uint8_t> {
                    std::vector<uint8_t> bytes;
                    if (seq.IsDefined() && seq.IsSequence())
                        for (const auto& v : seq)
                            bytes.push_back(static_cast<uint8_t>(v.as<int>()));
                    return bytes;
                };
                auto readSendCfg = [&](const YAML::Node& cfgNode, ProtocolSendConfig& p) {
                    if (const YAML::Node& n = cfgNode["name"]; n.IsDefined()) {
                        std::string nm = n.as<std::string>();
                        strncpy_s(p.name, nm.c_str(), sizeof(p.name) - 1);
                    }
                    p.command_bytes = readBytes(cfgNode["command_byte"]);
                    if (p.command_bytes.empty() && cfgNode["command_byte"].IsDefined() && !cfgNode["command_byte"].IsSequence())
                        p.command_bytes.push_back(static_cast<uint8_t>(cfgNode["command_byte"].as<int>()));
                    if (const YAML::Node& n = cfgNode["enabled"]; n.IsDefined())
                        p.enabled = n.as<bool>();
                    if (const YAML::Node& n = cfgNode["include_length"]; n.IsDefined())
                        p.include_length = n.as<bool>();
                    if (const YAML::Node& n = cfgNode["checksum"]; n.IsDefined())
                        p.checksum = static_cast<ChecksumType>(n.as<int>());
                    if (const YAML::Node& n = cfgNode["checksum_range"]; n.IsDefined())
                        p.checksum_range = static_cast<ChecksumRange>(n.as<int>());
                    if (const YAML::Node& n = cfgNode["big_endian"]; n.IsDefined())
                        p.big_endian = n.as<bool>();
                    p.header = readBytes(cfgNode["header"]);
                    p.tail   = readBytes(cfgNode["tail"]);
                    const YAML::Node& fieldsNode = cfgNode["fields"];
                    if (fieldsNode.IsDefined() && fieldsNode.IsSequence()) {
                        p.fields.clear();
                        for (const auto& fItem : fieldsNode) {
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
                            if (const YAML::Node& n = fItem["fix_value"]; n.IsDefined())
                                sf.fix_value = n.as<double>();
                            if (const YAML::Node& n = fItem["raw_data"]; n.IsDefined() && n.IsSequence()) {
                                for (const auto& v : n) sf.raw_data.push_back(static_cast<uint8_t>(v.as<int>()));
                            }
                            p.fields.push_back(sf);
                        }
                    }
                };
                if (ps.IsSequence()) {
                    for (const auto& cfgItem : ps) {
                        ProtocolSendConfig p;
                        readSendCfg(cfgItem, p);
                        cfg.protocol_send.push_back(std::move(p));
                    }
                } else if (ps.IsMap()) {
                    ProtocolSendConfig p;
                    readSendCfg(ps, p);
                    cfg.protocol_send.push_back(std::move(p));
                }
            }

            // 协议接收配置
            const YAML::Node& pr = item["protocol_receive"];
            if (pr.IsDefined()) {
                auto readBytes = [](const YAML::Node& seq) -> std::vector<uint8_t> {
                    std::vector<uint8_t> bytes;
                    if (seq.IsDefined() && seq.IsSequence())
                        for (const auto& v : seq)
                            bytes.push_back(static_cast<uint8_t>(v.as<int>()));
                    return bytes;
                };
                auto readRecvCfg = [&](const YAML::Node& cfgNode, ProtocolReceiveConfig& rc) {
                    if (const YAML::Node& n = cfgNode["name"]; n.IsDefined()) {
                        std::string nm = n.as<std::string>();
                        strncpy_s(rc.name, nm.c_str(), sizeof(rc.name) - 1);
                    }
                    rc.command_bytes = readBytes(cfgNode["command_byte"]);
                    if (rc.command_bytes.empty() && cfgNode["command_byte"].IsDefined() && !cfgNode["command_byte"].IsSequence())
                        rc.command_bytes.push_back(static_cast<uint8_t>(cfgNode["command_byte"].as<int>()));
                    if (const YAML::Node& n = cfgNode["include_length"]; n.IsDefined())
                        rc.include_length = n.as<bool>();
                    if (const YAML::Node& n = cfgNode["checksum"]; n.IsDefined())
                        rc.checksum = static_cast<ChecksumType>(n.as<int>());
                    if (const YAML::Node& n = cfgNode["checksum_range"]; n.IsDefined())
                        rc.checksum_range = static_cast<ChecksumRange>(n.as<int>());
                    if (const YAML::Node& n = cfgNode["big_endian"]; n.IsDefined())
                        rc.big_endian = n.as<bool>();
                    rc.header = readBytes(cfgNode["header"]);
                    rc.tail   = readBytes(cfgNode["tail"]);
                    const YAML::Node& fieldsNode = cfgNode["fields"];
                    if (fieldsNode.IsDefined() && fieldsNode.IsSequence()) {
                        rc.fields.clear();
                        for (const auto& fItem : fieldsNode) {
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
                            rc.fields.push_back(rf);
                        }
                    }
                };
                if (pr.IsSequence()) {
                    for (const auto& cfgItem : pr) {
                        ProtocolReceiveConfig rc;
                        readRecvCfg(cfgItem, rc);
                        cfg.protocol_receive.push_back(std::move(rc));
                    }
                } else if (pr.IsMap()) {
                    ProtocolReceiveConfig rc;
                    readRecvCfg(pr, rc);
                    cfg.protocol_receive.push_back(std::move(rc));
                }
            }

            configs.push_back(cfg);
        }
    }

    return true;
}

// ============================================================================
// EmitGraphItems — NodeGraph 编辑项列表
// ============================================================================
void ConfigSerializer::EmitGraphItems(YAML::Emitter& out, const std::vector<NodeGraph>& items)
{
    out << YAML::Key << "graph_items" << YAML::Value << YAML::BeginSeq;
    for (const auto& item : items)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "id"          << YAML::Value << item.id;
        out << YAML::Key << "name"        << YAML::Value << item.name;
        out << YAML::Key << "is_selected" << YAML::Value << item.isSelected;
        out << YAML::Key << "editor_yaml" << YAML::Value << item.editorYaml;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
}

// ============================================================================
// ApplyGraphItems — NodeGraph 编辑项列表加载
// ============================================================================
bool ConfigSerializer::ApplyGraphItems(const YAML::Node& node, std::vector<NodeGraph>& items, std::string* outError)
{
    (void)outError;
    items.clear();
    if (!node.IsSequence()) return true;

    for (const auto& yn : node)
    {
        NodeGraph item;
        if (const YAML::Node& n = yn["id"];          n.IsDefined()) item.id          = n.as<int>();
        if (const YAML::Node& n = yn["name"];        n.IsDefined()) strncpy_s(item.name, n.as<std::string>().c_str(), sizeof(item.name) - 1);
        if (const YAML::Node& n = yn["is_selected"]; n.IsDefined()) item.isSelected  = n.as<bool>();
        if (const YAML::Node& n = yn["editor_yaml"]; n.IsDefined()) item.editorYaml  = n.as<std::string>();
        items.push_back(std::move(item));
    }
    return true;
}

// ============================================================================
// 软件 UI 快捷键序列化（存入 .kernel）
// ============================================================================

void ConfigSerializer::EmitSoftwareShortcuts(YAML::Emitter& out, const std::string& swYaml)
{
    try {
        YAML::Node node = YAML::Load(swYaml);
        out << YAML::Key << "software_shortcuts" << YAML::Value << node;
    } catch (...) {}
}

bool ConfigSerializer::ApplySoftwareShortcuts(const YAML::Node& node, std::string& outYaml, std::string* outError)
{
    (void)outError;
    try {
        YAML::Emitter em;
        em << node;
        outYaml = em.c_str();
        return true;
    } catch (...) {
        return false;
    }
}
