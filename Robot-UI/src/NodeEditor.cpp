#define IMGUI_DEFINE_MATH_OPERATORS
#include "NodeEditor.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <queue>
#include <mutex>
#include <unordered_map>

namespace ed = ax::NodeEditor;

// ============================================================================
// ManualSplitter — works with ImGui 1.87 (no SplitterBehavior available)
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
    bool active  = ImGui::IsItemActive();

    ImU32 col = active  ? IM_COL32(100, 100, 255, 255)
              : hovered ? IM_COL32(80, 80, 180, 255)
              :            IM_COL32(60, 60, 80, 150);

    dl->AddRectFilled(ImVec2(cursor.x, cursor.y),
                      ImVec2(cursor.x + thickness, cursor.y + avail.y), col);

    if (active)
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
// GetIconColor — per-pin-type coloring
// ============================================================================
static ImColor GetIconColor(PinType type)
{
    switch (type)
    {
    default:
    case PinType::Float: return ImColor(147, 226,  74);
    }
}

// ============================================================================
// GetNodeTitle / GetNodeHeaderColor
// ============================================================================
static const char* GetNodeTitle(NodeType type)
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

static ImColor GetNodeHeaderColor(NodeType type)
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
// Constructor / Destructor
// ============================================================================
NodeEditor::NodeEditor()
{
    ed::Config config;
    config.SettingsFile = nullptr;
    m_EditorCtx = ed::CreateEditor(&config);
    ed::SetCurrentEditor(m_EditorCtx);
}

NodeEditor::~NodeEditor()
{
    if (m_EditorCtx)
    {
        ed::SetCurrentEditor(nullptr);
        ed::DestroyEditor(m_EditorCtx);
        m_EditorCtx = nullptr;
    }
}

// ============================================================================
// BuildNode — wire up pin → node back-pointers and pin kinds
// ============================================================================
void NodeEditor::BuildNode(EditorNode* node)
{
    for (auto& pin : node->Inputs)
        pin.Node = node;
    for (auto& pin : node->Outputs)
        pin.Node = node;
}

// ============================================================================
// RebuildAllNodes — fix dangling pin→node pointers after vector reallocation
// ============================================================================
void NodeEditor::RebuildAllNodes()
{
    for (auto& node : m_Nodes)
        BuildNode(&node);
}

