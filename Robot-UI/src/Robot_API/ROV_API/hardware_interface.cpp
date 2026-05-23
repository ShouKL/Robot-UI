#include "hardware_interface.h"
#include <cstring>
#include <iostream>
#include <sstream>

#include <cstddef>

HardwareInterface::HardwareInterface() {
    m_CurrentSensorData.is_valid = false;  // 初始传感器数据无效

#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);  
#endif
}

HardwareInterface::~HardwareInterface() {
#if defined(_WIN32) || defined(_WIN64)
    if (m_Socket != INVALID_SOCKET) {
        closesocket(m_Socket);        // 关闭UDP套接字
        m_Socket = INVALID_SOCKET;    // 标记为无效
    }
    WSACleanup();                     // 释放Windows Socket资源
#endif
}

bool HardwareInterface::Initialize(const std::string& host_ip, int remote_port, int local_port) {
    std::lock_guard<std::mutex> lock(m_DataMutex);  // 线程安全锁

    // 保存通信参数
    m_TargetIP = host_ip;        // 目标设备IP（下位机/单片机）
    m_TargetPort = remote_port;  // 目标端口
    m_LocalPort = local_port;    // 本机监听端口
    m_IsConnected = false;       // 初始未连接

#if defined(_WIN32) || defined(_WIN64)
    // 如果已有Socket，先关闭
    if (m_Socket != INVALID_SOCKET) {
        closesocket(m_Socket);
        m_Socket = INVALID_SOCKET;
    }

    // 创建UDP Socket
    m_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_Socket != INVALID_SOCKET) {
        unsigned long mode = 1;
        ioctlsocket(m_Socket, FIONBIO, &mode);  // 设置为非阻塞模式

        // 绑定本地地址和端口
        sockaddr_in localAddr;
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
        localAddr.sin_port = htons(local_port);  // 本地端口

        if (bind(m_Socket, (sockaddr*)&localAddr, sizeof(localAddr)) != SOCKET_ERROR) {
            // 初始化目标地址信息
            memset(&m_RemoteAddr, 0, sizeof(m_RemoteAddr));
            m_RemoteAddr.sin_family = AF_INET;
            m_RemoteAddr.sin_port = htons(remote_port);

            // 设置目标IP
            if (host_ip == "0.0.0.0" || host_ip.empty()) {
                m_RemoteAddr.sin_addr.s_addr = INADDR_ANY;
            }
            else {
                m_RemoteAddr.sin_addr.s_addr = inet_addr(host_ip.c_str());
            }

            m_IsConnected = true;  // 初始化成功
        }
        else {
            // 绑定失败，关闭Socket
            closesocket(m_Socket);
            m_Socket = INVALID_SOCKET;
        }
    }
#endif

    return m_IsConnected;
}

bool HardwareInterface::HardwareInit(int max_retries) {
    int attempts = 0;
    while (attempts < max_retries) {
        if (m_IsConnected) {
            std::cout << "Hardware Init Success." << std::endl;
            return true;
        }
        attempts++;
        std::cout << "Hardware Init Retry..." << std::endl;
        Initialize(m_TargetIP, m_TargetPort, m_LocalPort);  // 重新初始化
    }
    std::cout << "Hardware Init Failed after " << max_retries << " retries." << std::endl;
    return false;
}

SensorData HardwareInterface::GetSensorData() {
    std::lock_guard<std::mutex> lock(m_DataMutex);  // 线程安全

#if defined(_WIN32) || defined(_WIN64)
    if (m_Socket != INVALID_SOCKET) {
        char buffer[1024];  // 接收缓冲区
        sockaddr_in senderAddr;
        int senderAddrSize = sizeof(senderAddr);

        // 非阻塞接收UDP数据
        int bytesRead = recvfrom(m_Socket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&senderAddr, &senderAddrSize);
        if (bytesRead > 0) {
            // 转为字节流并解析JSON
            std::vector<uint8_t> raw_data(buffer, buffer + bytesRead);
            SensorData parsed = DeserializeSensorData(raw_data);

            // 解析成功则更新当前传感器数据，并记录发送方地址（自动同步目标地址）
            if (parsed.is_valid) {
                m_CurrentSensorData = parsed;
                m_RemoteAddr = senderAddr;
            }
        }
    }
#endif

    return m_CurrentSensorData;  // 返回最新有效数据
}

void HardwareInterface::SendActuatorData(const ActuatorData& data) {
    std::lock_guard<std::mutex> lock(m_DataMutex);
    m_CurrentActuatorData = data;  // 保存当前控制指令

#if defined(_WIN32) || defined(_WIN64)
    // 只有Socket有效且目标地址正确才发送
    if (m_Socket != INVALID_SOCKET && m_RemoteAddr.sin_family == AF_INET) {
        auto bytes = SerializeActuatorData(m_CurrentActuatorData);
        // 发送UDP数据包
        sendto(m_Socket, (const char*)bytes.data(), static_cast<int>(bytes.size()), 0,
            (sockaddr*)&m_RemoteAddr, sizeof(m_RemoteAddr));
    }
#endif
}

