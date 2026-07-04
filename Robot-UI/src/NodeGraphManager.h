#pragma once

// ============================================================================
// NodeGraphManager — visual node graph editor with item list management.
// Contains one active NodeGraph + EditorContext. Each item is a named
// workspace snapshot (unique_ptr<NodeGraph>). Inherits ManagerBase.
// ============================================================================

#include "ManagerBase.h"
#include "NodeGraph.h"
#include "Walnut/Image.h"

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
    int    GetItemId(int index) const override { return m_Items[index]->id; }
    char*  GetItemNameBuf(int index) override { return m_Items[index]->name; }
    int    GetSelectedIndex() const override { return m_SelectedIndex; }
    void   SelectItem(int index) override { SetSelectedIndex(index); }

    void DrawContent() override;

    std::string ClipboardCopySelected() override;
    void        ClipboardPaste(const std::string& yaml) override;

    // ---- Drawing (all UI rendering moved from NodeGraph to Manager) ----
    void DrawNodeGraphEditor(NodeGraph& ng);
    void DrawNodeContents(NodeGraph& ng, EditorNode& node,
                          const std::set<std::string>& analogKeys,
                          const std::vector<OutputTargetInfo>& outputTargets);
    void DrawKeyValuesSidebar(NodeGraph& ng, float sideWidth,
                              const std::set<std::string>& analogKeys);
    void DrawGlobalsSidebar(NodeGraph& ng, float sideWidth);
    void DrawCommRefsSidebar(NodeGraph& ng, float sideWidth);
    void DrawTriggerSidebar(NodeGraph& ng, float sideWidth);

    // ---- Dependency injection ----
    void SetRobotComponentManager(RobotComponentManager* c);
    void SetGamepadMapperManager(GamepadMapperManager* g);
    void SetRobotCommManager(RobotCommManager* comm);
    void SetShortcutManager(ShortcutManager* sm);
    void SetSendActionCb(std::function<void(int,bool,bool)> cb);

    // ---- Internal (called by RobotSettingPanel) ----
    void ApplyChanges();
    void RequestNavigate() { m_SelectedGraph->RequestNavigate(); }

    // ---- Item snapshot / restore ----
    std::vector<NodeGraph> GetAllItems() const;
    void LoadItems(const std::vector<NodeGraph>& items);

    void ResetToDefault();

    // ---- Graph YAML (for SaveCurrentToItem / LoadItemToCurrent) ----
    std::string GetGraphYaml() const;
    bool        LoadGraphYaml(const std::string& yamlStr);
    void SaveCurrentToItem();

    // ----   RobotStatus    NodeGraph    ----
    std::string GetGraphYamlForIndex(int idx);
    std::string GetGraphDataYamlForIndex(int idx);  // no SaveCurrentToItem side effect

private:
    void LoadItemToCurrent();

    std::vector<std::unique_ptr<NodeGraph>> m_Items;
    int m_SelectedIndex = 0;

    ax::NodeEditor::EditorContext* m_EditorCtx = nullptr;
    NodeGraph* m_SelectedGraph = nullptr;

    RobotComponentManager* m_RobotMgr = nullptr;
    GamepadMapperManager*  m_GamepadMgr = nullptr;
    RobotCommManager*      m_RobotCommMgr = nullptr;
    ShortcutManager*       m_ShortcutMgr = nullptr;
    std::function<void(int,bool,bool)> m_StoredSendActionCb;

    // ---- Clipboard (Copy/Paste) ----
    std::string m_ClipboardYaml;  // YAML snapshot of copied nodes + links
};
