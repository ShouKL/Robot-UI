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

void RobotComponentManager::RenameItem(int id, const char* newName)
{
    for (auto& c : m_Components)
        if (c.id == id) { strncpy_s(c.component.name, newName, sizeof(c.component.name) - 1); break; }
}

void RobotComponentManager::DrawContent()
{
    ImGui::Indent(10.0f);
    ImGui::Spacing();
    auto* sel = GetSelectedComponent();
    if (sel) sel->DrawConfigPanel();
    else ImGui::TextDisabled("No item selected.");
    ImGui::Unindent(10.0f);
}

RobotComponent* RobotComponentManager::GetSelectedComponent()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Components.size())
        return &m_Components[m_SelectedIndex];
    return nullptr;
}

std::vector<RobotMode> RobotComponentManager::GetAllItems() const
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

void RobotComponentManager::LoadItems(const std::vector<RobotMode>& modes)
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
