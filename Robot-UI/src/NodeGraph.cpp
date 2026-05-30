#define IMGUI_DEFINE_MATH_OPERATORS
#include "NodeGraph.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <sstream>

// ============================================================================
// Free functions — shared helpers
// ============================================================================
ImColor GetIconColor(PinType type)
{
    switch (type)
    {
    default:
    case PinType::Float: return ImColor(147, 226,  74);
    }
}

const char* GetNodeTitle(NodeType type)
{
    switch (type)
    {
    case NodeType::KeySource:    return "Key Source";
    case NodeType::ConstValue:   return "Const";
    case NodeType::Add:          return "Add";
    case NodeType::Subtract:     return "Subtract";
    case NodeType::Multiply:     return "Multiply";
    case NodeType::Divide:       return "Divide";
    case NodeType::Scale:        return "Scale";
    case NodeType::Clamp:        return "Clamp";
    case NodeType::Compare:      return "Compare";
    case NodeType::And:          return "AND";
    case NodeType::Or:           return "OR";
    case NodeType::Not:          return "NOT";
    case NodeType::If:           return "IF";
    case NodeType::While:        return "WHILE";
    case NodeType::CustomOutput: return "Output";
    default:                     return "???";
    }
}

ImColor GetNodeHeaderColor(NodeType type)
{
    switch (type)
    {
    case NodeType::KeySource:    return ImColor( 66, 150,  66);
    case NodeType::ConstValue:   return ImColor( 66, 120, 180);
    case NodeType::Add:          return ImColor(180, 120,  66);
    case NodeType::Subtract:     return ImColor(180, 100,  50);
    case NodeType::Multiply:     return ImColor(180,  66, 120);
    case NodeType::Divide:       return ImColor(180,  50, 100);
    case NodeType::Scale:        return ImColor(120,  66, 180);
    case NodeType::Clamp:        return ImColor(180, 180,  66);
    case NodeType::Compare:      return ImColor( 66, 180, 180);
    case NodeType::And:          return ImColor(100, 140, 200);
    case NodeType::Or:           return ImColor(200, 140, 100);
    case NodeType::Not:          return ImColor(200, 100, 100);
    case NodeType::If:           return ImColor(100, 180, 255);
    case NodeType::While:        return ImColor(255, 180, 100);
    case NodeType::CustomOutput: return ImColor( 66, 180, 120);
    default:                     return ImColor(128, 128, 128);
    }
}

