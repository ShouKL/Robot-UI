#include "NodeGraph.h"
#include "FileManager.h"
#include <queue>
#include <unordered_map>
#include <sstream>
#include <chrono>

namespace ed = ax::NodeEditor;

// ============================================================================
// Helper: get delta time from atomic timestamp
// ============================================================================
static float GetEvalDeltaTime(std::atomic<int64_t>& lastTimeNs)
{
    auto now = std::chrono::steady_clock::now();
    int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    int64_t prevNs = lastTimeNs.exchange(nowNs, std::memory_order_relaxed);
    if (prevNs == 0) return 0.0f;
    float dt = (float)(nowNs - prevNs) * 1e-9f;
    if (dt > 1.0f) dt = 0.0f;
    return dt;
}

// ============================================================================
// NodeGraph — Constructor / Destructor
// ============================================================================
NodeGraph::NodeGraph() {}
NodeGraph::~NodeGraph() {}

// ============================================================================
// Explicit copy — copies all data fields; mutex/atomic default-constructed
// ============================================================================
NodeGraph::NodeGraph(const NodeGraph& other)
{
    // ---- Public fields ----
    id         = other.id;
    isSelected = other.isSelected;
    strncpy_s(name, other.name, sizeof(name) - 1);
    editorYaml = other.editorYaml;

    // ---- Node/Link data ----
    m_Nodes = other.m_Nodes;
    m_Links = other.m_Links;
    RebuildAllNodes();  // fix pin→node back-pointers

    // ---- IDs / flags ----
    m_NextId    = other.m_NextId;
    m_Modified  = other.m_Modified;
    m_NavigateFrame = other.m_NavigateFrame;

    // ---- Key-values / outputs ----
    m_LastKeyValues = other.m_LastKeyValues;
    m_LastOutputs   = other.m_LastOutputs;

    // ---- Graph map ----
    m_GraphMap            = other.m_GraphMap;
    m_ActiveRobotModeName   = other.m_ActiveRobotModeName;
    m_ActiveGamepadModeName = other.m_ActiveGamepadModeName;

    // ---- Editor UI data ----
    m_IsRunning         = other.m_IsRunning;
    m_PlayIcon          = other.m_PlayIcon;
    m_StopIcon          = other.m_StopIcon;
    m_ActiveKeySourceId = other.m_ActiveKeySourceId;
    m_ActiveOutputId    = other.m_ActiveOutputId;
    m_ActiveTriggerId   = other.m_ActiveTriggerId;
    m_LinkSourcePin     = other.m_LinkSourcePin;
    m_LinkSourceMouse   = other.m_LinkSourceMouse;

    // ---- Manager pointers ----
    m_RobotMgr    = other.m_RobotMgr;
    m_GamepadMgr  = other.m_GamepadMgr;
    m_CommMgr     = other.m_CommMgr;
    m_ShortcutMgr = other.m_ShortcutMgr;
    m_SendActionCb = other.m_SendActionCb;

    // ---- Comm refs ----
    m_CommRefs = other.m_CommRefs;

    // ---- Cached per-frame data ----
    m_AvailableKeys  = other.m_AvailableKeys;
    m_OutputTargets  = other.m_OutputTargets;
    m_FieldValues    = other.m_FieldValues;
    m_AnalogKeys     = other.m_AnalogKeys;

    // ---- Global vars ----
    m_GlobalVars     = other.m_GlobalVars;
    m_GlobalTempVals = other.m_GlobalTempVals;

    // ---- Layout ----
    m_LeftSideWidth  = other.m_LeftSideWidth;
    m_RightSideWidth = other.m_RightSideWidth;

    // ---- Rename state ----
    m_ActiveGlobalReadId = other.m_ActiveGlobalReadId;
    m_RenamingGlobalIdx  = other.m_RenamingGlobalIdx;
    m_LastRenamingIdx    = other.m_LastRenamingIdx;
    m_EditingEnumIdx     = other.m_EditingEnumIdx;

    // m_EvalMutex, m_KvMutex: default-constructed (non-copyable)
    // m_LastEvalTimeNs: default-constructed (atomic not copyable)
}

// ============================================================================
// Explicit copy assignment — same as copy ctor, skips mutex/atomic
// ============================================================================
NodeGraph& NodeGraph::operator=(const NodeGraph& other)
{
    if (this == &other) return *this;

    id         = other.id;
    isSelected = other.isSelected;
    strncpy_s(name, other.name, sizeof(name) - 1);
    editorYaml = other.editorYaml;

    m_Nodes = other.m_Nodes;
    m_Links = other.m_Links;
    RebuildAllNodes();

    m_NextId    = other.m_NextId;
    m_Modified  = other.m_Modified;
    m_NavigateFrame = other.m_NavigateFrame;

    m_LastKeyValues = other.m_LastKeyValues;
    m_LastOutputs   = other.m_LastOutputs;

    m_GraphMap            = other.m_GraphMap;
    m_ActiveRobotModeName   = other.m_ActiveRobotModeName;
    m_ActiveGamepadModeName = other.m_ActiveGamepadModeName;

    m_IsRunning         = other.m_IsRunning;
    m_PlayIcon          = other.m_PlayIcon;
    m_StopIcon          = other.m_StopIcon;
    m_ActiveKeySourceId = other.m_ActiveKeySourceId;
    m_ActiveOutputId    = other.m_ActiveOutputId;
    m_ActiveTriggerId   = other.m_ActiveTriggerId;
    m_LinkSourcePin     = other.m_LinkSourcePin;
    m_LinkSourceMouse   = other.m_LinkSourceMouse;

    m_RobotMgr    = other.m_RobotMgr;
    m_GamepadMgr  = other.m_GamepadMgr;
    m_CommMgr     = other.m_CommMgr;
    m_ShortcutMgr = other.m_ShortcutMgr;
    m_SendActionCb = other.m_SendActionCb;

    m_CommRefs = other.m_CommRefs;

    m_AvailableKeys  = other.m_AvailableKeys;
    m_OutputTargets  = other.m_OutputTargets;
    m_FieldValues    = other.m_FieldValues;
    m_AnalogKeys     = other.m_AnalogKeys;

    m_GlobalVars     = other.m_GlobalVars;
    m_GlobalTempVals = other.m_GlobalTempVals;

    m_LeftSideWidth  = other.m_LeftSideWidth;
    m_RightSideWidth = other.m_RightSideWidth;

    m_ActiveGlobalReadId = other.m_ActiveGlobalReadId;
    m_RenamingGlobalIdx  = other.m_RenamingGlobalIdx;
    m_LastRenamingIdx    = other.m_LastRenamingIdx;
    m_EditingEnumIdx     = other.m_EditingEnumIdx;

    // m_EvalMutex, m_KvMutex, m_LastEvalTimeNs: unchanged (skip)
    return *this;
}

// ============================================================================
// Explicit move — default-construct mutex/atomic, then copy data
// ============================================================================
NodeGraph::NodeGraph(NodeGraph&& other) noexcept
    : NodeGraph()
{
    *this = static_cast<const NodeGraph&>(other);
}

