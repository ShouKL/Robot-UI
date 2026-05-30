#pragma once

// ============================================================================
// NodeGraph — pure data model + evaluation for the node graph.
// No UI dependencies (no ImGui drawing, no ed:: API calls in interface).
// Used by both NodeEditor (visual editing) and RobotStatus (runtime execution).
// ============================================================================

#include "Robot_API/robot_api.h"
#include <imgui.h>
#include <imgui_node_editor.h>   // only for ed::NodeId / ed::PinId / ed::LinkId types
#include <vector>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <shared_mutex>
#include <mutex>

// ============================================================================
// PinType
// ============================================================================
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
// OutputTargetInfo — describes a protocol send field as an output target
// ============================================================================
struct OutputTargetInfo {
    std::string name;         // display name (e.g., "Motor Left > Target Speed")
    std::string field_path;   // e.g., "brushlessmotor.0.target_speed"
    DataEncoding encoding = DataEncoding::Float32;
};

// ============================================================================
// KeyNameList
// ============================================================================
using KeyNameList = std::vector<std::string>;

// ============================================================================
// NodeGraph
// ============================================================================
class NodeGraph
{
public:
    NodeGraph();
    ~NodeGraph() = default;

    // ---- Node/Link data (public for direct iteration by editor) ----
    std::vector<EditorNode>  m_Nodes;
    std::vector<EditorLink>  m_Links;

    // ---- Spawn factories ----
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
    void RebuildAllNodes();

    // ---- Lookup ----
    EditorNode* FindNode(ax::NodeEditor::NodeId id);
    EditorPin*  FindPin(ax::NodeEditor::PinId id);
    EditorLink* FindLink(ax::NodeEditor::LinkId id);
    bool        IsPinLinked(ax::NodeEditor::PinId id) const;

    // ---- ID generation ----
    int  GetNextId()  { return m_NextId++; }
    void ResetIDs()   { m_NextId = 1; }
    int  PeekNextId() const { return m_NextId; }
    void SetNextId(int id) { m_NextId = id; }

    // ---- Clear ----
    void Clear();
    void Clear_NoLock();

    // ---- Graph Map: RobotMode × GamepadMode → node graph ----
    const std::map<std::string, std::string>& GetGraphMap() const { return m_GraphMap; }
    void SetGraphMap(const std::map<std::string, std::string>& map) { m_GraphMap = map; }
    void SaveGraphToMap();
    // Switch active graph by mode pair (loads from map or clears)
    void SwitchGraph(const std::string& robotMode, const std::string& gamepadMode);

    // ---- Serialization ----
    // Get graph as YAML (data only, no ed:: position API)
    std::string GetGraphDataYaml() const;
    // Load graph from YAML (data only, no ed:: API — for headless evaluator)
    bool        LoadGraphData(const std::string& yamlStr);

    // ---- Evaluation ----
    // Pure compute — thread-safe, no member mutation
    std::map<std::string, float> EvaluateCompute(const std::map<std::string, float>& keyValues) const;
    // Thread-safe read-only evaluation
    std::map<std::string, float> Evaluate(const std::map<std::string, float>& keyValues);
    // Evaluate and write outputs into ActuatorConfig
    void EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorConfig& data);
    // Evaluate and update node display fields (InputA, InputB, Value) — UI thread only
    void EvaluateForDisplay(const std::map<std::string, float>& keyValues);

    // ---- Thread-safe key value snapshot ----
    void SetKeyValues(const std::map<std::string, float>& kv);
    std::map<std::string, float> GetKeyValuesSnapshot() const;

    // ---- Modified flag ----
    bool IsModified() const { return m_Modified; }
    void SetModified(bool v) { m_Modified = v; }

    // ---- Active mode names (for graph map key) ----
    const std::string& GetActiveRobotModeName()   const { return m_ActiveRobotModeName; }
    const std::string& GetActiveGamepadModeName() const { return m_ActiveGamepadModeName; }
    void SetActiveRobotModeName(const std::string& n)   { m_ActiveRobotModeName = n; }
    void SetActiveGamepadModeName(const std::string& n) { m_ActiveGamepadModeName = n; }

    // ---- Thread-safe mutex access (private — only NodeEditor may lock directly) ----
private:
    friend class NodeEditor;
    std::shared_mutex& GetEvalMutex() const { return m_EvalMutex; }
    std::mutex&        GetKvMutex()   const { return m_KvMutex; }

    int                      m_NextId  = 1;
    bool                     m_Modified = false;

    mutable std::shared_mutex m_EvalMutex;
    mutable std::mutex        m_KvMutex;

    std::map<std::string, float>       m_LastKeyValues;
    std::map<std::string, float>       m_LastOutputs;

    std::map<std::string, std::string> m_GraphMap;
    std::string                        m_ActiveRobotModeName;
    std::string                        m_ActiveGamepadModeName;
};

// ============================================================================
// Free functions (shared by NodeGraph and NodeEditor)
// ============================================================================
const char* GetNodeTitle(NodeType type);
ImColor     GetNodeHeaderColor(NodeType type);
ImColor     GetIconColor(PinType type);
void        WriteOutputToActuator(const std::string& outputTarget, float value, ActuatorConfig& data);
std::vector<OutputTargetInfo> BuildOutputTargetsFromProtocol(const ProtocolSendConfig& cfg, const ActuatorConfig& actuator);
