#include "RobotComponentManager.h"

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
    strncpy_s(comp.name, sizeof(comp.name), buf, sizeof(comp.name) - 1);
    m_Components.push_back(comp);
    if (m_Components.size() == 1) {
        m_SelectedIndex = 0;
        m_Components[0].isSelected = true;
    }
    WL_INFO_TAG("COMP", "Item added: {} (id={})", comp.name, comp.id);
}

void RobotComponentManager::RemoveItem(int id)
{
    int index = FindNodeIndex(m_Components, id);
    if (index < 0 || index >= (int)m_Components.size()) return;
    if (m_Components.size() <= 1) return;
    WL_INFO_TAG("COMP", "Item deleted: {} (id={})", m_Components[index].name, m_Components[index].id);
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
        if (c.id == id) { strncpy_s(c.name, newName, sizeof(c.name) - 1); break; }
}

void RobotComponentManager::DrawContent()
{
    ImGui::Indent(10.0f);
    ImGui::Spacing();
    auto* sel = GetSelectedComponent();
    if (sel) DrawConfigPanel(*sel);
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
    for (const auto& c : m_Components) {
        RobotMode m;
        strncpy_s(m.name, sizeof(m.name), c.name, sizeof(m.name) - 1);
        m.actuator_config = c.actuator_config;
        m.sensor_config   = c.sensor_config;
        out.push_back(m);
    }
    return out;
}

void RobotComponentManager::LoadComponents(const std::vector<RobotMode>& modes, int selectedIdx)
{
    m_Components.clear();
    ResetNextId(1);
    for (const auto& item : modes) {
        RobotComponent comp;
        comp.id = NextId();
        strncpy_s(comp.name, sizeof(comp.name), item.name, sizeof(comp.name) - 1);
        comp.actuator_config = item.actuator_config;
        comp.sensor_config   = item.sensor_config;
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
        strncpy_s(comp.name, sizeof(comp.name), item.name, sizeof(comp.name) - 1);
        comp.actuator_config = item.actuator_config;
        comp.sensor_config   = item.sensor_config;
        m_Components.push_back(comp);
    }
    if (m_SelectedIndex >= (int)m_Components.size()) m_SelectedIndex = 0;
    if (!m_Components.empty()) m_Components[m_SelectedIndex].isSelected = true;
}

std::string RobotComponentManager::ClipboardCopySelected()
{
    int idx = m_SelectedIndex;
    if (idx < 0 || idx >= (int)m_Components.size()) return {};
    // Store indexed copy hint: just the name, actual data cloned on paste
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "type"  << YAML::Value << "component";
    out << YAML::Key << "name"  << YAML::Value << m_Components[idx].name;
    out << YAML::Key << "index" << YAML::Value << idx;
    out << YAML::EndMap;
    return out.c_str();
}

void RobotComponentManager::ClipboardPaste(const std::string& yaml)
{
    try {
        YAML::Node root = YAML::Load(yaml);
        if (!root.IsMap()) return;
        std::string type = root["type"] ? root["type"].as<std::string>() : "";
        int srcIdx = root["index"] ? root["index"].as<int>() : -1;
        if (type != "component" || srcIdx < 0 || srcIdx >= (int)m_Components.size()) return;

        std::string baseName = root["name"] ? root["name"].as<std::string>() : "Pasted";
        int n = 1;
        std::string finalName = baseName;
        while (true) {
            bool dup = false;
            for (auto& c : m_Components)
                if (std::string(c.name) == finalName) { dup = true; break; }
            if (!dup) break;
            finalName = baseName + "_" + std::to_string(n++);
        }

        auto& src = m_Components[srcIdx];
        RobotComponent comp;
        comp.id = NextId();
        strncpy_s(comp.name, sizeof(comp.name), finalName.c_str(), sizeof(comp.name) - 1);
        comp.actuator_config = src.actuator_config;
        comp.sensor_config   = src.sensor_config;
        m_Components.push_back(comp);
    } catch (...) {}
}
