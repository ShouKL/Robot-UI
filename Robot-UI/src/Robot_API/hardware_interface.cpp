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
    if (m_SerialHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_SerialHandle);
        m_SerialHandle = INVALID_HANDLE_VALUE;
    }
    WSACleanup();
#endif
}

// ======== 初始化与连接 ========
bool HardwareInterface::Initialize(const std::string& host_ip, int remote_port, int local_port, int transport_type) {
    m_ShuttingDown = false;  // 新连接开始，重置 shutdown 标记
    std::unique_lock<std::mutex> lock(m_DataMutex);

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
    if (m_SerialHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_SerialHandle);
        m_SerialHandle = INVALID_HANDLE_VALUE;
    }

    if (transport_type == 2)  // Serial: 释放锁后委托给 InitSerial
    {
        m_ComPort  = host_ip;
        m_BaudRate = remote_port;
        lock.unlock();
        return InitSerial(m_ComPort, m_BaudRate, m_DataBits, m_StopBits, m_Parity);
    }
    else if (transport_type == 1)  // TCP 客户端
    {
        m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_Socket == INVALID_SOCKET) {
            WL_ERROR_TAG("HW", "TCP socket() failed: {}", WSAGetLastError());
            return false;
        }

        unsigned long mode = 1;
        ioctlsocket(m_Socket, FIONBIO, &mode);

        memset(&m_RemoteAddr, 0, sizeof(m_RemoteAddr));
        m_RemoteAddr.sin_family = AF_INET;
        m_RemoteAddr.sin_port   = htons(remote_port);
        if (host_ip == "0.0.0.0" || host_ip.empty())
            m_RemoteAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        else
            m_RemoteAddr.sin_addr.s_addr = inet_addr(host_ip.c_str());

        if (connect(m_Socket, (sockaddr*)&m_RemoteAddr, sizeof(m_RemoteAddr)) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                WL_ERROR_TAG("HW", "TCP connect() to {}:{} failed: {}", host_ip, remote_port, err);
                closesocket(m_Socket);
                m_Socket = INVALID_SOCKET;
                return false;
            }
            // 非阻塞连接进行中，释放锁后用 select 等最多 3 秒（允许 Shutdown 关闭 socket）
            SOCKET selSocket = m_Socket;
            lock.unlock();
            fd_set fdWrite, fdExcept;
            FD_ZERO(&fdWrite); FD_ZERO(&fdExcept);
            FD_SET(selSocket, &fdWrite);
            FD_SET(selSocket, &fdExcept);
            timeval tv = {3, 0};
            int selRet = select(0, nullptr, &fdWrite, &fdExcept, &tv);
            lock.lock();
            // Shutdown 可能在 select 期间执行了
            if (m_ShuttingDown.load()) {
                WL_INFO_TAG("HW", "TCP connect cancelled by shutdown");
                return false;
            }
            if (selRet <= 0 || FD_ISSET(selSocket, &fdExcept)) {
                WL_ERROR_TAG("HW", "TCP connect() to {}:{} timeout or error", host_ip, remote_port);
                closesocket(m_Socket);
                m_Socket = INVALID_SOCKET;
                return false;
            }
        }

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

bool HardwareInterface::HardwareInit(int max_retries, int start_attempt) {
    int attempts = 0;
    while (attempts < max_retries) {
        if (m_IsConnected) {
            WL_INFO_TAG("HW", "Hardware Init Success.");
            return true;
        }
        attempts++;
        WL_INFO_TAG("HW", "Hardware Init Retry...");
        if (m_TransportType == 2)
            InitSerial(m_ComPort, m_BaudRate, m_DataBits, m_StopBits, m_Parity);
        else
            Initialize(m_TargetIP, m_TargetPort, m_LocalPort, m_TransportType);
    }
    WL_ERROR_TAG("HW", "Hardware Init Failed after {} retries.", max_retries);
    return false;
}

