#pragma once

#include "ManagerBase.h"
#include "RobotComm.h"
#include <memory>
#include <string>
#include <vector>

class RobotComponentManager;

// ============================================================================
// RobotCommNode — 通信配置节点（纯编辑，不持连接）
// ============================================================================

struct RobotCommNode
{
    int  id;
    bool isSelected  = false;
    RobotCommConfig  component;
};

// ============================================================================
// RobotCommManager — 通信配置编辑器
// 纯编辑管理：增删改查通信配置，不涉及连接/断开/收发
// ============================================================================

class RobotCommManager : public ManagerBase
{
public:
    RobotCommManager();
    ~RobotCommManager() = default;

    // ---- 节点管理 ----
    void AddItem() override;
    void RemoveItem(int id) override;
    void RenameItem(int id, const char* newName) override;
    int    GetItemCount() const override { return (int)m_Nodes.size(); }
    int    GetItemId(int index) const override { return m_Nodes[index].id; }
    char*  GetItemNameBuf(int index) override { return m_Nodes[index].component.name; }
    bool   IsItemSelected(int index) const override { return m_Nodes[index].isSelected; }
    void   SelectItem(int index) override;
    const char* GetDeleteLabel() const override { return "Delete Comm"; }
    void DrawContent() override;

    // ---- 外部依赖注入（仅透传给 RobotComm 的编辑 UI） ----
    void SetRobotComponentManager(RobotComponentManager* c) { m_RobotMgr = c; }

    // ---- 配置访问 ----
    RobotComm&              GetRobotComm()    { return m_RobotComm; }
    std::vector<RobotCommConfig> GetAllItems() const;
    std::vector<RobotCommNode>&   GetNodes()   { return m_Nodes; }
    RobotCommNode*          GetSelectedNode();

    // ---- 批量配置加载（替换所有现有节点） ----
    void LoadItems(const std::vector<RobotCommConfig>& configs);

private:
    std::vector<RobotCommNode>  m_Nodes;

    std::shared_ptr<RobotAPI>   m_RobotAPI;
    RobotComponentManager*      m_RobotMgr = nullptr;
    RobotComm                   m_RobotComm;
};
