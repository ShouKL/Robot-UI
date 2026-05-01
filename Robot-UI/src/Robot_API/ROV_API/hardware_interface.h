#pragma once

#include "../robot_api.h"
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

    bool Initialize(const std::string& host_ip, int remote_port, int local_port) override;
    bool HardwareInit(int max_retries = 3) override;

    SensorData GetSensorData() override;
    void SendActuatorData(const ActuatorData& data) override;

private:
    SensorData m_CurrentSensorData;
    ActuatorData m_CurrentActuatorData;
    std::mutex m_DataMutex;

    int m_LocalPort;
    bool m_IsConnected;
    std::string m_TargetIP;
    int m_TargetPort;

#if defined(_WIN32) || defined(_WIN64)
    SOCKET m_Socket;
    sockaddr_in m_RemoteAddr;
#endif

    std::vector<uint8_t> SerializeActuatorData(const ActuatorData& data);
    SensorData DeserializeSensorData(const std::vector<uint8_t>& raw_data);
};