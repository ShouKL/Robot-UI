#include "RobotComponentManager.h"
#include "Walnut/Core/Log.h"
#include "imgui.h"

RobotComponentManager::RobotComponentManager()
{
    AddItem();
    m_Components[0].isSelected = true;
}

void RobotComponentManager::AddItem()
{
    RobotComponent comp;
    comp.id = NextId();
    char buf[64];
    snprintf(buf, sizeof(buf), "Item_%d", comp.id);
    strncpy(comp.component.name, buf, sizeof(comp.component.name) - 1);
    m_Components.push_back(comp);
    if (m_Components.size() == 1) {
        m_SelectedIndex = 0;
        m_Components[0].isSelected = true;
    }
    WL_INFO_TAG("COMP", "Item added: {} (id={})", comp.component.name, comp.id);
}

void RobotComponentManager::RemoveItem(int id)
{
    int index = FindNodeIndex(m_Components, id);
    if (index < 0) return;
    DeleteByIndex(index);
}

void RobotComponentManager::DeleteByIndex(int index)
{
    if (index < 0 || index >= (int)m_Components.size()) return;
    if (m_Components.size() <= 1) return;
    WL_INFO_TAG("COMP", "Item deleted: {} (id={})", m_Components[index].component.name, m_Components[index].id);
    m_Components.erase(m_Components.begin() + index);
    if (m_SelectedIndex >= (int)m_Components.size())
        m_SelectedIndex = (int)m_Components.size() - 1;
    if (!m_Components.empty()) m_Components[m_SelectedIndex].isSelected = true;
}

void RobotComponentManager::SetSelectedIndex(int idx)
{
    if (idx >= 0 && idx < (int)m_Components.size()) {
        for (auto& c : m_Components) c.isSelected = false;
        m_SelectedIndex = idx;
        m_Components[idx].isSelected = true;
    }
}

void RobotComponentManager::DrawItemList(float width)
{
    auto& comps = m_Components;
    for (int i = 0; i < (int)comps.size(); ++i) {
        auto& comp = comps[i];
        ImGui::PushID(comp.id);
        bool isSel = (i == m_SelectedIndex);
        if (ImGui::Selectable(comp.component.name, isSel, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30)))
            SetSelectedIndex(i);
        if (comps.size() > 1) {
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete Item"))
                    RemoveItem(comp.id);
                ImGui::EndPopup();
            }
        }
        ImGui::PopID();
    }
    if (ImGui::BeginPopupContextWindow("RobotListPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Add Item"))
            AddItem();
        ImGui::EndPopup();
    }
}

void RobotComponentManager::DrawContent()
{
    auto* sel = GetSelectedComponent();
    if (sel) sel->DrawConfigPanel();
    else ImGui::TextDisabled("No item selected.");
}

RobotComponent* RobotComponentManager::GetSelectedComponent()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Components.size())
        return &m_Components[m_SelectedIndex];
    return nullptr;
}

std::vector<RobotMode> RobotComponentManager::GetAllComponents() const
{
    std::vector<RobotMode> out;
    for (const auto& c : m_Components)
        out.push_back(c.component);
    return out;
}

void RobotComponentManager::LoadComponents(const std::vector<RobotMode>& modes, int selectedIdx)
{
    m_Components.clear();
    ResetNextId(1);
    for (const auto& item : modes) {
        RobotComponent comp;
        comp.id = NextId();
        comp.component = item;
        m_Components.push_back(comp);
    }
    SetSelectedIndex(selectedIdx >= 0 && selectedIdx < (int)m_Components.size() ? selectedIdx : 0);
}

void RobotComponentManager::RestoreComponents(const std::vector<RobotMode>& modes)
{
    m_Components.clear();
    ResetNextId(1);
    for (const auto& item : modes) {
        RobotComponent comp;
        comp.id = NextId();
        comp.component = item;
        m_Components.push_back(comp);
    }
    if (m_SelectedIndex >= (int)m_Components.size()) m_SelectedIndex = 0;
    if (!m_Components.empty()) m_Components[m_SelectedIndex].isSelected = true;
}

// ==================== UI ====================

void RobotComponentManager::DrawContent(float availableHeight)
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
    if (ImGui::BeginTable("CompLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("ItemList", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("component", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("CompItemList", ImVec2(0, availableHeight), true))
        {
            int nodeToDelete = -1;
            for (int i = 0; i < (int)m_Components.size(); ++i) {
                auto& comp = m_Components[i];
                char label[128];
                snprintf(label, sizeof(label), "%s##%d", comp.component.name, comp.id);
                ImGui::PushID(comp.id);
                if (ImGui::Selectable(label, comp.isSelected))
                    SetSelectedIndex(i);
                if (m_Components.size() > 1) {
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Delete Item"))
                            nodeToDelete = comp.id;
                        ImGui::EndPopup();
                    }
                }
                ImGui::PopID();
            }
            if (nodeToDelete != -1) RemoveItem(nodeToDelete);
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("CompConfigPanel", ImVec2(0, availableHeight), false))
        {
            ImGui::Indent(10.0f);
            auto* sel = GetSelectedComponent();
            if (sel) {
                sel->DrawConfigPanel();
            } else {
                ImGui::TextDisabled("Select an item from the list.");
            }
            ImGui::Unindent(10.0f);
        }
        ImGui::EndChild();

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}