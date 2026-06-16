#define IMGUI_DEFINE_MATH_OPERATORS
#include "NodeGraph.h"
#include "FileManager.h"
#include "RobotComponentManager.h"
#include "GamepadMapperManager.h"
#include "RobotCommManager.h"
#include "Walnut/Core/Log.h"
#include <yaml-cpp/yaml.h>
#include <imgui_internal.h>
#include "Walnut/Image.h"
#include <algorithm>
#include <cmath>
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
// Local helpers
// ============================================================================
static void ManualSplitter(const char* id, float* size, float minSize, float thickness, bool reverse = false)
{
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 avail  = ImGui::GetContentRegionAvail();
    ImRect bb(cursor.x, cursor.y, cursor.x + thickness, cursor.y + avail.y);

    ImGui::InvisibleButton("##splitter", ImVec2(thickness, avail.y));

    bool hovered = ImGui::IsItemHovered();
    bool selected  = ImGui::IsItemActive();

    ImU32 col = selected  ? IM_COL32(100, 100, 255, 255)
              : hovered ? IM_COL32(80, 80, 180, 255)
              :            IM_COL32(60, 60, 80, 150);

    dl->AddRectFilled(ImVec2(cursor.x, cursor.y),
                      ImVec2(cursor.x + thickness, cursor.y + avail.y), col);

    if (selected)
    {
        float delta = ImGui::GetIO().MouseDelta.x;
        if (reverse) delta = -delta;
        *size += delta;
        if (*size < minSize) *size = minSize;
        if (*size > avail.y) *size = ImGui::GetWindowWidth() * 0.5f;
    }

    ImGui::PopID();
}

// ============================================================================
// NodeGraph — Constructor / Destructor
// ============================================================================
NodeGraph::NodeGraph() {}
NodeGraph::~NodeGraph() {}

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
// EvaluateCompute — pure compute
// ============================================================================
std::map<std::string, float> NodeGraph::EvaluateCompute(const std::map<std::string, float>& keyValues)
{
    std::map<std::string, float> outputs;
    if (m_Nodes.empty()) return outputs;

    // Compute real delta time (thread-safe via atomic)
    float dt = GetEvalDeltaTime(m_LastEvalTimeNs);

    auto order = TopoSortNodes(m_Nodes, m_Links);
    std::unordered_map<int, float> pinVals;

    // Build temp float array indexed by GlobalVarId for O(1) evaluation lookup
    {
        int maxId = -1;
        for (const auto& gv : m_GlobalVars)
            if (gv.id > maxId) maxId = gv.id;
        m_GlobalTempVals.assign(maxId + 1, 0.0f);
        for (const auto& gv : m_GlobalVars)
            m_GlobalTempVals[gv.id] = gv.value;
    }

    for (int nid : order)
    {
        EditorNode* node = nullptr;
        for (auto& n : m_Nodes)
            if ((int)n.ID.Get() == nid) { node = &n; break; }
        if (!node) continue;

        float out = ComputeNodeOutput(*node, keyValues, pinVals, dt, m_GlobalTempVals.data(), (int)m_GlobalTempVals.size());

        // Cast output to match output pin type
        if (!node->Outputs.empty()) {
            auto otype = node->Outputs[0].Type;
            if (otype == PinType::Bool)      out = (out >= 0.5f) ? 1.0f : 0.0f;
            else if (otype == PinType::Int)  out = std::round(out);
        }

        if (!node->OutputTarget.empty())
            outputs[node->OutputTarget] = out;

        // Propagate output to connected pins
        if (!node->Outputs.empty()) {
            int outPinId = (int)node->Outputs[0].ID.Get();
            for (const auto& link : m_Links)
                if ((int)link.StartPinID.Get() == outPinId)
                    pinVals[(int)link.EndPinID.Get()] = out;
        }
    }

    // Write back modified globals from evaluation (indexed by GlobalVarId)
    for (auto& gv : m_GlobalVars)
        if (gv.id < (int)m_GlobalTempVals.size())
            gv.value = m_GlobalTempVals[gv.id];

    return outputs;
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

    auto order = TopoSortNodes(m_Nodes, m_Links);
    std::unordered_map<int, float> pinVals;

    // Build temp float array indexed by GlobalVarId for O(1) evaluation lookup
    {
        int maxId = -1;
        for (const auto& gv : m_GlobalVars)
            if (gv.id > maxId) maxId = gv.id;
        m_GlobalTempVals.assign(maxId + 1, 0.0f);
        for (const auto& gv : m_GlobalVars)
            m_GlobalTempVals[gv.id] = gv.value;
    }

    for (int nid : order)
    {
        EditorNode* node = nullptr;
        for (auto& n : m_Nodes)
            if ((int)n.ID.Get() == nid) { node = &n; break; }
        if (!node) continue;

        float out = ComputeNodeOutput(*node, keyValues, pinVals, dt, m_GlobalTempVals.data(), (int)m_GlobalTempVals.size());

        // Cast output to match output pin type
        if (!node->Outputs.empty()) {
            auto otype = node->Outputs[0].Type;
            if (otype == PinType::Bool)      out = (out >= 0.5f) ? 1.0f : 0.0f;
            else if (otype == PinType::Int)  out = std::round(out);
        }

        // Update generic display fields
        for (int i = 0; i < (int)node->Inputs.size() && i < 4; ++i) {
            auto it = pinVals.find((int)node->Inputs[i].ID.Get());
            float v = (it != pinVals.end()) ? it->second : 0.0f;
            node->InputValues[i] = v;
            node->InputBools[i] = (v >= 0.5f);
        }
        node->Value = out;

        if (!node->OutputTarget.empty())
            m_LastOutputs[node->OutputTarget] = out;

        if (!node->Outputs.empty()) {
            int outPinId = (int)node->Outputs[0].ID.Get();
            for (auto& link : m_Links)
                if ((int)link.StartPinID.Get() == outPinId)
                    pinVals[(int)link.EndPinID.Get()] = out;
        }
    }

    // Write back modified globals from evaluation (indexed by GlobalVarId)
    for (auto& gv : m_GlobalVars)
        if (gv.id < (int)m_GlobalTempVals.size())
            gv.value = m_GlobalTempVals[gv.id];
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
        out << YAML::Key << "global_var_id" << YAML::Value << n.GlobalVarId;
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
        out << YAML::Key << "type"  << YAML::Value << (int)gv.type;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "active_robot_mode"   << YAML::Value << m_ActiveRobotModeName;
    out << YAML::Key << "active_gamepad_mode" << YAML::Value << m_ActiveGamepadModeName;
    out << YAML::Key << "comm_index"          << YAML::Value << m_CommIndex;

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
                node->GlobalVarId  = yn["global_var_id"]? yn["global_var_id"].as<int>()  : -1;

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
                    gv.type  = g["type"]  ? (PinType)g["type"].as<int>() : PinType::Float;
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
        if (root["comm_index"])          m_CommIndex             = root["comm_index"].as<int>();

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
        out << YAML::Key << "global_var_id" << YAML::Value << n.GlobalVarId;
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
        out << YAML::Key << "type"  << YAML::Value << (int)gv.type;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "active_robot_mode"   << YAML::Value << m_ActiveRobotModeName;
    out << YAML::Key << "active_gamepad_mode" << YAML::Value << m_ActiveGamepadModeName;

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
                node->GlobalVarId  = yn["global_var_id"]? yn["global_var_id"].as<int>()  : -1;

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
                    gv.type  = g["type"]  ? (PinType)g["type"].as<int>() : PinType::Float;
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

        m_Modified = false;
        return true;
    }
    catch (const std::exception&) { return false; }
}

