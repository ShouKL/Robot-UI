#include "hardware_interface.h"
#include <cstdio>
#include <cstring>

#include "Walnut/Core/Log.h"

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
        closesocket(m_Socket);
        m_Socket = INVALID_SOCKET;
    }
    WSACleanup();
#endif
}

// ======== 初始化与连接 ========
bool HardwareInterface::Initialize(const std::string& host_ip, int remote_port, int local_port, int transport_type) {
    std::lock_guard<std::mutex> lock(m_DataMutex);

    m_TargetIP      = host_ip;
    m_TargetPort    = remote_port;
    m_LocalPort     = local_port;
    m_TransportType = transport_type;
    m_IsConnected   = false;

#if defined(_WIN32) || defined(_WIN64)
    if (m_Socket != INVALID_SOCKET) {
        closesocket(m_Socket);
        m_Socket = INVALID_SOCKET;
    }

    if (transport_type == 1)  // TCP 客户端：connect 到远端服务器，同一条连接双向收发
    {
        m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_Socket == INVALID_SOCKET) {
            WL_ERROR_TAG("HW", "TCP socket() failed: {}", WSAGetLastError());
            return false;
        }

        memset(&m_RemoteAddr, 0, sizeof(m_RemoteAddr));
        m_RemoteAddr.sin_family = AF_INET;
        m_RemoteAddr.sin_port   = htons(remote_port);
        if (host_ip == "0.0.0.0" || host_ip.empty())
            m_RemoteAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        else
            m_RemoteAddr.sin_addr.s_addr = inet_addr(host_ip.c_str());

        if (connect(m_Socket, (sockaddr*)&m_RemoteAddr, sizeof(m_RemoteAddr)) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            WL_ERROR_TAG("HW", "TCP connect() to {}:{} failed: {}", host_ip, remote_port, err);
            closesocket(m_Socket);
            m_Socket = INVALID_SOCKET;
            return false;
        }

        unsigned long mode = 1;
        ioctlsocket(m_Socket, FIONBIO, &mode);

        int nodelay = 1;
        setsockopt(m_Socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

        m_IsConnected = true;
        WL_INFO_TAG("HW", "TCP connected to " + host_ip + ":" + std::to_string(remote_port));
    }
    else  // UDP (default)
    {
        m_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_Socket == INVALID_SOCKET) {
            WL_ERROR_TAG("HW", "UDP socket() failed: {}", WSAGetLastError());
            return false;
        }

        unsigned long mode = 1;
        ioctlsocket(m_Socket, FIONBIO, &mode);

        sockaddr_in localAddr;
        memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = INADDR_ANY;
        localAddr.sin_port = htons(local_port);

        if (bind(m_Socket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            WL_ERROR_TAG("HW", "UDP bind() port {} failed: {}", local_port, err);
            closesocket(m_Socket);
            m_Socket = INVALID_SOCKET;
            return false;
        }

        memset(&m_RemoteAddr, 0, sizeof(m_RemoteAddr));
        m_RemoteAddr.sin_family = AF_INET;
        m_RemoteAddr.sin_port   = htons(remote_port);
        if (host_ip == "0.0.0.0" || host_ip.empty())
            m_RemoteAddr.sin_addr.s_addr = INADDR_ANY;
        else
            m_RemoteAddr.sin_addr.s_addr = inet_addr(host_ip.c_str());

        m_IsConnected = true;
        WL_INFO_TAG("HW", "UDP bound to :{} -> {}:{}", local_port, host_ip, remote_port);
    }
#endif

    return m_IsConnected;
}

bool HardwareInterface::HardwareInit(int max_retries) {
    int attempts = 0;
    while (attempts < max_retries) {
        if (m_IsConnected) {
            WL_INFO_TAG("HW", "Hardware Init Success.");
            return true;
        }
        attempts++;
        WL_INFO_TAG("HW", "Hardware Init Retry...");
        Initialize(m_TargetIP, m_TargetPort, m_LocalPort, m_TransportType);
    }
    WL_ERROR_TAG("HW", "Hardware Init Failed after {} retries.", max_retries);
    return false;
}

