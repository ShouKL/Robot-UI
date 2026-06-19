#pragma once

// ============================================================================
// NodeGraph — pure data model + evaluation for the node graph.
// No UI dependencies (no ImGui drawing, no ed:: API calls in interface).
// Used by both NodeEditor (visual editing) and RobotStatus (runtime execution).
// Depends on NodeLibrary for PinType, EditorPin, NodeType, EditorNode.
// ============================================================================

#include "NodeLibrary.h"
#include "Robot_API/robot_api.h"
#include <imgui_node_editor.h>   // only for ed::NodeId / ed::PinId / ed::LinkId types
#include <vector>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <shared_mutex>
#include <mutex>
#include <chrono>

namespace Walnut { class Image; }

class RobotComponentManager;
class GamepadMapperManager;
class ShortcutManager;
#include "RobotCommManager.h"

// ============================================================================
// EditorLink — a connection between two pins
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
    std::string name;         // display name (e.g., "MyComm > Motor Left > Target Speed")
    std::string field_path;   // e.g., "brushlessmotor.0.target_speed"
    DataEncoding encoding = DataEncoding::Float32;
    int  comm_index = 0;      // which RobotComm (index into m_CommRefs) this target belongs to
};

// ============================================================================
// KeyNameList
// ============================================================================
using KeyNameList = std::vector<std::string>;

// ============================================================================
// GlobalVar — a typed, ID-stable graph variable (persisted with the graph)
// ============================================================================
struct GlobalVar {
    int      id      = 0;
    std::string name;
    float    value   = 0.0f;
    PinType  type    = PinType::Float;
    bool     visible = false;  // show in RobotStatus panel
};

// ============================================================================
// NodeGraph
// ============================================================================
class NodeGraph
{
public:
    NodeGraph();
    ~NodeGraph();

    // ---- Node/Link data (public for direct iteration by editor) ----
    std::vector<EditorNode>  m_Nodes;
    std::vector<EditorLink>  m_Links;

    // ---- Spawn factory (delegates to NodeLibrary::CreateEditorNodeByType) ----
    EditorNode* SpawnNode(NodeType type);

    void BuildNode(EditorNode* node);
    void RebuildAllNodes();

    // ---- Lookup ----
    EditorNode* FindNode(ax::NodeEditor::NodeId id);
    EditorPin*  FindPin(ax::NodeEditor::PinId id);
    bool        IsPinLinked(ax::NodeEditor::PinId id) const;

    // ---- ID generation ----
    int  GetNextId()  { return m_NextId++; }
    void ResetIDs()   { m_NextId = 1; }

    // ---- Clear ----
    void Clear();
    void Clear_NoLock();

    // ---- View navigation ----
    void NavigateToOrigin();
    void RequestNavigate() { m_NavigateFrame = 1; }

    // ---- Dependency injection (pulls data from managers directly) ----
    void SetRobotComponentManager(RobotComponentManager* c) { m_RobotMgr = c; }
    void SetGamepadMapperManager(GamepadMapperManager* g)    { m_GamepadMgr = g; }
    void SetRobotCommManager(RobotCommManager* comm)         { m_CommMgr = comm; }
    void SetShortcutManager(ShortcutManager* sm)             { m_ShortcutMgr = sm; }

    // ---- Send action callback (ShortcutTrigger → RobotStatus) ----
    // Called on rising edge: (sendFlatIndex, enable, oneShot)
    void SetSendActionCb(std::function<void(int,bool,bool)> cb) { m_SendActionCb = std::move(cb); }

    // ---- Comm 配置管理（由 NodeGraph 决定连接哪些 RobotComm） ----
    // m_CommRefs 存储 RobotCommManager 中的索引列表
    // CustomOutput 节点的 CommIndex 引用 m_CommRefs 的下标
    const std::vector<int>& GetCommRefs() const { return m_CommRefs; }
    int GetNodeCount() const { return (int)m_Nodes.size(); }
    int  GetCommRef(int graphCommIdx) const;  // graphCommIdx → RobotCommManager 索引
    int  GetCommRefCount() const { return (int)m_CommRefs.size(); }
    void AddCommRef(int robotCommMgrIdx);
    void RemoveCommRef(int graphCommIdx);
    void SetCommRefs(const std::vector<int>& refs) { m_CommRefs = refs; }