// ============================================================================
// Spawn*Node factories
// ============================================================================
EditorNode* NodeEditor::SpawnKeySource()
{
    m_Nodes.emplace_back(GetNextId(), "Key Source", NodeType::KeySource, GetNodeHeaderColor(NodeType::KeySource));
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Value", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnConstValue()
{
    m_Nodes.emplace_back(GetNextId(), "Const", NodeType::ConstValue, GetNodeHeaderColor(NodeType::ConstValue));
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Value", PinType::Float);
    m_Nodes.back().Value = 0.0f;
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnAdd()
{
    m_Nodes.emplace_back(GetNextId(), "Add", NodeType::Add, GetNodeHeaderColor(NodeType::Add));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A+B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnMultiply()
{
    m_Nodes.emplace_back(GetNextId(), "Multiply", NodeType::Multiply, GetNodeHeaderColor(NodeType::Multiply));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A*B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnScale()
{
    m_Nodes.emplace_back(GetNextId(), "Scale", NodeType::Scale, GetNodeHeaderColor(NodeType::Scale));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "In", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Out", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnClamp()
{
    m_Nodes.emplace_back(GetNextId(), "Clamp", NodeType::Clamp, GetNodeHeaderColor(NodeType::Clamp));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "In", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Out", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnSubtract()
{
    m_Nodes.emplace_back(GetNextId(), "Subtract", NodeType::Subtract, GetNodeHeaderColor(NodeType::Subtract));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A-B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnDivide()
{
    m_Nodes.emplace_back(GetNextId(), "Divide", NodeType::Divide, GetNodeHeaderColor(NodeType::Divide));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A/B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnCompare()
{
    m_Nodes.emplace_back(GetNextId(), "Compare", NodeType::Compare, GetNodeHeaderColor(NodeType::Compare));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Bool", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnAnd()
{
    m_Nodes.emplace_back(GetNextId(), "AND", NodeType::And, GetNodeHeaderColor(NodeType::And));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A&B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnOr()
{
    m_Nodes.emplace_back(GetNextId(), "OR", NodeType::Or, GetNodeHeaderColor(NodeType::Or));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "B", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "A|B", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnNot()
{
    m_Nodes.emplace_back(GetNextId(), "NOT", NodeType::Not, GetNodeHeaderColor(NodeType::Not));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "A", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "!A", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnIf()
{
    m_Nodes.emplace_back(GetNextId(), "IF", NodeType::If, GetNodeHeaderColor(NodeType::If));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "Cond", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "True", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "False", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Out", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnWhile()
{
    m_Nodes.emplace_back(GetNextId(), "WHILE", NodeType::While, GetNodeHeaderColor(NodeType::While));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "Cond", PinType::Float);
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "Val", PinType::Float);
    m_Nodes.back().Outputs.emplace_back(GetNextId(), "Out", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

EditorNode* NodeEditor::SpawnOutput()
{
    m_Nodes.emplace_back(GetNextId(), "Output", NodeType::CustomOutput, GetNodeHeaderColor(NodeType::CustomOutput));
    m_Nodes.back().Inputs.emplace_back(GetNextId(), "Value", PinType::Float);
    BuildNode(&m_Nodes.back());
    return &m_Nodes.back();
}

void NodeEditor::AddNode(NodeType type)
{
    // 新节点放在画布原点 (0, 0)
    AddNodeAt(type, ImVec2(0, 0), false);
}

void NodeEditor::NavigateToOrigin()
{
    if (!m_Nodes.empty())
    {
        ed::NavigateToContent(0.0f);
        return;
    }

    // 空画布：创建两个对称虚拟节点包裹原点，确保 NavigateToContent 精确居中于 (0,0)
    auto makeAnchor = [&](const ImVec2& pos) -> int {
        m_Nodes.emplace_back(GetNextId(), "##anchor", NodeType::ConstValue, ImColor(0,0,0,0));
        int idx = (int)m_Nodes.size() - 1;
        m_Nodes.back().Outputs.emplace_back(GetNextId(), "##a", PinType::Float);
        BuildNode(&m_Nodes.back());
        ed::SetNodePosition(m_Nodes.back().ID, pos);
        return idx;
    };

    int idxNw = makeAnchor(ImVec2(-400, -400));
    int idxSe = makeAnchor(ImVec2( 400,  400));

    ed::NavigateToContent(0.0f);

    // 移除虚拟锚点，顺序从后往前删避免索引失效
    m_Nodes.erase(m_Nodes.begin() + idxNw);
    if (idxSe > idxNw) idxSe--;
    m_Nodes.erase(m_Nodes.begin() + idxSe);
}

void NodeEditor::AddNodeAt(NodeType type, const ImVec2& screenPos)
{
    AddNodeAt(type, screenPos, true);
}

void NodeEditor::AddNodeAt(NodeType type, const ImVec2& pos, bool fromScreen)
{
    EditorNode* node = nullptr;
    switch (type)
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
    if (node)
    {
        ed::SetNodePosition(node->ID, fromScreen ? ed::ScreenToCanvas(pos) : pos);
        m_Modified = true;
        m_NavigateFrame = 1;  // 新节点加入，下一帧导航到内容
    }

    // emplace_back inside Spawn*() may have invalidated pin→node pointers
    RebuildAllNodes();
}

// ============================================================================
// Clear
// ============================================================================
void NodeEditor::Clear()
{
    m_Nodes.clear();
    m_Links.clear();
    m_LastKeyValues.clear();
    m_LastOutputs.clear();
    ResetIDs();
    m_Modified = false;
    m_NavigateFrame = 1;
}

// ============================================================================
// Mode switching & Apply/Cancel
// ============================================================================
void NodeEditor::SetModeNames(const std::vector<std::string>& names, int activeIdx)
{
    bool sizeChanged = (names.size() != m_ModeNames.size());
    m_ModeNames = names;
    // 仅在首次设置或列表长度变化时同步索引（避免覆盖用户在 Combo 中的选择）
    if (sizeChanged || m_ModeNames.empty())
    {
        m_SelectedModeIdx = (activeIdx >= 0 && activeIdx < (int)names.size()) ? activeIdx : 0;
        m_AppliedModeIdx = m_SelectedModeIdx;
    }
}

void NodeEditor::OnOpen()
{
    m_AppliedGraphYaml = GetGraphYaml();
    m_AppliedModeIdx = m_SelectedModeIdx;
}

void NodeEditor::ApplyChanges()
{
    m_AppliedGraphYaml = GetGraphYaml();
    m_AppliedModeIdx = m_SelectedModeIdx;
    m_Modified = false;
    if (m_OnModeSwitch)
        m_OnModeSwitch(m_SelectedModeIdx);
}

void NodeEditor::CancelChanges()
{
    LoadGraphYaml(m_AppliedGraphYaml);
    m_SelectedModeIdx = m_AppliedModeIdx;
    m_Modified = false;
}

// ============================================================================
// Lookup helpers
// ============================================================================
EditorNode* NodeEditor::FindNode(ed::NodeId id)
{
    for (auto& node : m_Nodes)
        if (node.ID == id)
            return &node;
    return nullptr;
}

EditorPin* NodeEditor::FindPin(ed::PinId id)
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

EditorLink* NodeEditor::FindLink(ed::LinkId id)
{
    for (auto& link : m_Links)
        if (link.ID == id)
            return &link;
    return nullptr;
}

bool NodeEditor::IsPinLinked(ed::PinId id) const
{
    if (!id) return false;
    for (auto& link : m_Links)
        if (link.StartPinID == id || link.EndPinID == id)
            return true;
    return false;
}

bool NodeEditor::CanCreateLink(EditorPin* a, EditorPin* b) const
{
    if (!a || !b || a == b || a->Node == b->Node || a->Type != b->Type)
        return false;
    return true;
}

// ============================================================================
// DrawPinIcon — copied from blueprints example
// ============================================================================
void NodeEditor::DrawPinIcon(const EditorPin& pin, bool connected, int alpha)
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

static void ShowBool(float v)
{
    if (v >= 0.5f)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
    else
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
}

// ============================================================================
// DrawNodeContents — widgets and pins inside each node
// ============================================================================
void NodeEditor::DrawNodeContents(EditorNode& node)
{
    switch (node.Type)
    {
    case NodeType::KeySource:
    {
        // Button row: [key name] | current value
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        std::string btnLabel = node.KeyName.empty() ? "(select key)" : node.KeyName;
        if (ImGui::Button(btnLabel.c_str(), ImVec2(105, 0)))
        {
            m_KeySourcePopupNodeId = node.ID;
            m_KeySourcePopupRequested = true;
        }
        ImGui::PopStyleVar();
        ImGui::SameLine();
        // 区分 digital (True/False) 和 analog (浮点值)
        bool isAnalog = (m_AnalogKeys.count(node.KeyName) > 0);
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

        // Output pin row: icon → "Value"
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if (ImGui::DragFloat("##Val", &node.Value, 0.01f, -100.0f, 100.0f, "%.3f"))
                m_Modified = true;
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            float v = (pin.Name == "A") ? node.InputA : node.InputB;
            ImGui::TextDisabled("%.2f", v);
            ed::EndPin();
        }
        {
            // Width-constrained horizontal rule instead of full-width Separator
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("In");
            ed::EndPin();
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::SliderFloat("##Factor", &node.Factor, -10.0f, 10.0f, "x %.2f"))
            m_Modified = true;
        ImGui::Indent(10);
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("In");
            ed::EndPin();
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::DragFloat("##Min", &node.MinVal, 0.01f, -10.0f, 10.0f, "Min %.2f"))
            m_Modified = true;
        ImGui::SetNextItemWidth(100);
        if (ImGui::DragFloat("##Max", &node.MaxVal, 0.01f, -10.0f, 10.0f, "Max %.2f"))
            m_Modified = true;
        if (node.MinVal > node.MaxVal) node.MinVal = node.MaxVal;
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ImGui::SameLine(0, 20);
            float v = (pin.Name == "A") ? node.InputA : node.InputB;
            ImGui::TextDisabled("%.2f", v);
            ed::EndPin();
        }
        {
            const char* modes[] = { "A>B", "A>=B", "A<=B", "A<B", "A==B", "A!=B" };
            ImGui::SetNextItemWidth(110);
            if (ImGui::Combo("##Op", &node.OpMode, modes, IM_ARRAYSIZE(modes)))
                m_Modified = true;
        }
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pinNames[idx]);
            ImGui::SameLine(0, 20);
            if (idx == 0) ShowBool(node.InputA);                // Cond
            else if (idx == 1) ImGui::TextDisabled("%.2f", node.InputB);  // True
            else              ImGui::TextDisabled("%.2f", node.Factor);    // False
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("Value");
            ImGui::SameLine(0, 12);
            ImGui::TextDisabled("%.3f", node.Value);
            ed::EndPin();
        }
        // Dropdown button to pick output target (ActuatorData field names)
        {
            ImVec2 btnSize(150, 0);
            if (!node.OutputTarget.empty())
            {
                if (ImGui::Button(node.OutputTarget.c_str(), btnSize))
                {
                    m_OutputComboNodeId = node.ID;
                    m_OutputComboRequested = true;
                }
            }
            else
            {
                if (ImGui::Button("(select target)", btnSize))
                {
                    m_OutputComboNodeId = node.ID;
                    m_OutputComboRequested = true;
                }
            }
        }
        break;
    }
    }
}

// ============================================================================
// Sidebars
// ============================================================================
void NodeEditor::DrawKeyValuesSidebar(float sideWidth) const
{
    ImGui::BeginChild("##KVSide", ImVec2(sideWidth, 0), true);
    ImGui::TextUnformatted("Input Keys");
    ImGui::Separator();
    if (m_LastKeyValues.empty())
    {
        ImGui::TextDisabled("(empty)");
    }
    else
    {
        for (const auto& [name, val] : m_LastKeyValues)
        {
            ImGui::Text("%s", name.c_str());
            ImGui::SameLine(sideWidth - 50);
            // 根据键位类型区分显示：数字键显示 True/False，模拟键始终显示浮点值
            bool isAnalog = (m_AnalogKeys.count(name) > 0);
            if (!isAnalog)
            {
                // 数字键位：0 → False, 1 → True
                if (val >= 0.5f)
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
            }
            else
            {
                // 模拟键位：始终显示浮点值（含负数）
                ImVec4 color(0.4f, 0.8f, 1.0f, 1.0f);
                if (val < 0.0f) color = ImVec4(1.0f, 0.5f, 0.4f, 1.0f);
                ImGui::TextColored(color, "%.3f", val);
            }
        }
    }
    ImGui::EndChild();
}

void NodeEditor::DrawOutputValuesSidebar(float sideWidth) const
{
    ImGui::BeginChild("##OVSide", ImVec2(sideWidth, 0), true);
    ImGui::TextUnformatted("Output Values");
    ImGui::Separator();
    if (m_OutputTargets.empty())
    {
        ImGui::TextDisabled("(no targets)");
    }
    else
    {
        for (const auto& target : m_OutputTargets)
        {
            float val = 0.0f;
            auto it = m_LastOutputs.find(target);
            if (it != m_LastOutputs.end())
                val = it->second;

            ImGui::Text("%s", target.c_str());
            ImGui::SameLine(sideWidth - 55);
            ImVec4 color(0.4f, 1.0f, 0.4f, 1.0f);
            if (val < 0.0f) color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            else if (val == 0.0f && it == m_LastOutputs.end())
                color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // 默认值灰色
            ImGui::TextColored(color, "%.3f", val);
        }
    }
    ImGui::EndChild();
}

// ============================================================================
// Menu Bar
// ============================================================================
void NodeEditor::DrawMenuBar()
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
        if (ImGui::MenuItem("Clear All"))   Clear();
        if (ImGui::MenuItem("Reset View"))  ed::NavigateToContent();
        ImGui::EndMenuBar();
    }
}

// ============================================================================
// Draw — main entry point (follows blueprints example structure exactly)
// ============================================================================
bool NodeEditor::Draw()
{
    bool open = true;
    ed::SetCurrentEditor(m_EditorCtx);

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Node Editor##NodeEditorWindow", &open, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return open;
    }

    DrawMenuBar();

    // -------- Mode selector & Apply/Cancel bar --------
    if (!m_ModeNames.empty())
    {
        ImGui::TextUnformatted("Mode:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160);
        if (ImGui::Combo("##ModeCombo", &m_SelectedModeIdx,
            [](void* data, int idx, const char** out) {
                auto& vec = *(const std::vector<std::string>*)data;
                *out = vec[idx].c_str(); return true;
            }, &m_ModeNames, (int)m_ModeNames.size()))
        {
            // Mode switched — no auto-apply; user must click Apply
            m_Modified = true;
        }

        ImGui::SameLine();
        bool hasChanges = (m_SelectedModeIdx != m_AppliedModeIdx) || m_Modified;
        if (hasChanges)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.7f, 0.2f, 0.8f));
            if (ImGui::Button("Apply", ImVec2(70, 0)))
                ApplyChanges();
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        if (ImGui::Button("Cancel", ImVec2(70, 0)))
            CancelChanges();

        ImGui::Separator();
    }

    // -------- interaction state (static, per-editor) --------
    static ed::NodeId contextNodeId = 0;
    static ed::LinkId contextLinkId = 0;
    static ed::PinId  contextPinId  = 0;

    // -------- Resizable three-panel layout: left | splitter | canvas | splitter | right --------
    float splitterW = 5.0f;
    float totalAvail = ImGui::GetContentRegionAvail().x;

    float canvasW = totalAvail - m_LeftSideWidth - m_RightSideWidth - splitterW * 2;
    if (canvasW < 100.0f) {
        canvasW = 100.0f;
        // Shrink sidebars proportionally
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
    DrawKeyValuesSidebar(m_LeftSideWidth);
    ImGui::SameLine(0, 0);
    ManualSplitter("##S1", &m_LeftSideWidth, 80.0f, splitterW);
    ImGui::SameLine(0, 0);

    // Editor canvas
    ed::Begin("##Canvas", ImVec2(canvasW, 0));

    // 当添加节点或加载图后，导航到原点
    if (m_NavigateFrame > 0)
    {
        NavigateToOrigin();
        m_NavigateFrame = 0;
    }

    // --- Draw nodes ---
    for (auto& node : m_Nodes)
    {
        ed::PushStyleColor(ed::StyleColor_NodeBg,        ImColor(60, 60, 60, 200));
        ed::PushStyleColor(ed::StyleColor_NodeBorder,    node.Color);
        ed::PushStyleColor(ed::StyleColor_PinRect,       ImColor(60, 180, 255, 150));
        ed::PushStyleColor(ed::StyleColor_PinRectBorder, ImColor(60, 180, 255, 150));

        // Constrain node width — the Dummy sets a minimum content width
        ed::BeginNode(node.ID);
        {
            float nodeWidth = 140.0f;
            // Wider for Clamp (two sliders) and KeySource (combo)
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

            // Header
            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)node.Color);
            ImGui::TextUnformatted(GetNodeTitle(node.Type));
            ImGui::PopStyleColor();

            // Horizontal rule instead of full-width Separator
            ImGui::Dummy(ImVec2(nodeWidth, 2));
            auto* dl = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(cursor.x - ImGui::GetStyle().FramePadding.x, cursor.y),
                        ImVec2(cursor.x + nodeWidth - ImGui::GetStyle().FramePadding.x, cursor.y),
                        IM_COL32(100, 100, 100, 255), 1.0f);
            ImGui::Dummy(ImVec2(nodeWidth, 2));

            DrawNodeContents(node);

            // Force min width
            ImGui::Dummy(ImVec2(nodeWidth, 1));
        }
        ed::EndNode();

        ed::PopStyleColor(4);
    }

    // --- Draw links ---
    for (auto& link : m_Links)
        ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);

    // ==================================================================
    // Create — link / node creation handling
    // ==================================================================
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
                    // We need Output → Input direction.
                    // The startPin should be the output, endPin should be input.
                    auto* outPin = startPin;
                    auto* inPin  = endPin;

                    // Check if both are same kind (both inputs or both outputs)
                    bool startIsInput  = false;
                    bool endIsInput    = false;
                    for (auto& n : m_Nodes)
                    {
                        for (auto& p : n.Inputs)  { if (p.ID == startPinId) startIsInput = true; if (p.ID == endPinId) endIsInput = true; }
                        for (auto& p : n.Outputs) { if (p.ID == startPinId) startIsInput = false; if (p.ID == endPinId) endIsInput = false; }
                    }

                    // Simple heuristic: swap if start is input and end is output
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
                        m_Modified = true;
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

    // ==================================================================
    // Delete
    // ==================================================================
    if (ed::BeginDelete())
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
                m_Modified = true;
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
                m_Modified = true;
            }
        }
    }
    ed::EndDelete();

    // ==================================================================
    // Origin marker — 原点红色十字标记
    // ==================================================================
    {
        ed::Suspend();
        ImVec2 originScreen = ed::CanvasToScreen(ImVec2(0, 0));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = 6.0f;
        const float arm = 12.0f;
        const ImU32 red = IM_COL32(255, 60, 60, 255);
        const ImU32 redDim = IM_COL32(200, 40, 40, 140);
        // 十字线
        dl->AddLine(ImVec2(originScreen.x - arm, originScreen.y), ImVec2(originScreen.x + arm, originScreen.y), redDim, 2.5f);
        dl->AddLine(ImVec2(originScreen.x, originScreen.y - arm), ImVec2(originScreen.x, originScreen.y + arm), redDim, 2.5f);
        // 实心圆
        dl->AddCircleFilled(originScreen, r, red);
        dl->AddCircle(originScreen, r, IM_COL32(255, 255, 255, 80), 12, 2.0f);
        // 标签
        dl->AddText(ImVec2(originScreen.x + arm + 4, originScreen.y - 8), IM_COL32(200, 200, 200, 200), "Origin (0,0)");
        ed::Resume();
    }

    // ==================================================================
    // Context menus + Deferred KeySource combo (all inside Suspend)
    // ==================================================================
    {
        ed::Suspend();

        // --- Show context menu triggers ---
        if (ed::ShowNodeContextMenu(&contextNodeId))
            ImGui::OpenPopup("Node Context Menu");
        else if (ed::ShowPinContextMenu(&contextPinId))
            ImGui::OpenPopup("Pin Context Menu");
        else if (ed::ShowLinkContextMenu(&contextLinkId))
            ImGui::OpenPopup("Link Context Menu");
        else if (ed::ShowBackgroundContextMenu())
            ImGui::OpenPopup("Create New Node");

        // --- Deferred OutputTarget combo ---
        if (m_OutputComboRequested)
        {
            ImGui::OpenPopup("##OutputComboPopup");
            ImGui::SetNextWindowSize(ImVec2(220, 0));
            m_OutputComboRequested = false;
        }

        // --- Deferred KeySource combo ---
        if (m_KeySourcePopupRequested)
        {
            ImGui::OpenPopup("##KeySourcePopup");
            ImGui::SetNextWindowSize(ImVec2(180, 0));
            m_KeySourcePopupRequested = false;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

        // -- Node Context Menu --
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

        // -- Pin Context Menu --
        if (ImGui::BeginPopup("Pin Context Menu"))
        {
            if (ImGui::MenuItem("Break Links"))
            {
                int pid = (int)contextPinId.Get();
                m_Links.erase(
                    std::remove_if(m_Links.begin(), m_Links.end(),
                        [pid](auto& l) { return (int)l.StartPinID.Get() == pid || (int)l.EndPinID.Get() == pid; }),
                    m_Links.end());
                m_Modified = true;
            }
            ImGui::EndPopup();
        }

        // -- Link Context Menu --
        if (ImGui::BeginPopup("Link Context Menu"))
        {
            if (ImGui::MenuItem("Delete Link"))
                ed::DeleteLink(contextLinkId);
            ImGui::EndPopup();
        }

        // -- Create New Node --
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

        // -- OutputCombo Popup --
        if (ImGui::BeginPopup("##OutputComboPopup"))
        {
            auto* node = FindNode(m_OutputComboNodeId);
            if (node && node->Type == NodeType::CustomOutput)
            {
                for (const auto& target : m_OutputTargets)
                {
                    bool sel = (node->OutputTarget == target);
                    if (ImGui::Selectable(target.c_str(), sel))
                    {
                        node->OutputTarget = target;
                        m_Modified = true;
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

        // -- KeySource Popup --
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
                        if (key == lastKey) continue;  // skip duplicates
                        lastKey = key;
                        bool sel = (node->KeyName == key);
                        if (ImGui::Selectable(key.c_str(), sel))
                        {
                            node->KeyName = key;
                            m_Modified = true;
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
        ed::Resume();
    }

    ed::End(); // Canvas

    ImGui::SameLine(0, 0);
    ManualSplitter("##S2", &m_RightSideWidth, 80.0f, splitterW, true);
    ImGui::SameLine(0, 0);

    DrawOutputValuesSidebar(m_RightSideWidth);

    ImGui::End(); // Window
    return open;
}

// ============================================================================
// Evaluate
// ============================================================================
void NodeEditor::EvaluateGraph(const std::map<std::string, float>& keyValues)
{
    std::lock_guard<std::mutex> lock(m_EvalMutex);
    m_LastKeyValues = keyValues;
    m_LastOutputs.clear();
    if (m_Nodes.empty()) return;

    // ---- Topological sort by link dependencies ----
    // Pin IDs are independent; resolve via FindPin to get node IDs
    std::unordered_map<int, std::vector<int>> adj;
    std::unordered_map<int, int> indeg;

    for (auto& node : m_Nodes)
        indeg[(int)node.ID.Get()] = 0;

    for (auto& link : m_Links)
    {
        auto* fromPin = FindPin(link.StartPinID);
        auto* toPin   = FindPin(link.EndPinID);
        if (!fromPin || !toPin || !fromPin->Node || !toPin->Node)
            continue;
        int fromNode = (int)fromPin->Node->ID.Get();
        int toNode   = (int)toPin->Node->ID.Get();
        adj[fromNode].push_back(toNode);
        indeg[toNode]++;
    }

    // Kahn
    std::queue<int> qq;
    for (auto& [id, d] : indeg)
        if (d == 0) qq.push(id);

    std::vector<int> order;
    while (!qq.empty())
    {
        int id = qq.front(); qq.pop();
        order.push_back(id);
        for (int nxt : adj[id])
            if (--indeg[nxt] == 0)
                qq.push(nxt);
    }

    // ---- Evaluate in topological order ----
    std::unordered_map<int, float> pinVals; // pinId → value

    for (int nid : order)
    {
        EditorNode* node = nullptr;
        for (auto& n : m_Nodes)
            if ((int)n.ID.Get() == nid) { node = &n; break; }
        if (!node) continue;

        float out = 0.0f;

        switch (node->Type)
        {
        case NodeType::KeySource:
            out = keyValues.count(node->KeyName) ? keyValues.at(node->KeyName) : 0.0f;
            break;
        case NodeType::ConstValue:
            out = node->Value;
            break;
        case NodeType::Add:
        {
            float a = 0, b = 0;
            for (auto& pin : node->Inputs)
            {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end())
                    (pin.Name == "A" ? a : b) = it->second;
            }
            node->InputA = a; node->InputB = b;
            out = a + b;
            break;
        }
        case NodeType::Multiply:
        {
            float a = 0, b = 0;
            for (auto& pin : node->Inputs)
            {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end())
                    (pin.Name == "A" ? a : b) = it->second;
            }
            node->InputA = a; node->InputB = b;
            out = a * b;
            break;
        }
        case NodeType::Subtract:
        {
            float a = 0, b = 0;
            for (auto& pin : node->Inputs)
            {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end())
                    (pin.Name == "A" ? a : b) = it->second;
            }
            node->InputA = a; node->InputB = b;
            out = a - b;
            break;
        }
        case NodeType::Divide:
        {
            float a = 0, b = 0;
            for (auto& pin : node->Inputs)
            {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end())
                    (pin.Name == "A" ? a : b) = it->second;
            }
            node->InputA = a; node->InputB = b;
            out = (b != 0.0f) ? (a / b) : 0.0f;
            break;
        }
        case NodeType::Scale:
        {
            float in = 0;
            if (!node->Inputs.empty())
            {
                auto it = pinVals.find((int)node->Inputs[0].ID.Get());
                if (it != pinVals.end()) in = it->second;
            }
            out = in * node->Factor;
            break;
        }
        case NodeType::Clamp:
        {
            float in = 0;
            if (!node->Inputs.empty())
            {
                auto it = pinVals.find((int)node->Inputs[0].ID.Get());
                if (it != pinVals.end()) in = it->second;
            }
            out = std::clamp(in, node->MinVal, node->MaxVal);
            break;
        }
        case NodeType::Compare:
        {
            float a = 0, b = 0;
            for (auto& pin : node->Inputs)
            {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end())
                    (pin.Name == "A" ? a : b) = it->second;
            }
            node->InputA = a; node->InputB = b;
            // OpMode: 0=>, 1=>=, 2=<=, 3=<, 4==, 5=!=
            switch (node->OpMode) {
            case 0: out = (a > b) ? 1.0f : 0.0f; break;
            case 1: out = (a >= b) ? 1.0f : 0.0f; break;
            case 2: out = (a <= b) ? 1.0f : 0.0f; break;
            case 3: out = (a < b) ? 1.0f : 0.0f; break;
            case 4: out = (a == b) ? 1.0f : 0.0f; break;
            case 5: out = (a != b) ? 1.0f : 0.0f; break;
            default: out = (a > b) ? 1.0f : 0.0f; break;
            }
            break;
        }
        case NodeType::And:
        {
            float a = 0, b = 0;
            for (auto& pin : node->Inputs)
            {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end())
                    (pin.Name == "A" ? a : b) = it->second;
            }
            node->InputA = a; node->InputB = b;
            out = ((a >= 0.5f) && (b >= 0.5f)) ? 1.0f : 0.0f;
            break;
        }
        case NodeType::Or:
        {
            float a = 0, b = 0;
            for (auto& pin : node->Inputs)
            {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end())
                    (pin.Name == "A" ? a : b) = it->second;
            }
            node->InputA = a; node->InputB = b;
            out = ((a >= 0.5f) || (b >= 0.5f)) ? 1.0f : 0.0f;
            break;
        }
        case NodeType::Not:
        {
            float in = 0;
            if (!node->Inputs.empty())
            {
                auto it = pinVals.find((int)node->Inputs[0].ID.Get());
                if (it != pinVals.end()) in = it->second;
            }
            out = (in >= 0.5f) ? 0.0f : 1.0f;
            break;
        }
        case NodeType::If:
        {
            float cond = 0, tv = 0, fv = 0;
            for (auto& pin : node->Inputs)
            {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end())
                {
                    if      (pin.Name == "Cond")  cond = it->second;
                    else if (pin.Name == "True")  tv   = it->second;
                    else if (pin.Name == "False") fv   = it->second;
                }
            }
            node->InputA = cond; node->InputB = tv;
            node->Factor = fv;  // 借用 Factor 暂存
            out = (cond >= 0.5f) ? tv : fv;
            break;
        }
        case NodeType::While:
        {
            float cond = 0, val = 0;
            for (auto& pin : node->Inputs)
            {
                auto it = pinVals.find((int)pin.ID.Get());
                if (it != pinVals.end())
                {
                    if      (pin.Name == "Cond") cond = it->second;
                    else if (pin.Name == "Val")  val  = it->second;
                }
            }
            node->InputA = cond; node->InputB = val;
            out = (cond >= 0.5f) ? val : 0.0f;
            break;
        }
        case NodeType::CustomOutput:
        {
            float in = 0;
            if (!node->Inputs.empty())
            {
                auto it = pinVals.find((int)node->Inputs[0].ID.Get());
                if (it != pinVals.end()) in = it->second;
            }
            out = in;
            if (!node->OutputTarget.empty())
                m_LastOutputs[node->OutputTarget] = out;
            break;
        }
        }

        node->Value = out;

        // Propagate output pin value to connected input pins
        if (!node->Outputs.empty())
        {
            int outPinId = (int)node->Outputs[0].ID.Get();
            for (auto& link : m_Links)
                if ((int)link.StartPinID.Get() == outPinId)
                    pinVals[(int)link.EndPinID.Get()] = out;
        }
    }
}

