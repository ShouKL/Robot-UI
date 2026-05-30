#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdio>

// ================== 编码枚举（必须在传感器/执行器结构体之前） ==================

enum class DataEncoding : uint8_t {
    Float32 = 0,
    Float64 = 1,
    Int8    = 2,
    Int16   = 3,
    Int32   = 4,
    Int64   = 5,
    Uint8   = 6,
    Uint16  = 7,
    Uint32  = 8,
    Uint64  = 9,
    Bool    = 10,
};

// ================== 带编码的数值容器 ==================

struct EncodedValue {
    double value = 0.0;
    DataEncoding encoding = DataEncoding::Float32;
    EncodedValue() = default;
    EncodedValue(double v) : value(v) {}
    EncodedValue(double v, DataEncoding e) : value(v), encoding(e) {}
    operator double() const { return value; }
    EncodedValue& operator=(double v) { value = v; return *this; }
};

// ================== 传感器数据结构 (Sensor) ==================

struct TemperatureSensor { EncodedValue value{0.0, DataEncoding::Float32}; };
struct HumiditySensor    { EncodedValue value{0.0, DataEncoding::Float32}; };
struct DepthSensor       { EncodedValue value{0.0, DataEncoding::Float32}; };

struct SensorData {
    TemperatureSensor temperature;
    HumiditySensor    humidity;
    DepthSensor       depth;
    bool is_valid = false;
};

// ================== 传感器配置 (Sensor Config) ==================
// 与 ActuatorConfig 并列，描述模式中有哪些传感器

struct SensorConfig {
    bool has_temperature = false;
    bool has_humidity    = false;
    bool has_depth       = false;
};

// ================== 执行器数据结构 (Actuator) ==================

struct ThrustCurve {
    EncodedValue np_mid{0, DataEncoding::Float32};
    EncodedValue np_ini{0, DataEncoding::Float32};
    EncodedValue pp_ini{0, DataEncoding::Float32};
    EncodedValue pp_mid{0, DataEncoding::Float32};
    EncodedValue nt_end{0, DataEncoding::Float32};
    EncodedValue nt_mid{0, DataEncoding::Float32};
    EncodedValue pt_mid{0, DataEncoding::Float32};
    EncodedValue pt_end{0, DataEncoding::Float32};
    float pwm_min = 0.0f;
    float pwm_max = 0.0f;
    float default_pwm = 500.0f;
    std::vector<double> raw_thrust;
    std::vector<float> raw_pwm;
};

struct BrushlessMotor {
    int id = 0;
    std::string name;
    ThrustCurve curve;
    EncodedValue target_speed{0, DataEncoding::Float32};
};

struct Servo {
    int id = 0;
    std::string name;
    EncodedValue angle{0, DataEncoding::Float32};
};

struct MotionControl {
    EncodedValue x{0, DataEncoding::Float32};
    EncodedValue y{0, DataEncoding::Float32};
    EncodedValue z{0, DataEncoding::Float32};
    EncodedValue rx{0, DataEncoding::Float32};
    EncodedValue ry{0, DataEncoding::Float32};
    EncodedValue rz{0, DataEncoding::Float32};
};

struct ActuatorConfig {
    std::map<int, BrushlessMotor> brushlessmotor;
    std::map<int, Servo> servo;
    MotionControl motion;
    bool has_motion = false;
};

// ================== 协议配置 (Protocol) ==================

enum class ChecksumType : uint8_t {
    None  = 0,
    Sum8  = 1,
    XOR8  = 2,
    CRC16 = 3,
};

// ================== 字段/协议结构体（必须在 inline 函数之前） ==================

struct SendField {
    std::string name;
    std::string field_path;
    DataEncoding encoding = DataEncoding::Float32;
    std::string group;
    bool visible = true;
    bool fix = false;
};

struct ReceiveField {
    std::string name;
    std::string field_path;
    DataEncoding encoding = DataEncoding::Float32;
    std::string group;
    bool visible = true;
    bool fix = false;
};