    // ---- External data feeding (runtime values from gamepad thread) ----
    void SetCurrentModePair(const std::string& robotMode, const std::string& gamepadMode);

    // ---- UI layout ----
    float GetLeftSideWidth()  const { return m_LeftSideWidth; }
    float GetRightSideWidth() const { return m_RightSideWidth; }
    void  SetLeftSideWidth(float w)  { m_LeftSideWidth = w; }
    void  SetRightSideWidth(float w) { m_RightSideWidth = w; }

    // ---- Graph Map: RobotMode × GamepadMode → node graph ----
    const std::map<std::string, std::string>& GetGraphMap() const { return m_GraphMap; }
    void SetGraphMap(const std::map<std::string, std::string>& map) { m_GraphMap = map; }
    void SaveGraphToMap();
    // Switch active graph by mode pair (loads from map or clears)
    void SwitchGraph(const std::string& robotMode, const std::string& gamepadMode);
    void SwitchRobotMode(const std::string& newRobotMode, const std::string& curGamepadMode);
    void SwitchGamepadMode(const std::string& curRobotMode, const std::string& newGamepadMode);

    // ---- Serialization ----
    // Get graph as YAML (data only, no ed:: position API)
    std::string GetGraphDataYaml() const;
    // Load graph from YAML (data only, no ed:: API — for headless evaluator)
    bool        LoadGraphData(const std::string& yamlStr);
    // Get/set graph YAML with ed:: node positions (requires active editor context)
    std::string GetGraphYaml() const;
    bool        LoadGraphYaml(const std::string& yamlStr);

    // Deep-clone via data YAML round-trip (no ed:: positions)
    std::unique_ptr<NodeGraph> Clone() const;

    // ---- Evaluation ----
    // Evaluate into per-comm ActuatorConfig vector.
    // dataVec[commIndex] receives outputs from CustomOutput nodes with that CommIndex.
    // dataVec is resized to cover all referenced comm indices.
    // Internal use only — use EvaluateIntoActuators() for public API.
    void EvaluateComputeInto(const std::map<std::string, float>& keyValues,
                             std::vector<ActuatorConfig>& dataVec,
                             std::set<int>* pWrittenIndices = nullptr);
    // Compute output map from keyValues (legacy, single-comm).
    std::map<std::string, float> EvaluateCompute(const std::map<std::string, float>& keyValues);
    // Thread-safe evaluation (acquires unique_lock — serializes concurrent evals)
    std::map<std::string, float> Evaluate(const std::map<std::string, float>& keyValues);
    // Evaluate and write outputs into ActuatorConfig (legacy, single target)
    void EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorConfig& data);
    // Evaluate and write outputs into per-comm ActuatorConfig vector
    void EvaluateIntoActuators(const std::map<std::string, float>& keyValues,
                               std::vector<ActuatorConfig>& dataVec,
                               std::set<int>* pWrittenIndices = nullptr);
    // Evaluate and update node display fields (InputA, InputB, Value) — UI thread only
    void EvaluateForDisplay(const std::map<std::string, float>& keyValues);

    // ---- Thread-safe key value snapshot ----
    void SetKeyValues(const std::map<std::string, float>& kv);
    std::map<std::string, float> GetKeyValuesSnapshot() const;

    // ---- Editor rendering ----
    void Draw(ax::NodeEditor::EditorContext* editorCtx);

    // ---- Per-node widget drawing ----
    void DrawNodeContents(EditorNode& node,
                          const std::set<std::string>& analogKeys,
                          const std::vector<OutputTargetInfo>& outputTargets);
    void DrawMenuBar();  // deprecated, removed
    void DrawKeyValuesSidebar(float sideWidth, const std::set<std::string>& analogKeys);
    void DrawGlobalsSidebar(float sideWidth);
    void DrawCommRefsSidebar(float sideWidth);
    void DrawTriggerSidebar(float sideWidth);

    // ---- Node creation (editor-side) ----
    void AddNode(NodeType type);
    bool AddNodeAt(NodeType type, const ImVec2& pos, bool fromScreen);

    // ---- Modified flag ----
    void SetModified(bool v) { m_Modified = v; }

    // ---- Run control (play/stop evaluation) ----
    bool IsRunning() const { return m_IsRunning; }
    void SetRunning(bool r) { m_IsRunning = r; }
    void ToggleRunning()    { m_IsRunning = !m_IsRunning; }

    // ---- Active mode names (for graph map key) ----
    const std::string& GetActiveRobotModeName()   const { return m_ActiveRobotModeName; }
    const std::string& GetActiveGamepadModeName() const { return m_ActiveGamepadModeName; }
    void SetActiveRobotModeName(const std::string& n)   { m_ActiveRobotModeName = n; }
    void SetActiveGamepadModeName(const std::string& n) { m_ActiveGamepadModeName = n; }

    // ---- Global variables (persisted with graph) ----
    int  FindGlobalIndex(int id) const;
    int  FindGlobalByName(const std::string& name) const;
    void AddGlobal(const std::string& name, float val, PinType type = PinType::Float);
    void RemoveGlobal(int id);
    int  GlobalCount() const;
    float GetGlobal(int idx) const;
    void  SetGlobal(int idx, float v);
    PinType GetGlobalType(int idx) const;
    void    SetGlobalType(int idx, PinType t);
    const std::vector<GlobalVar>& GetGlobals() const { return m_GlobalVars; }

    // ---- Thread-safe mutex access (private — only NodeGraphManager may lock directly) ----