// ======== 数据收发 ========
SensorData HardwareInterface::GetSensorData() {
    std::lock_guard<std::mutex> lock(m_DataMutex);

#if defined(_WIN32) || defined(_WIN64)
    if (m_Socket == INVALID_SOCKET) {
        static bool s_LoggedInvalid = false;
        if (!s_LoggedInvalid) {
            WL_ERROR_TAG("HW", "GetSensorData: socket INVALID");
            s_LoggedInvalid = true;
        }
        return m_CurrentSensorData;
    }

    char buffer[1024];
    int bytesRead = 0;

    if (m_TransportType == 1)  // TCP
    {
        bytesRead = recv(m_Socket, buffer, sizeof(buffer) - 1, 0);
    }
    else  // UDP
    {
        sockaddr_in senderAddr;
        int senderAddrSize = sizeof(senderAddr);
        bytesRead = recvfrom(m_Socket, buffer, sizeof(buffer) - 1, 0,
                             (sockaddr*)&senderAddr, &senderAddrSize);
    }

    if (bytesRead > 0) {
        std::vector<uint8_t> raw_data(buffer, buffer + bytesRead);
        SensorData parsed = DeserializeSensorData(raw_data);
        if (parsed.is_valid)
            m_CurrentSensorData = parsed;

        // 每条接收都打印（调试期间）
        static int s_RecvOkCount = 0;
        ++s_RecvOkCount;
        {
            char hexBuf[192] = {0};
            int off = 0;
            for (int i = 0; i < bytesRead && i < 64; ++i)
                off += snprintf(hexBuf + off, sizeof(hexBuf) - off, "%02X ", (unsigned char)buffer[i]);
            WL_INFO_TAG("HW", "recv #{} {}B valid={} hex: {}", s_RecvOkCount, bytesRead, parsed.is_valid ? 1 : 0, hexBuf);
            if (parsed.is_valid) {
                WL_INFO_TAG("HW", "  temperature: {:.3f}  humidity: {:.3f}  depth: {:.3f}",
                            parsed.temperature.value.value, parsed.humidity.value.value, parsed.depth.value.value);
            }
        }
    }
    else if (bytesRead == 0) {
        WL_WARN_TAG("HW", "{} recv=0 — connection closed by remote", m_TransportType == 1 ? "TCP" : "UDP");
        // TCP 优雅关闭：远端发送了 FIN，标记断开
        if (m_TransportType == 1) {
            m_IsConnected = false;
            closesocket(m_Socket);
            m_Socket = INVALID_SOCKET;
            WL_ERROR_TAG("HW", "TCP connection closed by remote — marked disconnected");
        }
    }
    else {
        // bytesRead == SOCKET_ERROR (-1)
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            // 每 300 帧打印，确认 recv 在正常轮询
            static int s_IdleCount = 0;
            if (++s_IdleCount % 300 == 0) {
                // 顺便检查 socket 是否出错了（send 可能成功但 socket 已半关闭）
                int sockErr = 0;
                int sockErrLen = sizeof(sockErr);
                getsockopt(m_Socket, SOL_SOCKET, SO_ERROR, (char*)&sockErr, &sockErrLen);
                WL_INFO_TAG("HW", "[{}] recv idle #{} — socket_err={} connected={} -> {}:{}",
                            m_TransportType == 1 ? "TCP" : "UDP",
                            s_IdleCount, sockErr, m_IsConnected ? 1 : 0,
                            m_TargetIP, m_TargetPort);
            }
        }
        else {
            WL_ERROR_TAG("HW", "{} recv error: {} ({}:{})", m_TransportType == 1 ? "TCP" : "UDP",
                         err, m_TargetIP, m_TargetPort);
            // 致命错误：远端重置连接或网络不可达，标记断开
            if (m_TransportType == 1) {
                m_IsConnected = false;
                closesocket(m_Socket);
                m_Socket = INVALID_SOCKET;
                WL_ERROR_TAG("HW", "TCP connection lost — marked disconnected");
            }
        }
    }
