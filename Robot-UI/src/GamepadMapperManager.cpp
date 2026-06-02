#include "GamepadMapperManager.h"
#include "Walnut/Core/Log.h"
#include "imgui.h"

GamepadMapperManager::GamepadMapperManager()
{
    AddItem();
    m_Mappers[0].isSelected = true;
}

void GamepadMapperManager::AddItem()
{
    GamepadMapper mapper;
    mapper.id = NextId();
    char buf[64];
    snprintf(buf, sizeof(buf), "Item_%d", mapper.id);
    strncpy(mapper.name, buf, sizeof(mapper.name) - 1);
    m_Mappers.push_back(std::move(mapper));
    if (m_Mappers.size() == 1) {
        m_SelectedIndex = 0;
        m_Mappers[0].isSelected = true;
    }
    WL_INFO_TAG("GAMEPAD", "Item added: {} (id={})", m_Mappers.back().name, m_Mappers.back().id);
}

void GamepadMapperManager::RemoveItem(int id)
{
    int index = FindNodeIndex(m_Mappers, id);
    if (index < 0) return;
    DeleteByIndex(index);
}

void GamepadMapperManager::DeleteByIndex(int index)
{
    if (index < 0 || index >= (int)m_Mappers.size()) return;
    if (m_Mappers.size() <= 1) return;
    WL_INFO_TAG("GAMEPAD", "Item deleted: {} (id={})", m_Mappers[index].name, m_Mappers[index].id);
    m_Mappers.erase(m_Mappers.begin() + index);
    if (m_SelectedIndex >= (int)m_Mappers.size())
        m_SelectedIndex = (int)m_Mappers.size() - 1;
    if (!m_Mappers.empty()) m_Mappers[m_SelectedIndex].isSelected = true;
}

void GamepadMapperManager::SetSelectedIndex(int idx)
{
    if (idx >= 0 && idx < (int)m_Mappers.size()) {
        for (auto& m : m_Mappers) m.isSelected = false;
        m_SelectedIndex = idx;
        m_Mappers[idx].isSelected = true;
    }
}

void GamepadMapperManager::DrawItemList(float width)
{
    auto& mappers = m_Mappers;
    for (int i = 0; i < (int)mappers.size(); ++i) {
        auto& mapper = mappers[i];
        ImGui::PushID(mapper.id);
        bool isSel = (i == m_SelectedIndex);
        if (ImGui::Selectable(mapper.name, isSel, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30)))
            SetSelectedIndex(i);
        if (mappers.size() > 1) {
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete Item"))
                    RemoveItem(mapper.id);
                ImGui::EndPopup();
            }
        }
        ImGui::PopID();
    }
    if (ImGui::BeginPopupContextWindow("GamepadListPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Add Item"))
            AddItem();
        ImGui::EndPopup();
    }
}

void GamepadMapperManager::DrawContent()
{
    auto* sel = GetSelectedMapper();
    if (sel) sel->DrawGamepadMapper();
    else ImGui::TextDisabled("No item selected.");
}

GamepadMapper* GamepadMapperManager::GetSelectedMapper()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Mappers.size())
        return &m_Mappers[m_SelectedIndex];
    return nullptr;
}

std::vector<GamepadMapper> GamepadMapperManager::GetAllMappers() const
{
    std::vector<GamepadMapper> out;
    for (const auto& m : m_Mappers)
        out.push_back(m);
    return out;
}

void GamepadMapperManager::LoadMappers(const std::vector<GamepadMapper>& items, int selectedIdx)
{
    m_Mappers.clear();
    ResetNextId(1);
    for (const auto& item : items) {
        GamepadMapper mapper;
        mapper.id = NextId();
        strncpy(mapper.name, item.name, sizeof(mapper.name) - 1);
        mapper.keys = item.keys;
        mapper.mappings = item.mappings;
        mapper.gamepad_type = item.gamepad_type;
        m_Mappers.push_back(std::move(mapper));
    }
    SetSelectedIndex(selectedIdx >= 0 && selectedIdx < (int)m_Mappers.size() ? selectedIdx : 0);
}

void GamepadMapperManager::RestoreMappers(const std::vector<GamepadMapper>& items)
{
    m_Mappers.clear();
    ResetNextId(1);
    for (const auto& item : items) {
        GamepadMapper mapper;
        mapper.id = NextId();
        strncpy(mapper.name, item.name, sizeof(mapper.name) - 1);
        mapper.keys = item.keys;
        mapper.mappings = item.mappings;
        mapper.gamepad_type = item.gamepad_type;
        m_Mappers.push_back(std::move(mapper));
    }
    if (m_SelectedIndex >= (int)m_Mappers.size()) m_SelectedIndex = 0;
    if (!m_Mappers.empty()) m_Mappers[m_SelectedIndex].isSelected = true;
}

void GamepadMapperManager::DrawContent(float availableHeight)
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
    if (ImGui::BeginTable("GpLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("ItemList", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("component", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("GpItemList", ImVec2(0, availableHeight), true))
        {
            int nodeToDelete = -1;
            for (int i = 0; i < (int)m_Mappers.size(); ++i) {
                auto& mapper = m_Mappers[i];
                char label[128];
                snprintf(label, sizeof(label), "%s##%d", mapper.name, mapper.id);
                ImGui::PushID(mapper.id);
                if (ImGui::Selectable(label, mapper.isSelected))
                    SetSelectedIndex(i);
                if (m_Mappers.size() > 1) {
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Delete Item"))
                            nodeToDelete = mapper.id;
                        ImGui::EndPopup();
                    }
                }
                ImGui::PopID();
            }
            if (nodeToDelete != -1) RemoveItem(nodeToDelete);
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("GpConfigPanel", ImVec2(0, availableHeight), false))
        {
            ImGui::Indent(10.0f);
            auto* sel = GetSelectedMapper();
            if (sel) {
                sel->DrawGamepadMapper();
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