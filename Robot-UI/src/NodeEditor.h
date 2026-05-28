#pragma once

#include "EditDraftBase.h"
#include <vector>
#include <string>
#include <map>
#include <set>
#include <functional>
#include <imgui_node_editor.h>
#include "Robot_API/robot_api.h"

enum class PinType
{
    Float,
};

// ============================================================================
// Forward declaration
// ============================================================================
struct EditorNode;

// ============================================================================
// Pin — one input or output connector on a node
// ============================================================================
struct EditorPin
{
    ax::NodeEditor::PinId   ID;
    EditorNode*             Node = nullptr;
    std::string             Name;
    PinType                 Type = PinType::Float;

    EditorPin(int id, const char* name, PinType type = PinType::Float)
        : ID(id), Name(name), Type(type) {}
};

// ============================================================================
// Node
// ============================================================================
enum class NodeType
{
    KeySource,
    ConstValue,
    Add,
    Subtract,
    Multiply,
    Divide,
    Scale,
    Clamp,
    Compare,
    And,
    Or,
    Not,
    If,
    While,
    CustomOutput,
};

struct EditorNode
{
    ax::NodeEditor::NodeId  ID;
    std::string             Name;
    NodeType                Type = NodeType::ConstValue;
    std::vector<EditorPin>  Inputs;
    std::vector<EditorPin>  Outputs;
    ImColor                 Color = ImColor(255, 255, 255);

    // -- evaluation data --
    float Value     = 0.0f;
    float InputA    = 0.0f;
    float InputB    = 0.0f;
    float Factor    = 1.0f;
    float MinVal    = 0.0f;
    float MaxVal    = 1.0f;
    bool  Digital   = false;   // KeySource: clamp output to 0 or 1
    int   OpMode    = 0;       // Compare: 0=> 1=>= 2=<= 3=< 4=>= 5=!=
    std::string KeyName;
    std::string OutputTarget;

    EditorNode(int id, const char* name, NodeType type, ImColor color = ImColor(255, 255, 255))
        : ID(id), Name(name), Type(type), Color(color) {}
};

// ============================================================================
// Link
// ============================================================================
struct EditorLink
{
    ax::NodeEditor::LinkId  ID;
    ax::NodeEditor::PinId   StartPinID;
    ax::NodeEditor::PinId   EndPinID;
    ImColor                 Color = ImColor(255, 255, 255);

    EditorLink(ax::NodeEditor::LinkId id,
               ax::NodeEditor::PinId start, ax::NodeEditor::PinId end)
        : ID(id), StartPinID(start), EndPinID(end) {}
};

// ============================================================================
// KeyNameList
// ============================================================================
using KeyNameList = std::vector<std::string>;

// ============================================================================
// OutputTargetInfo — describes a protocol send field as an output target
// ============================================================================
struct OutputTargetInfo {
    std::string name;         // display name (e.g., "Motor Left > Target Speed")
    std::string field_path;   // e.g., "brushlessmotor.0.target_speed"
    DataEncoding encoding = DataEncoding::Float32;
};

// ============================================================================
// NodeEditor class
// ============================================================================
class NodeEditor : public EditDraftBase
{
public:
    NodeEditor();
    ~NodeEditor();

    bool Draw();  // returns false when window should close

    void SetAvailableKeyNames(const KeyNameList& keys) { m_AvailableKeys = keys; }
    const KeyNameList& GetAvailableKeyNames() const     { return m_AvailableKeys; }

    // 标记哪些键是模拟量（用于侧边栏区分显示 True/False vs 浮点值）
    void SetAnalogKeys(const std::set<std::string>& names) { m_AnalogKeys = names; }

    // Set available output target names (from protocol send fields)
    void SetAvailableOutputTargets(const std::vector<OutputTargetInfo>& targets) { m_OutputTargets = targets; }

    // Set current field values from actuator (field_path → value)
    void SetFieldValues(const std::map<std::string, double>& values) { m_FieldValues = values; }

    // Mode switching support
    void SetModeNames(const std::vector<std::string>& names, int activeIdx);
    std::string GetCurrentModeName() const {
        if (m_SelectedModeIdx >= 0 && m_SelectedModeIdx < (int)m_ModeNames.size())
            return m_ModeNames[m_SelectedModeIdx];
        return "None";
    }
    void SetModeSwitchCallback(std::function<void(int)> cb) { m_OnModeSwitch = cb; }

    // Apply / Cancel editing state
    void OnOpen();    // save snapshot
    void ApplyChanges();   // commit to caller's storage (through callback)
    void CancelChanges();  // revert to snapshot

    // Evaluate node graph: key values → map of output-target → value
    std::map<std::string, float> Evaluate(const std::map<std::string, float>& keyValues);