struct ProtocolReceiveConfig {
    std::vector<uint8_t> header;
    std::vector<uint8_t> tail;
    ChecksumType checksum = ChecksumType::Sum8;
    bool include_length = true;
    uint8_t msg_type = 0;
    std::vector<ReceiveField> fields;
};

struct ProtocolSendConfig {
    std::string format_name;
    std::vector<uint8_t> header;
    std::vector<uint8_t> tail;
    ChecksumType checksum = ChecksumType::Sum8;
    bool include_length = true;
    std::vector<SendField> fields;
};

// ================== 机器人模式配置 (RobotMode) ==================
// 包含一个模式的全部配置：执行器、传感器、协议、网络等

struct RobotMode {
    char name[64] = "";
    std::string gamepad_mapping_Mode;
    std::string node_graph;  // 旧版单节点图（兼容）
    std::map<std::string, std::string> node_graph_pairs;  // gamepadModeName → graph yaml

    // 执行器与传感器并列
    ActuatorConfig  actuator_config;
    SensorConfig  sensor_config;

    // 协议配置
    ProtocolSendConfig    protocol_send;
    ProtocolReceiveConfig protocol_receive;

    // 网络
    std::string host_ip;
    int remote_port   = 0;
    int local_port    = 0;
    int protocol_type = 0;
};

// ================== Inline 工具函数 ==================

inline std::vector<const char*> GetEncodingNames() {
    return { "Float32", "Float64", "Int8", "Int16", "Int32", "Int64", "Uint8", "Uint16", "Uint32", "Uint64", "Bool" };
}

inline DataEncoding IndexToEncoding(int idx) {
    switch (idx) {
    case 0:  return DataEncoding::Float32;
    case 1:  return DataEncoding::Float64;
    case 2:  return DataEncoding::Int8;
    case 3:  return DataEncoding::Int16;
    case 4:  return DataEncoding::Int32;
    case 5:  return DataEncoding::Int64;
    case 6:  return DataEncoding::Uint8;
    case 7:  return DataEncoding::Uint16;
    case 8:  return DataEncoding::Uint32;
    case 9:  return DataEncoding::Uint64;
    case 10: return DataEncoding::Bool;
    default: return DataEncoding::Float32;
    }
}

inline int EncodingToIndex(DataEncoding enc) {
    switch (enc) {
    case DataEncoding::Float32: return 0;
    case DataEncoding::Float64: return 1;
    case DataEncoding::Int8:    return 2;
    case DataEncoding::Int16:   return 3;
    case DataEncoding::Int32:   return 4;
    case DataEncoding::Int64:   return 5;
    case DataEncoding::Uint8:   return 6;
    case DataEncoding::Uint16:  return 7;
    case DataEncoding::Uint32:  return 8;
    case DataEncoding::Uint64:  return 9;
    case DataEncoding::Bool:    return 10;
    default: return 0;
    }
}

inline int GetEncodingByteSize(DataEncoding enc) {
    switch (enc) {
    case DataEncoding::Float32: return 4;
    case DataEncoding::Float64: return 8;
    case DataEncoding::Int8:    return 1;
    case DataEncoding::Int16:   return 2;
    case DataEncoding::Int32:   return 4;
    case DataEncoding::Int64:   return 8;
    case DataEncoding::Uint8:   return 1;
    case DataEncoding::Uint16:  return 2;
    case DataEncoding::Uint32:  return 4;
    case DataEncoding::Uint64:  return 8;
    case DataEncoding::Bool:    return 1;
    default: return 0;
    }
}

// ================== 级联下拉框：组件 → 子字段 → 编码 ==================

// 组件描述
struct FieldComponent {
    std::string id;          // "motion", "m0", "s1", "temp", "hum", "depth"
    std::string label;       // "Motion", "Motor #0", "Servo #1", "Temperature", "Humidity", "Depth"
    std::string path_prefix; // "motion.", "brushlessmotor.0.", "servo.1.", "temperature.", "humidity.", "depth."
    bool is_sensor;
};

