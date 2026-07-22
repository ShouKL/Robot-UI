#include "NodeGraphManager.h"
#include "FileManager.h"
#include "RobotStatus.h"
#include <functional>

namespace ed = ax::NodeEditor;

// ============================================================================
// ManualSplitter — shared resize handle helper
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
// ManagerBase: AddItem / RemoveItem / Select
// ============================================================================
void NodeGraphManager::AddItem()
{
    auto graph = std::make_unique<NodeGraph>();
    graph->id = NextId();
    snprintf(graph->name, sizeof(graph->name), "Item_%d", graph->id);
    if (m_RobotMgr)     graph->SetRobotComponentManager(m_RobotMgr);
    if (m_GamepadMgr)   graph->SetGamepadMapperManager(m_GamepadMgr);
    if (m_RobotCommMgr) graph->SetRobotCommManager(m_RobotCommMgr);
    if (m_ShortcutMgr)  graph->SetShortcutManager(m_ShortcutMgr);
    if (m_StoredSendActionCb) graph->SetSendActionCb(m_StoredSendActionCb);
    m_Items.push_back(std::move(graph));
    if (m_Items.size() == 1) {
        m_SelectedIndex = 0;
        m_Items[0]->isSelected = true;
        m_SelectedGraph = m_Items[0].get();
    }
}

void NodeGraphManager::RemoveItem(int id)
{
    //    unique_ptr  ，  FindNodeIndex（  .id → ->id）
    int index = -1;
    for (int i = 0; i < (int)m_Items.size(); ++i)
        if (m_Items[i]->id == id) { index = i; break; }
    if (index < 0 || index >= (int)m_Items.size()) return;
    if (m_Items.size() <= 1) return;
    m_Items.erase(m_Items.begin() + index);
    if (m_SelectedIndex >= (int)m_Items.size())
        m_SelectedIndex = (int)m_Items.size() - 1;
    if (!m_Items.empty()) {
        m_Items[m_SelectedIndex]->isSelected = true;
        m_SelectedGraph = m_Items[m_SelectedIndex].get();
    }
}

void NodeGraphManager::SetSelectedIndex(int idx)
{
    if (idx >= 0 && idx < (int)m_Items.size()) {
        SaveCurrentToItem();
        for (auto& it : m_Items) it->isSelected = false;
        m_SelectedIndex = idx;
        m_Items[idx]->isSelected = true;
        LoadItemToCurrent();
        m_SelectedGraph->RequestNavigate();
    }
}

void NodeGraphManager::RenameItem(int id, const char* newName)
{
    for (auto& graph : m_Items)
        if (graph->id == id) { strncpy_s(graph->name, newName, sizeof(graph->name) - 1); break; }
}

void NodeGraphManager::SaveCurrentToItem()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Items.size()) {
        auto& graph = m_Items[m_SelectedIndex];
        graph->editorYaml = GetGraphYaml();
        graph->LoadGraphData(graph->editorYaml);
    }
}

void NodeGraphManager::LoadItemToCurrent()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Items.size()) {
        auto& graph = m_Items[m_SelectedIndex];
        m_SelectedGraph = graph.get();
        //        \n        if (m_RobotMgr)     m_SelectedGraph->SetRobotComponentManager(m_RobotMgr);
        if (m_GamepadMgr)   m_SelectedGraph->SetGamepadMapperManager(m_GamepadMgr);
        if (m_RobotCommMgr) m_SelectedGraph->SetRobotCommManager(m_RobotCommMgr);
        if (m_ShortcutMgr)  m_SelectedGraph->SetShortcutManager(m_ShortcutMgr);
        if (m_StoredSendActionCb) m_SelectedGraph->SetSendActionCb(m_StoredSendActionCb);
        // mode name   YAML   （GetGraphYaml    ）
        LoadGraphYaml(graph->editorYaml);
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
NodeGraphManager::NodeGraphManager()
{
    ed::Config cfg;
    cfg.SettingsFile = nullptr;
    cfg.DragButtonIndex = ImGuiMouseButton_Right;     //      
    cfg.SelectButtonIndex = ImGuiMouseButton_Left;     //    /   
    cfg.NavigateButtonIndex = ImGuiMouseButton_Middle;
    m_EditorCtx = ed::CreateEditor(&cfg);
    ed::SetCurrentEditor(m_EditorCtx);
    AddItem();
    m_Items[0]->isSelected = true;
    m_SelectedGraph = m_Items[0].get();
    ed::SetCurrentEditor(nullptr);  // clear global context — DrawContent sets it on open
}

void NodeGraphManager::SetRobotCommManager(RobotCommManager* comm)
{
    m_RobotCommMgr = comm;
    if (m_SelectedGraph) m_SelectedGraph->SetRobotCommManager(comm);
}

NodeGraphManager::~NodeGraphManager()
{
    if (m_EditorCtx)
    {
        ed::SetCurrentEditor(nullptr);
        ed::DestroyEditor(m_EditorCtx);
        m_EditorCtx = nullptr;
    }
}

void NodeGraphManager::ApplyChanges()
{
    m_SelectedGraph->SaveGraphToMap();
    m_SelectedGraph->SetModified(false);
}

void NodeGraphManager::SetRobotComponentManager(RobotComponentManager* c)
{
    m_RobotMgr = c;
    if (m_SelectedGraph) m_SelectedGraph->SetRobotComponentManager(c);
}

void NodeGraphManager::SetGamepadMapperManager(GamepadMapperManager* g)
{
    m_GamepadMgr = g;
    if (m_SelectedGraph) m_SelectedGraph->SetGamepadMapperManager(g);
}

void NodeGraphManager::SetRobotStatus(RobotStatus* rs)
{
    m_RobotStatus = rs;
}

void NodeGraphManager::SetShortcutManager(ShortcutManager* sm)
{
    m_ShortcutMgr = sm;
    if (m_SelectedGraph) m_SelectedGraph->SetShortcutManager(sm);
    for (auto& g : m_Items)
        if (g) g->SetShortcutManager(sm);
}

void NodeGraphManager::SetSendActionCb(std::function<void(int,bool,bool)> cb)
{
    m_StoredSendActionCb = cb;
    if (m_SelectedGraph) m_SelectedGraph->SetSendActionCb(cb);
    for (auto& g : m_Items)
        if (g) g->SetSendActionCb(cb);
}

std::vector<NodeGraph> NodeGraphManager::GetAllItems() const
{
    // Save current changes to the active item before snapshotting
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Items.size()) {
        auto& graph = const_cast<NodeGraphManager*>(this)->m_Items[m_SelectedIndex];
        graph->editorYaml = const_cast<NodeGraphManager*>(this)->GetGraphYaml();
        graph->LoadGraphData(graph->editorYaml);
    }
    // Deep-clone all items
    std::vector<NodeGraph> snap;
    for (const auto& g : m_Items) {
        NodeGraph clone;
        clone.id = g->id;
        clone.isSelected = g->isSelected;
        strncpy_s(clone.name, g->name, sizeof(clone.name) - 1);
        clone.editorYaml = g->editorYaml;
        // Clone graph internals via YAML round-trip
        std::string yamlData = g->GetGraphDataYaml();
        if (!yamlData.empty()) clone.LoadGraphData(yamlData);
        snap.push_back(std::move(clone));
    }
    return snap;
}

void NodeGraphManager::LoadItems(const std::vector<NodeGraph>& items)
{
    auto kvSnapshot = m_SelectedGraph ? m_SelectedGraph->GetKeyValuesSnapshot() : std::map<std::string, float>{};
    int oldSelectedIdx = m_SelectedIndex;
    m_Items.clear();
    for (const auto& src : items) {
        auto graph = std::make_unique<NodeGraph>();
        graph->id = src.id;
        graph->isSelected = src.isSelected;
        strncpy_s(graph->name, src.name, sizeof(graph->name) - 1);
        graph->editorYaml = src.editorYaml;
        // Restore graph internals from YAML
        if (!src.editorYaml.empty()) {
            graph->LoadGraphData(src.editorYaml);
        }
        if (m_RobotMgr)     graph->SetRobotComponentManager(m_RobotMgr);
        if (m_GamepadMgr)   graph->SetGamepadMapperManager(m_GamepadMgr);
        if (m_RobotCommMgr) graph->SetRobotCommManager(m_RobotCommMgr);
        if (m_ShortcutMgr)  graph->SetShortcutManager(m_ShortcutMgr);
        m_Items.push_back(std::move(graph));
    }
    if (m_Items.empty()) {
        AddItem();
        m_Items[0]->isSelected = true;
        m_SelectedIndex = 0;
    } else {
        m_SelectedIndex = (oldSelectedIdx >= 0 && oldSelectedIdx < (int)m_Items.size()) ? oldSelectedIdx : 0;
        for (auto& it : m_Items) it->isSelected = false;
        m_Items[m_SelectedIndex]->isSelected = true;
    }
    LoadItemToCurrent();
    //     key values     
    if (m_SelectedGraph) m_SelectedGraph->SetKeyValues(kvSnapshot);
}