// ============================================================================
// Explicit move assignment — delegate to copy assignment
// ============================================================================
NodeGraph& NodeGraph::operator=(NodeGraph&& other) noexcept
{
    if (this == &other) return *this;
    return operator=(static_cast<const NodeGraph&>(other));
}

// ============================================================================
// BuildNode — wire up pin → node back-pointers
// ============================================================================
void NodeGraph::BuildNode(EditorNode* node)
{
    for (auto& pin : node->Inputs)
        pin.Node = node;
    for (auto& pin : node->Outputs)
        pin.Node = node;
}

// ============================================================================
// RebuildAllNodes — fix dangling pin→node pointers after vector reallocation
// ============================================================================
void NodeGraph::RebuildAllNodes()
{
    for (auto& node : m_Nodes)
        BuildNode(&node);
}

// ============================================================================
// SpawnNode — convenience factory dispatching on NodeType
// ============================================================================
EditorNode* NodeGraph::SpawnNode(NodeType type)
{
    EditorNode node = CreateEditorNodeByType(type, [this]() { return GetNextId(); });
    m_Nodes.emplace_back(std::move(node));
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

// ============================================================================
// Clear
// ============================================================================
void NodeGraph::Clear_NoLock()
{
    m_Nodes.clear();
    m_Links.clear();
    m_LastOutputs.clear();
    ResetIDs();
    m_Modified = false;
}

void NodeGraph::Clear()
{
    std::unique_lock<std::shared_mutex> lock(m_EvalMutex);
    Clear_NoLock();
    m_GlobalVars.clear();
    m_GlobalTempVals.clear();
    m_CommRefs.clear();
}

// ============================================================================
// CommRef helpers
// ============================================================================
int NodeGraph::GetCommRef(int graphCommIdx) const
{
    if (graphCommIdx >= 0 && graphCommIdx < (int)m_CommRefs.size())
        return m_CommRefs[graphCommIdx];
    return 0;
}

void NodeGraph::AddCommRef(int robotCommMgrIdx)
{
    m_CommRefs.push_back(robotCommMgrIdx);
}

void NodeGraph::RemoveCommRef(int graphCommIdx)
{
    if (graphCommIdx >= 0 && graphCommIdx < (int)m_CommRefs.size())
        m_CommRefs.erase(m_CommRefs.begin() + graphCommIdx);
}

// ============================================================================
// Global variables helpers
// ============================================================================
int NodeGraph::FindGlobalIndex(int id) const
{
    for (size_t i = 0; i < m_GlobalVars.size(); ++i)
        if (m_GlobalVars[i].id == id) return (int)i;
    return -1;
}

int NodeGraph::FindGlobalByName(const std::string& name) const
{
    for (size_t i = 0; i < m_GlobalVars.size(); ++i)
        if (m_GlobalVars[i].name == name) return (int)i;
    return -1;
}

void NodeGraph::AddGlobal(const std::string& name, float val, PinType type)
{
    int nextId = 0;
    for (const auto& gv : m_GlobalVars)
        if (gv.id >= nextId) nextId = gv.id + 1;

    GlobalVar gv;
    gv.id    = nextId;
    gv.name  = name;
    gv.value = val;
    gv.type  = type;
    m_GlobalVars.push_back(gv);
}

void NodeGraph::RemoveGlobal(int id)
{
    int idx = FindGlobalIndex(id);
    if (idx >= 0)
        m_GlobalVars.erase(m_GlobalVars.begin() + idx);
}

int NodeGraph::GlobalCount() const { return (int)m_GlobalVars.size(); }
float NodeGraph::GetGlobal(int idx) const { return (idx >= 0 && idx < (int)m_GlobalVars.size()) ? m_GlobalVars[idx].value : 0.0f; }
void NodeGraph::SetGlobal(int idx, float v) { if (idx >= 0 && idx < (int)m_GlobalVars.size()) m_GlobalVars[idx].value = v; }
PinType NodeGraph::GetGlobalType(int idx) const { return (idx >= 0 && idx < (int)m_GlobalVars.size()) ? m_GlobalVars[idx].type : PinType::Float; }
void    NodeGraph::SetGlobalType(int idx, PinType t) { if (idx >= 0 && idx < (int)m_GlobalVars.size()) m_GlobalVars[idx].type = t; }

// ============================================================================
// NavigateToOrigin — reset view to show graph content.
// Must be called BEFORE ed::Begin(), so NavigateToContent takes effect on the
// same frame and avoids a one-frame flicker.
// ============================================================================
void NodeGraph::NavigateToOrigin()
{
    // For an empty graph the default view (centered at origin) is fine.
    if (!m_Nodes.empty())
        ed::NavigateToContent(0.0f);
}

// ============================================================================
// Lookup helpers
// ============================================================================
EditorNode* NodeGraph::FindNode(ax::NodeEditor::NodeId id)
{
    for (auto& node : m_Nodes)
        if (node.ID == id)
            return &node;
    return nullptr;
}

EditorPin* NodeGraph::FindPin(ax::NodeEditor::PinId id)
{
    if (!id) return nullptr;
    for (auto& node : m_Nodes)
    {
        for (auto& pin : node.Inputs)
            if (pin.ID == id) return &pin;
        for (auto& pin : node.Outputs)
            if (pin.ID == id) return &pin;
    }
    return nullptr;
}

bool NodeGraph::IsPinLinked(ax::NodeEditor::PinId id) const
{
    if (!id) return false;
    for (auto& link : m_Links)
        if (link.StartPinID == id || link.EndPinID == id)
            return true;
    return false;
}

// ============================================================================
// Graph Map
// ============================================================================
static std::string MakeGraphKey(const std::string& robotMode, const std::string& gamepadMode)
{
    return robotMode + "|" + gamepadMode;
}

void NodeGraph::SaveGraphToMap()
{
    if (m_ActiveRobotModeName.empty()) return;
    std::string key = MakeGraphKey(m_ActiveRobotModeName, m_ActiveGamepadModeName);
    m_GraphMap[key] = GetGraphDataYaml();
}

void NodeGraph::SwitchGraph(const std::string& robotMode, const std::string& gamepadMode)
{
    m_ActiveRobotModeName = robotMode;
    m_ActiveGamepadModeName = gamepadMode;

    std::string key = MakeGraphKey(robotMode, gamepadMode);
    auto it = m_GraphMap.find(key);
    // Save current globals before clear
    auto savedVars = std::move(m_GlobalVars);
    if (it != m_GraphMap.end()) {
        LoadGraphData(it->second);
    } else {
        Clear();
    }
    // Restore globals (persist across graph switches for same session)
    m_GlobalVars = std::move(savedVars);
    m_Modified = false;
}

void NodeGraph::SetCurrentModePair(const std::string& robotMode, const std::string& gamepadMode)
{
    m_ActiveRobotModeName = robotMode;
    m_ActiveGamepadModeName = gamepadMode;
    SwitchGraph(robotMode, gamepadMode);
    m_NavigateFrame = 1;
}

void NodeGraph::SwitchRobotMode(const std::string& newRobotMode, const std::string& curGamepadMode)
{
    SwitchGraph(newRobotMode, curGamepadMode);
    m_NavigateFrame = 1;
}

void NodeGraph::SwitchGamepadMode(const std::string& curRobotMode, const std::string& newGamepadMode)
{
    SwitchGraph(curRobotMode, newGamepadMode);
    m_NavigateFrame = 1;
}

void NodeGraph::SetKeyValues(const std::map<std::string, float>& kv)
{
    std::lock_guard<std::mutex> lock(m_KvMutex);
    m_LastKeyValues = kv;
}

std::map<std::string, float> NodeGraph::GetKeyValuesSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_KvMutex);
    return m_LastKeyValues;
}