// 子字段描述
struct FieldSubField {
    std::string key;         // "x", "target_speed", "curve.np_mid", "value" ...
    std::string label;       // "X", "Target Speed", "Curve np_mid", "Value"
};

// 从 ActuatorConfig + SensorConfig 生成可用组件列表
inline std::vector<FieldComponent> GetSendComponents(const ActuatorConfig& data, const SensorConfig& /*sensor*/) {
    std::vector<FieldComponent> comps;
    if (data.has_motion)
        comps.push_back({"motion", "Motion", "motion.", false});
    for (const auto& kv : data.brushlessmotor) {
        char buf[16];
        _snprintf_s(buf, sizeof(buf), "m%d", kv.first);
        std::string label = kv.second.name.empty()
            ? std::string("Motor #") + std::to_string(kv.first)
            : kv.second.name;
        comps.push_back({buf, label,
            std::string("brushlessmotor.") + std::to_string(kv.first) + ".", false});
    }
    for (const auto& kv : data.servo) {
        char buf[16];
        _snprintf_s(buf, sizeof(buf), "s%d", kv.first);
        std::string label = kv.second.name.empty()
            ? std::string("Servo #") + std::to_string(kv.first)
            : kv.second.name;
        comps.push_back({buf, label,
            std::string("servo.") + std::to_string(kv.first) + ".", false});
    }
    return comps;
}

inline std::vector<FieldComponent> GetRecvComponents(const SensorConfig& sensor) {
    std::vector<FieldComponent> comps;
    if (sensor.has_temperature) comps.push_back({"temp", "Temperature", "temperature.", true});
    if (sensor.has_humidity)    comps.push_back({"hum",  "Humidity",    "humidity.",    true});
    if (sensor.has_depth)       comps.push_back({"depth","Depth",       "depth.",       true});
    return comps;
}

// 根据组件获取其子字段列表
inline std::vector<FieldSubField> GetSubFields(const FieldComponent& comp) {
    if (comp.id == "motion") {
        return {
            {"x",  "X"},
            {"y",  "Y"},
            {"z",  "Z"},
            {"rx", "RX"},
            {"ry", "RY"},
            {"rz", "RZ"},
        };
    }
    if (comp.id[0] == 'm') { // motor
        return {
            {"id",           "ID"},
            {"target_speed", "Target Speed"},
            {"curve.np_mid", "Curve np_mid"},
            {"curve.np_ini", "Curve np_ini"},
            {"curve.pp_ini", "Curve pp_ini"},
            {"curve.pp_mid", "Curve pp_mid"},
            {"curve.nt_end", "Curve nt_end"},
            {"curve.nt_mid", "Curve nt_mid"},
            {"curve.pt_mid", "Curve pt_mid"},
            {"curve.pt_end", "Curve pt_end"},
        };
    }
    if (comp.id[0] == 's') { // servo
        return {
            {"id",    "ID"},
            {"angle", "Angle"},
        };
    }
    // sensors
    return {{"value", "Value"}};
}

// 根据组件 id 解析 field_path 中的组件部分
inline std::string ResolveComponentId(const std::string& field_path) {
    // 返回组件 id: "motion", "m0", "s1", "temp", "hum", "depth"
    if (field_path.rfind("motion.", 0) == 0) return "motion";
    if (field_path.rfind("brushlessmotor.", 0) == 0) {
        auto dot = field_path.find('.', 15);
        std::string idStr = field_path.substr(15, dot - 15);
        return "m" + idStr;
    }
    if (field_path.rfind("servo.", 0) == 0) {
        auto dot = field_path.find('.', 6);
        std::string idStr = field_path.substr(6, dot - 6);
        return "s" + idStr;
    }
    if (field_path.rfind("temperature.", 0) == 0) return "temp";
    if (field_path.rfind("humidity.", 0) == 0)    return "hum";
    if (field_path.rfind("depth.", 0) == 0)       return "depth";
    return "";
}