void NodeGraph::DrawNodeContents(EditorNode& node,
                                  const std::set<std::string>& analogKeys,
                                  const std::vector<OutputTargetInfo>& outputTargets)
{
    auto isLinked = [this](int pid) { return IsPinLinked(ax::NodeEditor::PinId(pid)); };
    auto onMod = [this]() { SetModified(true); };
    auto showBool = [](float v) {
        if (v >= 0.5f) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
        else           ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
    };

    // Helper to display a pin value according to its type
    auto ShowPinValue = [&](PinType type, float v) {
        switch (type) {
        case PinType::Bool: showBool(v); break;
        case PinType::Int:  ImGui::TextDisabled("%d", (int)std::round(v)); break;
        default:            ImGui::TextDisabled("%.3f", v); break;
        }
    };

    // Special: KeySource
    if (node.Type == NodeType::KeySource) {
        ImGui::PushID((int)node.ID.Get());
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        std::string btnLabel = node.KeyName.empty() ? "(click to select)" : node.KeyName;
        bool isActive = (m_ActiveKeySourceId == node.ID);
        ImVec4 btnCol = isActive ? ImVec4(0.3f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        if (ImGui::Button(btnLabel.c_str(), ImVec2(105, 0))) {
            m_ActiveKeySourceId = (m_ActiveKeySourceId == node.ID) ? ax::NodeEditor::NodeId(0) : node.ID;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            node.KeyName.clear();
            SetModified(true);
        }
        ImGui::PopStyleVar();
        ImGui::SameLine();
        bool isAnalog = (analogKeys.count(node.KeyName) > 0);
        if (!isAnalog) {
            if (node.Value >= 0.5f) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
            else                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
        } else {
            ImVec4 c(0.4f, 0.8f, 1.0f, 1.0f);
            if (node.Value < 0.0f) c = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            ImGui::TextColored(c, "%.3f", node.Value);
        }
        for (auto& pin : node.Outputs) {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine(); ImGui::TextUnformatted(pin.Name.c_str());
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            ShowPinValue(pin.Type, node.Value);
        }
        ImGui::PopID();
        return;
    }

    // Special: ConstValue (has drag-float on output)
    if (node.Type == NodeType::ConstValue) {
        ImGui::PushID((int)node.ID.Get());
        for (auto& pin : node.Outputs) {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine(); ImGui::SetNextItemWidth(70);
            if (ImGui::DragFloat("##Val", &node.Value, 0.01f, -100.0f, 100.0f, "%.3f"))
                SetModified(true);
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            ShowPinValue(pin.Type, node.Value);
        }
        ImGui::PopID();
        return;
    }

    // Special: CustomOutput (click to select node, right-click to clear)
    if (node.Type == NodeType::CustomOutput) {
        ImGui::PushID((int)node.ID.Get());
        for (auto& pin : node.Inputs) {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine(); ImGui::TextUnformatted("Value");
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            ShowPinValue(pin.Type, node.Value);
        }
        ImVec2 btnSize(150, 0);
        std::string btnLabel = node.OutputTarget;
        if (!btnLabel.empty()) {
            for (const auto& t : outputTargets) {
                if (t.field_path == node.OutputTarget) { btnLabel = t.name; break; }
            }
        }
        if (btnLabel.empty()) btnLabel = "(click to select)";
        bool isActive = (m_ActiveOutputId == node.ID);
        ImVec4 btnCol = isActive ? ImVec4(0.3f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        if (ImGui::Button(btnLabel.c_str(), btnSize)) {
            m_ActiveOutputId = (m_ActiveOutputId == node.ID) ? ax::NodeEditor::NodeId(0) : node.ID;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            node.OutputTarget.clear();
            SetModified(true);
        }
        ImGui::PopID();
        return;
    }

    // Special: GlobalRead / GlobalWrite (click to select variable)
    if (node.Type == NodeType::GlobalRead || node.Type == NodeType::GlobalWrite) {
        ImGui::PushID((int)node.ID.Get());

        // Draw input pins (if any) — same as generic
        for (size_t i = 0; i < node.Inputs.size(); ++i) {
            auto& pin = node.Inputs[i];
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            float v = (i < 4) ? node.InputValues[i] : 0.0f;
            if (pin.Type == PinType::Bool) showBool(v);
            else ImGui::TextDisabled("%.3f", v);
        }

        // Variable selector button (like KeySource, uses GlobalVarId)
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        std::string btnLabel = "(click to select)";
        if (node.GlobalVarId >= 0) {
            int gidx = FindGlobalIndex(node.GlobalVarId);
            if (gidx >= 0 && gidx < (int)m_GlobalVars.size())
                btnLabel = m_GlobalVars[gidx].name;
        }
        bool isActive = (m_ActiveGlobalReadId == node.ID);
        ImVec4 btnCol = isActive ? ImVec4(0.3f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        if (ImGui::Button(btnLabel.c_str(), ImVec2(105, 0))) {
            m_ActiveGlobalReadId = (m_ActiveGlobalReadId == node.ID) ? ax::NodeEditor::NodeId(0) : node.ID;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            node.GlobalVarId = -1;
            SetModified(true);
        }
        ImGui::PopStyleVar();

        // Output pin
        for (auto& pin : node.Outputs) {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine(); ImGui::TextUnformatted(pin.Name.c_str());
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            if (pin.Type == PinType::Bool) showBool(node.Value);
            else ImGui::TextDisabled("%.3f", node.Value);
        }
        ImGui::PopID();
        return;
    }

    // Generic: all other node types
    std::vector<std::string> avKeys(m_AvailableKeys.begin(), m_AvailableKeys.end());
    std::vector<std::string> outNames, outPaths;
    for (auto& t : outputTargets) { outNames.push_back(t.name); outPaths.push_back(t.field_path); }
    DrawGenericNodeBody(node, analogKeys, avKeys, outNames, outPaths, isLinked, onMod);
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
    // UI 线程独占 — 无需锁（Draw 内从菜单/popup 调用时全局共享锁可能已持，unique_lock 会死锁）
    EditorNode* node = SpawnNode(type);
    if (node)
    {
        ed::SetNodePosition(node->ID, fromScreen ? ed::ScreenToCanvas(pos) : pos);
        SetModified(true);
    }

    RebuildAllNodes();
    return node != nullptr;
}

// ============================================================================
// DrawCategorySubmenus — draw "Add Node" submenus grouped by category
// ============================================================================
static void DrawAddNodeCategoryMenus(std::function<void(NodeType)> addFn)
{
    for (int catIdx = 0; catIdx < 5; ++catIdx) {
        NodeCategory cat = (NodeCategory)(catIdx);
        int cnt = GetCategoryNodeCount(cat);
        if (cnt == 0) continue;
        // Skip KeySource/ConstValue/CustomOutput in categories
        if (ImGui::BeginMenu(GetCategoryName(cat))) {
            for (int i = 0; i < cnt; ++i) {
                NodeType nt = GetCategoryNodeType(cat, i);
                if (ImGui::MenuItem(GetNodeTitle(nt))) addFn(nt);
            }
            ImGui::EndMenu();
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Key Source"))  addFn(NodeType::KeySource);
    if (ImGui::MenuItem("Const Value")) addFn(NodeType::ConstValue);
    ImGui::Separator();
    if (ImGui::MenuItem("Output"))      addFn(NodeType::CustomOutput);
}

// ============================================================================
// DrawMenuBar — top menu bar (Add Node / Clear All / Reset View)
// ============================================================================
void NodeGraph::DrawMenuBar()
{
    if (ImGui::BeginMenuBar())
    {
        if (!m_IsRunning)
        {
            if (ImGui::BeginMenu("Add Node"))
            {
                DrawAddNodeCategoryMenus([this](NodeType nt) { AddNode(nt); });
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Clear All"))
            {
                Clear();
                m_NavigateFrame = 1;
            }
        }
        if (ImGui::MenuItem("Reset View"))  ed::NavigateToContent();
        ImGui::EndMenuBar();
    }
}

// ============================================================================
// DrawKeyValuesSidebar — left sidebar showing input key values
// ============================================================================
void NodeGraph::DrawKeyValuesSidebar(float sideWidth, const std::set<std::string>& analogKeys)
{
    std::map<std::string, float> snapshot = GetKeyValuesSnapshot();

    ImGui::BeginChild("##KVSide", ImVec2(sideWidth, 0), true);
    ImGui::TextUnformatted("Input Keys");
    ImGui::Separator();

    // Build deduped key list
    std::set<std::string> keys;
    for (const auto& k : m_AvailableKeys) keys.insert(k);
    for (const auto& [name, val] : snapshot) keys.insert(name);

    EditorNode* activeKS = FindNode(m_ActiveKeySourceId);
    if (activeKS && activeKS->Type != NodeType::KeySource) activeKS = nullptr;

    if (keys.empty())
    {
        ImGui::TextDisabled("(empty)");
    }
    else
    {
        for (const auto& name : keys)
        {
            float val = (snapshot.count(name) > 0) ? snapshot.at(name) : 0.0f;
            bool isAnalog = (analogKeys.count(name) > 0);
            bool isSelected = (activeKS && activeKS->KeyName == name);

            if (ImGui::Selectable(name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (activeKS) {
                    activeKS->KeyName = name;
                    SetModified(true);
                }
            }
            ImGui::SameLine(sideWidth - 65);
            if (!isAnalog)
            {
                if (val >= 0.5f)
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
            }
            else
            {
                ImVec4 color(0.4f, 0.8f, 1.0f, 1.0f);
                if (val < 0.0f) color = ImVec4(1.0f, 0.5f, 0.4f, 1.0f);
                ImGui::TextColored(color, "%.3f", val);
            }
        }
    }

    ImGui::EndChild();
}

// ============================================================================
// DrawGlobalsSidebar — DEPRECATED, merged into DrawKeyValuesSidebar
// ============================================================================
void NodeGraph::DrawGlobalsSidebar(float) {}

// ============================================================================
// Draw — full editor render (called from Manager)
// ============================================================================
void NodeGraph::Draw(ax::NodeEditor::EditorContext* editorCtx)
{
    if (m_IsRunning)
        EvaluateForDisplay(GetKeyValuesSnapshot());

    // -------- Play/Stop toolbar (image button, outside ed::) --------
    {
        if (!m_PlayIcon)  m_PlayIcon  = std::make_shared<Walnut::Image>(FileManager::GetExeDir() + "..\\..\\..\\asset\\picture\\PlayButton.png");
        if (!m_StopIcon)  m_StopIcon  = std::make_shared<Walnut::Image>(FileManager::GetExeDir() + "..\\..\\..\\asset\\picture\\StopButton.png");

        auto icon = m_IsRunning ? m_StopIcon : m_PlayIcon;
        ImVec2 iconSize(32, 32);

        float avail = ImGui::GetContentRegionAvail().x;
        float offset = (avail - iconSize.x) * 0.5f;
        if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

        if (icon->GetDescriptorSet()) {
            if (ImGui::ImageButton((ImTextureID)icon->GetDescriptorSet(), iconSize)) {
                ToggleRunning();
            }
        } else {
            const char* label = m_IsRunning ? " Stop " : " Play ";
            ImVec4 col = m_IsRunning ? ImVec4(0.7f, 0.15f, 0.15f, 1.0f) : ImVec4(0.15f, 0.6f, 0.15f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            if (ImGui::Button(label, ImVec2(iconSize.x * 2, 0))) ToggleRunning();
            ImGui::PopStyleColor(2);
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(m_IsRunning ? "Stop evaluation" : "Start evaluation");

        ImGui::Separator();
    }

    ed::SetCurrentEditor(editorCtx);

    DrawMenuBar();

    // -------- Pull data from managers --------
    std::vector<std::string> gamepadModeNames;
    {
        if (m_GamepadMgr) {
            for (const auto& gm : m_GamepadMgr->GetMappers())
                gamepadModeNames.push_back(gm.name);
        }
    }

    // Compute analog keys & output targets & available keys from active modes
    {
        m_AnalogKeys.clear();
        m_AvailableKeys.clear();
        m_OutputTargets.clear();
        if (m_GamepadMgr) {
            for (auto& gm : m_GamepadMgr->GetMappers()) {
                if (std::string(gm.name) == m_ActiveGamepadModeName) {
                    for (const auto& mapping : gm.mappings) {
                        m_AvailableKeys.push_back(mapping.key_name);
                        if (mapping.is_analog)
                            m_AnalogKeys.insert(mapping.key_name);
                    }
                    // 直接从当前选中 mapper 读取实时值，更新侧栏
                    std::map<std::string, float> kv;
                    for (const auto& m : gm.mappings) {
                        if (m.is_bound)
                            kv[m.key_name] = gm.GetKeyValue(m.key_name);
                    }
                    SetKeyValues(kv);
                    break;
                }
            }
        }
        if (m_RobotMgr) {
            for (const auto& c : m_RobotMgr->GetComponents()) {
                const auto& mode = c.component;
                if (std::string(mode.name) == m_ActiveRobotModeName) {
                    m_OutputTargets = BuildOutputTargetsFromProtocol(mode.protocol_send, mode.actuator_config);
                    break;
                }
            }
        }
    }

    // -------- item selector --------
    {
        if (!gamepadModeNames.empty())
        {
            ImGui::TextUnformatted("Gamepad Mapper:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140);
            int selectedGamepadIdx = 0;
            for (int i = 0; i < (int)gamepadModeNames.size(); ++i) {
                if (gamepadModeNames[i] == m_ActiveGamepadModeName) { selectedGamepadIdx = i; break; }
            }
            if (ImGui::Combo("##GamepadCombo", &selectedGamepadIdx,
                [](void* data, int idx, const char** out) {
                    auto& vec = *(const std::vector<std::string>*)data;
                    *out = vec[idx].c_str(); return true;
                }, (void*)&gamepadModeNames, (int)gamepadModeNames.size()))
            {
                if (selectedGamepadIdx >= 0 && selectedGamepadIdx < (int)gamepadModeNames.size()) {
                    SwitchGamepadMode(GetActiveRobotModeName(), gamepadModeNames[selectedGamepadIdx]);
                }
            }
            ImGui::SameLine();
        }

        // ---- Comm Config 选择（来自 RobotCommManager）----
        if (m_CommMgr && m_CommMgr->GetItemCount() > 0)
        {
            ImGui::TextUnformatted("Comm Config:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140);
            if (m_CommIndex >= m_CommMgr->GetItemCount())
                m_CommIndex = 0;
            const char* preview = m_CommMgr->GetItemNameBuf(m_CommIndex);
            if (ImGui::BeginCombo("##CommConfigCombo", preview))
            {
                for (int i = 0; i < m_CommMgr->GetItemCount(); ++i)
                {
                    bool sel = (i == m_CommIndex);
                    if (ImGui::Selectable(m_CommMgr->GetItemNameBuf(i), sel))
                        m_CommIndex = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Separator();
    }

    // -------- interaction state --------
    static ed::NodeId contextNodeId = 0;
    static ed::LinkId contextLinkId = 0;
    static ed::PinId  contextPinId  = 0;

    // -------- Resizable three-panel layout --------
    float splitterW = 5.0f;
    float totalAvail = ImGui::GetContentRegionAvail().x;

    float canvasW = totalAvail - m_LeftSideWidth - m_RightSideWidth - splitterW * 2;
    if (canvasW < 100.0f) {
        canvasW = 100.0f;
        float excess = totalAvail - canvasW - splitterW * 2;
        float ratio = excess > 0 ? m_LeftSideWidth / (m_LeftSideWidth + m_RightSideWidth) : 0.5f;
        m_LeftSideWidth  = ratio * excess;
        m_RightSideWidth = (1.0f - ratio) * excess;
        if (m_LeftSideWidth < 80.0f) m_LeftSideWidth = 80.0f;
        if (m_RightSideWidth < 80.0f) m_RightSideWidth = 80.0f;
        canvasW = totalAvail - m_LeftSideWidth - m_RightSideWidth - splitterW * 2;
        if (canvasW < 100.0f) canvasW = 100.0f;
    }

    // Left sidebar
    DrawKeyValuesSidebar(m_LeftSideWidth, m_AnalogKeys);
    ImGui::SameLine(0, 0);
    ManualSplitter("##S1", &m_LeftSideWidth, 80.0f, splitterW);
    ImGui::SameLine(0, 0);

    // Navigate BEFORE ed::Begin so NavigateToContent processes on this frame.
    // Calling it inside Begin/End would defer to the next frame, causing a flicker.
    if (m_NavigateFrame > 0)
    {
        NavigateToOrigin();
        m_NavigateFrame = 0;
    }

    ed::Begin("##Canvas", ImVec2(canvasW, 0));

    // --- Draw nodes ---
    {
        std::shared_lock<std::shared_mutex> evalLock(GetEvalMutex());
        for (auto& node : m_Nodes)
    {
        ed::PushStyleColor(ed::StyleColor_NodeBg,        ImColor(60, 60, 60, 200));
        ed::PushStyleColor(ed::StyleColor_NodeBorder,    node.Color);
        ed::PushStyleColor(ed::StyleColor_PinRect,       ImColor(60, 180, 255, 150));
        ed::PushStyleColor(ed::StyleColor_PinRectBorder, ImColor(60, 180, 255, 150));

        ed::BeginNode(node.ID);
        {
            float nodeWidth = 140.0f;
            int totalPins = (int)node.Inputs.size() + (int)node.Outputs.size();
            if (totalPins >= 5) nodeWidth = 180.0f;
            else if (node.Type == NodeType::CustomOutput) nodeWidth = 190.0f;
            else if (node.Type == NodeType::KeySource) nodeWidth = 150.0f;
            else if (totalPins <= 4) nodeWidth = 120.0f;

            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)node.Color);
            ImGui::TextUnformatted(GetNodeTitle(node.Type));
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(nodeWidth, 2));
            auto* dl = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(cursor.x - ImGui::GetStyle().FramePadding.x, cursor.y),
                        ImVec2(cursor.x + nodeWidth - ImGui::GetStyle().FramePadding.x, cursor.y),
                        IM_COL32(100, 100, 100, 255), 1.0f);
            ImGui::Dummy(ImVec2(nodeWidth, 2));

            DrawNodeContents(node, m_AnalogKeys, m_OutputTargets);

            ImGui::Dummy(ImVec2(nodeWidth, 1));
        }
        ed::EndNode();

        ed::PopStyleColor(4);
    }

    // --- Draw links ---
    for (auto& link : m_Links)
        ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);
    } // end shared_lock

    // --- Click-to-connect ---
    // Left-click a pin to start, left-click another to connect, right-click to cancel.
    if (!m_IsRunning)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        ed::PinId hoveredId = ed::GetHoveredPin();
        bool leftReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

        if (m_LinkSourcePin.Get())
        {
            auto* srcPin = FindPin(m_LinkSourcePin);
            if (srcPin)
            {
                ImVec2 mouse = ImGui::GetMousePos();
                drawList->AddLine(m_LinkSourceMouse, mouse, IM_COL32(255,255,128,200), 2.0f);
                drawList->AddCircleFilled(m_LinkSourceMouse, 4.0f, IM_COL32(255,255,128,255));

                if (leftReleased)
                {
                    if (hoveredId.Get())
                    {
                        auto* tgt = FindPin(hoveredId);
                        if (tgt && tgt != srcPin && tgt->Node != srcPin->Node
                            && PinTypesCompatible(srcPin->Type, tgt->Type))
                        {
                            ed::PinId outId = m_LinkSourcePin, inId = hoveredId;
                            bool si=false, ti=false;
                            for (auto& n : m_Nodes) {
                                for (auto& p : n.Inputs) { if(p.ID==outId)si=true; if(p.ID==inId)ti=true; }
                            }
                            if (si && !ti) std::swap(outId, inId);
                            else if (si == ti) { m_LinkSourcePin = ed::PinId(0); goto cc_end; }
                            m_Links.emplace_back(GetNextId(), outId, inId);
                            m_Links.back().Color = GetIconColor(srcPin->Type);
                            SetModified(true);
                        }
                    }
                    m_LinkSourcePin = ed::PinId(0);
                }
                else if (rightClicked) { m_LinkSourcePin = ed::PinId(0); }
                cc_end:;
            }
            else { m_LinkSourcePin = ed::PinId(0); }
        }
        else if (hoveredId.Get() && leftReleased)
        {
            m_LinkSourcePin = hoveredId;
            m_LinkSourceMouse = ImGui::GetMousePos();
        }
    }

    if (ed::BeginDelete())
    {
        if (!m_IsRunning)
        {
            ed::NodeId nodeId = 0;
            while (ed::QueryDeletedNode(&nodeId))
            {
                if (ed::AcceptDeletedItem())
                {
                    m_Nodes.erase(
                        std::remove_if(m_Nodes.begin(), m_Nodes.end(),
                            [nodeId](auto& n) { return n.ID == nodeId; }),
                        m_Nodes.end());
                    SetModified(true);
                }
            }

            ed::LinkId linkId = 0;
            while (ed::QueryDeletedLink(&linkId))
            {
                if (ed::AcceptDeletedItem())
                {
                    m_Links.erase(
                        std::remove_if(m_Links.begin(), m_Links.end(),
                            [linkId](auto& l) { return l.ID == linkId; }),
                        m_Links.end());
                    SetModified(true);
                }
            }
        } // if (!m_IsRunning)
    }
    ed::EndDelete();

    bool showNewNodePopup = false;
    bool showNodePopup    = false;
    bool showPinPopup     = false;
    bool showLinkPopup    = false;
    {
        ed::Suspend();

        if (!m_IsRunning) {
            if (ed::ShowNodeContextMenu(&contextNodeId))
                showNodePopup = true;
            else if (ed::ShowPinContextMenu(&contextPinId))
                showPinPopup = true;
            else if (ed::ShowLinkContextMenu(&contextLinkId))
                showLinkPopup = true;
            else if (ed::ShowBackgroundContextMenu())
                showNewNodePopup = true;
        }

        ed::Resume();
    }

    ed::End();

    if (showNewNodePopup)
        ImGui::OpenPopup("Create New Node");
    if (showNodePopup)
        ImGui::OpenPopup("Node Context Menu");
    if (showPinPopup)
        ImGui::OpenPopup("Pin Context Menu");
    if (showLinkPopup)
        ImGui::OpenPopup("Link Context Menu");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    if (ImGui::BeginPopup("Node Context Menu"))
    {
        auto* node = FindNode(contextNodeId);
        if (node)
        {
            ImGui::Text("%s", GetNodeTitle(node->Type));
            ImGui::Separator();
        }
        if (ImGui::MenuItem("Delete Node"))
            ed::DeleteNode(contextNodeId);
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Pin Context Menu"))
    {
        if (ImGui::MenuItem("Break Links"))
        {
            int pid = (int)contextPinId.Get();
            m_Links.erase(
                std::remove_if(m_Links.begin(), m_Links.end(),
                    [pid](auto& l) { return (int)l.StartPinID.Get() == pid || (int)l.EndPinID.Get() == pid; }),
                m_Links.end());
            SetModified(true);
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Link Context Menu"))
    {
        if (ImGui::MenuItem("Delete Link"))
            ed::DeleteLink(contextLinkId);
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Create New Node"))
    {
        DrawAddNodeCategoryMenus([this](NodeType nt) { AddNode(nt); });
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (auto& cmd : dl->CmdBuffer)
        {
            if (cmd.UserCallback != nullptr && cmd.ElemCount == 0)
                cmd.UserCallback = nullptr;
        }
    }

    // Right sidebar — draw output values (clickable targets)
    ImGui::SameLine(0, 0);
    ManualSplitter("##S2", &m_RightSideWidth, 80.0f, splitterW, true);
    ImGui::SameLine(0, 0);

    {
        ImGui::BeginChild("##OVSide", ImVec2(m_RightSideWidth, 0), true);
        ImGui::TextUnformatted("Output Values");
        ImGui::Separator();

        EditorNode* activeOut = FindNode(m_ActiveOutputId);
        if (activeOut && activeOut->Type != NodeType::CustomOutput) activeOut = nullptr;

        if (m_OutputTargets.empty())
        {
            ImGui::TextDisabled("(no targets)");
        }
        else
        {
            std::map<std::string, float> kvSnapshot = GetKeyValuesSnapshot();
            // Reuse the outputs already computed by EvaluateForDisplay()
            // to avoid double-evaluating stateful nodes (Counter, etc.).
            std::map<std::string, float> outputs;
            {
                std::shared_lock<std::shared_mutex> lock(GetEvalMutex());
                outputs = m_LastOutputs;
            }

            for (const auto& target : m_OutputTargets)
            {
                float val = 0.0f;
                bool fromGraph = false;
                auto it = outputs.find(target.field_path);
                if (it != outputs.end()) {
                    val = it->second;
                    fromGraph = true;
                } else {
                    auto fit = m_FieldValues.find(target.field_path);
                    if (fit != m_FieldValues.end())
                        val = (float)fit->second;
                }

                bool isSelected = (activeOut && activeOut->OutputTarget == target.field_path);
                if (ImGui::Selectable(target.name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    if (activeOut) {
                        activeOut->OutputTarget = target.field_path;
                        SetModified(true);
                    }
                }
                ImGui::SameLine(m_RightSideWidth - 70);

                ImVec4 color(0.4f, 0.8f, 1.0f, 1.0f);
                if (fromGraph) color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                if (val < 0.0f) color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

                switch (target.encoding) {
                case DataEncoding::Bool:
                    ImGui::TextColored(color, "%s", val >= 0.5f ? "True" : "False");
                    break;
                case DataEncoding::Int8:
                case DataEncoding::Int16:
                case DataEncoding::Int32:
                case DataEncoding::Int64:
                    ImGui::TextColored(color, "%lld", (long long)val);
                    break;
                case DataEncoding::Uint8:
                case DataEncoding::Uint16:
                case DataEncoding::Uint32:
                case DataEncoding::Uint64:
                    ImGui::TextColored(color, "%llu", (unsigned long long)(val > 0.0f ? val : 0.0f));
                    break;
                case DataEncoding::Float32:
                case DataEncoding::Float64:
                default:
                    ImGui::TextColored(color, "%.3f", val);
                    break;
                }
            }
        }

        // ---- Global variables table ----
        ImGui::Separator();
        ImGui::Text("Variables    (%d)", (int)m_GlobalVars.size());

        int delIdx = -1, renameIdx = -1, addVar = 0;
        if (ImGui::BeginPopupContextWindow("##VarCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            ImGui::TextUnformatted("Variables");
            ImGui::Separator();
            if (ImGui::MenuItem("New Variable")) addVar = 1;
            ImGui::EndPopup();
        }

        if (addVar) {
            int n = 0; std::string dn;
            do { dn = "var" + std::to_string(n++); }
            while (FindGlobalByName(dn) >= 0);
            AddGlobal(dn, 0.0f, PinType::Float);
        }

        const int nvars = (int)m_GlobalVars.size();
        for (int i = 0; i < nvars; ++i) {
            ImGui::PushID(i + 10000);

            // Click to select variable for active GlobalRead/Write node
            EditorNode* activeGR = FindNode(m_ActiveGlobalReadId);
            bool isGR = (activeGR && (activeGR->Type == NodeType::GlobalRead || activeGR->Type == NodeType::GlobalWrite));
            bool isSelected = isGR && activeGR->GlobalVarId == m_GlobalVars[i].id;

            bool renaming = (m_RenamingGlobalIdx == i);
            if (renaming) {
                static char renameBuf[64];
                if (m_RenamingGlobalIdx != m_LastRenamingIdx) {
                    strncpy(renameBuf, m_GlobalVars[i].name.c_str(), sizeof(renameBuf)-1); renameBuf[sizeof(renameBuf)-1]=0;
                    m_LastRenamingIdx = i;
                }
                ImGui::SetNextItemWidth(m_RightSideWidth * 0.4f);
                if (ImGui::InputText("##Rn", renameBuf, sizeof(renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (renameBuf[0]) m_GlobalVars[i].name = renameBuf;
                    m_RenamingGlobalIdx = -1;
                    SetModified(true);
                }
            } else {
                // Compact type indicator (F/I/B) — click to cycle
                static const char* typeLabels[] = {"F", "I", "B"};
                static const ImVec4 typeCols[] = {
                    ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                    ImVec4(0.4f, 0.6f, 1.0f, 1.0f),
                    ImVec4(0.9f, 0.6f, 0.3f, 1.0f)
                };
                int curT = (int)m_GlobalVars[i].type;
                if (curT < 0 || curT > 2) curT = 0;

                ImGui::PushStyleColor(ImGuiCol_Text, typeCols[curT]);
                if (ImGui::Button(typeLabels[curT], ImVec2(20, 0))) {
                    m_GlobalVars[i].type = (PinType)((curT + 1) % 3);
                    SetModified(true);
                }
                ImGui::PopStyleColor();
                ImGui::SameLine();

                if (ImGui::Selectable(m_GlobalVars[i].name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (isGR) {
                        activeGR->GlobalVarId = m_GlobalVars[i].id;
                        SetModified(true);
                        m_ActiveGlobalReadId = 0;
                    }
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        renameIdx = i;
                }
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) renameIdx = i;
                    if (ImGui::MenuItem("Delete")) delIdx = i;
                    ImGui::EndPopup();
                }
            }

            ImGui::SameLine(m_RightSideWidth - 45);
            float v = m_GlobalVars[i].value;
            PinType vt = m_GlobalVars[i].type;
            if (vt == PinType::Bool) {
                if (v >= 0.5f)
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
            } else if (vt == PinType::Int) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%d", (int)v);
            } else {
                if (v >= 0.5f || v <= -0.5f) ImGui::TextColored(ImVec4(0.4f,1,0.4f,1), "%.2f", v);
                else ImGui::TextDisabled("%.2f", v);
            }

            ImGui::PopID();
        }
        if (delIdx >= 0) {
            if (delIdx < (int)m_GlobalVars.size())
                RemoveGlobal(m_GlobalVars[delIdx].id);
            if (m_RenamingGlobalIdx == delIdx) m_RenamingGlobalIdx = -1;
        }
        if (renameIdx >= 0) m_RenamingGlobalIdx = renameIdx;

        ImGui::EndChild();
    }

    // Restore editor context so it doesn't leak into other windows.
    ed::SetCurrentEditor(nullptr);
}

// ============================================================================
// WriteOutputToActuator
// ============================================================================
void WriteOutputToActuator(const std::string& target, float val, ActuatorConfig& data)
{
    auto segs = [](const std::string& s, char delim) -> std::vector<std::string> {
        std::vector<std::string> parts;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) parts.push_back(item);
        return parts;
    }(target, '.');

    if (segs.empty()) return;

    // --- motion ---
    if (segs[0] == "motion" && segs.size() == 2) {
        if      (segs[1] == "x")  data.motion.x  = val;
        else if (segs[1] == "y")  data.motion.y  = val;
        else if (segs[1] == "z")  data.motion.z  = val;
        else if (segs[1] == "rx") data.motion.rx = val;
        else if (segs[1] == "ry") data.motion.ry = val;
        else if (segs[1] == "rz") data.motion.rz = val;
        return;
    }

    // --- brushlessmotor ---
    if (segs[0] == "brushlessmotor" && segs.size() >= 3) {
        const std::string& motorName = segs[1];
        for (auto& motor : data.brushlessmotor) {
            if (motor.name != motorName) continue;
            if (segs[2] == "target_speed") { motor.target_speed = val; return; }
            if (segs[2] == "curve" && segs.size() == 4) {
                if      (segs[3] == "np_mid") motor.curve.np_mid = val;
                else if (segs[3] == "np_ini") motor.curve.np_ini = val;
                else if (segs[3] == "pp_ini") motor.curve.pp_ini = val;
                else if (segs[3] == "pp_mid") motor.curve.pp_mid = val;
                else if (segs[3] == "nt_end") motor.curve.nt_end = val;
                else if (segs[3] == "nt_mid") motor.curve.nt_mid = val;
                else if (segs[3] == "pt_mid") motor.curve.pt_mid = val;
                else if (segs[3] == "pt_end") motor.curve.pt_end = val;
                return;
            }
            return;
        }
        return;
    }

    // --- servo ---
    if (segs[0] == "servo" && segs.size() == 3) {
        const std::string& servoName = segs[1];
        for (auto& sv : data.servo) {
            if (sv.name != servoName) continue;
            if (segs[2] == "angle") { sv.angle = val; return; }
            return;
        }
        return;
    }
}

// ============================================================================
// BuildOutputTargetsFromProtocol
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
