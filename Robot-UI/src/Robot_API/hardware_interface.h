#pragma once

#include "robot_api.h"
#include <mutex>
#include <vector>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib,"ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#undef connect
#undef bind
#undef listen
#undef accept
#undef send
#undef recv
#endif

class HardwareInterface : public RobotAPI
{
public:
    HardwareInterface();
    ~HardwareInterface() override;

    bool Initialize(const std::string& host_ip, int remote_port, int local_port, int transport_type = 0) override;
    bool HardwareInit(int max_retries = 3) override;
    bool IsConnected() const { return m_IsConnected; }

    SensorData GetSensorData() override;
    void SendActuatorData(const ActuatorConfig& data) override;

    void SetProtocolConfig(const std::vector<ProtocolSendConfig>& configs) override;
    void SetProtocolReceiveConfig(const std::vector<ProtocolReceiveConfig>& configs) override;

private:
    SensorData m_CurrentSensorData;
    ActuatorConfig m_CurrentActuatorData;
    std::mutex m_DataMutex;

    int m_LocalPort = 0;
    int m_TransportType = 0;  // 0=UDP, 1=TCP
    bool m_IsConnected = false;
    std::string m_TargetIP;
    int m_TargetPort = 0;

    std::vector<ProtocolSendConfig>    m_ProtocolCfgs;   // 用户自定义发送协议（多帧）
    std::vector<ProtocolReceiveConfig> m_ReceiveCfgs;    // 用户自定义接收协议（多帧）

#if defined(_WIN32) || defined(_WIN64)
    SOCKET m_Socket = INVALID_SOCKET;      // TCP/UDP 共用 socket
    sockaddr_in m_RemoteAddr;
#endif

    std::vector<std::vector<uint8_t>> SerializeActuatorData(const ActuatorConfig& data);
    SensorData DeserializeSensorData(const std::vector<uint8_t>& raw_data);
};