// 根据组件 id 解析 field_path 中的子字段部分
inline std::string ResolveSubField(const std::string& field_path) {
    // motion:   "motion.x"              → "x"
    // servo:    "servo.0.angle"         → "angle"
    // motor:    "brushlessmotor.0.target_speed" → "target_speed"
    // motor:    "brushlessmotor.0.curve.np_mid" → "curve.np_mid"
    // sensors:  "temperature.value"     → "value"
    if (field_path.rfind("motion.", 0) == 0) {
        return field_path.substr(7);  // "motion." = 7 chars
    }
    if (field_path.rfind("brushlessmotor.", 0) == 0) {
        // "brushlessmotor." = 15 chars, then "N." where N = motor ID
        auto dot = field_path.find('.', 15);  // dot after "brushlessmotor"
        if (dot != std::string::npos)
            return field_path.substr(dot + 1);  // everything after the ID: "target_speed" or "curve.np_mid"
        return "";
    }
    if (field_path.rfind("servo.", 0) == 0) {
        // "servo." = 6 chars, then "N." where N = servo ID
        auto dot = field_path.find('.', 6);
        if (dot != std::string::npos)
            return field_path.substr(dot + 1);  // everything after the ID: "angle"
        return "";
    }
    // sensors: "temperature.", "humidity.", "depth."
    auto prefixLen = field_path.find('.') + 1;
    if (prefixLen > 0 && prefixLen < field_path.size())
        return field_path.substr(prefixLen);
    return "";
}

// 根据 component path_prefix + sub_field.key 拼接 field_path
inline std::string MakeFieldPath(const FieldComponent& comp, const FieldSubField& sf) {
    return comp.path_prefix + sf.key;
}

/// 根据 field_path 从 ActuatorConfig 中取出对应 double 值
/// 支持路径格式：
///   "motion.x" / "motion.y" / ... / "motion.rz"
///   "brushlessmotor.<id>.target_speed"
///   "brushlessmotor.<id>.curve.np_mid" / ... / ".pt_end"
///   "servo.<id>.angle"
inline bool GetActuatorField(const ActuatorConfig& data, const std::string& path, double& out) {
    auto split = [](const std::string& s, char delim) -> std::vector<std::string> {
        std::vector<std::string> parts;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) parts.push_back(item);
        return parts;
    };

    std::vector<std::string> segs = split(path, '.');
    if (segs.empty()) return false;

    // --- motion ---
    if (segs[0] == "motion" && segs.size() == 2) {
        const MotionControl& m = data.motion;
        if      (segs[1] == "x")  { out = m.x;  return true; }
        else if (segs[1] == "y")  { out = m.y;  return true; }
        else if (segs[1] == "z")  { out = m.z;  return true; }
        else if (segs[1] == "rx") { out = m.rx; return true; }
        else if (segs[1] == "ry") { out = m.ry; return true; }
        else if (segs[1] == "rz") { out = m.rz; return true; }
        return false;
    }

    // --- brushlessmotor ---
    if (segs[0] == "brushlessmotor" && segs.size() >= 3) {
        int id = std::stoi(segs[1]);
        auto it = data.brushlessmotor.find(id);
        if (it == data.brushlessmotor.end()) return false;
        const BrushlessMotor& motor = it->second;

        if (segs[2] == "id" && segs.size() == 3) {
            out = motor.id; return true;
        }
        if (segs[2] == "target_speed" && segs.size() == 3) {
            out = motor.target_speed; return true;
        }
        if (segs[2] == "curve" && segs.size() == 4) {
            const ThrustCurve& c = motor.curve;
            if      (segs[3] == "np_mid") { out = c.np_mid; return true; }
            else if (segs[3] == "np_ini") { out = c.np_ini; return true; }
            else if (segs[3] == "pp_ini") { out = c.pp_ini; return true; }
            else if (segs[3] == "pp_mid") { out = c.pp_mid; return true; }
            else if (segs[3] == "nt_end") { out = c.nt_end; return true; }
            else if (segs[3] == "nt_mid") { out = c.nt_mid; return true; }
            else if (segs[3] == "pt_mid") { out = c.pt_mid; return true; }
            else if (segs[3] == "pt_end") { out = c.pt_end; return true; }
            return false;
        }
        return false;
    }

    // --- servo ---
    if (segs[0] == "servo" && segs.size() == 3) {
        int id = std::stoi(segs[1]);
        auto it = data.servo.find(id);
        if (it == data.servo.end()) return false;
        if (segs[2] == "id")    { out = it->second.id;    return true; }
        if (segs[2] == "angle") { out = it->second.angle; return true; }
        return false;
    }

    return false;
}