// ======== Serial 初始化 ========
bool HardwareInterface::InitSerial(const std::string& com_port, int baud_rate, int data_bits, int stop_bits, int parity) {
    m_ShuttingDown = false;  // 新连接开始，重置 shutdown 标记
    std::unique_lock<std::mutex> lock(m_DataMutex);

    m_ComPort   = com_port;
    m_BaudRate  = baud_rate;
    m_DataBits  = data_bits;
    m_StopBits  = stop_bits;
    m_Parity    = parity;
    m_TransportType = 2;
    m_IsConnected = false;

#if defined(_WIN32) || defined(_WIN64)
    if (m_SerialHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_SerialHandle);
        m_SerialHandle = INVALID_HANDLE_VALUE;
    }
    if (m_Socket != INVALID_SOCKET) {
        closesocket(m_Socket);
        m_Socket = INVALID_SOCKET;
    }

    std::string portPath = "\\\\.\\" + com_port;
    lock.unlock();  // 释放锁，允许 Shutdown 在其他线程关闭句柄
    HANDLE newHandle = CreateFileA(portPath.c_str(), GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    lock.lock();
    // Shutdown 可能在 CreateFile 期间执行了
    if (m_ShuttingDown.load()) {
        if (newHandle != INVALID_HANDLE_VALUE) CloseHandle(newHandle);
        WL_INFO_TAG("HW", "Serial init cancelled by shutdown");
        return false;
    }
    m_SerialHandle = newHandle;
    if (m_SerialHandle == INVALID_HANDLE_VALUE) {
        WL_ERROR_TAG("HW", "Serial: Cannot open {}", com_port);
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(m_SerialHandle, &dcb)) {
        WL_ERROR_TAG("HW", "Serial: GetCommState failed for {}", com_port);
        CloseHandle(m_SerialHandle);
        m_SerialHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    dcb.BaudRate = baud_rate;
    int actualDataBits[] = {5, 6, 7, 8};
    dcb.ByteSize = (BYTE)((data_bits >= 0 && data_bits < 4) ? actualDataBits[data_bits] : 8);
    dcb.StopBits = (stop_bits == 2) ? TWOSTOPBITS : (stop_bits == 1 ? ONE5STOPBITS : ONESTOPBIT);
    dcb.Parity   = (parity == 1) ? ODDPARITY : (parity == 2) ? EVENPARITY : (parity == 3) ? MARKPARITY : (parity == 4) ? SPACEPARITY : NOPARITY;
    dcb.fBinary  = TRUE;
    dcb.fParity  = (parity != 0) ? TRUE : FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(m_SerialHandle, &dcb)) {
        WL_ERROR_TAG("HW", "Serial: SetCommState failed for {}", com_port);
        CloseHandle(m_SerialHandle);
        m_SerialHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(m_SerialHandle, &timeouts);

    PurgeComm(m_SerialHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);

    m_IsConnected = true;
    WL_INFO_TAG("HW", "Serial connected: {} @ {} {}N{}", com_port, baud_rate, actualDataBits[data_bits],
        (parity == 0 ? "" : (parity == 1 ? "O" : "E")));
#endif
    return m_IsConnected;
}

// ======== 数据收发 ========
SensorData HardwareInterface::GetSensorData() {
    std::lock_guard<std::mutex> lock(m_DataMutex);

#if defined(_WIN32) || defined(_WIN64)
    char buffer[1024];
    int bytesRead = 0;

    if (m_TransportType == 2)  // Serial
    {
        if (m_SerialHandle == INVALID_HANDLE_VALUE) return m_CurrentSensorData;

        DWORD dwRead = 0;
        OVERLAPPED ov = {};
        ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) return m_CurrentSensorData;

        BOOL result = ReadFile(m_SerialHandle, buffer, sizeof(buffer) - 1, &dwRead, &ov);
        if (!result && GetLastError() == ERROR_IO_PENDING) {
            DWORD waitResult = WaitForSingleObject(ov.hEvent, 5); // 5ms timeout
            if (waitResult == WAIT_OBJECT_0)
                GetOverlappedResult(m_SerialHandle, &ov, &dwRead, FALSE);
            else
                CancelIo(m_SerialHandle);
        }
        CloseHandle(ov.hEvent);
        bytesRead = (int)dwRead;
    }
    else if (m_Socket == INVALID_SOCKET) {
        if (!m_LoggedInvalid) {
            WL_ERROR_TAG("HW", "GetSensorData: socket INVALID");
            m_LoggedInvalid = true;
        }
        return m_CurrentSensorData;
    }
    else if (m_TransportType == 1)  // TCP
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

        // 每条接收都输出数据帧（~1 Hz 节流）
        ++m_RecvOkCount;
        {
            auto now = std::chrono::steady_clock::now();
            if (m_LastRecvLogTime.time_since_epoch().count() == 0 ||
                std::chrono::duration_cast<std::chrono::seconds>(now - m_LastRecvLogTime).count() >= 1) {
                if (m_RecvLogSkipCount > 0) {
                    WL_INFO_TAG("HW", "[{}][{}] recv ... {} frames skipped", m_ConnName,
                        m_TransportType == 1 ? "TCP" : "UDP", m_RecvLogSkipCount);
                    m_RecvLogSkipCount = 0;
                }
                m_LastRecvLogTime = now;
                char hexBuf[192] = {0};
                int off = 0;
                for (int i = 0; i < bytesRead && i < 64; ++i)
                    off += snprintf(hexBuf + off, sizeof(hexBuf) - off, "%02X ", (unsigned char)buffer[i]);
                WL_INFO_TAG("HW", "[{}][{}] recv #{} {}B valid={}: {}", m_ConnName,
                    m_TransportType == 1 ? "TCP" : "UDP", m_RecvOkCount, bytesRead,
                    parsed.is_valid ? 1 : 0, hexBuf);
            } else {
                ++m_RecvLogSkipCount;
            }
        }
    }
    else if (bytesRead == 0) {
        const char* tname = (m_TransportType == 2) ? "Serial" : (m_TransportType == 1 ? "TCP" : "UDP");
        WL_WARN_TAG("HW", "{} recv=0 — connection closed by remote", tname);
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
            // 静默轮询，不打印日志
            ++m_IdleCount;
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
    if (m_TransportType == 2) {
        if (m_SerialHandle == INVALID_HANDLE_VALUE) return;
        auto allFrames = SerializeActuatorData(m_CurrentActuatorData);
        for (size_t fi = 0; fi < allFrames.size(); ++fi) {
            const auto& bytes = allFrames[fi];
            if (bytes.empty()) continue;
            DWORD dwWritten = 0;
            OVERLAPPED ov = {};
            ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
            if (!ov.hEvent) continue;
            BOOL result = WriteFile(m_SerialHandle, bytes.data(), (DWORD)bytes.size(), &dwWritten, &ov);
            if (!result && GetLastError() == ERROR_IO_PENDING) {
                WaitForSingleObject(ov.hEvent, 100);
                GetOverlappedResult(m_SerialHandle, &ov, &dwWritten, FALSE);
            }
            CloseHandle(ov.hEvent);

            {
                auto now = std::chrono::steady_clock::now();
                auto& last = m_LastSendLogTime[fi];
                if (dwWritten > 0 && (last.time_since_epoch().count() == 0 ||
                    std::chrono::duration_cast<std::chrono::seconds>(now - last).count() >= 1)) {
                    last = now;
                    char hexBuf[192] = {0};
                    int off = 0;
                    for (size_t i = 0; i < bytes.size() && i < 64; ++i)
                        off += snprintf(hexBuf + off, sizeof(hexBuf) - off, "%02X ", bytes[i]);
                    WL_INFO_TAG("HW", "serial [frame#{}] send {}B: {}", fi, bytes.size(), hexBuf);
                }
            }
        }
        return;
    }

    if (m_Socket == INVALID_SOCKET) return;

    auto allFrames = SerializeActuatorData(m_CurrentActuatorData);
    if (allFrames.empty()) {
        return;
    }

    // 逐帧发送
    for (size_t fi = 0; fi < allFrames.size(); ++fi) {
        const auto& bytes = allFrames[fi];
        if (bytes.empty()) continue;

        int sent = SOCKET_ERROR;
        if (m_TransportType == 1)  // TCP: send()
        {
            sent = send(m_Socket, (const char*)bytes.data(), static_cast<int>(bytes.size()), 0);
            if (sent == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK) {
                    WL_ERROR_TAG("HW", "TCP send() failed: {}", err);
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

        // 按帧索引计时，每帧类型每秒最多输出一条日志
        {
            auto now = std::chrono::steady_clock::now();
            auto& last = m_LastSendLogTime[fi];
            if (sent > 0 && (last.time_since_epoch().count() == 0 ||
                std::chrono::duration_cast<std::chrono::seconds>(now - last).count() >= 1)) {
                last = now;
                char hexBuf[192] = {0};
                int off = 0;
                for (size_t i = 0; i < bytes.size() && i < 64; ++i)
                    off += snprintf(hexBuf + off, sizeof(hexBuf) - off, "%02X ", bytes[i]);
                WL_INFO_TAG("HW", "[{}][{}] [frame#{}] send {}B: {}", m_ConnName,
                    m_TransportType == 1 ? "TCP" : "UDP", fi, bytes.size(), hexBuf);
            }
        }
    }
#endif
}

// ======== 协议配置 ========
// 序列化：根据用户配置的多个 ProtocolSendConfig 动态构建多个帧
std::vector<std::vector<uint8_t>> HardwareInterface::SerializeActuatorData(const ActuatorConfig& data) {
    std::vector<std::vector<uint8_t>> frames;
    for (const auto& cfg : m_ProtocolCfgs) {
        if (!cfg.enabled) continue;
        auto frame = BuildFrame(data, cfg);
        if (!frame.empty())
            frames.push_back(std::move(frame));
    }
    return frames;
}

// ======== 安全关闭（线程安全） ========
void HardwareInterface::Shutdown() {
    m_ShuttingDown = true;  // 先标记（无锁），让 InitSerial/Initialize 能感知

    // 1. 持锁读取并清空句柄（与 Initialize/InitSerial 的 unlock→lock 窗口互斥）
    SOCKET s;
    HANDLE h;
    {
        std::lock_guard<std::mutex> lock(m_DataMutex);
        s = m_Socket;
        h = m_SerialHandle;
        m_Socket = INVALID_SOCKET;
        m_SerialHandle = INVALID_HANDLE_VALUE;
        m_IsConnected = false;
    }

    // 2. 释放锁后关闭 OS 资源（避免阻塞其他需要锁的线程）
#if defined(_WIN32) || defined(_WIN64)
    if (s != INVALID_SOCKET) {
        shutdown(s, SD_BOTH);
        closesocket(s);
        WL_INFO_TAG("HW", "Socket closed (shutdown)");
    }
    if (h != INVALID_HANDLE_VALUE) {
        PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
        CloseHandle(h);
        WL_INFO_TAG("HW", "Serial port closed (shutdown)");
    }
#endif
}

void HardwareInterface::SetProtocolConfig(const std::vector<ProtocolSendConfig>& configs) {
    std::lock_guard<std::mutex> lock(m_DataMutex);
    m_ProtocolCfgs = configs;
}

void HardwareInterface::SetProtocolReceiveConfig(const std::vector<ProtocolReceiveConfig>& configs) {
    std::lock_guard<std::mutex> lock(m_DataMutex);
    m_ReceiveCfgs = configs;
}

SensorData HardwareInterface::DeserializeSensorData(const std::vector<uint8_t>& raw_data) {
    if (m_ReceiveCfgs.empty()) {
        WL_WARN_TAG("HW", "recv {}B but m_ReceiveCfgs is EMPTY — configure receive protocol in RobotMode!", raw_data.size());
        char hexBuf[192] = {0};
        int off = 0;
        for (size_t i = 0; i < raw_data.size() && i < 64; ++i)
            off += snprintf(hexBuf + off, sizeof(hexBuf) - off, "%02X ", raw_data[i]);
        WL_WARN_TAG("HW", "  hex: {}", hexBuf);
        SensorData data;
        data.is_valid = false;
        return data;
    }

    // 遍历所有接收帧配置，第一个匹配的即成功
    for (const auto& rc : m_ReceiveCfgs) {
        SensorData result = ParseSensorFrame(raw_data, rc);
        if (result.is_valid)
            return result;
    }

    // 解析失败：打印收到的数据 vs 期望的协议配置
    if (++m_FailCount % 50 == 0) {
        char hexBuf[192] = {0};
        int off = 0;
        for (size_t i = 0; i < raw_data.size() && i < 64; ++i)
            off += snprintf(hexBuf + off, sizeof(hexBuf) - off, "%02X ", raw_data[i]);
        WL_WARN_TAG("HW", "ParseSensorFrame FAIL #{} — {}B matches none of {} receive config(s)", m_FailCount, raw_data.size(), m_ReceiveCfgs.size());
        for (size_t ci = 0; ci < m_ReceiveCfgs.size(); ++ci) {
            const auto& rc = m_ReceiveCfgs[ci];
            WL_WARN_TAG("HW", "  cfg[{}]: header_size={} include_len={} cmd_bytes={} checksum={} tail_size={} fields={}",
                        ci, rc.header.size(), rc.include_length ? 1 : 0,
                        rc.command_bytes.size(), static_cast<int>(rc.checksum),
                        rc.tail.size(), rc.fields.size());
        }
        WL_WARN_TAG("HW", "  received hex: {}", hexBuf);
    }
    SensorData data;
    data.is_valid = false;
    return data;
}

