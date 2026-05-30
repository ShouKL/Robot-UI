#define IMGUI_DEFINE_MATH_OPERATORS
#include "NodeEditor.h"
#include "Walnut/Core/Log.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>

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

static void ShowBool(float v)
{
    if (v >= 0.5f)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
    else
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
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
// AddNode — create a node of the given type via m_Graph, position at origin
// ============================================================================
void NodeEditor::AddNode(NodeType type)
{
    AddNodeAt(type, ImVec2(0, 0), false);
}

void NodeEditor::AddNodeAt(NodeType type, const ImVec2& pos, bool fromScreen)
{
    // Must hold exclusive lock because spawn methods modify m_Nodes
    // and call GetNextId(), which races with the evaluation thread.
    // This is safe because AddNodeAt is only called from UI context
    // (menus / context menus) outside the draw loop's shared_lock scope.
    std::unique_lock<std::shared_mutex> lock(m_Graph.GetEvalMutex());

    EditorNode* node = nullptr;
    switch (type)
    {
    case NodeType::KeySource:    node = m_Graph.SpawnKeySource();    break;
    case NodeType::ConstValue:   node = m_Graph.SpawnConstValue();   break;
    case NodeType::Add:          node = m_Graph.SpawnAdd();          break;
    case NodeType::Subtract:     node = m_Graph.SpawnSubtract();     break;
    case NodeType::Multiply:     node = m_Graph.SpawnMultiply();     break;
    case NodeType::Divide:       node = m_Graph.SpawnDivide();       break;
    case NodeType::Scale:        node = m_Graph.SpawnScale();        break;
    case NodeType::Clamp:        node = m_Graph.SpawnClamp();        break;
    case NodeType::Compare:      node = m_Graph.SpawnCompare();      break;
    case NodeType::And:          node = m_Graph.SpawnAnd();          break;
    case NodeType::Or:           node = m_Graph.SpawnOr();           break;
    case NodeType::Not:          node = m_Graph.SpawnNot();          break;
    case NodeType::If:           node = m_Graph.SpawnIf();           break;
    case NodeType::While:        node = m_Graph.SpawnWhile();        break;
    case NodeType::CustomOutput: node = m_Graph.SpawnOutput();       break;
    }
    if (node)
    {
        ed::SetNodePosition(node->ID, fromScreen ? ed::ScreenToCanvas(pos) : pos);
        m_Graph.SetModified(true);
        m_NavigateFrame = 1;
    }

    m_Graph.RebuildAllNodes();
}

// ============================================================================
// NavigateToOrigin
// ============================================================================
void NodeEditor::NavigateToOrigin()
{
    if (!m_Graph.m_Nodes.empty())
    {
        ed::NavigateToContent(0.0f);
        return;
    }

    auto makeAnchor = [&](const ImVec2& pos) -> int {
        m_Graph.m_Nodes.emplace_back(m_Graph.GetNextId(), "##anchor", NodeType::ConstValue, ImColor(0,0,0,0));
        int idx = (int)m_Graph.m_Nodes.size() - 1;
        m_Graph.m_Nodes.back().Outputs.emplace_back(m_Graph.GetNextId(), "##a", PinType::Float);
        m_Graph.BuildNode(&m_Graph.m_Nodes.back());
        ed::SetNodePosition(m_Graph.m_Nodes.back().ID, pos);
        return idx;
    };

    int idxNw = makeAnchor(ImVec2(-400, -400));
    int idxSe = makeAnchor(ImVec2( 400,  400));

    ed::NavigateToContent(0.0f);

    m_Graph.m_Nodes.erase(m_Graph.m_Nodes.begin() + idxNw);
    if (idxSe > idxNw) idxSe--;
    m_Graph.m_Nodes.erase(m_Graph.m_Nodes.begin() + idxSe);
}

// ============================================================================
// Mode switching (delegates data to m_Graph)
// ============================================================================
void NodeEditor::SwitchRobotMode(const std::string& newRobotMode, const std::string& curGamepadMode)
{
    for (int i = 0; i < (int)m_RobotModeNames.size(); ++i) {
        if (m_RobotModeNames[i] == newRobotMode) {
            m_SelectedRobotModeIdx = i;
            m_AppliedRobotModeIdx = i;
            break;
        }
    }
    m_Graph.SwitchGraph(newRobotMode, curGamepadMode);
    m_NavigateFrame = 1;
}

void NodeEditor::SwitchGamepadMode(const std::string& curRobotMode, const std::string& newGamepadMode)
{
    m_Graph.SwitchGraph(curRobotMode, newGamepadMode);
    m_NavigateFrame = 1;
}

// ============================================================================
// Mode names & Apply/Cancel
// ============================================================================
void NodeEditor::SetRobotModeNames(const std::vector<std::string>& names, int activeIdx)
{
    bool sizeChanged = (names.size() != m_RobotModeNames.size());
    m_RobotModeNames = names;
    if (sizeChanged || m_RobotModeNames.empty())
    {
        m_SelectedRobotModeIdx = (activeIdx >= 0 && activeIdx < (int)names.size()) ? activeIdx : 0;
        m_AppliedRobotModeIdx = m_SelectedRobotModeIdx;
    }
}

void NodeEditor::SetGamepadModeNames(const std::vector<std::string>& names)
{
    m_GamepadModeNames = names;
    for (int i = 0; i < (int)names.size(); ++i) {
        if (names[i] == m_Graph.GetActiveGamepadModeName()) {
            m_SelectedGamepadModeIdx = i;
            return;
        }
    }
    m_SelectedGamepadModeIdx = 0;
}

void NodeEditor::SetCurrentModePair(const std::string& robotMode, const std::string& gamepadMode)
{
    m_Graph.SetActiveRobotModeName(robotMode);
    m_Graph.SetActiveGamepadModeName(gamepadMode);
    for (int i = 0; i < (int)m_RobotModeNames.size(); ++i) {
        if (m_RobotModeNames[i] == robotMode) {
            m_SelectedRobotModeIdx = i;
            m_AppliedRobotModeIdx = i;
            break;
        }
    }
    m_Graph.SwitchGraph(robotMode, gamepadMode);
    m_NavigateFrame = 1;
}

void NodeEditor::OnOpen()
{
    m_AppliedGraphYaml = GetGraphYaml();
    m_AppliedRobotModeIdx = m_SelectedRobotModeIdx;
}

void NodeEditor::ApplyChanges()
{
    m_Graph.SaveGraphToMap();
    m_AppliedRobotModeIdx = m_SelectedRobotModeIdx;
    m_Graph.SetModified(false);
}

void NodeEditor::CancelChanges()
{
    LoadGraphYaml(m_AppliedGraphYaml);
    m_SelectedRobotModeIdx = m_AppliedRobotModeIdx;
    m_Graph.SetModified(false);
}

// ============================================================================
// DrawPinIcon
// ============================================================================
static ImColor GetIconColorLocal(PinType type)
{
    switch (type)
    {
    default:
    case PinType::Float: return ImColor(147, 226, 74);
    }
}

void NodeEditor::DrawPinIcon(const EditorPin& pin, bool connected, int alpha)
{
    ImColor color = GetIconColorLocal(pin.Type);
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
// DrawNodeContents — widgets and pins inside each node
// ============================================================================
void NodeEditor::DrawNodeContents(EditorNode& node)
{
    switch (node.Type)
    {
    case NodeType::KeySource:
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        std::string btnLabel = node.KeyName.empty() ? "(select key)" : node.KeyName;
        if (ImGui::Button(btnLabel.c_str(), ImVec2(105, 0)))
        {
            m_KeySourcePopupNodeId = node.ID;
            m_KeySourcePopupRequested = true;
        }
        ImGui::PopStyleVar();
        ImGui::SameLine();
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

        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if (ImGui::DragFloat("##Val", &node.Value, 0.01f, -100.0f, 100.0f, "%.3f"))
                m_Graph.SetModified(true);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("In");
            ed::EndPin();
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::SliderFloat("##Factor", &node.Factor, -10.0f, 10.0f, "x %.2f"))
            m_Graph.SetModified(true);
        ImGui::Indent(10);
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted("In");
            ed::EndPin();
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::DragFloat("##Min", &node.MinVal, 0.01f, -10.0f, 10.0f, "Min %.2f"))
            m_Graph.SetModified(true);
        ImGui::SetNextItemWidth(100);
        if (ImGui::DragFloat("##Max", &node.MaxVal, 0.01f, -10.0f, 10.0f, "Max %.2f"))
            m_Graph.SetModified(true);
        if (node.MinVal > node.MaxVal) node.MinVal = node.MaxVal;
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
                m_Graph.SetModified(true);
        }
        for (auto& pin : node.Outputs)
        {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
            DrawPinIcon(pin, m_Graph.IsPinLinked(pin.ID), 255);
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
                for (const auto& t : m_OutputTargets) {
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
void NodeEditor::DrawKeyValuesSidebar(float sideWidth)
{
    std::map<std::string, float> snapshot = m_Graph.GetKeyValuesSnapshot();

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
            bool isAnalog = (m_AnalogKeys.count(name) > 0);
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

void NodeEditor::DrawOutputValuesSidebar(float sideWidth)
{
    // Use cached outputs from Evaluate
    std::map<std::string, float> outputs;
    {
        std::map<std::string, float> kvSnapshot = m_Graph.GetKeyValuesSnapshot();
        outputs = m_Graph.Evaluate(kvSnapshot);
    }
    ImGui::BeginChild("##OVSide", ImVec2(sideWidth, 0), true);
    ImGui::TextUnformatted("Output Values");
    ImGui::Separator();
    // Output drawing is handled inline in Draw() since it needs the evaluated snapshot
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
        if (ImGui::MenuItem("Clear All"))
        {
            m_Graph.Clear();
            m_NavigateFrame = 1;
        }
        if (ImGui::MenuItem("Reset View"))  ed::NavigateToContent();
        ImGui::EndMenuBar();
    }
}

// ============================================================================
// GetGraphYaml / LoadGraphYaml — includes ed:: position data
// ============================================================================
std::string NodeEditor::GetGraphYaml() const
{
    ed::SetCurrentEditor(m_EditorCtx);
    std::shared_lock<std::shared_mutex> lock(m_Graph.GetEvalMutex());
    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
    for (auto& n : m_Graph.m_Nodes)
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
    for (auto& l : m_Graph.m_Links)
    {
        int fromNodeId = -1, fromPinIdx = -1;
        int toNodeId = -1,   toPinIdx = -1;
        for (auto& n : m_Graph.m_Nodes) {
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

        std::unique_lock<std::shared_mutex> lock(m_Graph.GetEvalMutex());
        m_Graph.Clear_NoLock();
        ed::SetCurrentEditor(m_EditorCtx);

        if (root["nodes"] && root["nodes"].IsSequence())
        {
            for (auto yn : root["nodes"])
            {
                NodeType nt = (NodeType)yn["type"].as<int>();
                EditorNode* node = nullptr;

                switch (nt)
                {
                case NodeType::KeySource:    node = m_Graph.SpawnKeySource();    break;
                case NodeType::ConstValue:   node = m_Graph.SpawnConstValue();   break;
                case NodeType::Add:          node = m_Graph.SpawnAdd();          break;
                case NodeType::Subtract:     node = m_Graph.SpawnSubtract();     break;
                case NodeType::Multiply:     node = m_Graph.SpawnMultiply();     break;
                case NodeType::Divide:       node = m_Graph.SpawnDivide();       break;
                case NodeType::Scale:        node = m_Graph.SpawnScale();        break;
                case NodeType::Clamp:        node = m_Graph.SpawnClamp();        break;
                case NodeType::Compare:      node = m_Graph.SpawnCompare();      break;
                case NodeType::And:          node = m_Graph.SpawnAnd();          break;
                case NodeType::Or:           node = m_Graph.SpawnOr();           break;
                case NodeType::Not:          node = m_Graph.SpawnNot();          break;
                case NodeType::If:           node = m_Graph.SpawnIf();           break;
                case NodeType::While:        node = m_Graph.SpawnWhile();        break;
                case NodeType::CustomOutput: node = m_Graph.SpawnOutput();       break;
                }
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
                if (maxUsed >= m_Graph.PeekNextId()) m_Graph.SetNextId(maxUsed + 1);
            }
        }

        m_Graph.RebuildAllNodes();

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
                    EditorNode* fromNode = m_Graph.FindNode(ed::NodeId(fn));
                    EditorNode* toNode   = m_Graph.FindNode(ed::NodeId(tn));
                    sp = (fromNode && fi < (int)fromNode->Outputs.size()) ? fromNode->Outputs[fi].ID : ed::PinId(0);
                    ep = (toNode   && ti < (int)toNode->Inputs.size())   ? toNode->Inputs[ti].ID     : ed::PinId(0);
                } else if (yl["start_pin"] && yl["end_pin"]) {
                    int rawSp = yl["start_pin"].as<int>();
                    int rawEp = yl["end_pin"].as<int>();
                    int spNodeId = rawSp / 10, spRem = rawSp % 10;
                    int epNodeId = rawEp / 10, epRem = rawEp % 10;
                    EditorNode* fromNode = m_Graph.FindNode(ed::NodeId(spNodeId));
                    EditorNode* toNode   = m_Graph.FindNode(ed::NodeId(epNodeId));
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
                    m_Graph.m_Links.emplace_back(ed::LinkId(lid), sp, ep);
                if (lid >= m_Graph.PeekNextId()) m_Graph.SetNextId(lid + 1);
            }
        }

        m_Graph.SetModified(false);
        return true;
    }
    catch (const std::exception&) { return false; }
}

// ============================================================================
// Draw — main entry point
// ============================================================================
bool NodeEditor::Draw()
{
    // Update sidebar output values for display
    {
        std::map<std::string, float> kvSnapshot = m_Graph.GetKeyValuesSnapshot();
        m_Graph.EvaluateForDisplay(kvSnapshot);
    }

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
                }, &m_RobotModeNames, (int)m_RobotModeNames.size()))
            {
                if (m_SelectedRobotModeIdx >= 0 && m_SelectedRobotModeIdx < (int)m_RobotModeNames.size()) {
                    SwitchRobotMode(m_RobotModeNames[m_SelectedRobotModeIdx], m_Graph.GetActiveGamepadModeName());
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
                }, &m_GamepadModeNames, (int)m_GamepadModeNames.size()))
            {
                if (m_SelectedGamepadModeIdx >= 0 && m_SelectedGamepadModeIdx < (int)m_GamepadModeNames.size()) {
                    SwitchGamepadMode(m_Graph.GetActiveRobotModeName(), m_GamepadModeNames[m_SelectedGamepadModeIdx]);
                }
            }
            ImGui::SameLine();
        }

        bool hasChanges = (m_SelectedRobotModeIdx != m_AppliedRobotModeIdx) || m_Graph.IsModified();
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
    DrawKeyValuesSidebar(m_LeftSideWidth);
    ImGui::SameLine(0, 0);
    ManualSplitter("##S1", &m_LeftSideWidth, 80.0f, splitterW);
    ImGui::SameLine(0, 0);

    // Editor canvas
    ed::Begin("##Canvas", ImVec2(canvasW, 0));

    if (m_NavigateFrame > 0)
    {
        NavigateToOrigin();
        m_NavigateFrame = 0;
    }

    // --- Draw nodes ---
    {
        std::shared_lock<std::shared_mutex> evalLock(m_Graph.GetEvalMutex());
        for (auto& node : m_Graph.m_Nodes)
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

            DrawNodeContents(node);

            ImGui::Dummy(ImVec2(nodeWidth, 1));
        }
        ed::EndNode();

        ed::PopStyleColor(4);
    }

    // --- Draw links ---
    for (auto& link : m_Graph.m_Links)
        ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);
    } // end shared_lock

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
            auto startPin = m_Graph.FindPin(startPinId);
            auto endPin   = m_Graph.FindPin(endPinId);

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
                    for (auto& n : m_Graph.m_Nodes)
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
                        m_Graph.m_Links.emplace_back(m_Graph.GetNextId(), startPinId, endPinId);
                        m_Graph.m_Links.back().Color = GetIconColor(startPin->Type);
                        m_Graph.SetModified(true);
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
                m_Graph.m_Nodes.erase(
                    std::remove_if(m_Graph.m_Nodes.begin(), m_Graph.m_Nodes.end(),
                        [nodeId](auto& n) { return n.ID == nodeId; }),
                    m_Graph.m_Nodes.end());
                m_Graph.SetModified(true);
            }
        }

        ed::LinkId linkId = 0;
        while (ed::QueryDeletedLink(&linkId))
        {
            if (ed::AcceptDeletedItem())
            {
                m_Graph.m_Links.erase(
                    std::remove_if(m_Graph.m_Links.begin(), m_Graph.m_Links.end(),
                        [linkId](auto& l) { return l.ID == linkId; }),
                    m_Graph.m_Links.end());
                m_Graph.SetModified(true);
            }
        }
        }
    }
    ed::EndDelete();

    {
        ed::Suspend();
        ImVec2 originScreen = ed::CanvasToScreen(ImVec2(0, 0));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = 6.0f;
        const float arm = 12.0f;
        const ImU32 red = IM_COL32(255, 60, 60, 255);
        const ImU32 redDim = IM_COL32(200, 40, 40, 140);
        dl->AddLine(ImVec2(originScreen.x - arm, originScreen.y), ImVec2(originScreen.x + arm, originScreen.y), redDim, 2.5f);
        dl->AddLine(ImVec2(originScreen.x, originScreen.y - arm), ImVec2(originScreen.x, originScreen.y + arm), redDim, 2.5f);
        dl->AddCircleFilled(originScreen, r, red);
        dl->AddCircle(originScreen, r, IM_COL32(255, 255, 255, 80), 12, 2.0f);
        dl->AddText(ImVec2(originScreen.x + arm + 4, originScreen.y - 8), IM_COL32(200, 200, 200, 200), "Origin (0,0)");
        ed::Resume();
    }

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

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

        // -- Node Context Menu --
        if (ImGui::BeginPopup("Node Context Menu"))
        {
            auto* node = m_Graph.FindNode(contextNodeId);
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
                m_Graph.m_Links.erase(
                    std::remove_if(m_Graph.m_Links.begin(), m_Graph.m_Links.end(),
                        [pid](auto& l) { return (int)l.StartPinID.Get() == pid || (int)l.EndPinID.Get() == pid; }),
                    m_Graph.m_Links.end());
                m_Graph.SetModified(true);
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
            auto* node = m_Graph.FindNode(m_OutputComboNodeId);
            if (node && node->Type == NodeType::CustomOutput)
            {
                for (const auto& target : m_OutputTargets)
                {
                    bool sel = (node->OutputTarget == target.field_path);
                    if (ImGui::Selectable(target.name.c_str(), sel))
                    {
                        node->OutputTarget = target.field_path;
                        m_Graph.SetModified(true);
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
            auto* node = m_Graph.FindNode(m_KeySourcePopupNodeId);
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
                            m_Graph.SetModified(true);
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
            std::map<std::string, float> kvSnapshot = m_Graph.GetKeyValuesSnapshot();
            auto outputs = m_Graph.Evaluate(kvSnapshot);

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

    ImGui::End(); // Window
    return open;
}