/// 根据 SendField 列表构建 payload 字节流
inline std::vector<uint8_t> BuildPayload(const ActuatorConfig& data, const std::vector<SendField>& fields) {
    std::vector<uint8_t> payload;
    for (const auto& f : fields) {
        double val = 0.0;
        if (!GetActuatorField(data, f.field_path, val)) continue;

        switch (f.encoding) {
        case DataEncoding::Float32: {
            float fv = static_cast<float>(val);
            uint8_t buf[4]; std::memcpy(buf, &fv, 4);
            payload.insert(payload.end(), buf, buf + 4);
            break;
        }
        case DataEncoding::Float64: {
            uint8_t buf[8]; std::memcpy(buf, &val, 8);
            payload.insert(payload.end(), buf, buf + 8);
            break;
        }
        case DataEncoding::Int8: {
            int8_t iv = static_cast<int8_t>(std::clamp(val, -128.0, 127.0));
            payload.push_back(static_cast<uint8_t>(iv));
            break;
        }
        case DataEncoding::Int16: {
            int16_t iv = static_cast<int16_t>(std::clamp(val, -32768.0, 32767.0));
            uint8_t buf[2]; std::memcpy(buf, &iv, 2);
            payload.insert(payload.end(), buf, buf + 2);
            break;
        }
        case DataEncoding::Int32: {
            int32_t iv = static_cast<int32_t>(std::clamp(val, -2147483648.0, 2147483647.0));
            uint8_t buf[4]; std::memcpy(buf, &iv, 4);
            payload.insert(payload.end(), buf, buf + 4);
            break;
        }
        case DataEncoding::Int64: {
            int64_t iv = static_cast<int64_t>(val);
            uint8_t buf[8]; std::memcpy(buf, &iv, 8);
            payload.insert(payload.end(), buf, buf + 8);
            break;
        }
        case DataEncoding::Uint8: {
            uint8_t uv = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
            payload.push_back(uv);
            break;
        }
        case DataEncoding::Uint16: {
            uint16_t uv = static_cast<uint16_t>(std::clamp(val, 0.0, 65535.0));
            uint8_t buf[2]; std::memcpy(buf, &uv, 2);
            payload.insert(payload.end(), buf, buf + 2);
            break;
        }
        case DataEncoding::Uint32: {
            uint32_t uv = static_cast<uint32_t>(std::clamp(val, 0.0, 4294967295.0));
            uint8_t buf[4]; std::memcpy(buf, &uv, 4);
            payload.insert(payload.end(), buf, buf + 4);
            break;
        }
        case DataEncoding::Uint64: {
            uint64_t uv = static_cast<uint64_t>(val > 0.0 ? val : 0.0);
            uint8_t buf[8]; std::memcpy(buf, &uv, 8);
            payload.insert(payload.end(), buf, buf + 8);
            break;
        }
        case DataEncoding::Bool: {
            uint8_t bv = (val >= 0.5) ? 1 : 0;
            payload.push_back(bv);
            break;
        }
        default: break;
        }
    }
    return payload;
}

/// 计算校验和
inline uint16_t ComputeChecksum(ChecksumType type, const uint8_t* data, size_t len) {
    switch (type) {
    case ChecksumType::Sum8: {
        uint8_t sum = 0;
        for (size_t i = 0; i < len; ++i) sum += data[i];
        return sum;
    }
    case ChecksumType::XOR8: {
        uint8_t x = 0;
        for (size_t i = 0; i < len; ++i) x ^= data[i];
        return x;
    }
    case ChecksumType::CRC16: {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j) {
                if (crc & 1) crc = (crc >> 1) ^ 0xA001;
                else         crc >>= 1;
            }
        }
        return crc;
    }
    default:
        return 0;
    }
}

