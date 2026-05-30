#pragma once

// ============================================================================
// NodeEditor — visual node graph editor (ImGui + imgui-node-editor drawing).
// Contains a NodeGraph for the data model. This class only handles drawing,
// user interaction, and mode-switch UI. It is independent of all other files
// (except NodeGraph).
// ============================================================================

#include "EditDraftBase.h"
#include "NodeGraph.h"
#include <imgui_node_editor.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <set>
#include <mutex>

// ============================================================================
// NodeEditor class — UI-only drawing layer over NodeGraph
// ============================================================================
class NodeEditor : public EditDraftBase
{
public:
    NodeEditor();
    ~NodeEditor();

    bool Draw();  // returns false when window should close

    // ---- External data feeding (from Robot_UI) ----
    void SetAvailableKeyNames(const KeyNameList& keys) { m_AvailableKeys = keys; }
    void SetAnalogKeys(const std::set<std::string>& names) { m_AnalogKeys = names; }
    void SetAvailableOutputTargets(const std::vector<OutputTargetInfo>& targets) { m_OutputTargets = targets; }
    void SetFieldValues(const std::map<std::string, double>& values) { m_FieldValues = values; }

    // ---- Mode names for Combo display ----
    void SetRobotModeNames(const std::vector<std::string>& names, int activeIdx);
    void SetGamepadModeNames(const std::vector<std::string>& names);
    void SetCurrentModePair(const std::string& robotMode, const std::string& gamepadMode);

    // ---- Mode switching (called from Robot_UI) ----
    void SwitchRobotMode(const std::string& newRobotMode, const std::string& curGamepadMode);
    void SwitchGamepadMode(const std::string& curRobotMode, const std::string& newGamepadMode);

    // ---- Accessors for Robot_UI ----
    std::string GetCurrentRobotModeName()   const { return m_Graph.GetActiveRobotModeName(); }
    std::string GetCurrentGamepadModeName() const { return m_Graph.GetActiveGamepadModeName(); }
    const std::map<std::string, std::string>& GetGraphMap() const { return m_Graph.GetGraphMap(); }
    void SetGraphMap(const std::map<std::string, std::string>& map) { m_Graph.SetGraphMap(map); }

    void SaveGraphToMap() { m_Graph.SaveGraphToMap(); }
    bool IsModified() const { return m_Graph.IsModified(); }

    // ---- Apply/Cancel ----
    void OnOpen();
    void ApplyChanges();
    void CancelChanges();

    // ---- Sidebar width persistence ----
    float GetLeftSideWidth()  const { return m_LeftSideWidth; }
    float GetRightSideWidth() const { return m_RightSideWidth; }
    void  SetLeftSideWidth(float w)  { m_LeftSideWidth = w; }
    void  SetRightSideWidth(float w) { m_RightSideWidth = w; }

    // Thread-safe key value snapshot delegation
    void SetKeyValues(const std::map<std::string, float>& kv) { m_Graph.SetKeyValues(kv); }

    // ---- YAML serialization (includes ed:: node positions) ----
    std::string GetGraphYaml() const;
    bool        LoadGraphYaml(const std::string& yamlStr);

private:
    // ---- Graph data access (delegate to m_Graph) ----
    void AddNode(NodeType type);
    void AddNodeAt(NodeType type, const ImVec2& pos, bool fromScreen);

    // ---- Drawing helpers ----
    void DrawMenuBar();
    void DrawKeyValuesSidebar(float sideWidth);
    void DrawOutputValuesSidebar(float sideWidth);
    void DrawPinIcon(const EditorPin& pin, bool connected, int alpha);
    void DrawNodeContents(EditorNode& node);
    void NavigateToOrigin();

    // ---- Editor context ----
    ax::NodeEditor::EditorContext* m_EditorCtx = nullptr;

    // ---- Pure data (NodeGraph owns m_Nodes, m_Links, evaluation, YAML) ----
    NodeGraph m_Graph;

    // ---- UI-only state (not serialized / evaluated) ----
    KeyNameList                        m_AvailableKeys;
    std::vector<OutputTargetInfo>      m_OutputTargets;
    std::map<std::string, double>      m_FieldValues;
    std::set<std::string>              m_AnalogKeys;

    // Mode switching UI state
    std::vector<std::string>  m_RobotModeNames;
    int                       m_SelectedRobotModeIdx = 0;
    std::vector<std::string>  m_GamepadModeNames;
    int                       m_SelectedGamepadModeIdx = 0;

    // Apply/Cancel snapshot
    std::string               m_AppliedGraphYaml;
    int                       m_AppliedRobotModeIdx = 0;

    // Deferred popup state
    bool m_OutputComboRequested = false;
    ax::NodeEditor::NodeId m_OutputComboNodeId = 0;
    bool m_KeySourcePopupRequested = false;
    ax::NodeEditor::NodeId m_KeySourcePopupNodeId = 0;

    // Viewport origin navigation
    int  m_NavigateFrame = 0;

    // Resizable sidebars
    float m_LeftSideWidth  = 180.0f;
    float m_RightSideWidth = 200.0f;
};
