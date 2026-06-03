#define IMGUI_DEFINE_MATH_OPERATORS
#include "NodeGraph.h"
#include <yaml-cpp/yaml.h>
#include <imgui_internal.h>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <sstream>

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

void DrawPinIcon(const EditorPin& pin, bool connected, int alpha)
{
    ImColor color = GetIconColor(pin.Type);
    color.Value.w = alpha / 255.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float iconSize = 24.0f;
    float half = iconSize * 0.5f;
    ImVec2 center(pos.x + half, pos.y + half);

    ImU32 col = ImColor(color);
    ImU32 bg  = ImColor(32, 32, 32, alpha);

    if (connected)
    {
        drawList->AddCircleFilled(center, half * 0.5f, col, 12);
        drawList->AddCircle(center, half * 0.5f, col, 12, 2.0f);
    }
    else
    {
        drawList->AddCircleFilled(center, half * 0.5f, bg, 12);
        drawList->AddCircle(center, half * 0.5f, col, 12, 1.5f);
    }

    ImGui::Dummy(ImVec2(iconSize, iconSize));
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
// SpawnNode — convenience factory dispatching on NodeType
// ============================================================================
EditorNode* NodeGraph::SpawnNode(NodeType type)
{
    switch (type)
    {
    case NodeType::KeySource:    return SpawnKeySource();
    case NodeType::ConstValue:   return SpawnConstValue();
    case NodeType::Add:          return SpawnAdd();
    case NodeType::Subtract:     return SpawnSubtract();
    case NodeType::Multiply:     return SpawnMultiply();
    case NodeType::Divide:       return SpawnDivide();
    case NodeType::Scale:        return SpawnScale();
    case NodeType::Clamp:        return SpawnClamp();
    case NodeType::Compare:      return SpawnCompare();
    case NodeType::And:          return SpawnAnd();
    case NodeType::Or:           return SpawnOr();
    case NodeType::Not:          return SpawnNot();
    case NodeType::If:           return SpawnIf();
    case NodeType::While:        return SpawnWhile();
    case NodeType::CustomOutput: return SpawnOutput();
    }
    return nullptr;
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

void NodeGraph::SetRobotModeNames(const std::vector<std::string>& names, int activeIdx)
{
    bool sizeChanged = (names.size() != m_RobotModeNames.size());
    m_RobotModeNames = names;
    if (sizeChanged || m_RobotModeNames.empty())
        m_SelectedRobotModeIdx = (activeIdx >= 0 && activeIdx < (int)names.size()) ? activeIdx : 0;
}

void NodeGraph::SetGamepadModeNames(const std::vector<std::string>& names)
{
    m_GamepadModeNames = names;
    for (int i = 0; i < (int)names.size(); ++i) {
        if (names[i] == m_ActiveGamepadModeName) {
            m_SelectedGamepadModeIdx = i;
            return;
        }
    }
    m_SelectedGamepadModeIdx = 0;
}

void NodeGraph::SetCurrentModePair(const std::string& robotMode, const std::string& gamepadMode)
{
    m_ActiveRobotModeName = robotMode;
    m_ActiveGamepadModeName = gamepadMode;
    for (int i = 0; i < (int)m_RobotModeNames.size(); ++i) {
        if (m_RobotModeNames[i] == robotMode) {
            m_SelectedRobotModeIdx = i;
            break;
        }
    }
    SwitchGraph(robotMode, gamepadMode);
    m_NavigateFrame = 1;
}

void NodeGraph::SwitchRobotMode(const std::string& newRobotMode, const std::string& curGamepadMode)
{
    for (int i = 0; i < (int)m_RobotModeNames.size(); ++i) {
        if (m_RobotModeNames[i] == newRobotMode) {
            m_SelectedRobotModeIdx = i;
            break;
        }
    }
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
                EditorNode* node = SpawnNode(nt);
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
        out << YAML::Key << "factor"        << YAML::Value << n.Factor;
        out << YAML::Key << "min_val"       << YAML::Value << n.MinVal;
        out << YAML::Key << "max_val"       << YAML::Value << n.MaxVal;
        out << YAML::Key << "digital"       << YAML::Value << n.Digital;
        out << YAML::Key << "op_mode"       << YAML::Value << n.OpMode;
        out << YAML::Key << "output_target" << YAML::Value << n.OutputTarget;

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
                node->Factor       = yn["factor"]       ? yn["factor"].as<float>()       : 1.0f;
                node->MinVal       = yn["min_val"]      ? yn["min_val"].as<float>()      : 0.0f;
                node->MaxVal       = yn["max_val"]      ? yn["max_val"].as<float>()      : 1.0f;
                node->Digital      = yn["digital"]      ? yn["digital"].as<bool>()       : false;
                node->OpMode       = yn["op_mode"]      ? yn["op_mode"].as<int>()        : 0;
                node->OutputTarget = yn["output_target"]? yn["output_target"].as<std::string>() : "";

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

        m_Modified = false;
        return true;
    }
    catch (const std::exception&) { return false; }
}

static void ShowBool(float v)
{
    if (v >= 0.5f)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
    else
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
}

void NodeGraph::DrawNodeContents(EditorNode& node,
                                  const std::set<std::string>& analogKeys,
                                  const std::vector<OutputTargetInfo>& outputTargets,
                                  ax::NodeEditor::NodeId& keySourcePopupNodeId,
                                  bool& keySourcePopupRequested,
                                  ax::NodeEditor::NodeId& outputComboNodeId,
                                  bool& outputComboRequested)
{
    switch (node.Type)
    {
    case NodeType::KeySource:
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        std::string btnLabel = node.KeyName.empty() ? "(select key)" : node.KeyName;
        if (ImGui::Button(btnLabel.c_str(), ImVec2(105, 0)))
        {
            keySourcePopupNodeId = node.ID;
            keySourcePopupRequested = true;
        }
        ImGui::PopStyleVar();
        ImGui::SameLine();
        bool isAnalog = (analogKeys.count(node.KeyName) > 0);
        if (!isAnalog)
        {
            if (node.Value >= 0.5f)
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
        }
        else
        {
            ImVec4 c(0.4f, 0.8f, 1.0f, 1.0f);
            if (node.Value < 0.0f) c = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            ImGui::TextColored(c, "%.3f", node.Value);
        }

        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ed::EndPin();
        }
        break;
    }

    case NodeType::ConstValue:
    {
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if (ImGui::DragFloat("##Val", &node.Value, 0.01f, -100.0f, 100.0f, "%.3f"))
                SetModified(true);
            ed::EndPin();
        }
        break;
    }

    case NodeType::Add:
    case NodeType::Multiply:
    {
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            float v = (pin.Name == "A") ? node.InputA : node.InputB;
            ImGui::TextDisabled("%.2f", v);
            ed::EndPin();
        }
        {
            float sepWidth = 130.0f;
            ImGui::Dummy(ImVec2(sepWidth, 2));
            auto* dl = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(cursor.x, cursor.y),
                        ImVec2(cursor.x + sepWidth, cursor.y),
                        IM_COL32(100, 100, 100, 255), 1.0f);
            ImGui::Dummy(ImVec2(sepWidth, 2));
        }
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            ImGui::TextDisabled("%.3f", node.Value);
            ed::EndPin();
        }
        break;
    }

    case NodeType::Subtract:
    case NodeType::Divide:
    {
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            float v = (pin.Name == "A") ? node.InputA : node.InputB;
            ImGui::TextDisabled("%.2f", v);
            ed::EndPin();
        }
        {
            float sepWidth = 130.0f;
            ImGui::Dummy(ImVec2(sepWidth, 2));
            auto* dl = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(cursor.x, cursor.y),
                        ImVec2(cursor.x + sepWidth, cursor.y),
                        IM_COL32(100, 100, 100, 255), 1.0f);
            ImGui::Dummy(ImVec2(sepWidth, 2));
        }
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            ImGui::TextDisabled("%.3f", node.Value);
            ed::EndPin();
        }
        break;
    }

    case NodeType::Scale:
    {
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("In");
            ed::EndPin();
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::SliderFloat("##Factor", &node.Factor, -10.0f, 10.0f, "x %.2f"))
            SetModified(true);
        ImGui::Indent(10);
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("Out");
            ed::EndPin();
        }
        ImGui::Unindent(10);
        break;
    }

    case NodeType::Clamp:
    {
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("In");
            ed::EndPin();
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::DragFloat("##Min", &node.MinVal, 0.01f, -10.0f, 10.0f, "Min %.2f"))
            SetModified(true);
        ImGui::SetNextItemWidth(100);
        if (ImGui::DragFloat("##Max", &node.MaxVal, 0.01f, -10.0f, 10.0f, "Max %.2f"))
            SetModified(true);
        if (node.MinVal > node.MaxVal) node.MinVal = node.MaxVal;
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("Out");
            ed::EndPin();
        }
        break;
    }

    case NodeType::Compare:
    {
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            float v = (pin.Name == "A") ? node.InputA : node.InputB;
            ImGui::TextDisabled("%.2f", v);
            ed::EndPin();
        }
        {
            const char* items[] = { "A>B", "A>=B", "A<=B", "A<B", "A==B", "A!=B" };
            ImGui::SetNextItemWidth(110);
            if (ImGui::Combo("##Op", &node.OpMode, items, IM_ARRAYSIZE(items)))
                SetModified(true);
        }
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            ShowBool(node.Value);
            ed::EndPin();
        }
        break;
    }

    case NodeType::And:
    case NodeType::Or:
    {
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            float v = (pin.Name == "A") ? node.InputA : node.InputB;
            ShowBool(v);
            ed::EndPin();
        }
        {
            float sepWidth = 130.0f;
            ImGui::Dummy(ImVec2(sepWidth, 2));
            auto* dl = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(cursor.x, cursor.y),
                        ImVec2(cursor.x + sepWidth, cursor.y),
                        IM_COL32(100, 100, 100, 255), 1.0f);
            ImGui::Dummy(ImVec2(sepWidth, 2));
        }
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            ShowBool(node.Value);
            ed::EndPin();
        }
        break;
    }

    case NodeType::Not:
    {
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            ShowBool(node.InputA);
            ed::EndPin();
        }
        float sepWidth = 130.0f;
        ImGui::Dummy(ImVec2(sepWidth, 2));
        auto* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(cursor.x, cursor.y),
                    ImVec2(cursor.x + sepWidth, cursor.y),
                    IM_COL32(100, 100, 100, 255), 1.0f);
        ImGui::Dummy(ImVec2(sepWidth, 2));
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            ShowBool(node.Value);
            ed::EndPin();
        }
        break;
    }

    case NodeType::If:
    {
        const char* pinNames[] = {"Cond", "True", "False"};
        int idx = 0;
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pinNames[idx]);
            ImGui::SameLine(0, 20);
            if (idx == 0) ShowBool(node.InputA);
            else if (idx == 1) ImGui::TextDisabled("%.2f", node.InputB);
            else              ImGui::TextDisabled("%.2f", node.Factor);
            idx++;
            ed::EndPin();
        }
        float sepWidth = 140.0f;
        ImGui::Dummy(ImVec2(sepWidth, 2));
        auto* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(cursor.x, cursor.y),
                    ImVec2(cursor.x + sepWidth, cursor.y),
                    IM_COL32(100, 100, 100, 255), 1.0f);
        ImGui::Dummy(ImVec2(sepWidth, 2));
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("Out");
            ImGui::SameLine(0, 20);
            ImGui::TextDisabled("%.3f", node.Value);
            ed::EndPin();
        }
        break;
    }

    case NodeType::While:
    {
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            if (pin.Name == "Cond") { ImGui::TextUnformatted("Cond"); ImGui::SameLine(0, 20); ShowBool(node.InputA); }
            else                    { ImGui::TextUnformatted("Val");  ImGui::SameLine(0, 20); ImGui::TextDisabled("%.2f", node.InputB); }
            ed::EndPin();
        }
        float sepWidth = 130.0f;
        ImGui::Dummy(ImVec2(sepWidth, 2));
        auto* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(cursor.x, cursor.y),
                    ImVec2(cursor.x + sepWidth, cursor.y),
                    IM_COL32(100, 100, 100, 255), 1.0f);
        ImGui::Dummy(ImVec2(sepWidth, 2));
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("Out");
            ImGui::SameLine(0, 20);
            ImGui::TextDisabled("%.3f", node.Value);
            ed::EndPin();
        }
        break;
    }

    case NodeType::CustomOutput:
    {
        for (auto& pin : node.Inputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("Value");
            ImGui::SameLine(0, 12);
            ImGui::TextDisabled("%.3f", node.Value);
            ed::EndPin();
        }
        {
            ImVec2 btnSize(150, 0);
            std::string btnLabel = node.OutputTarget;
            if (!btnLabel.empty()) {
                for (const auto& t : outputTargets) {
                    if (t.field_path == node.OutputTarget) {
                        btnLabel = t.name;
                        break;
                    }
                }
            }
            if (!btnLabel.empty())
            {
                if (ImGui::Button(btnLabel.c_str(), btnSize))
                {
                    outputComboNodeId = node.ID;
                    outputComboRequested = true;
                }
            }
            else
            {
                if (ImGui::Button("(select target)", btnSize))
                {
                    outputComboNodeId = node.ID;
                    outputComboRequested = true;
                }
            }
        }
        break;
    }
    }
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
    std::unique_lock<std::shared_mutex> lock(m_EvalMutex);

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
// DrawMenuBar — top menu bar (Add Node / Clear All / Reset View)
// ============================================================================
void NodeGraph::DrawMenuBar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Add Node"))
        {
            if (ImGui::MenuItem("Key Source"))   AddNode(NodeType::KeySource);
            if (ImGui::MenuItem("Const Value"))  AddNode(NodeType::ConstValue);
            ImGui::Separator();
            if (ImGui::MenuItem("Add"))          AddNode(NodeType::Add);
            if (ImGui::MenuItem("Subtract"))     AddNode(NodeType::Subtract);
            if (ImGui::MenuItem("Multiply"))     AddNode(NodeType::Multiply);
            if (ImGui::MenuItem("Divide"))       AddNode(NodeType::Divide);
            ImGui::Separator();
            if (ImGui::MenuItem("Scale"))        AddNode(NodeType::Scale);
            if (ImGui::MenuItem("Clamp"))        AddNode(NodeType::Clamp);
            if (ImGui::MenuItem("Compare"))      AddNode(NodeType::Compare);
            ImGui::Separator();
            if (ImGui::MenuItem("AND"))          AddNode(NodeType::And);
            if (ImGui::MenuItem("OR"))           AddNode(NodeType::Or);
            if (ImGui::MenuItem("NOT"))          AddNode(NodeType::Not);
            ImGui::Separator();
            if (ImGui::MenuItem("IF"))           AddNode(NodeType::If);
            if (ImGui::MenuItem("WHILE"))        AddNode(NodeType::While);
            ImGui::Separator();
            if (ImGui::MenuItem("Output"))       AddNode(NodeType::CustomOutput);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Clear All"))
        {
            Clear();
            m_NavigateFrame = 1;
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
    if (snapshot.empty())
    {
        ImGui::TextDisabled("(empty)");
    }
    else
    {
        for (const auto& [name, val] : snapshot)
        {
            ImGui::Text("%s", name.c_str());
            ImGui::SameLine(sideWidth - 50);
            bool isAnalog = (analogKeys.count(name) > 0);
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
    ed::SetCurrentEditor(editorCtx);

    // Update sidebar output values for display
    {
        std::map<std::string, float> kvSnapshot = GetKeyValuesSnapshot();
        EvaluateForDisplay(kvSnapshot);
    }

    DrawMenuBar();

    // -------- item selector --------
    {
        if (!m_RobotModeNames.empty())
        {
            ImGui::TextUnformatted("Robot:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160);
            if (ImGui::Combo("##RobotModeCombo", &m_SelectedRobotModeIdx,
                [](void* data, int idx, const char** out) {
                    auto& vec = *(const std::vector<std::string>*)data;
                    *out = vec[idx].c_str(); return true;
                }, (void*)&m_RobotModeNames, (int)m_RobotModeNames.size()))
            {
                if (m_SelectedRobotModeIdx >= 0 && m_SelectedRobotModeIdx < (int)m_RobotModeNames.size()) {
                    SwitchRobotMode(m_RobotModeNames[m_SelectedRobotModeIdx], GetActiveGamepadModeName());
                }
            }
            ImGui::SameLine();
        }

        if (!m_GamepadModeNames.empty())
        {
            ImGui::TextUnformatted("Gamepad:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160);
            if (ImGui::Combo("##GamepadCombo", &m_SelectedGamepadModeIdx,
                [](void* data, int idx, const char** out) {
                    auto& vec = *(const std::vector<std::string>*)data;
                    *out = vec[idx].c_str(); return true;
                }, (void*)&m_GamepadModeNames, (int)m_GamepadModeNames.size()))
            {
                if (m_SelectedGamepadModeIdx >= 0 && m_SelectedGamepadModeIdx < (int)m_GamepadModeNames.size()) {
                    SwitchGamepadMode(GetActiveRobotModeName(), m_GamepadModeNames[m_SelectedGamepadModeIdx]);
                }
            }
            ImGui::SameLine();
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
            if (node.Type == NodeType::Clamp)
                nodeWidth = 180.0f;
            else if (node.Type == NodeType::KeySource)
                nodeWidth = 150.0f;
            else if (node.Type == NodeType::Add || node.Type == NodeType::Multiply
                  || node.Type == NodeType::Subtract || node.Type == NodeType::Divide
                  || node.Type == NodeType::And || node.Type == NodeType::Or)
                nodeWidth = 120.0f;
            else if (node.Type == NodeType::Compare || node.Type == NodeType::Not || node.Type == NodeType::While)
                nodeWidth = 140.0f;
            else if (node.Type == NodeType::If)
                nodeWidth = 160.0f;
            else if (node.Type == NodeType::CustomOutput)
                nodeWidth = 190.0f;

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

            DrawNodeContents(node, m_AnalogKeys, m_OutputTargets, m_KeySourcePopupNodeId, m_KeySourcePopupRequested, m_OutputComboNodeId, m_OutputComboRequested);

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
    }
    ed::EndCreate();

    if (ed::BeginDelete())
    {
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
        }
    }
    ed::EndDelete();

    {
        ed::Suspend();

        if (ed::ShowNodeContextMenu(&contextNodeId))
            ImGui::OpenPopup("Node Context Menu");
        else if (ed::ShowPinContextMenu(&contextPinId))
            ImGui::OpenPopup("Pin Context Menu");
        else if (ed::ShowLinkContextMenu(&contextLinkId))
            ImGui::OpenPopup("Link Context Menu");
        else if (ed::ShowBackgroundContextMenu())
            ImGui::OpenPopup("Create New Node");

        if (m_OutputComboRequested)
        {
            ImGui::OpenPopup("##OutputComboPopup");
            ImGui::SetNextWindowSize(ImVec2(220, 0));
            m_OutputComboRequested = false;
        }

        if (m_KeySourcePopupRequested)
        {
            ImGui::OpenPopup("##KeySourcePopup");
            ImGui::SetNextWindowSize(ImVec2(180, 0));
            m_KeySourcePopupRequested = false;
        }

        ed::Resume();
    }

    ed::End();

    // ---- Popup rendering (plain ImGui, outside node editor) ----
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
        if (ImGui::MenuItem("Key Source"))   AddNode(NodeType::KeySource);
        if (ImGui::MenuItem("Const Value"))  AddNode(NodeType::ConstValue);
        ImGui::Separator();
        if (ImGui::MenuItem("Add"))          AddNode(NodeType::Add);
        if (ImGui::MenuItem("Subtract"))     AddNode(NodeType::Subtract);
        if (ImGui::MenuItem("Multiply"))     AddNode(NodeType::Multiply);
        if (ImGui::MenuItem("Divide"))       AddNode(NodeType::Divide);
        ImGui::Separator();
        if (ImGui::MenuItem("Scale"))        AddNode(NodeType::Scale);
        if (ImGui::MenuItem("Clamp"))        AddNode(NodeType::Clamp);
        if (ImGui::MenuItem("Compare"))      AddNode(NodeType::Compare);
        ImGui::Separator();
        if (ImGui::MenuItem("AND"))          AddNode(NodeType::And);
        if (ImGui::MenuItem("OR"))           AddNode(NodeType::Or);
        if (ImGui::MenuItem("NOT"))          AddNode(NodeType::Not);
        ImGui::Separator();
        if (ImGui::MenuItem("IF"))           AddNode(NodeType::If);
        if (ImGui::MenuItem("WHILE"))        AddNode(NodeType::While);
        ImGui::Separator();
        if (ImGui::MenuItem("Output"))       AddNode(NodeType::CustomOutput);

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##OutputComboPopup"))
    {
        auto* node = FindNode(m_OutputComboNodeId);
        if (node && node->Type == NodeType::CustomOutput)
        {
            for (const auto& target : m_OutputTargets)
            {
                bool sel = (node->OutputTarget == target.field_path);
                if (ImGui::Selectable(target.name.c_str(), sel))
                {
                    node->OutputTarget = target.field_path;
                    SetModified(true);
                    ImGui::CloseCurrentPopup();
                    m_OutputComboNodeId = 0;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            if (m_OutputTargets.empty())
                ImGui::TextDisabled("No actuator targets available");
        }
        ImGui::EndPopup();
    }
    else
    {
        m_OutputComboNodeId = 0;
    }

    if (ImGui::BeginPopup("##KeySourcePopup"))
    {
        auto* node = FindNode(m_KeySourcePopupNodeId);
        if (node && node->Type == NodeType::KeySource)
        {
            if (m_AvailableKeys.empty())
            {
                ImGui::TextDisabled("No keys available");
            }
            else
            {
                std::string lastKey;
                for (const auto& key : m_AvailableKeys)
                {
                    if (key == lastKey) continue;
                    lastKey = key;
                    bool sel = (node->KeyName == key);
                    if (ImGui::Selectable(key.c_str(), sel))
                    {
                        node->KeyName = key;
                        SetModified(true);
                        ImGui::CloseCurrentPopup();
                        m_KeySourcePopupNodeId = 0;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndPopup();
    }
    else
    {
        m_KeySourcePopupNodeId = 0;
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

    // Right sidebar — draw output values
    ImGui::SameLine(0, 0);
    ManualSplitter("##S2", &m_RightSideWidth, 80.0f, splitterW, true);
    ImGui::SameLine(0, 0);

    // Draw output values inline (use snapshot from EvaluateForDisplay)
    {
        ImGui::BeginChild("##OVSide", ImVec2(m_RightSideWidth, 0), true);
        ImGui::TextUnformatted("Output Values");
        ImGui::Separator();
        if (m_OutputTargets.empty())
        {
            ImGui::TextDisabled("(no targets)");
        }
        else
        {
            // Read m_LastOutputs from m_Graph via Evaluate
            std::map<std::string, float> kvSnapshot = GetKeyValuesSnapshot();
            auto outputs = Evaluate(kvSnapshot);

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

                ImGui::Text("%s", target.name.c_str());
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
