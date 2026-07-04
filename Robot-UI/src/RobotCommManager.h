#pragma once

#include "ManagerBase.h"
#include "RobotComm.h"
#include "RobotComponentManager.h"

// ============================================================================
// RobotCommManager —       （         ）
//    unique_ptr<RobotComm>    ，  RobotComm          UI
// ============================================================================

class RobotCommManager : public ManagerBase
{
public:
    RobotCommManager();
    ~RobotCommManager() = default;

    // ----    ----
    void AddItem() override;
    void RemoveItem(int id) override;
    void RenameItem(int id, const char* newName) override;
    int    GetItemCount() const override { return (int)m_Nodes.size(); }
    int    GetItemId(int index) const override { return m_Nodes[index]->id; }
    char*  GetItemNameBuf(int index) override { return m_Nodes[index]->name; }
    bool   IsItemSelected(int index) const override { return m_Nodes[index]->isSelected; }
    void   SelectItem(int index) override;
    const char* GetDeleteLabel() const override { return "Delete Comm"; }
    void DrawContent() override;

    std::string ClipboardCopySelected() override;
    void        ClipboardPaste(const std::string& yaml) override;

    // ---- Drawing (all UI rendering moved from RobotComm to Manager) ----
    void DrawSendFieldConfig(RobotComm& node, ProtocolSendConfig& cfg, ActuatorConfig& actuator);
    void DrawReceiveFieldConfig(RobotComm& node, ProtocolReceiveConfig& cfg, const SensorConfig& sensor);
    void DrawControlPanel(RobotComm& node,
                          RobotComponentManager* robotMgr);

    // ----         （   RobotComm    UI） ----
    void SetRobotComponentManager(RobotComponentManager* c) { m_RobotMgr = c; }

    // ----    ----
    std::vector<RobotComm> GetAllItems() const;
    std::vector<std::unique_ptr<RobotComm>>& GetNodes() { return m_Nodes; }
    RobotComm* GetSelectedNode();

    // ----        （       ） ----
    void LoadItems(const std::vector<RobotComm>& configs);

    void ResetToDefault();

private:
    std::vector<std::unique_ptr<RobotComm>> m_Nodes;
    RobotComponentManager*      m_RobotMgr = nullptr;
};