void NodeGraphManager::ResetToDefault()
{
    m_Items.clear();
    m_SelectedIndex = 0;
    AddItem();
    if (!m_Items.empty()) {
        m_Items[0]->isSelected = true;
        LoadItemToCurrent();
    }
}
// ============================================================================
std::string NodeGraphManager::GetGraphYaml() const
{
    ed::SetCurrentEditor(m_EditorCtx);
    return m_SelectedGraph->GetGraphYaml();
}

std::string NodeGraphManager::GetGraphYamlForIndex(int idx)
{
    if (idx < 0 || idx >= (int)m_Items.size()) return {};
    if (idx == m_SelectedIndex) {
        const_cast<NodeGraphManager*>(this)->SaveCurrentToItem();
    }
    return m_Items[idx]->GetGraphDataYaml();
}

std::string NodeGraphManager::GetGraphDataYamlForIndex(int idx)
{
    if (idx < 0 || idx >= (int)m_Items.size()) return {};
    //           
    if (idx == m_SelectedIndex && m_SelectedGraph)
        return m_SelectedGraph->GetGraphDataYaml();
    return m_Items[idx]->GetGraphDataYaml();
}

bool NodeGraphManager::LoadGraphYaml(const std::string& yamlStr)
{
    if (yamlStr.empty()) return false;
    ed::SetCurrentEditor(m_EditorCtx);
    return m_SelectedGraph->LoadGraphYaml(yamlStr);
}
// ============================================================================
// DrawContent — delegates all drawing to DrawNodeGraphEditor
// ============================================================================
void NodeGraphManager::DrawContent()
{
    if (m_SelectedGraph)
        DrawNodeGraphEditor(*m_SelectedGraph);
}

