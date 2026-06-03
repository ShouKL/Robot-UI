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

void GamepadMapperManager::RenameItem(int id, const char* newName)
{
    for (auto& m : m_Mappers)
        if (m.id == id) { strncpy_s(m.name, newName, sizeof(m.name) - 1); break; }
}

void GamepadMapperManager::DrawContent()
{
    ImGui::Indent(10.0f);
    ImGui::Spacing();
    auto* sel = GetSelectedMapper();
    if (sel) sel->DrawGamepadMapper();
    else ImGui::TextDisabled("No item selected.");
    ImGui::Unindent(10.0f);
}

GamepadMapper* GamepadMapperManager::GetSelectedMapper()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Mappers.size())
        return &m_Mappers[m_SelectedIndex];
    return nullptr;
}

std::vector<GamepadMapper> GamepadMapperManager::GetAllItems() const
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

void GamepadMapperManager::LoadItems(const std::vector<GamepadMapper>& items)
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