std::map<std::string, float> NodeEditor::Evaluate(const std::map<std::string, float>& keyValues)
{
    EvaluateGraph(keyValues);
    return m_LastOutputs;
}

void NodeEditor::EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorData& data)
{
    EvaluateGraph(keyValues);
    for (const auto& [target, val] : m_LastOutputs)
        WriteOutputToActuator(target, val, data);
}

// ============================================================================
// GetGraphYaml / LoadGraphYaml — persistence for ConfigSerializer
// ============================================================================
std::string NodeEditor::GetGraphYaml() const
{
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
        // Find the owning node and pin index for each end
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

bool NodeEditor::LoadGraphYaml(const std::string& yamlStr)
{
    if (yamlStr.empty()) return false;
    try
    {
        YAML::Node root = YAML::Load(yamlStr);
        if (!root.IsMap()) return false;

        Clear();
        ed::SetCurrentEditor(m_EditorCtx);

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
                node->ID = ed::NodeId(savedId);
                // Keep Spawn*-generated pin IDs (they are already unique).
                // No need to override them — links use node + pin index.

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

                // Update m_NextId past all IDs used by this node (node ID + pin IDs)
                int maxUsed = savedId;
                for (auto& pin : node->Inputs)  maxUsed = std::max(maxUsed, (int)pin.ID.Get());
                for (auto& pin : node->Outputs) maxUsed = std::max(maxUsed, (int)pin.ID.Get());
                if (maxUsed >= m_NextId) m_NextId = maxUsed + 1;
            }
        }

        // Fix dangling pin→node pointers caused by vector reallocation during emplace_back
        RebuildAllNodes();

        if (root["links"] && root["links"].IsSequence())
        {
            for (auto yl : root["links"])
            {
                int lid = yl["id"].as<int>();

                // New format: from_node + from_pin, to_node + to_pin
                int fn = yl["from_node"] ? yl["from_node"].as<int>() : -1;
                int fi = yl["from_pin"]  ? yl["from_pin"].as<int>()  : 0;
                int tn = yl["to_node"]   ? yl["to_node"].as<int>()   : -1;
                int ti = yl["to_pin"]    ? yl["to_pin"].as<int>()    : 0;

                // Old format compat: start_pin / end_pin (raw pin IDs)
                ed::PinId sp, ep;
                if (fn >= 0 && tn >= 0) {
                    // New format: resolve by node ID + pin index
                    EditorNode* fromNode = FindNode(ed::NodeId(fn));
                    EditorNode* toNode   = FindNode(ed::NodeId(tn));
                    sp = (fromNode && fi < (int)fromNode->Outputs.size()) ? fromNode->Outputs[fi].ID : ed::PinId(0);
                    ep = (toNode   && ti < (int)toNode->Inputs.size())   ? toNode->Inputs[ti].ID     : ed::PinId(0);
                } else if (yl["start_pin"] && yl["end_pin"]) {
                    // Old format: raw pin IDs using old formula nodeId*10 +(i or 10+i)
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

// ============================================================================
// WriteOutputToActuator — write an output-target→value into ActuatorData
// ============================================================================
void WriteOutputToActuator(const std::string& target, float val, ActuatorData& data)
{
    if (target == "motion.x")      { data.motion.x  = val; return; }
    if (target == "motion.y")      { data.motion.y  = val; return; }
    if (target == "motion.z")      { data.motion.z  = val; return; }
    if (target == "motion.rx")     { data.motion.rx = val; return; }
    if (target == "motion.ry")     { data.motion.ry = val; return; }
    if (target == "motion.rz")     { data.motion.rz = val; return; }

    // servo_<id>
    if (target.rfind("servo_", 0) == 0)
    {
        try {
            int id = std::stoi(target.substr(6));
            data.servo[id].id = id;
            data.servo[id].angle = val;
        } catch (...) {}
        return;
    }

    // motor_<id>
    if (target.rfind("motor_", 0) == 0)
    {
        try {
            int id = std::stoi(target.substr(6));
            data.brushlessmotor[id].id = id;
            data.brushlessmotor[id].target_speed = val;
        } catch (...) {}
        return;
    }
}

// ============================================================================
// GetActuatorOutputTargets — build list of available output field names
// ============================================================================
std::vector<std::string> GetActuatorOutputTargets(const ActuatorData& data)
{
    std::vector<std::string> targets;

    for (const auto& kv : data.servo) {
        targets.push_back("servo_" + std::to_string(kv.first));
    }

    for (const auto& kv : data.brushlessmotor) {
        targets.push_back("motor_" + std::to_string(kv.first));
    }

    return targets;
}