/// 根据 ProtocolSendConfig 将 ActuatorConfig 序列化为完整帧
inline std::vector<uint8_t> BuildFrame(const ActuatorConfig& data, const ProtocolSendConfig& cfg) {
    std::vector<uint8_t> payload = BuildPayload(data, cfg.fields);

    std::vector<uint8_t> frame;

    // 帧头
    frame.insert(frame.end(), cfg.header.begin(), cfg.header.end());

    // 长度字段（2 字节小端）
    if (cfg.include_length) {
        uint16_t len = static_cast<uint16_t>(payload.size());
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    }

    // 负载
    frame.insert(frame.end(), payload.begin(), payload.end());

    // 校验（对负载数据计算）
    if (cfg.checksum != ChecksumType::None && !payload.empty()) {
        uint16_t cs = ComputeChecksum(cfg.checksum, payload.data(), payload.size());
        if (cfg.checksum == ChecksumType::CRC16) {
            frame.push_back(static_cast<uint8_t>(cs & 0xFF));
            frame.push_back(static_cast<uint8_t>((cs >> 8) & 0xFF));
        } else {
            frame.push_back(static_cast<uint8_t>(cs & 0xFF));
        }
    }

    // 帧尾
    frame.insert(frame.end(), cfg.tail.begin(), cfg.tail.end());

    return frame;
}


// ================== 传感器字段读写 ==================

/// 根据 field_path 从 SensorData 中读取值
inline bool GetSensorField(const SensorData& data, const std::string& path, double& out) {
    if (path == "temperature.value") { out = data.temperature.value; return true; }
    if (path == "humidity.value")    { out = data.humidity.value;    return true; }
    if (path == "depth.value")       { out = data.depth.value;       return true; }
    return false;
}

/// 根据 field_path 向 SensorData 写入值
inline bool SetSensorField(SensorData& data, const std::string& path, double val) {
    if (path == "temperature.value") { data.temperature.value = val; return true; }
    if (path == "humidity.value")    { data.humidity.value    = val; return true; }
    if (path == "depth.value")       { data.depth.value       = val; return true; }
    return false;
}

