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

// ======== 初始化与连接 ========
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

// ======== 数据收发 ========
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

void HardwareInterface::SendActuatorData(const ActuatorConfig& data) {
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

// ======== 协议配置 ========
// 序列化：根据用户配置的 ProtocolSendConfig 动态构建帧
std::vector<uint8_t> HardwareInterface::SerializeActuatorData(const ActuatorConfig& data) {
    return BuildFrame(data, m_ProtocolCfg);
}

void HardwareInterface::SetProtocolConfig(const ProtocolSendConfig& config) {
    std::lock_guard<std::mutex> lock(m_DataMutex);
    m_ProtocolCfg = config;
}

void HardwareInterface::SetProtocolReceiveConfig(const ProtocolReceiveConfig& config) {
    std::lock_guard<std::mutex> lock(m_DataMutex);
    m_ReceiveCfg = config;
}

SensorData HardwareInterface::DeserializeSensorData(const std::vector<uint8_t>& raw_data) {
    if (m_ReceiveCfg.fields.empty()) {
        SensorData data;
        data.is_valid = false;
        return data;
    }

    return ParseSensorFrame(raw_data, m_ReceiveCfg);
}