// ============================================================================
// TopoSortNodes — topological sort for evaluation order
// ============================================================================
static std::vector<int> TopoSortNodes(const std::vector<EditorNode>& nodes, const std::vector<EditorLink>& links)
{
    std::unordered_map<int, std::vector<int>> adj;
    std::unordered_map<int, int> indeg;

    for (const auto& node : nodes)
        indeg[(int)node.ID.Get()] = 0;

    for (const auto& link : links)
    {
        int fromNode = -1, toNode = -1;
        for (const auto& n : nodes) {
            for (const auto& p : n.Inputs)  { if (p.ID == link.EndPinID)   toNode   = (int)n.ID.Get(); }
            for (const auto& p : n.Outputs) { if (p.ID == link.StartPinID) fromNode = (int)n.ID.Get(); }
        }
        if (fromNode < 0 || toNode < 0) continue;
        adj[fromNode].push_back(toNode);
        indeg[toNode]++;
    }

    std::queue<int> qq;
    for (auto& [id, d] : indeg)
        if (d == 0) qq.push(id);

    std::vector<int> order;
    while (!qq.empty()) {
        int id = qq.front(); qq.pop();
        order.push_back(id);
        for (int nxt : adj[id])
            if (--indeg[nxt] == 0)
                qq.push(nxt);
    }
    return order;
}

// ============================================================================
// EvaluateCore — shared evaluation loop (used by EvaluateCompute/Into/ForDisplay)
// ============================================================================
static void EvaluateCore(
    std::vector<EditorNode>& nodes,
    const std::vector<EditorLink>& links,
    const std::map<std::string, float>& keyValues,
    float dt,
    float* globals, int globalsCount,
    std::function<void(EditorNode& node, float out, std::unordered_map<int, float>& pinVals)> onNode)
{
    auto order = TopoSortNodes(nodes, links);
    std::unordered_map<int, float> pinVals;

    for (int nid : order)
    {
        EditorNode* node = nullptr;
        for (auto& n : nodes)
            if ((int)n.ID.Get() == nid) { node = &n; break; }
        if (!node) continue;

        float out = ComputeNodeOutput(*node, keyValues, pinVals, dt, globals, globalsCount);

        // Cast output to match output pin type
        if (!node->Outputs.empty()) {
            auto otype = node->Outputs[0].Type;
            if (otype == PinType::Bool)      out = (out >= 0.5f) ? 1.0f : 0.0f;
            else if (otype == PinType::Int)  out = std::round(out);
        }

        onNode(*node, out, pinVals);

        // Propagate output to connected pins
        if (!node->Outputs.empty()) {
            int outPinId = (int)node->Outputs[0].ID.Get();
            for (const auto& link : links)
                if ((int)link.StartPinID.Get() == outPinId)
                    pinVals[(int)link.EndPinID.Get()] = out;
        }
    }
}

// ============================================================================
// EvaluateCompute — pure compute (legacy, single output map)
// ============================================================================
std::map<std::string, float> NodeGraph::EvaluateCompute(const std::map<std::string, float>& keyValues)
{
    std::map<std::string, float> outputs;
    if (m_Nodes.empty()) return outputs;

    float dt = GetEvalDeltaTime(m_LastEvalTimeNs);

    // Build temp float array indexed by GlobalVarId
    {
        int maxId = -1;
        for (const auto& gv : m_GlobalVars)
            if (gv.id > maxId) maxId = gv.id;
        m_GlobalTempVals.assign(maxId + 1, 0.0f);
        for (const auto& gv : m_GlobalVars)
            m_GlobalTempVals[gv.id] = gv.value;
    }

    EvaluateCore(m_Nodes, m_Links, keyValues, dt, m_GlobalTempVals.data(), (int)m_GlobalTempVals.size(),
        [&](EditorNode& node, float out, std::unordered_map<int, float>& /*pinVals*/) {
            if (!node.OutputTarget.empty())
                outputs[node.OutputTarget] = out;
        });

    // Write back modified globals
    for (auto& gv : m_GlobalVars)
        if (gv.id < (int)m_GlobalTempVals.size())
            gv.value = m_GlobalTempVals[gv.id];

    return outputs;
}

// ============================================================================
// EvaluateComputeInto — evaluate and group outputs by graph-comm index
//   CustomOutput.CommIndex → m_CommRefs[CommIndex] → dataVec idx
// ============================================================================
void NodeGraph::EvaluateComputeInto(const std::map<std::string, float>& keyValues,
                                     std::vector<ActuatorConfig>& dataVec,
                                     std::set<int>* pWrittenIndices)
{
    if (m_Nodes.empty()) return;

    float dt = GetEvalDeltaTime(m_LastEvalTimeNs);

    // Build temp float array indexed by GlobalVarId
    {
        int maxId = -1;
        for (const auto& gv : m_GlobalVars)
            if (gv.id > maxId) maxId = gv.id;
        m_GlobalTempVals.assign(maxId + 1, 0.0f);
        for (const auto& gv : m_GlobalVars)
            m_GlobalTempVals[gv.id] = gv.value;
    }

    // Per graph-comm-index output map
    std::unordered_map<int, std::map<std::string, float>> commOutputs;

    EvaluateCore(m_Nodes, m_Links, keyValues, dt, m_GlobalTempVals.data(), (int)m_GlobalTempVals.size(),
        [&](EditorNode& node, float out, std::unordered_map<int, float>& /*pinVals*/) {
            node.Value = out;
            if (!node.OutputTarget.empty()) {
                int gIdx = node.CommIndex;  // graph-comm-index = position in m_CommRefs
                commOutputs[gIdx][node.OutputTarget] = out;
            }
        });

    // Write back modified globals
    for (auto& gv : m_GlobalVars)
        if (gv.id < (int)m_GlobalTempVals.size())
            gv.value = m_GlobalTempVals[gv.id];

    // ---- ShortcutTrigger action execution (rising edge detection) ----
    for (auto& node : m_Nodes) {
        if (node.Type != NodeType::ShortcutTrigger) continue;
        float prev = node.Param[0];
        node.Param[0] = node.Value;
        if (prev < 0.5f && node.Value >= 0.5f) {
            fprintf(stderr, "[TRACE] ShortcutTrigger FIRED val=%.2f sendIdx=%d sendMode=%d hasCb=%d\n",
                node.Value, node.ShortcutSendIndex, node.ShortcutSendMode, m_SendActionCb ? 1 : 0);
            if (node.ShortcutSendIndex >= 0 && m_SendActionCb) {
                bool enable = (node.ShortcutSendMode == 0);
                bool oneShot = (node.ShortcutSendMode == 1);
                fprintf(stderr, "[TRACE] Calling SendActionCb(idx=%d, enable=%d, oneShot=%d)\n",
                    node.ShortcutSendIndex, enable, oneShot);
                m_SendActionCb(node.ShortcutSendIndex, enable, oneShot);
            } else if (node.ShortcutActionIndex >= 0 && m_ShortcutMgr) {
                m_ShortcutMgr->ExecuteAction(node.ShortcutActionIndex);
            }
        }
    }

    // Ensure dataVec has enough slots (one per graph-comm-index)
    int maxIdx = (int)m_CommRefs.size();
    for (const auto& [idx, _] : commOutputs)
        if (idx >= maxIdx) maxIdx = idx + 1;
    if (maxIdx > 0 && dataVec.empty())
        dataVec.resize(maxIdx);
    else if ((int)dataVec.size() < maxIdx)
        dataVec.resize(maxIdx, dataVec.empty() ? ActuatorConfig{} : dataVec[0]);

    // Write outputs to correct ActuatorConfig
    for (const auto& [commIdx, targetMap] : commOutputs) {
        if (commIdx >= (int)dataVec.size()) continue;
        for (const auto& [target, val] : targetMap)
            WriteOutputToActuator(target, val, dataVec[commIdx]);
        if (pWrittenIndices) pWrittenIndices->insert(commIdx);
    }
}

