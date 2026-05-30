#pragma once

#include "ManagerBase.h"
#include "GamepadMapper.h"
#include "Robot_API/robot_api.h"
#include "Robot_API/hardware_interface.h"
#include "RobotComponent.h"
#include "RobotComm.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// RobotCommNode — 通信配置节点
// ============================================================================

struct RobotCommNode
{
    int  id;
    bool isConnected = false;
    bool isSelected  = false;
    RobotCommConfig  config;
};

// ============================================================================
// RobotCommManager — 通信管理器
// 管理多个通信配置节点，控制连接/断开/数据收发
// ============================================================================

class RobotCommManager : public ManagerBase
{
public:
    RobotCommManager();
    ~RobotCommManager();

    // ---- 节点管理 ----
    void AddConfig(const char* name);
    void AddItem() override         { AddConfig("New_Comm"); }
    void RemoveConfig(int id);
    void RemoveItem(int id) override { RemoveConfig(id); }

    // ---- 连接控制 ----
    bool Connect(int id);
    void Disconnect();
    bool IsConnected() const { return m_IsConnected; }

    // ---- 数据收发 ----
    void       SendActuatorData(const ActuatorConfig& data);
    SensorData GetSensorData();

    // ---- 外部依赖注入 ----
    void SetRobotComponent(RobotComponent* c)                { m_RobotComponent = c; }
    void SetGamepadMapper(GamepadMapper* g)                  { m_GamepadMapper = g; }
    void SetOnActiveModeChanged(std::function<void(int, int)> cb) { m_OnActiveModeChanged = std::move(cb); }
    void SetOnGamepadModeChanged(std::function<void(int, int)> cb) { m_OnGamepadModeChanged = std::move(cb); }

    // ---- 配置/状态访问 ----
    RobotComm&              GetRobotComm()    { return m_RobotComm; }
    std::vector<RobotCommConfig> GetAllConfigs() const;
    RobotCommConfig*        GetActiveConfig();
    RobotCommNode*          GetActiveNode();
    int                     GetActiveId() const { return m_ActiveId; }
    RobotAPI*               GetAPI()            { return m_RobotAPI.get(); }

    // ---- 批量配置加载（替换所有现有节点） ----
    void LoadConfigs(const std::vector<RobotCommConfig>& configs, int activeId);

    // ---- UI ----
    void DrawUI(const char* windowName, bool* p_open) override;

private:
    std::vector<RobotCommNode>  m_Nodes;
    int                         m_ActiveId    = -1;
    bool                        m_IsConnected = false;

    std::shared_ptr<RobotAPI>   m_RobotAPI;
    RobotComponent*             m_RobotComponent = nullptr;
    GamepadMapper*              m_GamepadMapper = nullptr;
    RobotComm                   m_RobotComm;
    std::function<void(int, int)> m_OnActiveModeChanged;  // (oldIdx, newIdx)
    std::function<void(int, int)> m_OnGamepadModeChanged;  // (oldIdx, newIdx)
};
