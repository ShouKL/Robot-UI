#pragma once

// ============================================================================
// NodeGraphManager — visual node graph editor with item list management.
// Contains one active NodeGraph + EditorContext. Each GraphItem is a named
// workspace snapshot. Inherits ManagerBase.
// ============================================================================

#include "ManagerBase.h"
#include "NodeGraph.h"
#include <imgui_node_editor.h>
#include <vector>
#include <string>
#include <map>
#include <set>

class RobotComponentManager;
class GamepadMapperManager;
class RobotCommManager;

struct GraphItem
{
    int  id = 0;
    bool isSelected = false;
    char name[64] = "Default";
    std::unique_ptr<NodeGraph> graph;
    std::string editorYaml;      // cached full YAML (with positions) for editor restore
    int  comm_index = 0;         // 关联的 RobotComm 配置索引
};

class NodeGraphManager : public ManagerBase
{
public:
    NodeGraphManager();
    ~NodeGraphManager();

    void AddItem() override;
    void RemoveItem(int id) override;
    void RenameItem(int id, const char* newName) override;

    ax::NodeEditor::EditorContext* GetEditorContext() const { return m_EditorCtx; }
    NodeGraph* GetSelectedGraph() const { return m_SelectedGraph; }

    void SetSelectedIndex(int idx);

    int    GetItemCount() const override { return (int)m_Items.size(); }
    int    GetItemId(int index) const override { return m_Items[index].id; }
    char*  GetItemNameBuf(int index) override { return m_Items[index].name; }
    int    GetSelectedIndex() const override { return m_SelectedIndex; }
    void   SelectItem(int index) override { SetSelectedIndex(index); }

    void DrawContent() override;

    // ---- Dependency injection ----
    void SetRobotComponentManager(RobotComponentManager* c);
    void SetGamepadMapperManager(GamepadMapperManager* g);
    void SetRobotCommManager(RobotCommManager* comm);

    // ---- Internal (called by RobotSettingPanel) ----
    void ApplyChanges();
    void RequestNavigate() { m_SelectedGraph->RequestNavigate(); }

    // ---- Item snapshot / restore ----
    std::vector<GraphItem> GetAllItems() const;
    void LoadItems(const std::vector<GraphItem>& items);

    // ---- Graph YAML (for SaveCurrentToItem / LoadItemToCurrent) ----
    std::string GetGraphYaml() const;
    bool        LoadGraphYaml(const std::string& yamlStr);

    // ---- 供 RobotStatus 同步活跃 NodeGraph 到求值器 ----
    std::string GetGraphYamlForIndex(int idx);

private:
    void SaveCurrentToItem();
    void LoadItemToCurrent();

    std::vector<GraphItem> m_Items;
    int m_SelectedIndex = 0;

    ax::NodeEditor::EditorContext* m_EditorCtx = nullptr;
    NodeGraph* m_SelectedGraph = nullptr;

    RobotComponentManager* m_RobotMgr = nullptr;
    GamepadMapperManager*  m_GamepadMgr = nullptr;
    RobotCommManager*      m_RobotCommMgr = nullptr;
};
