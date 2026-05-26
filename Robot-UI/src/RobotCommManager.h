#pragma once
#include "Robot_API/robot_api.h"
#include "Robot_API/hardware_interface.h"
#include "Robot_Config.h"
#include <string>
#include <memory>
#include <vector>
#include <mutex>

// ==================== Robot 通信配置结构体 ====================
struct RobotCommConfig {
    char name[64] = "Default";

    // Network
    char host_ip[64] = "192.168.0.10";
    int  remote_port = 8888;
    int  local_port  = 8888;

    // Transport
    int  transport_type = 0;  // 0=UDP, 1=TCP, 2=Serial
};

// ==================== 通信管理节点 ====================
struct RobotCommNode {
    int  id;
    bool isConnected = false;
    bool isSelected  = false;
    RobotCommConfig config;
    // HardwareInterface is owned by manager, shared_ptr here just for reference
};

// ==================== 通信管理器 ====================
class RobotCommManager {
public:
    RobotCommManager();
    ~RobotCommManager();

    // ---------- 设备管理 ----------
    void AddConfig(const char* name);
    void RemoveConfig(int id);

    // ---------- 连接控制 ----------
    bool Connect(int id);
    void Disconnect();

    // ---------- 数据收发 ----------
    void SendActuatorData(const ActuatorData& data);
    SensorData GetSensorData();

    // ---------- 配置访问 ----------
    void SetRobotConfig(Robot_Config* cfg) { m_RobotConfig = cfg; }
    std::vector<RobotCommConfig> GetAllConfigs() const;
    RobotCommConfig* GetActiveConfig();
    RobotCommNode*    GetActiveNode();
    int GetActiveId() const { return m_ActiveId; }

    // ---------- UI ----------
    void DrawUI(const char* windowName, bool* p_open);
    void DrawUI();  // uses internal open flag
    void OpenConfigWindow();
    bool IsConfigWindowOpen() const { return m_ConfigWindowOpen; }

    // ---------- 状态 ----------
    bool IsConnected() const { return m_IsConnected; }
    RobotAPI* GetAPI() { return m_RobotAPI.get(); }

private:
    std::vector<RobotCommNode> m_Nodes;
    int m_NextId = 1;
    int m_ActiveId = -1;
    bool m_IsConnected = false;
    bool m_ConfigWindowOpen = false;

    std::shared_ptr<RobotAPI> m_RobotAPI;
    Robot_Config* m_RobotConfig = nullptr;
    std::mutex m_DataMutex;

    // UI 子渲染
    void DrawConfigTree();
    void DrawControlPanel();
};