// ============================================================================
// EvaluateIntoActuators — thread-safe version of EvaluateComputeInto
// ============================================================================
void NodeGraph::EvaluateIntoActuators(const std::map<std::string, float>& keyValues,
                                       std::vector<ActuatorConfig>& dataVec,
                                       std::set<int>* pWrittenIndices)
{
    std::unique_lock<std::shared_mutex> lock(m_EvalMutex);
    EvaluateComputeInto(keyValues, dataVec, pWrittenIndices);
}

// ============================================================================
// EvaluateForDisplay — evaluate and update node display fields (UI thread only)
// ============================================================================
void NodeGraph::EvaluateForDisplay(const std::map<std::string, float>& keyValues)
{
    std::unique_lock<std::shared_mutex> lock(m_EvalMutex);
    m_LastOutputs.clear();
    if (m_Nodes.empty()) return;

    float dt = GetEvalDeltaTime(m_LastEvalTimeNs);

    // Build temp float array indexed by GlobalVarId
    {
        int maxId = -1;
        for (const auto& gv : m_GlobalVars)
            if (gv.id > maxId) maxId = gv.id;
        m_GlobalTempVals.assign(maxId + 1, 0.0f);
        for (const auto& gv : m_GlobalVars)
            m_GlobalTempVals[gv.id] = gv.value;
    }

    EvaluateCore(m_Nodes, m_Links, keyValues, dt, m_GlobalTempVals.data(), (int)m_GlobalTempVals.size(),
        [&](EditorNode& node, float out, std::unordered_map<int, float>& pinVals) {
            // Update display fields
            for (int i = 0; i < (int)node.Inputs.size() && i < 4; ++i) {
                auto it = pinVals.find((int)node.Inputs[i].ID.Get());
                float v = (it != pinVals.end()) ? it->second : 0.0f;
                node.InputValues[i] = v;
                node.InputBools[i] = (v >= 0.5f);
            }
            node.Value = out;

            if (!node.OutputTarget.empty())
                m_LastOutputs[node.OutputTarget] = out;
        });

    // Write back modified globals
    for (auto& gv : m_GlobalVars)
        if (gv.id < (int)m_GlobalTempVals.size())
            gv.value = m_GlobalTempVals[gv.id];

    // ---- ShortcutTrigger action execution (rising edge detection) ----
    for (auto& node : m_Nodes) {
        if (node.Type != NodeType::ShortcutTrigger) continue;
        // Param[0] stores previous frame output for edge detection
        float prev = node.Param[0];
        node.Param[0] = node.Value;
        if (prev < 0.5f && node.Value >= 0.5f) {
            // Rising edge: execute
            if (node.ShortcutSendIndex >= 0 && m_SendActionCb) {
                // Send frame action
                bool enable = (node.ShortcutSendMode == 0);  // Toggle mode
                bool oneShot = (node.ShortcutSendMode == 1);
                m_SendActionCb(node.ShortcutSendIndex, enable, oneShot);
            } else if (node.ShortcutActionIndex >= 0 && m_ShortcutMgr) {
                // Panel toggle action
                m_ShortcutMgr->ExecuteAction(node.ShortcutActionIndex);
            }
        }
    }
}

// ============================================================================
// Evaluate — thread-safe stateful evaluation (acquires write lock)
// ============================================================================
std::map<std::string, float> NodeGraph::Evaluate(const std::map<std::string, float>& keyValues)
{
    std::unique_lock<std::shared_mutex> lock(m_EvalMutex);
    return EvaluateCompute(keyValues);
}

// ============================================================================
// EvaluateIntoActuator
// ============================================================================
void NodeGraph::EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorConfig& data)
{
    std::unique_lock<std::shared_mutex> lock(m_EvalMutex);
    auto outputs = EvaluateCompute(keyValues);
    for (const auto& [target, val] : outputs)
        WriteOutputToActuator(target, val, data);
}