    // Pure compute — thread-safe, shared_lock, no member mutation, no ed API
    std::map<std::string, float> EvaluateCompute(const std::map<std::string, float>& keyValues) const;

    // Directly write evaluated outputs into an ActuatorData
    void EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorData& data);

    // Update key-value & output snapshots for sidebar display (UI thread only)
    void EvaluateGraph(const std::map<std::string, float>& keyValues);

    // Get the current YAML (for the caller to persist)
    std::string GetGraphYaml() const;
    bool        LoadGraphYaml(const std::string& yamlStr);

    bool IsModified() const { return m_Modified; }
    void Clear();
    void Clear_NoLock();

    // Thread-safe: defer graph load to UI thread
    void RequestLoadGraph(const std::string& yaml);
    void ProcessPendingLoad();

    // Thread-safe: set key-values snapshot for sidebar display (called from gamepad thread)
    void SetKeyValues(const std::map<std::string, float>& kv);

private:
    // -- Factory --
    EditorNode* SpawnKeySource();
    EditorNode* SpawnConstValue();
    EditorNode* SpawnAdd();
    EditorNode* SpawnSubtract();
    EditorNode* SpawnMultiply();
    EditorNode* SpawnDivide();
    EditorNode* SpawnScale();
    EditorNode* SpawnClamp();
    EditorNode* SpawnCompare();
    EditorNode* SpawnAnd();
    EditorNode* SpawnOr();
    EditorNode* SpawnNot();
    EditorNode* SpawnIf();
    EditorNode* SpawnWhile();
    EditorNode* SpawnOutput();

    void BuildNode(EditorNode* node);
    void RebuildAllNodes();   // rebuild all pin→node back-pointers after vector reallocation
    void AddNode(NodeType type);
    void AddNodeAt(NodeType type, const ImVec2& screenPos);
    void AddNodeAt(NodeType type, const ImVec2& pos, bool fromScreen);
    void NavigateToOrigin();

    // -- Lookup --
    EditorNode* FindNode(ax::NodeEditor::NodeId id);
    EditorPin*  FindPin(ax::NodeEditor::PinId id);
    EditorLink* FindLink(ax::NodeEditor::LinkId id);

    bool IsPinLinked(ax::NodeEditor::PinId id) const;
    bool CanCreateLink(EditorPin* a, EditorPin* b) const;

    // -- ID generation --
    int  GetNextId()  { return m_NextId++; }
    void ResetIDs()   { m_NextId = 1; }

    // -- Drawing --
    void DrawMenuBar();
    void DrawKeyValuesSidebar(float sideWidth) const;
    void DrawOutputValuesSidebar(float sideWidth) const;
    void DrawPinIcon(const EditorPin& pin, bool connected, int alpha);
    void DrawNodeContents(EditorNode& node);

    // -- Editor context --
    ax::NodeEditor::EditorContext* m_EditorCtx = nullptr;

    // -- Data --
    std::vector<EditorNode>  m_Nodes;
    std::vector<EditorLink>  m_Links;
    int                      m_NextId  = 1;
    bool                     m_Modified = false;

    KeyNameList                        m_AvailableKeys;
    std::vector<OutputTargetInfo>      m_OutputTargets;
    std::map<std::string, float>       m_LastKeyValues;
    std::map<std::string, float>       m_LastOutputs;
    std::map<std::string, double>      m_FieldValues;   // actuator field_path → current value
    std::set<std::string>              m_AnalogKeys;  // 模拟键名集合，用于侧边栏区分显示

    // Mode switching
    std::vector<std::string>  m_ModeNames;
    int                       m_SelectedModeIdx = 0;
    std::function<void(int)>  m_OnModeSwitch;

    // Apply/Cancel snapshot
    std::string               m_AppliedGraphYaml;
    int                       m_AppliedModeIdx = 0;

    // -- deferred popup --
    bool m_OutputComboRequested = false;
    ax::NodeEditor::NodeId m_OutputComboNodeId = 0;
    bool m_KeySourcePopupRequested = false;
    ax::NodeEditor::NodeId m_KeySourcePopupNodeId = 0;

    // -- pending load from another thread --
    std::string m_PendingGraphYaml;

    // -- viewport origin --
    int  m_NavigateFrame = 0;    // >0 时将在该帧导航到原点，倒计时

    // -- resizable sidebars --
    float m_LeftSideWidth  = 180.0f;
    float m_RightSideWidth = 200.0f;
};

// ============================================================================
// Free function — write output-target→value directly into ActuatorData
// ============================================================================
void WriteOutputToActuator(const std::string& outputTarget, float value, ActuatorData& data);

// ============================================================================
// Build list of output targets from ProtocolSendConfig fields (exclude fix)
// ============================================================================
std::vector<OutputTargetInfo> BuildOutputTargetsFromProtocol(const ProtocolSendConfig& cfg, const ActuatorData& actuator);