// ============================================================================
// NodeGraph — Constructor
// ============================================================================
NodeGraph::NodeGraph()
{
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
// Spawn*Node factories
// ============================================================================
EditorNode* NodeGraph::SpawnKeySource()
{
    m_Nodes.emplace_back(GetNextId(), "Key Source", NodeType::KeySource, GetNodeHeaderColor(NodeType::KeySource));
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Value", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnConstValue()
{
    m_Nodes.emplace_back(GetNextId(), "Const", NodeType::ConstValue, GetNodeHeaderColor(NodeType::ConstValue));
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Value", PinType::Float);
    m_Nodes.back().Value = 0.0f;
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnAdd()
{
    m_Nodes.emplace_back(GetNextId(), "Add", NodeType::Add, GetNodeHeaderColor(NodeType::Add));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A+B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnMultiply()
{
    m_Nodes.emplace_back(GetNextId(), "Multiply", NodeType::Multiply, GetNodeHeaderColor(NodeType::Multiply));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A*B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnScale()
{
    m_Nodes.emplace_back(GetNextId(), "Scale", NodeType::Scale, GetNodeHeaderColor(NodeType::Scale));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "In", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Out", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnClamp()
{
    m_Nodes.emplace_back(GetNextId(), "Clamp", NodeType::Clamp, GetNodeHeaderColor(NodeType::Clamp));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "In", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Out", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnSubtract()
{
    m_Nodes.emplace_back(GetNextId(), "Subtract", NodeType::Subtract, GetNodeHeaderColor(NodeType::Subtract));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A-B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnDivide()
{
    m_Nodes.emplace_back(GetNextId(), "Divide", NodeType::Divide, GetNodeHeaderColor(NodeType::Divide));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A/B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnCompare()
{
    m_Nodes.emplace_back(GetNextId(), "Compare", NodeType::Compare, GetNodeHeaderColor(NodeType::Compare));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Bool", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnAnd()
{
    m_Nodes.emplace_back(GetNextId(), "AND", NodeType::And, GetNodeHeaderColor(NodeType::And));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A&B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnOr()
{
    m_Nodes.emplace_back(GetNextId(), "OR", NodeType::Or, GetNodeHeaderColor(NodeType::Or));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A|B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnNot()
{
    m_Nodes.emplace_back(GetNextId(), "NOT", NodeType::Not, GetNodeHeaderColor(NodeType::Not));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "!A", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnIf()
{
    m_Nodes.emplace_back(GetNextId(), "IF", NodeType::If, GetNodeHeaderColor(NodeType::If));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "Cond", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "True", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "False", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Out", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnWhile()
{
    m_Nodes.emplace_back(GetNextId(), "WHILE", NodeType::While, GetNodeHeaderColor(NodeType::While));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "Cond", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "Val", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Out", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeGraph::SpawnOutput()
{
    m_Nodes.emplace_back(GetNextId(), "Output", NodeType::CustomOutput, GetNodeHeaderColor(NodeType::CustomOutput));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "Value", PinType::Float);
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

EditorLink* NodeGraph::FindLink(ax::NodeEditor::LinkId id)
{
    for (auto& link : m_Links)
        if (link.ID == id)
            return &link;
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
// ComputeNodeOutput — evaluate a single node
// ============================================================================
static float ComputeNodeOutput(const EditorNode& node,
    const std::map<std::string, float>& keyValues,
    const std::unordered_map<int, float>& pinVals)
{
    switch (node.Type)
    {
    case NodeType::KeySource:
        return (keyValues.count(node.KeyName) > 0) ? keyValues.at(node.KeyName) : 0.0f;
    case NodeType::ConstValue:
        return node.Value;
    case NodeType::Add:
    {
        float a = 0, b = 0;
        for (auto& pin : node.Inputs) {
            auto it = pinVals.find((int)pin.ID.Get());
            if (it != pinVals.end()) (pin.Name == "A" ? a : b) = it->second;
        }
        return a + b;
    }
    case NodeType::Multiply:
    {
        float a = 0, b = 0;
        for (auto& pin : node.Inputs) {
            auto it = pinVals.find((int)pin.ID.Get());
            if (it != pinVals.end()) (pin.Name == "A" ? a : b) = it->second;
        }
        return a * b;
    }
    case NodeType::Subtract:
    {
        float a = 0, b = 0;
        for (auto& pin : node.Inputs) {
            auto it = pinVals.find((int)pin.ID.Get());
            if (it != pinVals.end()) (pin.Name == "A" ? a : b) = it->second;
        }
        return a - b;
    }
    case NodeType::Divide:
    {
        float a = 0, b = 0;
        for (auto& pin : node.Inputs) {
            auto it = pinVals.find((int)pin.ID.Get());
            if (it != pinVals.end()) (pin.Name == "A" ? a : b) = it->second;
        }
        return (b != 0.0f) ? (a / b) : 0.0f;
    }
    case NodeType::Scale:
    {
        float in = 0;
        if (!node.Inputs.empty()) {
            auto it = pinVals.find((int)node.Inputs[0].ID.Get());
            if (it != pinVals.end()) in = it->second;
        }
        return in * node.Factor;
    }
    case NodeType::Clamp:
    {
        float in = 0;
        if (!node.Inputs.empty()) {
            auto it = pinVals.find((int)node.Inputs[0].ID.Get());
            if (it != pinVals.end()) in = it->second;
        }
        return std::clamp(in, node.MinVal, node.MaxVal);
    }
    case NodeType::Compare:
    {
        float a = 0, b = 0;
        for (auto& pin : node.Inputs) {
            auto it = pinVals.find((int)pin.ID.Get());
            if (it != pinVals.end()) (pin.Name == "A" ? a : b) = it->second;
        }
        switch (node.OpMode) {
        case 0: return (a > b) ? 1.0f : 0.0f;
        case 1: return (a >= b) ? 1.0f : 0.0f;
        case 2: return (a <= b) ? 1.0f : 0.0f;
        case 3: return (a < b) ? 1.0f : 0.0f;
        case 4: return (a == b) ? 1.0f : 0.0f;
        case 5: return (a != b) ? 1.0f : 0.0f;
        default: return (a > b) ? 1.0f : 0.0f;
        }
    }
    case NodeType::And:
    {
        float a = 0, b = 0;
        for (auto& pin : node.Inputs) {
            auto it = pinVals.find((int)pin.ID.Get());
            if (it != pinVals.end()) (pin.Name == "A" ? a : b) = it->second;
        }
        return ((a >= 0.5f) && (b >= 0.5f)) ? 1.0f : 0.0f;
    }
    case NodeType::Or:
    {
        float a = 0, b = 0;
        for (auto& pin : node.Inputs) {
            auto it = pinVals.find((int)pin.ID.Get());
            if (it != pinVals.end()) (pin.Name == "A" ? a : b) = it->second;
        }
        return ((a >= 0.5f) || (b >= 0.5f)) ? 1.0f : 0.0f;
    }
    case NodeType::Not:
    {
        float in = 0;
        if (!node.Inputs.empty()) {
            auto it = pinVals.find((int)node.Inputs[0].ID.Get());
            if (it != pinVals.end()) in = it->second;
        }
        return (in >= 0.5f) ? 0.0f : 1.0f;
    }
    case NodeType::If:
    {
        float cond = 0, tv = 0, fv = 0;
        for (auto& pin : node.Inputs) {
            auto it = pinVals.find((int)pin.ID.Get());
            if (it != pinVals.end()) {
                if      (pin.Name == "Cond")  cond = it->second;
                else if (pin.Name == "True")  tv   = it->second;
                else if (pin.Name == "False") fv   = it->second;
            }
        }
        return (cond >= 0.5f) ? tv : fv;
    }
    case NodeType::While:
    {
        float cond = 0, val = 0;
        for (auto& pin : node.Inputs) {
            auto it = pinVals.find((int)pin.ID.Get());
            if (it != pinVals.end()) {
                if      (pin.Name == "Cond") cond = it->second;
                else if (pin.Name == "Val")  val  = it->second;
            }
        }
        return (cond >= 0.5f) ? val : 0.0f;
    }
    case NodeType::CustomOutput:
    {
        float in = 0;
        if (!node.Inputs.empty()) {
            auto it = pinVals.find((int)node.Inputs[0].ID.Get());
            if (it != pinVals.end()) in = it->second;
        }
        return in;
    }
    }
    return 0.0f;
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
// EvaluateCompute — pure compute, const
// ============================================================================
std::map<std::string, float> NodeGraph::EvaluateCompute(const std::map<std::string, float>& keyValues) const
{
    std::map<std::string, float> outputs;
    if (m_Nodes.empty()) return outputs;

    auto order = TopoSortNodes(m_Nodes, m_Links);
    std::unordered_map<int, float> pinVals;

    for (int nid : order)
    {
        const EditorNode* node = nullptr;
        for (const auto& n : m_Nodes)
            if ((int)n.ID.Get() == nid) { node = &n; break; }
        if (!node) continue;

        float out = ComputeNodeOutput(*node, keyValues, pinVals);

        if (node->Type == NodeType::CustomOutput && !node->OutputTarget.empty())
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

    auto order = TopoSortNodes(m_Nodes, m_Links);
    std::unordered_map<int, float> pinVals;

    for (int nid : order)
    {
        EditorNode* node = nullptr;
        for (auto& n : m_Nodes)
            if ((int)n.ID.Get() == nid) { node = &n; break; }
        if (!node) continue;

        float out = ComputeNodeOutput(*node, keyValues, pinVals);

        // Update sidebar display fields
        if (node->Type == NodeType::Add || node->Type == NodeType::Multiply
            || node->Type == NodeType::Subtract || node->Type == NodeType::Divide
            || node->Type == NodeType::And || node->Type == NodeType::Or
            || node->Type == NodeType::Compare || node->Type == NodeType::If
            || node->Type == NodeType::While)
        {
            float a = 0, b = 0;
            for (auto& pin : node->Inputs) {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end()) (pin.Name == "A" ? a : b) = it->second;
            }
            node->InputA = a; node->InputB = b;
        }
        node->Value = out;

        if (node->Type == NodeType::CustomOutput && !node->OutputTarget.empty())
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
        out << YAML::Key << "factor"        << YAML::Value << n.Factor;
        out << YAML::Key << "min_val"       << YAML::Value << n.MinVal;
        out << YAML::Key << "max_val"       << YAML::Value << n.MaxVal;
        out << YAML::Key << "digital"       << YAML::Value << n.Digital;
        out << YAML::Key << "op_mode"       << YAML::Value << n.OpMode;
        out << YAML::Key << "output_target" << YAML::Value << n.OutputTarget;
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
                EditorNode* node = nullptr;

                switch (nt)
                {
                case NodeType::KeySource:    node = SpawnKeySource();    break;
                case NodeType::ConstValue:   node = SpawnConstValue();   break;
                case NodeType::Add:          node = SpawnAdd();          break;
                case NodeType::Subtract:     node = SpawnSubtract();     break;
                case NodeType::Multiply:     node = SpawnMultiply();     break;
                case NodeType::Divide:       node = SpawnDivide();       break;
                case NodeType::Scale:        node = SpawnScale();        break;
                case NodeType::Clamp:        node = SpawnClamp();        break;
                case NodeType::Compare:      node = SpawnCompare();      break;
                case NodeType::And:          node = SpawnAnd();          break;
                case NodeType::Or:           node = SpawnOr();           break;
                case NodeType::Not:          node = SpawnNot();          break;
                case NodeType::If:           node = SpawnIf();           break;
                case NodeType::While:        node = SpawnWhile();        break;
                case NodeType::CustomOutput: node = SpawnOutput();       break;
                }
                if (!node) continue;

                int savedId = yn["id"].as<int>();
                node->ID = ax::NodeEditor::NodeId(savedId);

                node->Value        = yn["value"]        ? yn["value"].as<float>()        : 0.0f;
                node->KeyName      = yn["key_name"]     ? yn["key_name"].as<std::string>() : "";
                node->Factor       = yn["factor"]       ? yn["factor"].as<float>()       : 1.0f;
                node->MinVal       = yn["min_val"]      ? yn["min_val"].as<float>()      : 0.0f;
                node->MaxVal       = yn["max_val"]      ? yn["max_val"].as<float>()      : 1.0f;
                node->Digital      = yn["digital"]      ? yn["digital"].as<bool>()       : false;
                node->OpMode       = yn["op_mode"]      ? yn["op_mode"].as<int>()        : 0;
                node->OutputTarget = yn["output_target"]? yn["output_target"].as<std::string>() : "";

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
        int id = std::stoi(segs[1]);
        auto& motor = data.brushlessmotor[id];
        motor.id = id;
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

    // --- servo ---
    if (segs[0] == "servo" && segs.size() == 3) {
        int id = std::stoi(segs[1]);
        auto& s = data.servo[id];
        s.id = id;
        if (segs[2] == "angle") { s.angle = val; return; }
        return;
    }
}

// ============================================================================
// BuildOutputTargetsFromProtocol
// ============================================================================
std::vector<OutputTargetInfo> BuildOutputTargetsFromProtocol(const ProtocolSendConfig& cfg, const ActuatorConfig& actuator)
{
    std::vector<OutputTargetInfo> targets;

    auto components = GetSendComponents(actuator, SensorConfig{});

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

    return targets;
}