// 序列化：将控制数据转为JSON格式（上位机 → 单片机）
std::vector<uint8_t> HardwareInterface::SerializeActuatorData(const ActuatorData& data) {
    // 自定义数据帧格式：
    // [0]      : 0xAA (start)
    // [1]      : msg_type (0x01 = actuator)
    // [2..3]   : payload length (uint16_t little-endian)
    // [4..N-2] : payload
    // [N-1]    : checksum (simple sum of payload bytes mod 256)

    std::vector<uint8_t> payload;

    auto push_float = [&](float v) {
        uint8_t buf[4];
        std::memcpy(buf, &v, sizeof(v));
        payload.insert(payload.end(), buf, buf + 4);
    };

    // motion as float32
    push_float(static_cast<float>(data.motion.x));
    push_float(static_cast<float>(data.motion.y));
    push_float(static_cast<float>(data.motion.z));
    push_float(static_cast<float>(data.motion.rx));
    push_float(static_cast<float>(data.motion.ry));
    push_float(static_cast<float>(data.motion.rz));

    // servos: count + entries (id: uint8, angle: float32)
    uint8_t servo_count = static_cast<uint8_t>(std::min<size_t>(data.servo.size(), 255));
    payload.push_back(servo_count);
    for (const auto& pair : data.servo) {
        uint8_t id = static_cast<uint8_t>(pair.second.id);
        payload.push_back(id);
        push_float(static_cast<float>(pair.second.angle));
    }

    // brushless motors: count + entries (id: uint8, speed: float32, 8 curve float32)
    uint8_t motor_count = static_cast<uint8_t>(std::min<size_t>(data.brushlessmotor.size(), 255));
    payload.push_back(motor_count);
    for (const auto& pair : data.brushlessmotor) {
        uint8_t id = static_cast<uint8_t>(pair.second.id);
        payload.push_back(id);
        push_float(static_cast<float>(pair.second.target_speed));
        const ThrustCurve& c = pair.second.curve;
        push_float(static_cast<float>(c.np_mid));
        push_float(static_cast<float>(c.np_ini));
        push_float(static_cast<float>(c.pp_ini));
        push_float(static_cast<float>(c.pp_mid));
        push_float(static_cast<float>(c.nt_end));
        push_float(static_cast<float>(c.nt_mid));
        push_float(static_cast<float>(c.pt_mid));
        push_float(static_cast<float>(c.pt_end));
    }

    // build frame
    std::vector<uint8_t> frame;
    frame.push_back(0xAA);
    frame.push_back(0x01); // actuator
    uint16_t len = static_cast<uint16_t>(payload.size());
    frame.push_back(static_cast<uint8_t>(len & 0xFF));
    frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    frame.insert(frame.end(), payload.begin(), payload.end());
    uint8_t checksum = 0;
    for (uint8_t b : payload) checksum = static_cast<uint8_t>((static_cast<unsigned>(checksum) + static_cast<unsigned>(b)) & 0xFF);
    frame.push_back(checksum);

    return frame;
}

// 反序列化：将下位机发来的JSON解析为传感器数据
SensorData HardwareInterface::DeserializeSensorData(const std::vector<uint8_t>& raw_data) {
    SensorData data;
    data.is_valid = false;

    if (raw_data.size() < 6) return data; // too small for header

    // basic frame checks
    if (raw_data[0] != 0xAA) return data;
    uint8_t msg_type = raw_data[1];
    uint16_t len = static_cast<uint16_t>(raw_data[2]) | static_cast<uint16_t>(static_cast<unsigned>(raw_data[3]) << 8);
    if (raw_data.size() != static_cast<size_t>(4u + len + 1u)) return data; // header + payload + checksum

    const uint8_t* payload = raw_data.data() + 4;
    uint8_t checksum = raw_data[4 + len];
    uint8_t calc = 0;
    for (size_t i = 0; i < len; ++i)
        calc = static_cast<uint8_t>((static_cast<unsigned>(calc) + static_cast<unsigned>(payload[i])) & 0xFF);
    if (calc != checksum) return data;

    if (msg_type != 0x02) return data; // not sensor frame

    // sensor payload format: float32 temperature, float32 humidity, float32 depth
    if (len < 12) return data;
    float t = 0.f, h = 0.f, d = 0.f;
    std::memcpy(&t, payload + 0, sizeof(float));
    std::memcpy(&h, payload + 4, sizeof(float));
    std::memcpy(&d, payload + 8, sizeof(float));

    data.temperature.value = static_cast<double>(t);
    data.humidity.value = static_cast<double>(h);
    data.depth.value = static_cast<double>(d);
    data.is_valid = true;
    return data;
}