private:
    friend class NodeGraphManager;
    std::shared_mutex& GetEvalMutex() const { return m_EvalMutex; }

    int                      m_NextId  = 1;
    bool                     m_Modified = false;
    int                      m_NavigateFrame = 0;

    mutable std::shared_mutex m_EvalMutex;
    mutable std::mutex        m_KvMutex;
    // Atomic for thread-safe time tracking across concurrent evals
    mutable std::atomic<int64_t> m_LastEvalTimeNs{0};

    std::map<std::string, float>       m_LastKeyValues;
    std::map<std::string, float>       m_LastOutputs;

    std::map<std::string, std::string> m_GraphMap;
    std::string                        m_ActiveRobotModeName;
    std::string                        m_ActiveGamepadModeName;

    // ---- Editor UI data ----
    bool m_IsRunning = false;
    std::shared_ptr<Walnut::Image> m_PlayIcon;
    std::shared_ptr<Walnut::Image> m_StopIcon;
    ax::NodeEditor::NodeId m_ActiveKeySourceId = 0;
    ax::NodeEditor::NodeId m_ActiveOutputId    = 0;
    ax::NodeEditor::NodeId m_ActiveTriggerId   = 0;  // ShortcutTrigger
    ax::NodeEditor::PinId  m_LinkSourcePin     = 0;   // pending link source (click-to-connect)
    ImVec2                 m_LinkSourceMouse   = ImVec2(0,0);
    RobotComponentManager* m_RobotMgr = nullptr;
    GamepadMapperManager*  m_GamepadMgr = nullptr;
    RobotCommManager*      m_CommMgr = nullptr;
    ShortcutManager*       m_ShortcutMgr = nullptr;
    std::function<void(int,bool,bool)> m_SendActionCb;  // (sendFlatIndex, enable, oneShot)

    std::vector<int> m_CommRefs;  // indices into RobotCommManager

    // ---- Cached per-frame data from managers (populated in Draw) ----
    KeyNameList                        m_AvailableKeys;
    std::vector<OutputTargetInfo>      m_OutputTargets;
    std::map<std::string, double>      m_FieldValues;
    std::set<std::string>              m_AnalogKeys;

    std::vector<GlobalVar>   m_GlobalVars;
    std::vector<float>       m_GlobalTempVals;  // temporary float array for evaluation

    float m_LeftSideWidth  = 200.0f;
    float m_RightSideWidth = 200.0f;

    ax::NodeEditor::NodeId m_ActiveGlobalReadId = 0;
    int m_RenamingGlobalIdx = -1;
    int m_LastRenamingIdx   = -2;
};

// ============================================================================
// Free functions (stay in NodeGraph — depend on robot_api.h types)
// ============================================================================
void        WriteOutputToActuator(const std::string& outputTarget, float value, ActuatorConfig& data);
std::vector<OutputTargetInfo> BuildOutputTargetsFromProtocol(const std::vector<ProtocolSendConfig>& cfgs, const ActuatorConfig& actuator);

// ============================================================================
// GraphNode — 纯数据（不含 NodeGraph 实例/EditorContext），用于序列化/传递
// ============================================================================
struct GraphNode
{
    int  id         = 0;
    bool isSelected = false;
    char name[64]   = "Default";
};
