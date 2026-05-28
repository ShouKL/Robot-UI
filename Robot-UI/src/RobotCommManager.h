#pragma once
#include "ManagerBase.h"
#include "Robot_API/robot_api.h"
#include "Robot_API/hardware_interface.h"
#include "RobotComponent.h"
#include "RobotComm.h"
#include <string>
#include <memory>
#include <vector>
struct RobotCommNode {
    int  id;
    bool isConnected = false;
    bool isSelected  = false;
    RobotCommConfig config;
};

class RobotCommManager : public ManagerBase {
public:
    RobotCommManager();
    ~RobotCommManager();

    void AddConfig(const char* name);
    void AddItem() override { AddConfig("New_Comm"); }
    void RemoveConfig(int id);
    void RemoveItem(int id) override { RemoveConfig(id); }

    bool Connect(int id);
    void Disconnect();

    void SendActuatorData(const ActuatorData& data);
    SensorData GetSensorData();

    void SetRobotComponent(RobotComponent* c) { m_RobotComponent = c; }
    void SetOnActiveModeChanged(std::function<void(int)> cb) { m_OnActiveModeChanged = std::move(cb); }

    RobotComm& GetRobotComm() { return m_RobotComm; }
    std::vector<RobotCommConfig> GetAllConfigs() const;
    RobotCommConfig* GetActiveConfig();
    RobotCommNode*    GetActiveNode();
    int GetActiveId() const { return m_ActiveId; }

    void DrawUI(const char* windowName, bool* p_open) override;

    bool IsConnected() const { return m_IsConnected; }
    RobotAPI* GetAPI() { return m_RobotAPI.get(); }

private:
    std::vector<RobotCommNode> m_Nodes;
    int m_ActiveId = -1;
    bool m_IsConnected = false;

    std::shared_ptr<RobotAPI> m_RobotAPI;
    RobotComponent* m_RobotComponent = nullptr;
    RobotComm m_RobotComm;
    std::function<void(int)> m_OnActiveModeChanged;
};