#endif

    return m_CurrentSensorData;
}

void HardwareInterface::SendActuatorData(const ActuatorConfig& data) {
    std::lock_guard<std::mutex> lock(m_DataMutex);
    m_CurrentActuatorData = data;

#if defined(_WIN32) || defined(_WIN64)
    if (m_Socket == INVALID_SOCKET) return;

    auto bytes = SerializeActuatorData(m_CurrentActuatorData);
    if (bytes.empty()) return;

    int sent = SOCKET_ERROR;
    if (m_TransportType == 1)  // TCP: send()
    {
        sent = send(m_Socket, (const char*)bytes.data(), static_cast<int>(bytes.size()), 0);
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                WL_ERROR_TAG("HW", "TCP send() failed: {}", err);
                // 致命错误：连接已断开，标记并关闭 socket
                m_IsConnected = false;
                closesocket(m_Socket);
                m_Socket = INVALID_SOCKET;
                WL_ERROR_TAG("HW", "TCP connection lost during send — marked disconnected");
            }
        }
    }
    else  // UDP: sendto()
    {
        if (m_RemoteAddr.sin_family == AF_INET) {
            sent = sendto(m_Socket, (const char*)bytes.data(), static_cast<int>(bytes.size()), 0,
                          (sockaddr*)&m_RemoteAddr, sizeof(m_RemoteAddr));
            if (sent == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK)
                    WL_ERROR_TAG("HW", "UDP sendto() failed: {}", err);
            }
        }
    }

    // 每 100 帧打印一次 hex dump（≈每秒1次 @100Hz）
    static int s_SendCount = 0;
    if (++s_SendCount % 100 == 0 && sent > 0) {
        char hexBuf[192] = {0};
        int off = 0;
        for (size_t i = 0; i < bytes.size() && i < 64; ++i)
            off += snprintf(hexBuf + off, sizeof(hexBuf) - off, "%02X ", bytes[i]);
        WL_INFO_TAG("HW", "send #{} {}B: {}", s_SendCount, bytes.size(), hexBuf);
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
        WL_WARN_TAG("HW", "recv {}B but m_ReceiveCfg.fields is EMPTY — configure receive protocol in RobotMode!", raw_data.size());
        char hexBuf[192] = {0};
        int off = 0;
        for (size_t i = 0; i < raw_data.size() && i < 64; ++i)
            off += snprintf(hexBuf + off, sizeof(hexBuf) - off, "%02X ", raw_data[i]);
        WL_WARN_TAG("HW", "  hex: {}", hexBuf);
        SensorData data;
        data.is_valid = false;
        return data;
    }

    SensorData result = ParseSensorFrame(raw_data, m_ReceiveCfg);
    if (!result.is_valid) {
        // 解析失败：打印收到的数据 vs 期望的协议配置
        static int s_FailCount = 0;
        if (++s_FailCount % 50 == 0) {
            char hexBuf[192] = {0};
            int off = 0;
            for (size_t i = 0; i < raw_data.size() && i < 64; ++i)
                off += snprintf(hexBuf + off, sizeof(hexBuf) - off, "%02X ", raw_data[i]);
            WL_WARN_TAG("HW", "ParseSensorFrame FAIL #{} — {}B does NOT match protocol config:", s_FailCount, raw_data.size());
            WL_WARN_TAG("HW", "  config: header_size={} include_len={} msg_type=0x{:02X} checksum={} tail_size={} fields={}",
                        m_ReceiveCfg.header.size(), m_ReceiveCfg.include_length ? 1 : 0,
                        m_ReceiveCfg.msg_type, static_cast<int>(m_ReceiveCfg.checksum),
                        m_ReceiveCfg.tail.size(), m_ReceiveCfg.fields.size());
            WL_WARN_TAG("HW", "  received hex: {}", hexBuf);
        }
    }
    return result;
}

