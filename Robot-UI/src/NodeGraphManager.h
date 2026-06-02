#pragma once

// ============================================================================
// NodeGraphManager — visual node graph editor with item list management.
// Contains one active NodeGraph + EditorContext. Each GraphItem is a named
// workspace snapshot. Inherits ManagerBase + EditDraftBase.
// ============================================================================

#include "EditDraftBase.h"
#include "ManagerBase.h"
#include "NodeGraph.h"
#include <imgui_node_editor.h>
#include <vector>
#include <string>
#include <map>
#include <set>

struct GraphItem
{
    int  id = 0;
    bool isSelected = false;
    char name[64] = "Default";
    std::string graphYaml;
};

class NodeGraphManager : public ManagerBase, public EditDraftBase
{
public:
    NodeGraphManager();
    ~NodeGraphManager();

    void AddItem() override;
    void RemoveItem(int id) override;

    int  GetSelectedIndex() const { return m_SelectedIndex; }
    void SetSelectedIndex(int idx);
    GraphItem* GetSelectedItem();

    void DrawContent() override;

    void DrawItemList(float width) override;

    // External data feeding
    void SetAvailableKeyNames(const KeyNameList& keys) { m_AvailableKeys = keys; }
    void SetAnalogKeys(const std::set<std::string>& ns) { m_AnalogKeys = ns; }
    void SetAvailableOutputTargets(const std::vector<OutputTargetInfo>& t) { m_OutputTargets = t; }
    void SetFieldValues(const std::map<std::string, double>& v) { m_FieldValues = v; }

    void SetRobotModeNames(const std::vector<std::string>& names, int activeIdx);
    void SetGamepadModeNames(const std::vector<std::string>& names);
    void SetCurrentModePair(const std::string& robotMode, const std::string& gamepadMode);
    void SwitchRobotMode(const std::string& newRobotMode, const std::string& curGamepadMode);
    void SwitchGamepadMode(const std::string& curRobotMode, const std::string& newGamepadMode);

    std::string GetCurrentRobotModeName()   const { return m_Graph.GetActiveRobotModeName(); }
    std::string GetCurrentGamepadModeName() const { return m_Graph.GetActiveGamepadModeName(); }
    const std::map<std::string, std::string>& GetGraphMap() const { return m_Graph.GetGraphMap(); }
    void SetGraphMap(const std::map<std::string, std::string>& map) { m_Graph.SetGraphMap(map); }
    void SaveGraphToMap() { m_Graph.SaveGraphToMap(); }
    bool IsModified() const { return m_Graph.IsModified(); }

    float GetLeftSideWidth()  const { return m_LeftSideWidth; }
    float GetRightSideWidth() const { return m_RightSideWidth; }
    void  SetLeftSideWidth(float w)  { m_LeftSideWidth = w; }
    void  SetRightSideWidth(float w) { m_RightSideWidth = w; }

    void SetKeyValues(const std::map<std::string, float>& kv) { m_Graph.SetKeyValues(kv); }

    // ---- Apply/Cancel (used internally) ----
    void OnOpen();
    void ApplyChanges();
    void CancelChanges();

    std::string GetGraphYaml() const;
    bool        LoadGraphYaml(const std::string& yamlStr);

private:
    void DeleteByIndex(int index);
    void SaveCurrentToItem();
    void LoadItemToCurrent();

    void AddNode(NodeType type);
    void AddNodeAt(NodeType type, const ImVec2& pos, bool fromScreen);
    void DrawMenuBar();
    void DrawKeyValuesSidebar(float sideWidth);
    void DrawOutputValuesSidebar(float sideWidth);
    void DrawPinIcon(const EditorPin& pin, bool connected, int alpha);
    void DrawNodeContents(EditorNode& node);
    void NavigateToOrigin();

    std::vector<GraphItem> m_Items;
    int m_SelectedIndex = 0;

    ax::NodeEditor::EditorContext* m_EditorCtx = nullptr;
    NodeGraph m_Graph;

    KeyNameList                        m_AvailableKeys;
    std::vector<OutputTargetInfo>      m_OutputTargets;
    std::map<std::string, double>      m_FieldValues;
    std::set<std::string>              m_AnalogKeys;

    std::vector<std::string>  m_RobotModeNames;
    int                       m_SelectedRobotModeIdx = 0;
    std::vector<std::string>  m_GamepadModeNames;
    int                       m_SelectedGamepadModeIdx = 0;

    std::string               m_AppliedGraphYaml;
    int                       m_AppliedRobotModeIdx = 0;

    bool m_OutputComboRequested = false;
    ax::NodeEditor::NodeId m_OutputComboNodeId = 0;
    bool m_KeySourcePopupRequested = false;
    ax::NodeEditor::NodeId m_KeySourcePopupNodeId = 0;

    int  m_NavigateFrame = 0;
    float m_LeftSideWidth  = 180.0f;
    float m_RightSideWidth = 200.0f;
};