void NodeGraphManager::DrawNodeContents(NodeGraph& ng, EditorNode& node,
                                  const std::set<std::string>& analogKeys,
                                  const std::vector<OutputTargetInfo>& outputTargets)
{
    auto isLinked = [&ng](int pid) { return ng.IsPinLinked(ax::NodeEditor::PinId(pid)); };
    auto onMod = [&ng]() { ng.SetModified(true); };
    auto showBool = [](float v) {
        if (v >= 0.5f) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
        else           ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
    };

    // Helper to display a pin value according to its type
    auto ShowPinValue = [&](PinType type, float v) {
        switch (type) {
        case PinType::Bool: showBool(v); break;
        case PinType::Int:  ImGui::TextDisabled("%d", (int)std::round(v)); break;
        case PinType::Enum: ImGui::TextDisabled("%d", (int)std::round(v)); break;
        default:            ImGui::TextDisabled("%.3f", v); break;
        }
    };

    // Special: KeySource
    if (node.Type == NodeType::KeySource) {
        ImGui::PushID((int)node.ID.Get());
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        std::string btnLabel = node.KeyName.empty() ? "(click to select)" : node.KeyName;
        bool isActive = (ng.m_ActiveKeySourceId == node.ID);
        ImVec4 btnCol = isActive ? ImVec4(0.3f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        if (ImGui::Button(btnLabel.c_str(), ImVec2(105, 0))) {
            ng.m_ActiveKeySourceId = (ng.m_ActiveKeySourceId == node.ID) ? ax::NodeEditor::NodeId(0) : node.ID;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            node.KeyName.clear();
            ng.SetModified(true);
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
            ::DrawPinIcon(pin, ng.IsPinLinked(pin.ID), 255);
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
            ::DrawPinIcon(pin, ng.IsPinLinked(pin.ID), 255);
            ImGui::SameLine(); ImGui::SetNextItemWidth(70);
            if (ImGui::DragFloat("##Val", &node.Value, 0.01f, -100.0f, 100.0f, "%.3f"))
                ng.SetModified(true);
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
            ::DrawPinIcon(pin, ng.IsPinLinked(pin.ID), 255);
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
        bool isActive = (ng.m_ActiveOutputId == node.ID);
        ImVec4 btnCol = isActive ? ImVec4(0.3f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        if (ImGui::Button(btnLabel.c_str(), btnSize)) {
            ng.m_ActiveOutputId = (ng.m_ActiveOutputId == node.ID) ? ax::NodeEditor::NodeId(0) : node.ID;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            node.OutputTarget.clear();
            ng.SetModified(true);
        }

        ImGui::PopID();
        return;
    }

    // Special: ShortcutTrigger (pick action via right sidebar, like Output)
    if (node.Type == NodeType::ShortcutTrigger) {
        ImGui::PushID((int)node.ID.Get());

        // Input pin (Trigger)
        for (size_t i = 0; i < node.Inputs.size(); ++i) {
            auto& pin = node.Inputs[i];
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, ng.IsPinLinked(pin.ID), 255);
            ImGui::SameLine(); ImGui::TextUnformatted(pin.Name.c_str());
            ed::EndPin();
            ImGui::SameLine(0, 6);
            float v = (i < 4) ? node.InputValues[i] : 0.0f;
            if (pin.Type == PinType::Bool) showBool(v);
            else ImGui::TextDisabled("%.3f", v);
        }

        // Action selection button (like Output node)
        std::string btnLabel = "(click to select)";
        if (node.ShortcutSendIndex >= 0) {
            if (ng.m_CommMgr) {
                auto& nodes = ng.m_CommMgr->GetNodes();
                int flatIdx = 0;
                for (auto& nd : nodes) {
                    for (auto& sc : nd->protocol_send) {
                        if (flatIdx == node.ShortcutSendIndex) {
                            btnLabel = std::string(node.ShortcutSendMode == 0 ? "[Toggle] " : "[OneShot] ") + sc.name;
                            break;
                        }
                        ++flatIdx;
                    }
                }
            }
        } else if (node.ShortcutActionIndex >= 0) {
            const char* lbl = ShortcutManager::GetActionLabel(node.ShortcutActionIndex);
            if (lbl && lbl[0]) btnLabel = lbl;
        }

        bool isActive = (ng.m_ActiveTriggerId == node.ID);
        ImVec4 btnCol = isActive ? ImVec4(0.3f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        if (ImGui::Button(btnLabel.c_str(), ImVec2(155, 0))) {
            ng.m_ActiveTriggerId = (ng.m_ActiveTriggerId == node.ID) ? ax::NodeEditor::NodeId(0) : node.ID;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            node.ShortcutActionIndex = -1;
            node.ShortcutSendIndex = -1;
            ng.SetModified(true);
        }

        // Output pin
        for (auto& pin : node.Outputs) {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, ng.IsPinLinked(pin.ID), 255);
            ImGui::SameLine(); ImGui::TextUnformatted("Value");
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            if (node.Value >= 0.5f)
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
        }

        ImGui::PopID();
        return;
    }

    // Special: GlobalRead / GlobalWrite (click to select variable)
    if (node.Type == NodeType::GlobalRead || node.Type == NodeType::GlobalWrite) {
        ImGui::PushID((int)node.ID.Get());

        // Look up bound global variable for enum label display
        int gvidx = -1;
        const GlobalVar* gv = nullptr;
        if (node.GlobalVarId >= 0) {
            gvidx = ng.FindGlobalIndex(node.GlobalVarId);
            if (gvidx >= 0 && gvidx < (int)ng.m_GlobalVars.size())
                gv = &ng.m_GlobalVars[gvidx];
        }

        // Helper to display a value with enum label support
        auto ShowPinVal = [&](PinType ptype, float v) {
            if (ptype == PinType::Enum && gv && !gv->enumLabels.empty()) {
                int iv = (int)std::round(v);
                const char* label = (iv >= 0 && iv < (int)gv->enumLabels.size())
                    ? gv->enumLabels[iv].c_str() : "?";
                ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.9f, 1.0f), "%s", label);
            } else if (ptype == PinType::Bool) {
                showBool(v);
            } else if (ptype == PinType::Int || ptype == PinType::Enum) {
                ImGui::TextDisabled("%d", (int)std::round(v));
            } else {
                ImGui::TextDisabled("%.3f", v);
            }
        };

        // Draw input pins (if any) — same as generic
        for (size_t i = 0; i < node.Inputs.size(); ++i) {
            auto& pin = node.Inputs[i];
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, ng.IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            float v = (i < 4) ? node.InputValues[i] : 0.0f;
            ShowPinVal(pin.Type, v);
        }

        // Variable selector button (like KeySource, uses GlobalVarId)
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        std::string btnLabel = "(click to select)";
        if (node.GlobalVarId >= 0) {
            int gidx = ng.FindGlobalIndex(node.GlobalVarId);
            if (gidx >= 0 && gidx < (int)ng.m_GlobalVars.size())
                btnLabel = ng.m_GlobalVars[gidx].name;
        }
        bool isActive = (ng.m_ActiveGlobalReadId == node.ID);
        ImVec4 btnCol = isActive ? ImVec4(0.3f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        if (ImGui::Button(btnLabel.c_str(), ImVec2(105, 0))) {
            ng.m_ActiveGlobalReadId = (ng.m_ActiveGlobalReadId == node.ID) ? ax::NodeEditor::NodeId(0) : node.ID;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            node.GlobalVarId = -1;
            ng.SetModified(true);
        }
        ImGui::PopStyleVar();

        // Output pin
        for (auto& pin : node.Outputs) {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, ng.IsPinLinked(pin.ID), 255);
            ImGui::SameLine(); ImGui::TextUnformatted(pin.Name.c_str());
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            ShowPinVal(pin.Type, node.Value);
        }
        ImGui::PopID();
        return;
    }

    // Special: TriggerTable — dynamic trigger pins + value table
    if (node.Type == NodeType::TriggerTable) {
        ImGui::PushID((int)node.ID.Get());

        // ---- Sync input pins to slot count ----
        int desired = (int)node.Param[0];
        if (desired < 1) { desired = 1; node.Param[0] = 1.0f; }
        if (desired > 8) { desired = 8; node.Param[0] = 8.0f; }

        // Remove excess pins (and any links to them)
        while ((int)node.Inputs.size() > desired) {
            auto& lastPin = node.Inputs.back();
            // Remove links connected to this pin
            ng.m_Links.erase(
                std::remove_if(ng.m_Links.begin(), ng.m_Links.end(),
                    [&](const EditorLink& l) {
                        return l.StartPinID == lastPin.ID || l.EndPinID == lastPin.ID;
                    }), ng.m_Links.end());
            node.Inputs.pop_back();
            ng.SetModified(true);
        }
        // Add needed pins
        while ((int)node.Inputs.size() < desired) {
            int idx = (int)node.Inputs.size();
            char nameBuf[8];
            snprintf(nameBuf, sizeof(nameBuf), "T%d", idx);
            node.Inputs.push_back(EditorPin(ng.GetNextId(), nameBuf, PinType::Float));
            ng.SetModified(true);
        }

        // ---- Draw trigger pins ----
        for (size_t i = 0; i < node.Inputs.size(); ++i) {
            auto& pin = node.Inputs[i];
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ::DrawPinIcon(pin, ng.IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            float v = (i < 4) ? node.InputValues[i] : 0.0f;
            ShowPinValue(pin.Type, v);
        }

        // ---- Slot controls & value table ----
        ImGui::PushID("Slots");
        auto& labels = node.ModeLabels;
        if (labels.empty()) labels.push_back("0");

        int slots = (int)node.Param[0];
        if (ImGui::Button("+ Slot") && slots < 8) {
            node.Param[0] = (float)(slots + 1);
            if ((int)labels.size() <= slots) labels.push_back("0");
            onMod();
        }
        ImGui::SameLine();
        if (ImGui::Button("- Slot") && slots > 1) {
            node.Param[0] = (float)(slots - 1);
            onMod();
        }

        int count = (int)labels.size();
        for (int s = 0; s < slots; ++s) {
            ImGui::PushID(s);

            bool trig = (s < 4) ? node.InputBools[s] : false;
            ImVec4 tCol = trig ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(0.5f, 0.2f, 0.2f, 1.0f);
            ImGui::TextColored(tCol, "[%d]", s);
            ImGui::SameLine();

            if (s < count) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%s", labels[s].c_str());
                ImGui::SetNextItemWidth(100);
                bool confirmed = ImGui::InputText("##val", buf, sizeof(buf),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal);
                if (confirmed || ImGui::IsItemDeactivatedAfterEdit()) {
                    labels[s] = buf;
                    onMod();
                }
            } else {
                ImGui::TextDisabled("(no label)");
            }

            ImGui::PopID();
        }
        ImGui::PopID();

        // ---- Separator & output pin ----
        {
            float sepWidth = 140.0f;
            float padX = ImGui::GetStyle().FramePadding.x;
            ImGui::Dummy(ImVec2(sepWidth, 2));
            auto* dl = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(cursor.x - padX, cursor.y),
                        ImVec2(cursor.x + sepWidth + padX, cursor.y),
                        IM_COL32(100, 100, 100, 255), 1.0f);
            ImGui::Dummy(ImVec2(sepWidth, 2));
        }

        for (auto& pin : node.Outputs) {
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ::DrawPinIcon(pin, ng.IsPinLinked(pin.ID), 255);
            ImGui::SameLine();
            ImGui::TextUnformatted(pin.Name.c_str());
            ed::EndPin();
            ImGui::SameLine(0, 6);
            DrawPinTypeSelector(&pin, onMod);
            ImGui::SameLine(0, 8);
            ShowPinValue(pin.Type, node.Value);
        }

        ImGui::PopID();
        return;
    }

    // Generic: all other node types
    std::vector<std::string> avKeys(ng.m_AvailableKeys.begin(), ng.m_AvailableKeys.end());
    std::vector<std::string> outNames, outPaths;
    for (auto& t : outputTargets) { outNames.push_back(t.name); outPaths.push_back(t.field_path); }
    DrawGenericNodeBody(node, analogKeys, avKeys, outNames, outPaths, isLinked, onMod);
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
        // Skip KeySource/ConstValue/CustomOutput/ShortcutTrigger in categories
        if (ImGui::BeginMenu(GetCategoryName(cat))) {
            for (int i = 0; i < cnt; ++i) {
                NodeType nt = GetCategoryNodeType(cat, i);
                if (nt == NodeType::KeySource || nt == NodeType::ConstValue ||
                    nt == NodeType::CustomOutput || nt == NodeType::ShortcutTrigger)
                    continue;
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
    if (ImGui::MenuItem("Shortcut"))    addFn(NodeType::ShortcutTrigger);
}

// ============================================================================
// DrawMenuBar — removed; Play/Stop moved inline, Add Node is right-click
// ============================================================================
void NodeGraphManager::DrawKeyValuesSidebar(NodeGraph& ng, float sideWidth, const std::set<std::string>& analogKeys)
{
    std::map<std::string, float> snapshot = ng.GetKeyValuesSnapshot();

    // Build deduped key list
    std::set<std::string> keys;
    for (const auto& k : ng.m_AvailableKeys) keys.insert(k);
    for (const auto& [name, val] : snapshot) keys.insert(name);

    EditorNode* activeKS = ng.FindNode(ng.m_ActiveKeySourceId);
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
                    ng.SetModified(true);
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
}

// ============================================================================
// DrawGlobalsSidebar — managed in RobotStatus panel "Graph Variables" section
// ============================================================================
void NodeGraphManager::DrawGlobalsSidebar(NodeGraph& ng, float) {}

// ============================================================================
// DrawCommRefsSidebar — manage which RobotComm items are referenced
// ============================================================================
void NodeGraphManager::DrawCommRefsSidebar(NodeGraph& ng, float sideWidth)
{
    if (!ng.m_CommMgr || ng.m_CommMgr->GetItemCount() <= 0) {
        ImGui::TextDisabled("No comm configs");
        return;
    }

    int commCount = ng.m_CommMgr->GetItemCount();
    (void)sideWidth;

    // List all comm configs as selectable items (no dropdown)
    for (int j = 0; j < commCount; ++j) {
        ImGui::PushID(20000 + j);

        // Check if this comm is already referenced
        bool isActive = false;
        for (int ref : ng.m_CommRefs) {
            if (ref == j) { isActive = true; break; }
        }

        if (ImGui::Selectable(ng.m_CommMgr->GetItemNameBuf(j), isActive)) {
            if (isActive) {
                // Remove it
                auto it = std::find(ng.m_CommRefs.begin(), ng.m_CommRefs.end(), j);
                if (it != ng.m_CommRefs.end()) ng.m_CommRefs.erase(it);
            } else {
                // Add it
                ng.m_CommRefs.push_back(j);
            }
            ng.SetModified(true);
        }
        if (isActive) ImGui::SetItemDefaultFocus();

        ImGui::PopID();
    }
}

// ============================================================================
// DrawShortcutSidebar — always shows all shortcuts; click to assign to active ShortcutTrigger
// ============================================================================
void NodeGraphManager::DrawTriggerSidebar(NodeGraph& ng, float sideWidth)
{
    EditorNode* activeTrig = ng.FindNode(ng.m_ActiveTriggerId);
    if (activeTrig && activeTrig->Type != NodeType::ShortcutTrigger) {
        ng.m_ActiveTriggerId = ax::NodeEditor::NodeId(0);
        activeTrig = nullptr;
    }

    (void)sideWidth;

    // ---- Panel actions ----
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Panel Actions");
    if (ng.m_ShortcutMgr) {
        for (int i = 0; i < ng.m_ShortcutMgr->GetActionCount(); ++i) {
            const char* label = ShortcutManager::GetActionLabel(i);
            if (!label || !label[0]) continue;
            if (strncmp(label, "File ", 5) == 0) continue;
            bool sel = (activeTrig && activeTrig->ShortcutSendIndex < 0 && activeTrig->ShortcutActionIndex == i);
            if (ImGui::Selectable(label, sel)) {
                if (activeTrig) {
                    activeTrig->ShortcutActionIndex = i;
                    activeTrig->ShortcutSendIndex = -1;
                    ng.SetModified(true);
                }
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
    }

    // ---- Send frame actions (only from active comm refs) ----
    if (ng.m_CommMgr) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Send Frames (Toggle)");
        auto& nodes = ng.m_CommMgr->GetNodes();
        int flatIdx = 0;
        for (int ci : ng.m_CommRefs) {
            if (ci < 0 || ci >= (int)nodes.size()) continue;
            for (auto& sc : nodes[ci]->protocol_send) {
                std::string label = std::string("T: ") + sc.name;
                bool sel = (activeTrig && activeTrig->ShortcutSendIndex == flatIdx && activeTrig->ShortcutSendMode == 0);
                if (ImGui::Selectable(label.c_str(), sel)) {
                    if (activeTrig) {
                        activeTrig->ShortcutSendIndex = flatIdx;
                        activeTrig->ShortcutSendMode = 0;
                        activeTrig->ShortcutActionIndex = -1;
                        ng.SetModified(true);
                    }
                }
                if (sel) ImGui::SetItemDefaultFocus();
                ++flatIdx;
            }
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Send Frames (One-Shot)");
        flatIdx = 0;
        for (int ci : ng.m_CommRefs) {
            if (ci < 0 || ci >= (int)nodes.size()) continue;
            for (auto& sc : nodes[ci]->protocol_send) {
                std::string label = std::string("1: ") + sc.name;
                bool sel = (activeTrig && activeTrig->ShortcutSendIndex == flatIdx && activeTrig->ShortcutSendMode == 1);
                if (ImGui::Selectable(label.c_str(), sel)) {
                    if (activeTrig) {
                        activeTrig->ShortcutSendIndex = flatIdx;
                        activeTrig->ShortcutSendMode = 1;
                        activeTrig->ShortcutActionIndex = -1;
                        ng.SetModified(true);
                    }
                }
                if (sel) ImGui::SetItemDefaultFocus();
                ++flatIdx;
            }
        }
    }

    if (!activeTrig && ng.m_ShortcutMgr)
        ImGui::TextDisabled("\n(click a ShortcutTrigger node to assign)");
}

// ============================================================================
// Draw — full editor render (called from Manager)
// ============================================================================
void NodeGraphManager::DrawNodeGraphEditor(NodeGraph& ng)
{
    if (ng.m_IsRunning)
        ng.EvaluateForDisplay(ng.GetKeyValuesSnapshot());

    // -------- Canvas width (computed early for button centering) --------
    float splitterW = 5.0f;
    float totalAvail = ImGui::GetContentRegionAvail().x;
    float canvasW = totalAvail - ng.m_LeftSideWidth - ng.m_RightSideWidth - splitterW * 2;
    if (canvasW < 100.0f) canvasW = 100.0f;

    // Play/Stop button — centered in canvas area only
    // MUST be rendered BEFORE BeginDisabled() so the Stop button remains clickable
    {
        if (!ng.m_PlayIcon)  ng.m_PlayIcon  = std::make_shared<Walnut::Image>(FileManager::GetExeDir() + "..\\..\\..\\asset\\picture\\PlayButton.png");
        if (!ng.m_StopIcon)  ng.m_StopIcon  = std::make_shared<Walnut::Image>(FileManager::GetExeDir() + "..\\..\\..\\asset\\picture\\StopButton.png");

        auto icon = ng.m_IsRunning ? ng.m_StopIcon : ng.m_PlayIcon;
        ImVec2 iconSize(20, 20);

        float btnCenterX = ng.m_LeftSideWidth + splitterW + canvasW * 0.5f;
        ImGui::SetCursorPosX(btnCenterX - iconSize.x * 0.5f);

        if (icon->GetDescriptorSet()) {
            if (ImGui::ImageButton((ImTextureID)icon->GetDescriptorSet(), iconSize)) {
                ng.ToggleRunning();
            }
        } else {
            const char* label = ng.m_IsRunning ? " Stop " : " Play ";
            ImVec4 col = ng.m_IsRunning ? ImVec4(0.7f, 0.15f, 0.15f, 1.0f) : ImVec4(0.15f, 0.6f, 0.15f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            if (ImGui::Button(label, ImVec2(iconSize.x * 2, 0))) {
                ng.ToggleRunning();
            }
            ImGui::PopStyleColor(2);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(ng.m_IsRunning ? "Stop evaluation" : "Start evaluation");
    }

    // Disable editing while running (after Play/Stop button so Stop remains clickable)
    ed::SetCurrentEditor(m_EditorCtx);
    bool wasRunning = ng.m_IsRunning;
    if (wasRunning) {
        ImGui::BeginDisabled();
        ed::EnableShortcuts(false);
    }

    // -------- Pull data from managers --------
    std::vector<std::string> gamepadModeNames;
    {
        if (ng.m_GamepadMgr) {
            for (const auto& gm : ng.m_GamepadMgr->GetMappers())
                gamepadModeNames.push_back(gm.name);
        }
    }

    // Compute analog keys & output targets & available keys from active modes
    {
        ng.m_AnalogKeys.clear();
        ng.m_AvailableKeys.clear();
        ng.m_OutputTargets.clear();
        if (ng.m_GamepadMgr) {
            for (auto& gm : ng.m_GamepadMgr->GetMappers()) {
                if (std::string(gm.name) == ng.m_ActiveGamepadModeName) {
                    for (const auto& mapping : gm.mappings) {
                        ng.m_AvailableKeys.push_back(mapping.key_name);
                        if (mapping.is_analog)
                            ng.m_AnalogKeys.insert(mapping.key_name);
                    }
                    // 直接从当前选中 mapper 读取实时值，更新侧栏
                    std::map<std::string, float> kv;
                    for (const auto& m : gm.mappings) {
                        if (m.is_bound)
                            kv[m.key_name] = gm.GetKeyValue(m.key_name);
                    }
                    ng.SetKeyValues(kv);
                    break;
                }
            }
        }
        // Build output targets from each RobotComm's protocol_send
        ng.m_OutputTargets.clear();
        if (ng.m_CommMgr) {
            // Get actuator config for component display names (optional)
            const ActuatorConfig* actuator = nullptr;
            if (ng.m_RobotMgr) {
                for (auto& c : ng.m_RobotMgr->GetComponents()) {
                    if (std::string(c.name) == ng.m_ActiveRobotModeName) {
                        actuator = &c.actuator_config;
                        break;
                    }
                }
            }
            for (int ci = 0; ci < (int)ng.m_CommRefs.size(); ++ci) {
                int realIdx = ng.m_CommRefs[ci];
                if (realIdx < 0 || realIdx >= ng.m_CommMgr->GetItemCount()) continue;
                auto commCfgs = ng.m_CommMgr->GetAllItems();
                if (realIdx >= (int)commCfgs.size()) continue;
                std::string commName = std::string(commCfgs[realIdx].name) + " > ";
                for (const auto& p : commCfgs[realIdx].protocol_send) {
                    if (actuator) {
                        auto targets = BuildOutputTargetsFromProtocol({p}, *actuator);
                        for (auto& t : targets) {
                            t.name = commName + t.name;
                            t.comm_index = ci;
                            ng.m_OutputTargets.push_back(t);
                        }
                    } else {
                        // No actuator match — use raw field names
                        for (const auto& f : p.fields) {
                            if (f.fix) continue;
                            OutputTargetInfo t;
                            t.name       = commName + (f.name.empty() ? f.field_path : f.name);
                            t.field_path = f.field_path;
                            t.encoding   = f.encoding;
                            t.comm_index = ci;
                            ng.m_OutputTargets.push_back(t);
                        }
                    }
                }
            }
        }
    }

    // -------- interaction state --------
    static ed::NodeId contextNodeId = 0;
    static ed::LinkId contextLinkId = 0;
    static ed::PinId  contextPinId  = 0;

    // -------- Resizable three-panel layout --------
    // Recalculate canvasW in case layout changed
    totalAvail = ImGui::GetContentRegionAvail().x;
    canvasW = totalAvail - ng.m_LeftSideWidth - ng.m_RightSideWidth - splitterW * 2;
    if (canvasW < 100.0f) {
        canvasW = 100.0f;
        float excess = totalAvail - canvasW - splitterW * 2;
        float ratio = excess > 0 ? ng.m_LeftSideWidth / (ng.m_LeftSideWidth + ng.m_RightSideWidth) : 0.5f;
        ng.m_LeftSideWidth  = ratio * excess;
        ng.m_RightSideWidth = (1.0f - ratio) * excess;
        if (ng.m_LeftSideWidth < 200.0f) ng.m_LeftSideWidth = 200.0f;
        if (ng.m_RightSideWidth < 80.0f) ng.m_RightSideWidth = 80.0f;
        canvasW = totalAvail - ng.m_LeftSideWidth - ng.m_RightSideWidth - splitterW * 2;
        if (canvasW < 100.0f) canvasW = 100.0f;
    }

    // Left sidebar — gamepad + key values
    ImGui::BeginChild("##LeftCol", ImVec2(ng.m_LeftSideWidth, 0));
    // Upper: Gamepad/Output mode selectors
    if (!gamepadModeNames.empty()) {
        ImGui::TextUnformatted("GamepadMapper");
        ImGui::Separator();
        float listH = ImGui::GetTextLineHeightWithSpacing() * (float)gamepadModeNames.size() + 8;
        if (listH > 200.0f) listH = 200.0f;
        ImGui::BeginChild("##GPPane", ImVec2(0, listH), true);
        for (int i = 0; i < (int)gamepadModeNames.size(); ++i) {
            bool sel = (gamepadModeNames[i] == ng.m_ActiveGamepadModeName);
            if (ImGui::Selectable(gamepadModeNames[i].c_str(), sel)) {
                ng.SwitchGamepadMode(ng.GetActiveRobotModeName(), gamepadModeNames[i]);
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndChild();
    }
    // Lower: Key Values
    ImGui::TextUnformatted("Input Keys");
    ImGui::Separator();
    ImGui::BeginChild("##KVPane", ImVec2(0.0f, gamepadModeNames.empty() ? 0.0f : -30.0f), true);
    DrawKeyValuesSidebar(ng, ng.m_LeftSideWidth - 16.0f, ng.m_AnalogKeys);
    ImGui::EndChild();
    ImGui::EndChild(); // ##LeftCol
    ImGui::SameLine(0, 0);
    ManualSplitter("##S1", &ng.m_LeftSideWidth, 200.0f, splitterW);
    ImGui::SameLine(0, 0);

    // Navigate BEFORE ed::Begin so NavigateToContent processes on this frame.
    // Calling it inside Begin/End would defer to the next frame, causing a flicker.
    if (ng.m_NavigateFrame > 0)
    {
        ng.NavigateToOrigin();
        ng.m_NavigateFrame = 0;
    }

    ed::Begin("##Canvas", ImVec2(canvasW, 0));

    // --- Draw nodes ---
    {
        std::shared_lock<std::shared_mutex> evalLock(ng.GetEvalMutex());
        for (auto& node : ng.m_Nodes)
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
            else if (node.Type == NodeType::ShortcutTrigger) nodeWidth = 200.0f;
            else if (totalPins <= 4) nodeWidth = 120.0f;

            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)node.Color);
            ImGui::TextUnformatted(GetNodeTitle(node.Type));
            ImGui::PopStyleColor();

            // Ensure a clickable dead-zone for node selection — interactives
            // (Button, DragFloat, etc.) eat clicks, so provide spare space.
            ImGui::Dummy(ImVec2(nodeWidth, 3));

            auto* dl = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(cursor.x - ImGui::GetStyle().FramePadding.x, cursor.y),
                        ImVec2(cursor.x + nodeWidth - ImGui::GetStyle().FramePadding.x, cursor.y),
                        IM_COL32(100, 100, 100, 255), 1.0f);
            ImGui::Dummy(ImVec2(nodeWidth, 2));

            DrawNodeContents(ng, node, ng.m_AnalogKeys, ng.m_OutputTargets);

            // Clickable dead-zone at the bottom of every node for selection
            ImGui::Dummy(ImVec2(nodeWidth, 6.0f));
        }
        ed::EndNode();

        ed::PopStyleColor(4);
    }

    // --- Draw links ---
    for (auto& link : ng.m_Links)
        ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);
    } // end shared_lock

    // --- Click-to-connect ---
    // Left-click a pin to start, left-click another to connect, right-click to cancel.
    if (!ng.m_IsRunning)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        ed::PinId hoveredId = ed::GetHoveredPin();
        bool leftReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

        if (ng.m_LinkSourcePin.Get())
        {
            auto* srcPin = ng.FindPin(ng.m_LinkSourcePin);
            if (srcPin)
            {
                ImVec2 mouse = ImGui::GetMousePos();
                drawList->AddLine(ng.m_LinkSourceMouse, mouse, IM_COL32(255,255,128,200), 2.0f);
                drawList->AddCircleFilled(ng.m_LinkSourceMouse, 4.0f, IM_COL32(255,255,128,255));

                if (leftReleased)
                {
                    if (hoveredId.Get())
                    {
                        auto* tgt = ng.FindPin(hoveredId);
                        if (tgt && tgt != srcPin && tgt->Node != srcPin->Node
                            && PinTypesCompatible(srcPin->Type, tgt->Type))
                        {
                            ed::PinId outId = ng.m_LinkSourcePin, inId = hoveredId;
                            bool si=false, ti=false;
                            for (auto& n : ng.m_Nodes) {
                                for (auto& p : n.Inputs) { if(p.ID==outId)si=true; if(p.ID==inId)ti=true; }
                            }
                            if (si && !ti) std::swap(outId, inId);
                            else if (si == ti) { ng.m_LinkSourcePin = ed::PinId(0); goto cc_end; }
                            ng.m_Links.emplace_back(ng.GetNextId(), outId, inId);
                            ng.m_Links.back().Color = GetIconColor(srcPin->Type);
                            ng.SetModified(true);
                        }
                    }
                    ng.m_LinkSourcePin = ed::PinId(0);
                }
                else if (rightClicked) { ng.m_LinkSourcePin = ed::PinId(0); }
                cc_end:;
            }
            else { ng.m_LinkSourcePin = ed::PinId(0); }
        }
        else if (hoveredId.Get() && leftReleased)
        {
            ng.m_LinkSourcePin = hoveredId;
            ng.m_LinkSourceMouse = ImGui::GetMousePos();
        }
    }

    if (ed::BeginDelete())
    {
        if (!ng.m_IsRunning)
        {
            ed::NodeId nodeId = 0;
            while (ed::QueryDeletedNode(&nodeId))
            {
                if (ed::AcceptDeletedItem())
                {
                    ng.m_Nodes.erase(
                        std::remove_if(ng.m_Nodes.begin(), ng.m_Nodes.end(),
                            [nodeId](auto& n) { return n.ID == nodeId; }),
                        ng.m_Nodes.end());
                    ng.SetModified(true);
                }
            }

            ed::LinkId linkId = 0;
            while (ed::QueryDeletedLink(&linkId))
            {
                if (ed::AcceptDeletedItem())
                {
                    ng.m_Links.erase(
                        std::remove_if(ng.m_Links.begin(), ng.m_Links.end(),
                            [linkId](auto& l) { return l.ID == linkId; }),
                        ng.m_Links.end());
                    ng.SetModified(true);
                }
            }
        } // if (!ng.m_IsRunning)
    }
    ed::EndDelete();

    bool showNewNodePopup = false;
    bool showNodePopup    = false;
    bool showPinPopup     = false;
    bool showLinkPopup    = false;
    {
        ed::Suspend();

        if (!ng.m_IsRunning) {
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

    // --- Copy / Paste (Ctrl+C / Ctrl+V) ---
    if (!ng.m_IsRunning)
    {
        ImGuiIO& io = ImGui::GetIO();
        bool ctrlC = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C);
        bool ctrlV = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V);

        if (ctrlC)
        {
            // Collect selected nodes and links between them
            std::set<int> selectedIds;
            for (auto& n : ng.m_Nodes)
            {
                if (ed::IsNodeSelected(n.ID))
                    selectedIds.insert((int)n.ID.Get());
            }

            if (!selectedIds.empty())
            {
                YAML::Emitter out;
                out << YAML::BeginMap;
                out << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
                for (auto& n : ng.m_Nodes)
                {
                    if (!selectedIds.count((int)n.ID.Get())) continue;
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
                    out << YAML::Key << "param" << YAML::Value << YAML::BeginSeq;
                    for (int i = 0; i < 8; ++i) out << n.Param[i];
                    out << YAML::EndSeq;
                    out << YAML::Key << "mode_labels" << YAML::Value << YAML::BeginSeq;
                    for (auto& s : n.ModeLabels) out << s;
                    out << YAML::EndSeq;
                    out << YAML::Key << "state_f" << YAML::Value << YAML::BeginSeq;
                    for (int i = 0; i < 4; ++i) out << n.StateF[i];
                    out << YAML::EndSeq;
                    out << YAML::Key << "input_types" << YAML::Value << YAML::BeginSeq;
                    for (auto& p : n.Inputs) out << (int)p.Type;
                    out << YAML::EndSeq;
                    out << YAML::Key << "output_types" << YAML::Value << YAML::BeginSeq;
                    for (auto& p : n.Outputs) out << (int)p.Type;
                    out << YAML::EndSeq;
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;

                out << YAML::Key << "links" << YAML::Value << YAML::BeginSeq;
                for (auto& l : ng.m_Links)
                {
                    int fromNodeId = -1, toNodeId = -1;
                    int fromPinIdx = -1, toPinIdx = -1;
                    for (auto& n : ng.m_Nodes) {
                        for (size_t i = 0; i < n.Outputs.size(); ++i)
                            if (n.Outputs[i].ID == l.StartPinID) { fromNodeId = (int)n.ID.Get(); fromPinIdx = (int)i; break; }
                        for (size_t i = 0; i < n.Inputs.size(); ++i)
                            if (n.Inputs[i].ID == l.EndPinID)   { toNodeId   = (int)n.ID.Get(); toPinIdx   = (int)i; break; }
                    }
                    // Only include links where BOTH endpoints are selected
                    if (fromNodeId < 0 || toNodeId < 0) continue;
                    if (!selectedIds.count(fromNodeId) || !selectedIds.count(toNodeId)) continue;

                    out << YAML::BeginMap;
                    out << YAML::Key << "from_node" << YAML::Value << fromNodeId;
                    out << YAML::Key << "from_pin"  << YAML::Value << fromPinIdx;
                    out << YAML::Key << "to_node"   << YAML::Value << toNodeId;
                    out << YAML::Key << "to_pin"    << YAML::Value << toPinIdx;
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
                out << YAML::EndMap;

                m_ClipboardYaml = out.c_str();
                ImGui::SetClipboardText(m_ClipboardYaml.c_str());
            }
        }

        if (ctrlV && !m_ClipboardYaml.empty())
        {
            // Paste at mouse position with offset
            ImVec2 pastePos = ed::ScreenToCanvas(ImGui::GetMousePos());

            std::map<int, int> oldToNewNode;
            std::map<int, int> oldToNewPin;
            std::vector<std::pair<int, int>> linkData; // (fromNode_old, fromPin), (toNode_old, toPin)

            try {
                YAML::Node root = YAML::Load(m_ClipboardYaml);
                if (!root.IsMap()) goto paste_end;

                // Find min position to compute offset
                ImVec2 minPos(FLT_MAX, FLT_MAX);
                bool hasPos = false;

                // Phase 1: Create nodes
                if (root["nodes"] && root["nodes"].IsSequence())
                {
                    for (auto yn : root["nodes"])
                    {
                        int oldId = yn["id"].as<int>();
                        NodeType nt = (NodeType)yn["type"].as<int>();

                        // Skip position-only nodes
                        if (nt == NodeType::CustomOutput || nt == NodeType::ShortcutTrigger ||
                            nt == NodeType::KeySource || nt == NodeType::ConstValue ||
                            nt == NodeType::GlobalRead || nt == NodeType::GlobalWrite)
                        {
                            // These are handled generically
                        }

                        EditorNode* newNodePtr = ng.SpawnNode(nt);
                        if (!newNodePtr) continue;

                        int newId = ng.GetNextId();
                        oldToNewNode[oldId] = newId;
                        newNodePtr->ID = ax::NodeEditor::NodeId(newId);

                        // Copy data
                        newNodePtr->Value = yn["value"] ? yn["value"].as<float>() : 0.0f;
                        newNodePtr->KeyName = yn["key_name"] ? yn["key_name"].as<std::string>() : "";
                        newNodePtr->OpMode = yn["op_mode"] ? yn["op_mode"].as<int>() : 0;
                        newNodePtr->OutputTarget = yn["output_target"] ? yn["output_target"].as<std::string>() : "";
                        newNodePtr->CommIndex = yn["comm_index"] ? yn["comm_index"].as<int>() : 0;
                        newNodePtr->GlobalVarId = yn["global_var_id"] ? yn["global_var_id"].as<int>() : -1;
                        if (yn["shortcut_action"])
                            newNodePtr->ShortcutActionIndex = yn["shortcut_action"].as<int>();
                        if (yn["shortcut_send_index"])
                        {
                            newNodePtr->ShortcutSendIndex = yn["shortcut_send_index"].as<int>();
                            newNodePtr->ShortcutSendMode  = yn["shortcut_send_mode"] ? yn["shortcut_send_mode"].as<int>() : 0;
                        }
                        if (yn["param"] && yn["param"].IsSequence())
                            for (size_t i = 0; i < yn["param"].size() && i < 8; ++i)
                                newNodePtr->Param[i] = yn["param"][i].as<float>();
                        if (yn["mode_labels"] && yn["mode_labels"].IsSequence())
                            for (auto ml : yn["mode_labels"])
                                newNodePtr->ModeLabels.push_back(ml.as<std::string>());

                        // Remap pins
                        for (auto& pin : newNodePtr->Inputs) {
                            int newPinId = ng.GetNextId();
                            oldToNewPin[(int)pin.ID.Get()] = newPinId;
                            pin.ID = ax::NodeEditor::PinId(newPinId);
                            pin.Node = nullptr;
                        }
                        for (auto& pin : newNodePtr->Outputs) {
                            int newPinId = ng.GetNextId();
                            oldToNewPin[(int)pin.ID.Get()] = newPinId;
                            pin.ID = ax::NodeEditor::PinId(newPinId);
                            pin.Node = nullptr;
                        }

                        // Position: offset from original or mouse
                        ed::SetNodePosition(newNodePtr->ID, pastePos);
                        pastePos.x += 200.0f;

                        ng.m_Nodes.push_back(std::move(*newNodePtr));
                        // Need to get reference to the pushed node for pin remap
                    }
                }

                // Phase 2: Restore links
                if (root["links"] && root["links"].IsSequence())
                {
                    for (auto yl : root["links"])
                    {
                        int fromNode = yl["from_node"].as<int>();
                        int fromPin  = yl["from_pin"].as<int>();
                        int toNode   = yl["to_node"].as<int>();
                        int toPin    = yl["to_pin"].as<int>();

                        auto itFromNode = oldToNewNode.find(fromNode);
                        auto itToNode   = oldToNewNode.find(toNode);
                        if (itFromNode == oldToNewNode.end() || itToNode == oldToNewNode.end())
                            continue;

                        // Find the new pins
                        EditorNode* srcNode = ng.FindNode(ax::NodeEditor::NodeId(itFromNode->second));
                        EditorNode* dstNode = ng.FindNode(ax::NodeEditor::NodeId(itToNode->second));
                        if (!srcNode || !dstNode) continue;

                        ed::PinId startPin(0), endPin(0);
                        if (fromPin >= 0 && fromPin < (int)srcNode->Outputs.size())
                            startPin = srcNode->Outputs[fromPin].ID;
                        if (toPin >= 0 && toPin < (int)dstNode->Inputs.size())
                            endPin = dstNode->Inputs[toPin].ID;

                        if (startPin.Get() && endPin.Get())
                        {
                            ng.m_Links.emplace_back(ng.GetNextId(), startPin, endPin);
                        }
                    }
                }

                ng.RebuildAllNodes();
                ng.SetModified(true);
            }
            catch (const std::exception&) {}
            paste_end:;
        }
    }

    // --- Handle node editor shortcuts (Space=create, Ctrl+D=duplicate, etc.) ---
    if (!ng.m_IsRunning && ed::BeginShortcut())
    {
        if (ed::AcceptCreateNode())
        {
            showNewNodePopup = true;
        }
        else if (ed::AcceptDuplicate())
        {
            // Duplicate selected nodes + their internal links
            int ctxSize = ed::GetActionContextSize();
            if (ctxSize > 0)
            {
                std::vector<ed::NodeId> ctxNodeIds(ctxSize);
                int nodeCount = ed::GetActionContextNodes(ctxNodeIds.data(), ctxSize);

                std::vector<ed::LinkId> ctxLinkIds(ctxSize);
                int linkCount = ed::GetActionContextLinks(ctxLinkIds.data(), ctxSize);

                // Build old→new ID maps
                std::map<int, int> oldToNewNode;  // old NodeId → new NodeId
                std::map<int, int> oldToNewPin;   // old PinId  → new PinId

                // Phase 1: Clone nodes
                for (int i = 0; i < nodeCount; ++i)
                {
                    EditorNode* src = ng.FindNode(ax::NodeEditor::NodeId(ctxNodeIds[i].Get()));
                    if (!src) continue;

                    int newId = ng.GetNextId();
                    int oldId = (int)ctxNodeIds[i].Get();
                    oldToNewNode[oldId] = newId;

                    EditorNode clone = *src;  // copy all fields
                    clone.ID = ax::NodeEditor::NodeId(newId);

                    // Remap pins
                    for (auto& pin : clone.Inputs) {
                        int newPinId = ng.GetNextId();
                        oldToNewPin[(int)pin.ID.Get()] = newPinId;
                        pin.ID = ax::NodeEditor::PinId(newPinId);
                        pin.Node = nullptr;  // will be fixed by RebuildAllNodes
                    }
                    for (auto& pin : clone.Outputs) {
                        int newPinId = ng.GetNextId();
                        oldToNewPin[(int)pin.ID.Get()] = newPinId;
                        pin.ID = ax::NodeEditor::PinId(newPinId);
                        pin.Node = nullptr;  // will be fixed by RebuildAllNodes
                    }

                    // Offset position slightly
                    ImVec2 srcPos = ed::GetNodePosition(src->ID);
                    ed::SetNodePosition(clone.ID, ImVec2(srcPos.x + 40.0f, srcPos.y + 40.0f));

                    clone.Value = 0.0f;
                    for (int j = 0; j < 4; ++j) { clone.InputValues[j] = 0.0f; clone.InputBools[j] = false; }
                    for (int j = 0; j < 4; ++j) { clone.StateF[j] = 0.0f; clone.StateB[j] = false; }
                    clone.StateDt = 0.0f;

                    ng.m_Nodes.push_back(std::move(clone));
                }

                // Phase 2: Clone internal links
                for (int i = 0; i < linkCount; ++i)
                {
                    int ctxLinkRaw = (int)ctxLinkIds[i].Get();
                    auto it = std::find_if(ng.m_Links.begin(), ng.m_Links.end(),
                        [ctxLinkRaw](const EditorLink& l) { return (int)l.ID.Get() == ctxLinkRaw; });
                    if (it == ng.m_Links.end()) continue;

                    int oldStart = (int)it->StartPinID.Get();
                    int oldEnd   = (int)it->EndPinID.Get();

                    auto itStart = oldToNewPin.find(oldStart);
                    auto itEnd   = oldToNewPin.find(oldEnd);
                    if (itStart != oldToNewPin.end() && itEnd != oldToNewPin.end())
                    {
                        ng.m_Links.emplace_back(ng.GetNextId(),
                            ax::NodeEditor::PinId(itStart->second),
                            ax::NodeEditor::PinId(itEnd->second));
                        ng.m_Links.back().Color = it->Color;
                    }
                }

                ng.RebuildAllNodes();
                ng.SetModified(true);
            }
        }
        ed::EndShortcut();
    }

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
        auto* node = ng.FindNode(contextNodeId);
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
            ng.m_Links.erase(
                std::remove_if(ng.m_Links.begin(), ng.m_Links.end(),
                    [pid](auto& l) { return (int)l.StartPinID.Get() == pid || (int)l.EndPinID.Get() == pid; }),
                ng.m_Links.end());
            ng.SetModified(true);
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
        DrawAddNodeCategoryMenus([&ng](NodeType nt) { ng.AddNode(nt); });
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

    // Right sidebar — comm refs + output values
    ImGui::SameLine(0, 0);
    ManualSplitter("##S2", &ng.m_RightSideWidth, 80.0f, splitterW, true);
    ImGui::SameLine(0, 0);
    {
        ImGui::BeginChild("##RCol", ImVec2(ng.m_RightSideWidth, 0));
        // Upper: Comm Config
        ImGui::TextUnformatted("Comm Config");
        ImGui::SameLine(ng.m_RightSideWidth - 50);
        ImGui::TextDisabled("%d", (int)ng.m_CommRefs.size());
        ImGui::Separator();
        ImGui::BeginChild("##CommPane", ImVec2(0, 140), true);
        DrawCommRefsSidebar(ng, ng.m_RightSideWidth - 16);
        ImGui::EndChild();
        // Middle: Shortcut (always visible; click to assign to active ShortcutTrigger node)
        ImGui::TextUnformatted("Shortcut");
        ImGui::Separator();
        ImGui::BeginChild("##TrigPane", ImVec2(0.0f, 150.0f), true);
        DrawTriggerSidebar(ng, ng.m_RightSideWidth - 16.0f);
        ImGui::EndChild();
        // Lower: Output Targets
        ImGui::TextUnformatted("Output Values");
        ImGui::Separator();
        ImGui::BeginChild("##OVSide", ImVec2(0, 0), true);

        EditorNode* activeOut = ng.FindNode(ng.m_ActiveOutputId);
        if (activeOut && activeOut->Type != NodeType::CustomOutput) activeOut = nullptr;

        if (ng.m_OutputTargets.empty())
        {
            if (ng.m_CommRefs.empty())
                ImGui::TextDisabled("(No connection added - click +Add Comm)");
            else if (!ng.m_CommMgr)
                ImGui::TextDisabled("(no comm manager)");
            else
                ImGui::TextDisabled("(No send frames or all fixed fields)");
        }
        else
        {
            std::map<std::string, float> kvSnapshot = ng.GetKeyValuesSnapshot();
            // Reuse the outputs already computed by ng.EvaluateForDisplay()
            // to avoid double-evaluating stateful nodes (Counter, etc.).
            std::map<std::string, float> outputs;
            {
                std::shared_lock<std::shared_mutex> lock(ng.GetEvalMutex());
                outputs = ng.m_LastOutputs;
            }

            for (const auto& target : ng.m_OutputTargets)
            {
                float val = 0.0f;
                bool fromGraph = false;
                auto it = outputs.find(target.field_path);
                if (it != outputs.end()) {
                    val = it->second;
                    fromGraph = true;
                } else {
                    auto fit = ng.m_FieldValues.find(target.field_path);
                    if (fit != ng.m_FieldValues.end())
                        val = (float)fit->second;
                }

                bool isSelected = (activeOut && activeOut->OutputTarget == target.field_path);
                if (ImGui::Selectable(target.name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    if (activeOut) {
                        activeOut->OutputTarget = target.field_path;
                        activeOut->CommIndex = target.comm_index;
                        ng.SetModified(true);
                    }
                }
                ImGui::SameLine(ng.m_RightSideWidth - 70);

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
        ImGui::TextUnformatted("Variables");

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
            while (ng.FindGlobalByName(dn) >= 0);
            ng.AddGlobal(dn, 0.0f, PinType::Float);
        }

        const float tableW = ng.m_RightSideWidth - 16.0f;
        if (ImGui::BeginTable("##VarTable", 5,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable,
                ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 9.0f)))
        {
            ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type",    ImGuiTableColumnFlags_WidthFixed, 56.0f);
            ImGui::TableSetupColumn("Labels",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableSetupColumn("Value",   ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableHeadersRow();

            const int nvars = (int)ng.m_GlobalVars.size();
            for (int i = 0; i < nvars; ++i) {
                ImGui::PushID(i + 10000);
                ImGui::TableNextRow();

                // Click to select variable for active GlobalRead/Write node
                EditorNode* activeGR = ng.FindNode(ng.m_ActiveGlobalReadId);
                bool isGR = (activeGR && (activeGR->Type == NodeType::GlobalRead || activeGR->Type == NodeType::GlobalWrite));
                bool isSelected = isGR && activeGR->GlobalVarId == ng.m_GlobalVars[i].id;

                bool renaming = (ng.m_RenamingGlobalIdx == i);

                // --- Column 0: Name ---
                ImGui::TableSetColumnIndex(0);
                if (renaming) {
                    static char renameBuf[64];
                    if (ng.m_RenamingGlobalIdx != ng.m_LastRenamingIdx) {
                        strncpy_s(renameBuf, ng.m_GlobalVars[i].name.c_str(), sizeof(renameBuf) - 1);
                        ng.m_LastRenamingIdx = i;
                    }
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##Rn", renameBuf, sizeof(renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        if (renameBuf[0]) ng.m_GlobalVars[i].name = renameBuf;
                        ng.m_RenamingGlobalIdx = -1;
                        ng.SetModified(true);
                    }
                } else {
                    if (ImGui::Selectable(ng.m_GlobalVars[i].name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (isGR) {
                            activeGR->GlobalVarId = ng.m_GlobalVars[i].id;
                            ng.SetModified(true);
                            ng.m_ActiveGlobalReadId = 0;
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

                // --- Column 1: Type ---
                ImGui::TableSetColumnIndex(1);
                {
                    static const char* typeLabels[] = {"Float", "Int", "Bool", "Enum"};
                    static PinType typeValues[]      = {PinType::Float, PinType::Int, PinType::Bool, PinType::Enum};
                    static const ImVec4 typeCols[]   = {
                        ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ImVec4(0.4f, 0.6f, 1.0f, 1.0f),
                        ImVec4(0.9f, 0.6f, 0.3f, 1.0f), ImVec4(0.8f, 0.5f, 0.9f, 1.0f)
                    };
                    int curT = 0;
                    for (int ti = 0; ti < 4; ++ti)
                        if (ng.m_GlobalVars[i].type == typeValues[ti]) { curT = ti; break; }

                    ImGui::PushStyleColor(ImGuiCol_Text, typeCols[curT]);
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::BeginCombo("##TypeCombo", typeLabels[curT])) {
                        for (int ti = 0; ti < 4; ++ti) {
                            bool sel = (ng.m_GlobalVars[i].type == typeValues[ti]);
                            if (ImGui::Selectable(typeLabels[ti], sel)) {
                                ng.m_GlobalVars[i].type = typeValues[ti];
                                ng.SetModified(true);
                            }
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::PopStyleColor();
                }

                // --- Column 2: Labels (comma-separated, only meaningful for Enum) ---
                ImGui::TableSetColumnIndex(2);
                {
                    auto& labels = ng.m_GlobalVars[i].enumLabels;
                    // Build comma-separated string
                    std::string labelsStr;
                    for (size_t li = 0; li < labels.size(); ++li) {
                        if (li > 0) labelsStr += ", ";
                        labelsStr += labels[li];
                    }
                    char lbuf[256] = {};
                    strncpy_s(lbuf, labelsStr.c_str(), sizeof(lbuf) - 1);
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##Labels", lbuf, sizeof(lbuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        labels.clear();
                        std::string s(lbuf);
                        size_t pos = 0;
                        while (pos < s.size()) {
                            while (pos < s.size() && s[pos] == ' ') ++pos;
                            size_t end = s.find(',', pos);
                            if (end == std::string::npos) end = s.size();
                            std::string label = s.substr(pos, end - pos);
                            while (!label.empty() && label.back() == ' ') label.pop_back();
                            if (!label.empty()) labels.push_back(label);
                            pos = end + 1;
                        }
                        ng.SetModified(true);
                    }
                }

                // --- Column 3: Visible ---
                ImGui::TableSetColumnIndex(3);
                ImGui::Checkbox("##vis", &ng.m_GlobalVars[i].visible);
                if (ImGui::IsItemEdited()) ng.SetModified(true);

                // --- Column 4: Value (read-only display) ---
                ImGui::TableSetColumnIndex(4);
                {
                    float v = ng.m_GlobalVars[i].value;
                    PinType vt = ng.m_GlobalVars[i].type;
                    if (vt == PinType::Bool) {
                        ImGui::TextColored(v >= 0.5f ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                            "%s", v >= 0.5f ? "True" : "False");
                    } else if (vt == PinType::Int) {
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%d", (int)v);
                    } else if (vt == PinType::Enum) {
                        int iv = (int)v;
                        auto& elabels = ng.m_GlobalVars[i].enumLabels;
                        if (!elabels.empty() && iv >= 0 && iv < (int)elabels.size())
                            ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.9f, 1.0f), "%s", elabels[iv].c_str());
                        else
                            ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.9f, 1.0f), "%d", iv);
                    } else {
                        if (v >= 0.5f || v <= -0.5f) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%.2f", v);
                        else ImGui::TextDisabled("%.2f", v);
                    }
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if (delIdx >= 0) {
            if (delIdx < (int)ng.m_GlobalVars.size())
                ng.RemoveGlobal(ng.m_GlobalVars[delIdx].id);
            if (ng.m_RenamingGlobalIdx == delIdx) ng.m_RenamingGlobalIdx = -1;
        }
        if (renameIdx >= 0) ng.m_RenamingGlobalIdx = renameIdx;

        ImGui::EndChild();  // ##OVSide
    ImGui::EndChild();  // ##RCol
    }

    // Restore editor context and editing state
    if (wasRunning) {
        ed::EnableShortcuts(true);
        ImGui::EndDisabled();
    }
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
    if (segs[0] == "motions" && segs.size() >= 3) {
        int motionIdx = -1;
        try { motionIdx = std::stoi(segs[1]); } catch (...) { return; }
        if (motionIdx < 0) return;
        // Ensure enough motion entries exist
        while ((int)data.motions.size() <= motionIdx)
            data.motions.push_back(MotionControl());
        auto& m = data.motions[motionIdx];
        if      (segs[2] == "x")  m.x  = val;
        else if (segs[2] == "y")  m.y  = val;
        else if (segs[2] == "z")  m.z  = val;
        else if (segs[2] == "rx") m.rx = val;
        else if (segs[2] == "ry") m.ry = val;
        else if (segs[2] == "rz") m.rz = val;
        return;
    }

    // --- brushlessmotor ---
    if (segs[0] == "brushlessmotor" && segs.size() >= 3) {
        const std::string& motorName = segs[1];
        // Match logic mirrors GetActuatorField() in robot_api.h:
        // try exact name, then "Motor_<id>" fallback, then numeric id extraction
        int fallbackId = -1;
        {
            std::string digits;
            for (int ci = (int)motorName.size() - 1; ci >= 0; --ci) {
                if (std::isdigit((unsigned char)motorName[ci]))
                    digits = motorName[ci] + digits;
                else break;
            }
            if (!digits.empty()) fallbackId = std::stoi(digits);
        }
        for (auto& motor : data.brushlessmotor) {
            std::string fb = std::string("Motor_") + std::to_string(motor.id);
            if (motor.name != motorName && fb != motorName && motor.id != fallbackId) continue;
            if (segs[2] == "target_speed")    { motor.target_speed    = val; return; }
            if (segs[2] == "target_position") { motor.target_position = val; return; }
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
        // Same fallback matching as brushlessmotor and GetActuatorField()
        int fallbackId = -1;
        {
            std::string digits;
            for (int ci = (int)servoName.size() - 1; ci >= 0; --ci) {
                if (std::isdigit((unsigned char)servoName[ci]))
                    digits = servoName[ci] + digits;
                else break;
            }
            if (!digits.empty()) fallbackId = std::stoi(digits);
        }
        for (auto& sv : data.servo) {
            std::string fb = std::string("Servo_") + std::to_string(sv.id);
            if (sv.name != servoName && fb != servoName && sv.id != fallbackId) continue;
            if (segs[2] == "angle") { sv.angle = val; return; }
            return;
        }
        return;
    }

    // --- gpio ---
    if (segs[0] == "gpio" && segs.size() == 3) {
        const std::string& gpioName = segs[1];
        int fallbackId = -1;
        {
            std::string digits;
            for (int ci = (int)gpioName.size() - 1; ci >= 0; --ci) {
                if (std::isdigit((unsigned char)gpioName[ci]))
                    digits = gpioName[ci] + digits;
                else break;
            }
            if (!digits.empty()) fallbackId = std::stoi(digits);
        }
        for (auto& gpio : data.gpio_pins) {
            std::string fb = std::string("GPIO_") + std::to_string(gpio.id);
            if (gpio.name != gpioName && fb != gpioName && gpio.id != fallbackId) continue;
            if (segs[2] == "value") { gpio.value = (val >= 0.5f) ? 1 : 0; return; }
            return;
        }
        return;
    }
}

// ============================================================================
// BuildOutputTargetsFromProtocol



std::string NodeGraphManager::ClipboardCopySelected()
{
    if (m_SelectedIndex < 0 || m_SelectedIndex >= (int)m_Items.size()) return {};
    SaveCurrentToItem(); // flush editor changes to item
    std::string yaml = m_Items[m_SelectedIndex]->GetGraphDataYaml();
    if (yaml.empty()) return {};

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "type"  << YAML::Value << "nodegraph";
    out << YAML::Key << "name"  << YAML::Value << m_Items[m_SelectedIndex]->name;
    out << YAML::Key << "graph" << YAML::Value << yaml;
    out << YAML::EndMap;
    return out.c_str();
}

void NodeGraphManager::ClipboardPaste(const std::string& yaml)
{
    try {
        YAML::Node root = YAML::Load(yaml);
        if (!root.IsMap()) return;
        std::string type = root["type"] ? root["type"].as<std::string>() : "";
        if (type != "nodegraph") return;

        std::string baseName = root["name"] ? root["name"].as<std::string>() : "Pasted";
        int n = 1;
        std::string finalName = baseName;
        while (true) {
            bool dup = false;
            for (auto& g : m_Items)
                if (std::string(g->name) == finalName) { dup = true; break; }
            if (!dup) break;
            finalName = baseName + "_" + std::to_string(n++);
        }

        AddItem();
        auto& newGraph = m_Items.back();
        strncpy_s(newGraph->name, sizeof(newGraph->name), finalName.c_str(), sizeof(newGraph->name) - 1);

        std::string graphData = root["graph"] ? root["graph"].as<std::string>() : "";
        if (!graphData.empty())
            newGraph->LoadGraphData(graphData);
    } catch (...) {}
}