/// 根据 ProtocolReceiveConfig 解析传感器数据帧
inline SensorData ParseSensorFrame(const std::vector<uint8_t>& raw_data, const ProtocolReceiveConfig& cfg) {
    SensorData data;
    data.is_valid = false;

    size_t minSize = cfg.header.size() + (cfg.include_length ? 2 : 0) + 1; // header + len + msg_type
    if (raw_data.size() < minSize) return data;

    // 检查帧头
    for (size_t i = 0; i < cfg.header.size(); ++i)
        if (raw_data[i] != cfg.header[i]) return data;

    size_t offset = cfg.header.size();
    uint16_t payloadLen = 0;
    if (cfg.include_length) {
        payloadLen = static_cast<uint16_t>(raw_data[offset]) | static_cast<uint16_t>(static_cast<unsigned>(raw_data[offset + 1]) << 8);
        offset += 2;
    } else {
        payloadLen = static_cast<uint16_t>(raw_data.size() - offset - (cfg.tail.empty() ? 0 : cfg.tail.size()) - 1); // -1 for checksum
    }

    // 检查总长度
    size_t totalNeeded = offset + payloadLen + (cfg.checksum != ChecksumType::None ? (cfg.checksum == ChecksumType::CRC16 ? 2 : 1) : 0) + cfg.tail.size();
    if (raw_data.size() != totalNeeded) return data;

    // 验证 msg_type
    uint8_t msgType = raw_data[offset++];
    if (msgType != cfg.msg_type) return data;

    // 校验
    if (cfg.checksum != ChecksumType::None) {
        const uint8_t* payloadStart = raw_data.data() + offset;
        size_t checkLen = payloadLen;
        uint16_t calc = ComputeChecksum(cfg.checksum, payloadStart, checkLen);
        size_t csOffset = offset + payloadLen;
        uint16_t expected = static_cast<uint16_t>(raw_data[csOffset]);
        if (cfg.checksum == ChecksumType::CRC16)
            expected |= static_cast<uint16_t>(static_cast<unsigned>(raw_data[csOffset + 1]) << 8);
        if (calc != expected) return data;
    }

    // 解析字段
    size_t fieldOffset = offset;
    for (const auto& f : cfg.fields) {
        int byteSize = GetEncodingByteSize(f.encoding);
        if (fieldOffset + byteSize > raw_data.size()) break;

        switch (f.encoding) {
        case DataEncoding::Float32: {
            float fv; std::memcpy(&fv, raw_data.data() + fieldOffset, 4);
            fieldOffset += 4;
            SetSensorField(data, f.field_path, static_cast<double>(fv));
            break;
        }
        case DataEncoding::Float64: {
            double dv; std::memcpy(&dv, raw_data.data() + fieldOffset, 8);
            fieldOffset += 8;
            SetSensorField(data, f.field_path, dv);
            break;
        }
        case DataEncoding::Int8: {
            int8_t iv = static_cast<int8_t>(raw_data[fieldOffset++]);
            SetSensorField(data, f.field_path, static_cast<double>(iv));
            break;
        }
        case DataEncoding::Int16: {
            int16_t iv; std::memcpy(&iv, raw_data.data() + fieldOffset, 2);
            fieldOffset += 2;
            SetSensorField(data, f.field_path, static_cast<double>(iv));
            break;
        }
        case DataEncoding::Int32: {
            int32_t iv; std::memcpy(&iv, raw_data.data() + fieldOffset, 4);
            fieldOffset += 4;
            SetSensorField(data, f.field_path, static_cast<double>(iv));
            break;
        }
        case DataEncoding::Int64: {
            int64_t iv; std::memcpy(&iv, raw_data.data() + fieldOffset, 8);
            fieldOffset += 8;
            SetSensorField(data, f.field_path, static_cast<double>(iv));
            break;
        }
        case DataEncoding::Uint8: {
            SetSensorField(data, f.field_path, static_cast<double>(raw_data[fieldOffset++]));
            break;
        }
        case DataEncoding::Uint16: {
            uint16_t uv; std::memcpy(&uv, raw_data.data() + fieldOffset, 2);
            fieldOffset += 2;
            SetSensorField(data, f.field_path, static_cast<double>(uv));
            break;
        }
        case DataEncoding::Uint32: {
            uint32_t uv; std::memcpy(&uv, raw_data.data() + fieldOffset, 4);
            fieldOffset += 4;
            SetSensorField(data, f.field_path, static_cast<double>(uv));
            break;
        }
        case DataEncoding::Uint64: {
            uint64_t uv; std::memcpy(&uv, raw_data.data() + fieldOffset, 8);
            fieldOffset += 8;
            SetSensorField(data, f.field_path, static_cast<double>(uv));
            break;
        }
        case DataEncoding::Bool: {
            SetSensorField(data, f.field_path, static_cast<double>(raw_data[fieldOffset++]));
            break;
        }
        default:
            fieldOffset += byteSize;
            break;
        }
    }

    data.is_valid = true;
    return data;
}

// ================== RobotAPI 接口 ==================

class RobotAPI {
public:
    virtual ~RobotAPI() = default;

    virtual bool Initialize(const std::string& host_ip, int remote_port, int local_port) = 0;
    
    virtual bool HardwareInit(int max_retries = 3) = 0;

    // 获取传感器数据
    virtual SensorData GetSensorData() = 0;

    // 发送执行器数据（使用内部存储的协议配置）
    virtual void SendActuatorData(const ActuatorConfig& data) = 0;

    // 设置协议发送配置
    virtual void SetProtocolConfig(const ProtocolSendConfig& config) = 0;

    // 设置协议接收配置
    virtual void SetProtocolReceiveConfig(const ProtocolReceiveConfig& config) = 0;
};