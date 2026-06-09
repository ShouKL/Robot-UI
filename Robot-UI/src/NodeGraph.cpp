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
#include <queue>
#include <unordered_map>
#include <sstream>
#include <chrono>

namespace ed = ax::NodeEditor;

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
}

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
    if (it != m_GraphMap.end()) {
        LoadGraphData(it->second);
    } else {
        Clear();
    }
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

    // Compute real delta time
    auto now = std::chrono::steady_clock::now();
    float dt = 0.0f;
    if (m_LastEvalTime.time_since_epoch().count() > 0) {
        dt = std::chrono::duration<float>(now - m_LastEvalTime).count();
        if (dt > 1.0f) dt = 0.0f;  // clamp large gaps (first frame / resume)
    }
    m_LastEvalTime = now;

    auto order = TopoSortNodes(m_Nodes, m_Links);
    std::unordered_map<int, float> pinVals;

    for (int nid : order)
    {
        EditorNode* node = nullptr;
        for (auto& n : m_Nodes)
            if ((int)n.ID.Get() == nid) { node = &n; break; }
        if (!node) continue;

        float out = ComputeNodeOutput(*node, keyValues, pinVals, dt);

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

    // Compute real delta time
    auto now = std::chrono::steady_clock::now();
    float dt = 0.0f;
    if (m_LastEvalTime.time_since_epoch().count() > 0) {
        dt = std::chrono::duration<float>(now - m_LastEvalTime).count();
        if (dt > 1.0f) dt = 0.0f;
    }
    m_LastEvalTime = now;

    auto order = TopoSortNodes(m_Nodes, m_Links);
    std::unordered_map<int, float> pinVals;

    for (int nid : order)
    {
        EditorNode* node = nullptr;
        for (auto& n : m_Nodes)
            if ((int)n.ID.Get() == nid) { node = &n; break; }
        if (!node) continue;

        float out = ComputeNodeOutput(*node, keyValues, pinVals, dt);

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
}

// ============================================================================
// Evaluate — thread-safe read-only evaluation
// ============================================================================
std::map<std::string, float> NodeGraph::Evaluate(const std::map<std::string, float>& keyValues)
{
    std::shared_lock<std::shared_mutex> lock(m_EvalMutex);
    return EvaluateCompute(keyValues);
}

// ============================================================================
// EvaluateIntoActuator
// ============================================================================
void NodeGraph::EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorConfig& data)
{
    std::shared_lock<std::shared_mutex> lock(m_EvalMutex);
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
        out << YAML::Key << "param"         << YAML::Value << YAML::BeginSeq;
        for (int i = 0; i < 8; ++i) out << n.Param[i];
        out << YAML::EndSeq;
        out << YAML::Key << "state_f" << YAML::Value << YAML::BeginSeq;
        for (int i = 0; i < 4; ++i) out << n.StateF[i];
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
                NodeType nt = (NodeType)yn["type"].as<int>();
                EditorNode* node = SpawnNode(nt);
                if (!node) continue;

                int savedId = yn["id"].as<int>();
                node->ID = ax::NodeEditor::NodeId(savedId);

                node->Value        = yn["value"]        ? yn["value"].as<float>()        : 0.0f;
                node->KeyName      = yn["key_name"]     ? yn["key_name"].as<std::string>() : "";
                node->OpMode       = yn["op_mode"]      ? yn["op_mode"].as<int>()        : 0;
                node->OutputTarget = yn["output_target"]? yn["output_target"].as<std::string>() : "";

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
        out << YAML::Key << "param"         << YAML::Value << YAML::BeginSeq;
        for (int i = 0; i < 8; ++i) out << n.Param[i];
        out << YAML::EndSeq;
        out << YAML::Key << "state_f" << YAML::Value << YAML::BeginSeq;
        for (int i = 0; i < 4; ++i) out << n.StateF[i];
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
                NodeType nt = (NodeType)yn["type"].as<int>();
                EditorNode* node = SpawnNode(nt);
                if (!node) continue;

                int savedId = yn["id"].as<int>();
                node->ID = ed::NodeId(savedId);

                node->Value        = yn["value"]        ? yn["value"].as<float>()        : 0.0f;
                node->KeyName      = yn["key_name"]     ? yn["key_name"].as<std::string>() : "";
                node->OpMode       = yn["op_mode"]      ? yn["op_mode"].as<int>()        : 0;
                node->OutputTarget = yn["output_target"]? yn["output_target"].as<std::string>() : "";

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
            ImGui::SameLine(0, 12); ImGui::TextDisabled("%.3f", node.Value);
            ed::EndPin();
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
    AddNodeAt(type, ImVec2(0, 0), false);
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
            // Adjust width by category / pin count
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

    if (ed::BeginCreate(ImColor(255, 255, 255), 2.0f))
    {
        if (!m_IsRunning)
        {
        auto showLabel = [](const char* label, ImColor color)
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
            auto size    = ImGui::CalcTextSize(label);
            auto padding = ImGui::GetStyle().FramePadding;
            auto spacing = ImGui::GetStyle().ItemSpacing;
            ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacing.x, -spacing.y));
            auto rectMin = ImGui::GetCursorScreenPos() - padding;
            auto rectMax = ImGui::GetCursorScreenPos() + size + padding;
            auto drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
            ImGui::TextUnformatted(label);
        };

        ed::PinId startPinId = 0, endPinId = 0;
        if (ed::QueryNewLink(&startPinId, &endPinId))
        {
            auto startPin = FindPin(startPinId);
            auto endPin   = FindPin(endPinId);

            if (startPin && endPin)
            {
                if (startPin == endPin)
                {
                    showLabel("x Cannot connect to self", ImColor(45, 32, 32, 180));
                    ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                }
                else if (startPin->Node == endPin->Node)
                {
                    showLabel("x Same node", ImColor(45, 32, 32, 180));
                    ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                }
                else if (startPin->Type != endPin->Type)
                {
                    showLabel("x Incompatible Type", ImColor(45, 32, 32, 180));
                    ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                }
                else
                {
                    auto* outPin = startPin;
                    auto* inPin  = endPin;

                    bool startIsInput  = false;
                    bool endIsInput    = false;
                    for (auto& n : m_Nodes)
                    {
                        for (auto& p : n.Inputs)  { if (p.ID == startPinId) startIsInput = true; if (p.ID == endPinId) endIsInput = true; }
                        for (auto& p : n.Outputs) { if (p.ID == startPinId) startIsInput = false; if (p.ID == endPinId) endIsInput = false; }
                    }

                    if (startIsInput && !endIsInput)
                    {
                        std::swap(startPin, endPin);
                        std::swap(startPinId, endPinId);
                    }
                    else if (startIsInput == endIsInput)
                    {
                        showLabel("x Must connect Output → Input", ImColor(45, 32, 32, 180));
                        ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                        goto end_link_query;
                    }

                    showLabel("+ Create Link", ImColor(32, 45, 32, 180));
                    if (ed::AcceptNewItem(ImColor(128, 255, 128), 4.0f))
                    {
                        m_Links.emplace_back(GetNextId(), startPinId, endPinId);
                        m_Links.back().Color = GetIconColor(startPin->Type);
                        SetModified(true);
                    }
                }
            }
            end_link_query:;
        }

        ed::PinId pinId = 0;
        if (ed::QueryNewNode(&pinId))
        {
            if (ed::AcceptNewItem())
            {
                ed::Suspend();
                ImGui::OpenPopup("Create New Node");
                ed::Resume();
            }
        }
        } // if (!m_IsRunning)
    }
    ed::EndCreate();

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
            std::map<std::string, float> outputs;
            if (m_IsRunning)
                outputs = Evaluate(kvSnapshot);

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
