#pragma once

#include "ManagerBase.h"
#include "GamepadMapperManager.h"
#include "Robot_API/robot_api.h"
#include "Robot_API/hardware_interface.h"
#include "RobotComponentManager.h"
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
    RobotCommConfig  component;
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
    void AddItem() override         { char buf[64]; snprintf(buf, sizeof(buf), "Item_%d", GetNextId()); AddConfig(buf); }
    void RemoveConfig(int id);
    void RemoveItem(int id) override { RemoveConfig(id); }
    void RenameItem(int id, const char* newName) override;
    int    GetItemCount() const override { return (int)m_Nodes.size(); }
    int    GetItemId(int index) const override { return m_Nodes[index].id; }
    char*  GetItemNameBuf(int index) override { return m_Nodes[index].component.name; }
    bool   IsItemSelected(int index) const override { return m_Nodes[index].isSelected; }
    void   SelectItem(int index) override;
    const char* GetDeleteLabel() const override { return "Delete Comm"; }
    void   DrawItemExtras(int index) override;

    void DrawContent() override;

    // ---- 连接控制 ----
    bool Connect(int id);
    void Disconnect();
    bool IsConnected() const { return m_IsConnected; }

    // ---- 数据收发 ----
    void       SendActuatorData(const ActuatorConfig& data);
    SensorData GetSensorData();

    // ---- 外部依赖注入 ----
    void SetRobotComponentManager(RobotComponentManager* c) { m_RobotMgr = c; }
    void SetGamepadMapperManager(GamepadMapperManager* g) { m_GamepadMgr = g; }
    void SetOnActiveModeChanged(std::function<void(int, int)> cb) { m_OnActiveModeChanged = std::move(cb); }
    void SetOnGamepadModeChanged(std::function<void(int, int)> cb) { m_OnGamepadModeChanged = std::move(cb); }

    // ---- 配置/状态访问 ----
    RobotComm&              GetRobotComm()    { return m_RobotComm; }
    std::vector<RobotCommConfig> GetAllItems() const;
    RobotCommNode*          GetSelectedNode();
    RobotAPI*               GetAPI()            { return m_RobotAPI.get(); }

    // ---- 批量配置加载（替换所有现有节点） ----
    void LoadItems(const std::vector<RobotCommConfig>& configs);

private:
    std::vector<RobotCommNode>  m_Nodes;
    bool                        m_IsConnected = false;

    std::shared_ptr<RobotAPI>   m_RobotAPI;
    RobotComponentManager* m_RobotMgr = nullptr;
    GamepadMapperManager*  m_GamepadMgr = nullptr;
    RobotComm                   m_RobotComm;
    std::function<void(int, int)> m_OnActiveModeChanged;  // (oldIdx, newIdx)
    std::function<void(int, int)> m_OnGamepadModeChanged;  // (oldIdx, newIdx)
};