// ============================================================================
// GetGraphDataYaml — data-only serialization (no ed:: position)
// ============================================================================
std::string NodeGraph::GetGraphDataYaml() const
{
    std::shared_lock<std::shared_mutex> lock(m_EvalMutex);
    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
    for (auto& n : m_Nodes)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "id"            << YAML::Value << (int)n.ID.Get();
        out << YAML::Key << "type"          << YAML::Value << (int)n.Type;
        out << YAML::Key << "value"         << YAML::Value << n.Value;
        out << YAML::Key << "key_name"      << YAML::Value << n.KeyName;
        out << YAML::Key << "op_mode"       << YAML::Value << n.OpMode;
        out << YAML::Key << "output_target" << YAML::Value << n.OutputTarget;
        out << YAML::Key << "comm_index"    << YAML::Value << n.CommIndex;
        out << YAML::Key << "global_var_id" << YAML::Value << n.GlobalVarId;
        if (n.ShortcutActionIndex >= 0)
            out << YAML::Key << "shortcut_action" << YAML::Value << n.ShortcutActionIndex;
        if (n.ShortcutSendIndex >= 0) {
            out << YAML::Key << "shortcut_send_index" << YAML::Value << n.ShortcutSendIndex;
            out << YAML::Key << "shortcut_send_mode"  << YAML::Value << n.ShortcutSendMode;
        }
        out << YAML::Key << "param"         << YAML::Value << YAML::BeginSeq;
        for (int i = 0; i < 8; ++i) out << n.Param[i];
        out << YAML::EndSeq;
        out << YAML::Key << "mode_labels"   << YAML::Value << YAML::BeginSeq;
        for (auto& s : n.ModeLabels) out << s;
        out << YAML::EndSeq;
        out << YAML::Key << "state_f" << YAML::Value << YAML::BeginSeq;
        for (int i = 0; i < 4; ++i) out << n.StateF[i];
        out << YAML::EndSeq;
        // Pin types
        out << YAML::Key << "input_types" << YAML::Value << YAML::BeginSeq;
        for (auto& p : n.Inputs)  out << (int)p.Type;
        out << YAML::EndSeq;
        out << YAML::Key << "output_types" << YAML::Value << YAML::BeginSeq;
        for (auto& p : n.Outputs) out << (int)p.Type;
        out << YAML::EndSeq;
        // No pos_x / pos_y — this is data-only
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "links" << YAML::Value << YAML::BeginSeq;
    for (auto& l : m_Links)
    {
        int fromNodeId = -1, fromPinIdx = -1;
        int toNodeId = -1,   toPinIdx = -1;
        for (auto& n : m_Nodes) {
            for (size_t i = 0; i < n.Outputs.size(); ++i)
                if (n.Outputs[i].ID == l.StartPinID) { fromNodeId = (int)n.ID.Get(); fromPinIdx = (int)i; break; }
            for (size_t i = 0; i < n.Inputs.size(); ++i)
                if (n.Inputs[i].ID == l.EndPinID)   { toNodeId   = (int)n.ID.Get(); toPinIdx   = (int)i; break; }
        }
        out << YAML::BeginMap;
        out << YAML::Key << "id"        << YAML::Value << (int)l.ID.Get();
        out << YAML::Key << "from_node" << YAML::Value << fromNodeId;
        out << YAML::Key << "from_pin"  << YAML::Value << fromPinIdx;
        out << YAML::Key << "to_node"   << YAML::Value << toNodeId;
        out << YAML::Key << "to_pin"    << YAML::Value << toPinIdx;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "globals" << YAML::Value << YAML::BeginSeq;
    for (const auto& gv : m_GlobalVars) {
        out << YAML::BeginMap;
        out << YAML::Key << "id"    << YAML::Value << gv.id;
        out << YAML::Key << "name"  << YAML::Value << gv.name;
        out << YAML::Key << "value" << YAML::Value << gv.value;
        out << YAML::Key << "type"    << YAML::Value << (int)gv.type;
        out << YAML::Key << "visible" << YAML::Value << gv.visible;
        if (!gv.enumLabels.empty()) {
            out << YAML::Key << "enum_labels" << YAML::Value << YAML::BeginSeq;
            for (const auto& label : gv.enumLabels) out << label;
            out << YAML::EndSeq;
        }
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "active_robot_mode"   << YAML::Value << m_ActiveRobotModeName;
    out << YAML::Key << "active_gamepad_mode" << YAML::Value << m_ActiveGamepadModeName;
    out << YAML::Key << "comm_refs" << YAML::Value << YAML::BeginSeq;
    for (int ref : m_CommRefs) out << ref;
    out << YAML::EndSeq;

    out << YAML::EndMap;
    return out.c_str();
}

// ============================================================================
// LoadGraphData — data-only deserialization (no ed:: API)
// ============================================================================
bool NodeGraph::LoadGraphData(const std::string& yamlStr)
{
    if (yamlStr.empty()) return false;
    try
    {
        YAML::Node root = YAML::Load(yamlStr);
        if (!root.IsMap()) return false;

        std::unique_lock<std::shared_mutex> lock(m_EvalMutex);
        Clear_NoLock();

        if (root["nodes"] && root["nodes"].IsSequence())
        {
            for (auto yn : root["nodes"])
            {
                int rawType = yn["type"].as<int>();
                // Backward compat: remap removed/merged node types
                switch (rawType) {
                case 10: case 11: rawType = (int)NodeType::MathFunc; break;
                case 17: case 18: case 19: rawType = (int)NodeType::LogicOp; break;
                case 21: rawType = (int)NodeType::RisingEdge; break;
                case 24: rawType = (int)NodeType::DelayOn; break;
                case 40: rawType = (int)NodeType::AddSubMulDiv; break;
                case 42: rawType = (int)NodeType::ScaleBias; break;
                }
                NodeType nt = (NodeType)rawType;
                EditorNode* node = SpawnNode(nt);
                if (!node) continue;

                int savedId = yn["id"].as<int>();
                node->ID = ax::NodeEditor::NodeId(savedId);

                node->Value        = yn["value"]        ? yn["value"].as<float>()        : 0.0f;
                node->KeyName      = yn["key_name"]     ? yn["key_name"].as<std::string>() : "";
                node->OpMode       = yn["op_mode"]      ? yn["op_mode"].as<int>()        : 0;
                node->OutputTarget = yn["output_target"]? yn["output_target"].as<std::string>() : "";
                node->CommIndex    = yn["comm_index"]   ? yn["comm_index"].as<int>()    : 0;
                node->GlobalVarId  = yn["global_var_id"]? yn["global_var_id"].as<int>()  : -1;
                node->ShortcutActionIndex = yn["shortcut_action"] ? yn["shortcut_action"].as<int>() : -1;
                node->ShortcutSendIndex   = yn["shortcut_send_index"] ? yn["shortcut_send_index"].as<int>() : -1;
                node->ShortcutSendMode    = yn["shortcut_send_mode"] ? yn["shortcut_send_mode"].as<int>() : 0;

                // Load Param array (new format) or fall back to legacy fields
                if (yn["param"] && yn["param"].IsSequence()) {
                    int i = 0;
                    for (auto p : yn["param"]) { if (i < 8) node->Param[i++] = p.as<float>(); }
                } else {
                    if (yn["factor"])  node->Param[0] = yn["factor"].as<float>();
                    if (yn["min_val"]) node->Param[0] = yn["min_val"].as<float>();
                    if (yn["max_val"]) node->Param[1] = yn["max_val"].as<float>();
                    if (yn["digital"]) node->Param[2] = yn["digital"].as<bool>() ? 1.0f : 0.0f;
                }
                if (yn["state_f"] && yn["state_f"].IsSequence()) {
                    int i = 0;
                    for (auto s : yn["state_f"]) { if (i < 4) node->StateF[i++] = s.as<float>(); }
                }
                if (yn["mode_labels"] && yn["mode_labels"].IsSequence()) {
                    node->ModeLabels.clear();
                    for (auto s : yn["mode_labels"])
                        node->ModeLabels.push_back(s.as<std::string>());
                }
                // Restore pin types
                if (yn["input_types"] && yn["input_types"].IsSequence()) {
                    int i = 0;
                    for (auto t : yn["input_types"]) {
                        if (i < (int)node->Inputs.size()) node->Inputs[i].Type = (PinType)t.as<int>();
                        ++i;
                    }
                }
                if (yn["output_types"] && yn["output_types"].IsSequence()) {
                    int i = 0;
                    for (auto t : yn["output_types"]) {
                        if (i < (int)node->Outputs.size()) node->Outputs[i].Type = (PinType)t.as<int>();
                        ++i;
                    }
                }

                int maxUsed = savedId;
                for (auto& pin : node->Inputs)  maxUsed = std::max(maxUsed, (int)pin.ID.Get());
                for (auto& pin : node->Outputs) maxUsed = std::max(maxUsed, (int)pin.ID.Get());
                if (maxUsed >= m_NextId) m_NextId = maxUsed + 1;
            }
        }

        RebuildAllNodes();

        if (root["links"] && root["links"].IsSequence())
        {
            for (auto yl : root["links"])
            {
                int lid = yl["id"].as<int>();
                int fn = yl["from_node"] ? yl["from_node"].as<int>() : -1;
                int fi = yl["from_pin"]  ? yl["from_pin"].as<int>()  : 0;
                int tn = yl["to_node"]   ? yl["to_node"].as<int>()   : -1;
                int ti = yl["to_pin"]    ? yl["to_pin"].as<int>()    : 0;

                ax::NodeEditor::PinId sp, ep;
                if (fn >= 0 && tn >= 0) {
                    EditorNode* fromNode = FindNode(ax::NodeEditor::NodeId(fn));
                    EditorNode* toNode   = FindNode(ax::NodeEditor::NodeId(tn));
                    sp = (fromNode && fi < (int)fromNode->Outputs.size()) ? fromNode->Outputs[fi].ID : ax::NodeEditor::PinId(0);
                    ep = (toNode   && ti < (int)toNode->Inputs.size())   ? toNode->Inputs[ti].ID     : ax::NodeEditor::PinId(0);
                } else if (yl["start_pin"] && yl["end_pin"]) {
                    // Old format compat: raw pin IDs
                    int rawSp = yl["start_pin"].as<int>();
                    int rawEp = yl["end_pin"].as<int>();
                    int spNodeId = rawSp / 10, spRem = rawSp % 10;
                    int epNodeId = rawEp / 10, epRem = rawEp % 10;
                    EditorNode* fromNode = FindNode(ax::NodeEditor::NodeId(spNodeId));
                    EditorNode* toNode   = FindNode(ax::NodeEditor::NodeId(epNodeId));
                    int spIdx = (spRem >= 10) ? (spRem - 10) : spRem;
                    int epIdx = (epRem >= 10) ? (epRem - 10) : epRem;
                    bool spIsOut = (spRem >= 10);
                    bool epIsOut = (epRem >= 10);
                    sp = (fromNode && spIsOut && spIdx < (int)fromNode->Outputs.size()) ? fromNode->Outputs[spIdx].ID : ax::NodeEditor::PinId(0);
                    ep = (toNode   && !epIsOut && epIdx < (int)toNode->Inputs.size())   ? toNode->Inputs[epIdx].ID     : ax::NodeEditor::PinId(0);
                    if (!sp && fromNode && !spIsOut && spIdx < (int)fromNode->Inputs.size())  sp = fromNode->Inputs[spIdx].ID;
                    if (!ep && toNode && epIsOut && epIdx < (int)toNode->Outputs.size()) ep = toNode->Outputs[epIdx].ID;
                }
                if (sp && ep)
                    m_Links.emplace_back(ax::NodeEditor::LinkId(lid), sp, ep);
                if (lid >= m_NextId) m_NextId = lid + 1;
            }
        }

        // Load globals (auto-assign IDs for old saves without 'id' field)
        m_GlobalVars.clear();
        if (root["globals"] && root["globals"].IsSequence()) {
            int autoId = 0;
            for (auto g : root["globals"]) {
                if (g.IsMap() && g["name"]) {
                    GlobalVar gv;
                    gv.name  = g["name"].as<std::string>();
                    gv.value = g["value"] ? g["value"].as<float>() : 0.0f;
                    gv.type    = g["type"]    ? (PinType)g["type"].as<int>() : PinType::Float;
                    gv.visible = g["visible"] ? g["visible"].as<bool>()    : false;
                    if (g["enum_labels"] && g["enum_labels"].IsSequence()) {
                        for (auto lbl : g["enum_labels"])
                            gv.enumLabels.push_back(lbl.as<std::string>());
                    }
                    if (g["id"] && g["id"].as<int>() >= 0)
                        gv.id = g["id"].as<int>();
                    else
                        gv.id = autoId++;  // assign unique ID for old-format globals
                    m_GlobalVars.push_back(gv);
                }
            }
        }

        if (root["active_robot_mode"])   m_ActiveRobotModeName   = root["active_robot_mode"].as<std::string>();
        if (root["active_gamepad_mode"]) m_ActiveGamepadModeName = root["active_gamepad_mode"].as<std::string>();
        // Load comm_refs (new format) or fall back to comm_index (old format)
        if (root["comm_refs"] && root["comm_refs"].IsSequence()) {
            m_CommRefs.clear();
            for (auto r : root["comm_refs"]) m_CommRefs.push_back(r.as<int>());
        } else if (root["comm_index"]) {
            m_CommRefs.clear();
            m_CommRefs.push_back(root["comm_index"].as<int>());
        }

        m_Modified = false;
        return true;
    }
    catch (const std::exception&) { return false; }
}

// ============================================================================
// GetGraphYaml — full serialization including ed:: node positions
//   Caller must ensure ed::SetCurrentEditor() is set before calling.
// ============================================================================
std::string NodeGraph::GetGraphYaml() const
{
    std::shared_lock<std::shared_mutex> lock(m_EvalMutex);
    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
    for (auto& n : m_Nodes)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "id"            << YAML::Value << (int)n.ID.Get();
        out << YAML::Key << "type"          << YAML::Value << (int)n.Type;
        out << YAML::Key << "value"         << YAML::Value << n.Value;
        out << YAML::Key << "key_name"      << YAML::Value << n.KeyName;
        out << YAML::Key << "op_mode"       << YAML::Value << n.OpMode;
        out << YAML::Key << "output_target" << YAML::Value << n.OutputTarget;
        out << YAML::Key << "comm_index"    << YAML::Value << n.CommIndex;
        out << YAML::Key << "global_var_id" << YAML::Value << n.GlobalVarId;
        if (n.ShortcutActionIndex >= 0)
            out << YAML::Key << "shortcut_action" << YAML::Value << n.ShortcutActionIndex;
        if (n.ShortcutSendIndex >= 0) {
            out << YAML::Key << "shortcut_send_index" << YAML::Value << n.ShortcutSendIndex;
            out << YAML::Key << "shortcut_send_mode"  << YAML::Value << n.ShortcutSendMode;
        }
        out << YAML::Key << "param"         << YAML::Value << YAML::BeginSeq;
        for (int i = 0; i < 8; ++i) out << n.Param[i];
        out << YAML::EndSeq;
        out << YAML::Key << "mode_labels"   << YAML::Value << YAML::BeginSeq;
        for (auto& s : n.ModeLabels) out << s;
        out << YAML::EndSeq;
        out << YAML::Key << "state_f" << YAML::Value << YAML::BeginSeq;
        for (int i = 0; i < 4; ++i) out << n.StateF[i];
        out << YAML::EndSeq;
        // Pin types
        out << YAML::Key << "input_types" << YAML::Value << YAML::BeginSeq;
        for (auto& p : n.Inputs)  out << (int)p.Type;
        out << YAML::EndSeq;
        out << YAML::Key << "output_types" << YAML::Value << YAML::BeginSeq;
        for (auto& p : n.Outputs) out << (int)p.Type;
        out << YAML::EndSeq;

        ImVec2 pos = ed::GetNodePosition(n.ID);
        out << YAML::Key << "pos_x" << YAML::Value << pos.x;
        out << YAML::Key << "pos_y" << YAML::Value << pos.y;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "links" << YAML::Value << YAML::BeginSeq;
    for (auto& l : m_Links)
    {
        int fromNodeId = -1, fromPinIdx = -1;
        int toNodeId = -1,   toPinIdx = -1;
        for (auto& n : m_Nodes) {
            for (size_t i = 0; i < n.Outputs.size(); ++i)
                if (n.Outputs[i].ID == l.StartPinID) { fromNodeId = (int)n.ID.Get(); fromPinIdx = (int)i; break; }
            for (size_t i = 0; i < n.Inputs.size(); ++i)
                if (n.Inputs[i].ID == l.EndPinID)   { toNodeId   = (int)n.ID.Get(); toPinIdx   = (int)i; break; }
        }
        out << YAML::BeginMap;
        out << YAML::Key << "id"        << YAML::Value << (int)l.ID.Get();
        out << YAML::Key << "from_node" << YAML::Value << fromNodeId;
        out << YAML::Key << "from_pin"  << YAML::Value << fromPinIdx;
        out << YAML::Key << "to_node"   << YAML::Value << toNodeId;
        out << YAML::Key << "to_pin"    << YAML::Value << toPinIdx;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "globals" << YAML::Value << YAML::BeginSeq;
    for (const auto& gv : m_GlobalVars) {
        out << YAML::BeginMap;
        out << YAML::Key << "id"    << YAML::Value << gv.id;
        out << YAML::Key << "name"  << YAML::Value << gv.name;
        out << YAML::Key << "value" << YAML::Value << gv.value;
        out << YAML::Key << "type"    << YAML::Value << (int)gv.type;
        out << YAML::Key << "visible" << YAML::Value << gv.visible;
        if (!gv.enumLabels.empty()) {
            out << YAML::Key << "enum_labels" << YAML::Value << YAML::BeginSeq;
            for (const auto& label : gv.enumLabels) out << label;
            out << YAML::EndSeq;
        }
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "active_robot_mode"   << YAML::Value << m_ActiveRobotModeName;
    out << YAML::Key << "active_gamepad_mode" << YAML::Value << m_ActiveGamepadModeName;
    out << YAML::Key << "comm_refs" << YAML::Value << YAML::BeginSeq;
    for (int ref : m_CommRefs) out << ref;
    out << YAML::EndSeq;

    out << YAML::EndMap;
    return out.c_str();
}

// ============================================================================
// LoadGraphYaml — full deserialization including ed:: node positions
//   Caller must ensure ed::SetCurrentEditor() is set before calling.
// ============================================================================
bool NodeGraph::LoadGraphYaml(const std::string& yamlStr)
{
    if (yamlStr.empty()) return false;
    try
    {
        YAML::Node root = YAML::Load(yamlStr);
        if (!root.IsMap()) return false;

        std::unique_lock<std::shared_mutex> lock(m_EvalMutex);
        Clear_NoLock();

        if (root["nodes"] && root["nodes"].IsSequence())
        {
            for (auto yn : root["nodes"])
            {
                int rawType = yn["type"].as<int>();
                switch (rawType) {
                case 10: case 11: rawType = (int)NodeType::MathFunc; break;
                case 17: case 18: case 19: rawType = (int)NodeType::LogicOp; break;
                case 21: rawType = (int)NodeType::RisingEdge; break;
                case 24: rawType = (int)NodeType::DelayOn; break;
                case 40: rawType = (int)NodeType::AddSubMulDiv; break;
                case 42: rawType = (int)NodeType::ScaleBias; break;
                }
                NodeType nt = (NodeType)rawType;
                EditorNode* node = SpawnNode(nt);
                if (!node) continue;

                int savedId = yn["id"].as<int>();
                node->ID = ed::NodeId(savedId);

                node->Value        = yn["value"]        ? yn["value"].as<float>()        : 0.0f;
                node->KeyName      = yn["key_name"]     ? yn["key_name"].as<std::string>() : "";
                node->OpMode       = yn["op_mode"]      ? yn["op_mode"].as<int>()        : 0;
                node->OutputTarget = yn["output_target"]? yn["output_target"].as<std::string>() : "";
                node->CommIndex    = yn["comm_index"]   ? yn["comm_index"].as<int>()    : 0;
                node->GlobalVarId  = yn["global_var_id"]? yn["global_var_id"].as<int>()  : -1;
                node->ShortcutActionIndex = yn["shortcut_action"] ? yn["shortcut_action"].as<int>() : -1;
                node->ShortcutSendIndex   = yn["shortcut_send_index"] ? yn["shortcut_send_index"].as<int>() : -1;
                node->ShortcutSendMode    = yn["shortcut_send_mode"] ? yn["shortcut_send_mode"].as<int>() : 0;

                // Load Param array (new format) or fall back to legacy fields
                if (yn["param"] && yn["param"].IsSequence()) {
                    int i = 0;
                    for (auto p : yn["param"]) { if (i < 8) node->Param[i++] = p.as<float>(); }
                } else {
                    if (yn["factor"])  node->Param[0] = yn["factor"].as<float>();
                    if (yn["min_val"]) node->Param[0] = yn["min_val"].as<float>();
                    if (yn["max_val"]) node->Param[1] = yn["max_val"].as<float>();
                    if (yn["digital"]) node->Param[2] = yn["digital"].as<bool>() ? 1.0f : 0.0f;
                }
                if (yn["state_f"] && yn["state_f"].IsSequence()) {
                    int i = 0;
                    for (auto s : yn["state_f"]) { if (i < 4) node->StateF[i++] = s.as<float>(); }
                }
                if (yn["mode_labels"] && yn["mode_labels"].IsSequence()) {
                    node->ModeLabels.clear();
                    for (auto s : yn["mode_labels"])
                        node->ModeLabels.push_back(s.as<std::string>());
                }
                // Restore pin types
                if (yn["input_types"] && yn["input_types"].IsSequence()) {
                    int i = 0;
                    for (auto t : yn["input_types"]) {
                        if (i < (int)node->Inputs.size()) node->Inputs[i].Type = (PinType)t.as<int>();
                        ++i;
                    }
                }
                if (yn["output_types"] && yn["output_types"].IsSequence()) {
                    int i = 0;
                    for (auto t : yn["output_types"]) {
                        if (i < (int)node->Outputs.size()) node->Outputs[i].Type = (PinType)t.as<int>();
                        ++i;
                    }
                }

                if (yn["pos_x"] && yn["pos_y"])
                    ed::SetNodePosition(node->ID, ImVec2(yn["pos_x"].as<float>(), yn["pos_y"].as<float>()));

                int maxUsed = savedId;
                for (auto& pin : node->Inputs)  maxUsed = std::max(maxUsed, (int)pin.ID.Get());
                for (auto& pin : node->Outputs) maxUsed = std::max(maxUsed, (int)pin.ID.Get());
                if (maxUsed >= m_NextId) m_NextId = maxUsed + 1;
            }
        }

        RebuildAllNodes();

        if (root["links"] && root["links"].IsSequence())
        {
            for (auto yl : root["links"])
            {
                int lid = yl["id"].as<int>();
                int fn = yl["from_node"] ? yl["from_node"].as<int>() : -1;
                int fi = yl["from_pin"]  ? yl["from_pin"].as<int>()  : 0;
                int tn = yl["to_node"]   ? yl["to_node"].as<int>()   : -1;
                int ti = yl["to_pin"]    ? yl["to_pin"].as<int>()    : 0;

                ed::PinId sp, ep;
                if (fn >= 0 && tn >= 0) {
                    EditorNode* fromNode = FindNode(ed::NodeId(fn));
                    EditorNode* toNode   = FindNode(ed::NodeId(tn));
                    sp = (fromNode && fi < (int)fromNode->Outputs.size()) ? fromNode->Outputs[fi].ID : ed::PinId(0);
                    ep = (toNode   && ti < (int)toNode->Inputs.size())   ? toNode->Inputs[ti].ID     : ed::PinId(0);
                } else if (yl["start_pin"] && yl["end_pin"]) {
                    int rawSp = yl["start_pin"].as<int>();
                    int rawEp = yl["end_pin"].as<int>();
                    int spNodeId = rawSp / 10, spRem = rawSp % 10;
                    int epNodeId = rawEp / 10, epRem = rawEp % 10;
                    EditorNode* fromNode = FindNode(ed::NodeId(spNodeId));
                    EditorNode* toNode   = FindNode(ed::NodeId(epNodeId));
                    int spIdx = (spRem >= 10) ? (spRem - 10) : spRem;
                    int epIdx = (epRem >= 10) ? (epRem - 10) : epRem;
                    bool spIsOut = (spRem >= 10);
                    bool epIsOut = (epRem >= 10);
                    sp = (fromNode && spIsOut && spIdx < (int)fromNode->Outputs.size()) ? fromNode->Outputs[spIdx].ID : ed::PinId(0);
                    ep = (toNode   && !epIsOut && epIdx < (int)toNode->Inputs.size())   ? toNode->Inputs[epIdx].ID     : ed::PinId(0);
                    if (!sp && fromNode && !spIsOut && spIdx < (int)fromNode->Inputs.size())  sp = fromNode->Inputs[spIdx].ID;
                    if (!ep && toNode && epIsOut && epIdx < (int)toNode->Outputs.size()) ep = toNode->Outputs[epIdx].ID;
                }
                if (sp && ep)
                    m_Links.emplace_back(ed::LinkId(lid), sp, ep);
                if (lid >= m_NextId) m_NextId = lid + 1;
            }
        }

        // 恢复 globals (auto-assign IDs for old saves)
        m_GlobalVars.clear();
        if (root["globals"] && root["globals"].IsSequence()) {
            int autoId = 0;
            for (auto g : root["globals"]) {
                if (g.IsMap() && g["name"]) {
                    GlobalVar gv;
                    gv.name  = g["name"].as<std::string>();
                    gv.value = g["value"] ? g["value"].as<float>() : 0.0f;
                    gv.type    = g["type"]    ? (PinType)g["type"].as<int>() : PinType::Float;
                    gv.visible = g["visible"] ? g["visible"].as<bool>()    : false;
                    if (g["enum_labels"] && g["enum_labels"].IsSequence()) {
                        for (auto lbl : g["enum_labels"])
                            gv.enumLabels.push_back(lbl.as<std::string>());
                    }
                    if (g["id"] && g["id"].as<int>() >= 0)
                        gv.id = g["id"].as<int>();
                    else
                        gv.id = autoId++;
                    m_GlobalVars.push_back(gv);
                }
            }
        }

        // 恢复 mode 名
        if (root["active_robot_mode"])
            m_ActiveRobotModeName = root["active_robot_mode"].as<std::string>();
        if (root["active_gamepad_mode"])
            m_ActiveGamepadModeName = root["active_gamepad_mode"].as<std::string>();
        if (root["comm_refs"] && root["comm_refs"].IsSequence()) {
            m_CommRefs.clear();
            for (auto r : root["comm_refs"]) m_CommRefs.push_back(r.as<int>());
        } else if (root["comm_index"]) {
            m_CommRefs.clear();
            m_CommRefs.push_back(root["comm_index"].as<int>());
        }

        m_Modified = false;
        return true;
    }
    catch (const std::exception&) { return false; }
}

// ============================================================================
std::vector<OutputTargetInfo> BuildOutputTargetsFromProtocol(const std::vector<ProtocolSendConfig>& cfgs, const ActuatorConfig& actuator)
{
    std::vector<OutputTargetInfo> targets;

    auto components = GetSendComponents(actuator, SensorConfig{});

    for (const auto& cfg : cfgs) {
        for (const auto& field : cfg.fields)
        {
            if (field.fix) continue;

            std::string curCompId = ResolveComponentId(field.field_path);
            std::string curSub = ResolveSubField(field.field_path);
            std::string displayName = field.name;

            for (const auto& c : components) {
                if (c.id == curCompId) {
                    auto sfs = GetSubFields(c);
                    for (const auto& sf : sfs) {
                        if (sf.key == curSub) {
                            displayName = c.label + " > " + sf.label;
                            break;
                        }
                    }
                    if (displayName == field.name) displayName = c.label + " > " + curSub;
                    break;
                }
            }

            targets.push_back({displayName, field.field_path, field.encoding});
        }
    }

    return targets;
}

// ============================================================================
// Clone — deep copy via data YAML round-trip (no ed:: positions)
// ============================================================================
std::unique_ptr<NodeGraph> NodeGraph::Clone() const
{
    auto c = std::make_unique<NodeGraph>();
    std::string yaml = GetGraphDataYaml();
    c->LoadGraphData(yaml);
    c->m_GraphMap = m_GraphMap;
    c->m_ActiveRobotModeName = m_ActiveRobotModeName;
    c->m_ActiveGamepadModeName = m_ActiveGamepadModeName;
    return c;
}

// ============================================================================
// AddNode / AddNodeAt — create nodes (editor-side)
// ============================================================================
void NodeGraph::AddNode(NodeType type)
{
    // Place new node at current mouse position in canvas space
    ImVec2 mousePos = ImGui::GetMousePos();
    // If we're inside an ed:: context, convert screen→canvas
    AddNodeAt(type, ed::ScreenToCanvas(mousePos), false);
}

bool NodeGraph::AddNodeAt(NodeType type, const ImVec2& pos, bool fromScreen)
{
    // UI thread exclusive — no lock needed
    EditorNode* node = SpawnNode(type);
    if (node)
    {
        ed::SetNodePosition(node->ID, fromScreen ? ed::ScreenToCanvas(pos) : pos);
        SetModified(true);
    }

    RebuildAllNodes();
    return node != nullptr;
}
