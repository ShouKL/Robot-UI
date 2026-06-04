#include "RobotCommManager.h"
#include "Walnut/Core/Log.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

// ==================== 构造/析构 ====================
RobotCommManager::RobotCommManager() {
    AddItem();
    m_Nodes[0].isSelected = true;
}

// ==================== 设备管理 ====================
void RobotCommManager::AddItem() {
    int id = NextId();
    RobotCommNode node;
    node.id = id;
    char buf[64];
    snprintf(buf, sizeof(buf), "Item_%d", id);
    strncpy(node.component.name, buf, sizeof(node.component.name) - 1);
    m_Nodes.push_back(node);
    WL_INFO_TAG("COMM", "component added: {} (id={})", node.component.name, node.id);
}

void RobotCommManager::RemoveItem(int id) {
    if (m_Nodes.size() <= 1) return;  // 至少保留一个
    int idx = FindNodeIndex(m_Nodes, id);
    if (idx < 0) return;
    auto& node = m_Nodes[idx];
    WL_INFO_TAG("COMM", "component removed: {} (id={})", node.component.name, id);
    m_Nodes.erase(m_Nodes.begin() + idx);
}

// ==================== 配置访问 ====================
std::vector<RobotCommConfig> RobotCommManager::GetAllItems() const {
    std::vector<RobotCommConfig> out;
    for (const auto& n : m_Nodes) out.push_back(n.component);
    return out;
}

void RobotCommManager::LoadItems(const std::vector<RobotCommConfig>& configs) {
    m_Nodes.clear();
    ResetNextId(1);

    for (const auto& cfg : configs) {
        RobotCommNode node;
        node.id = NextId();
        node.component = cfg;
        m_Nodes.push_back(node);
    }

    if (!m_Nodes.empty())
        m_Nodes[0].isSelected = true;

    WL_INFO_TAG("COMM", "Loaded {} comm configs", configs.size());
}

void RobotCommManager::RenameItem(int id, const char* newName)
{
    for (auto& n : m_Nodes)
        if (n.id == id) { strncpy_s(n.component.name, newName, sizeof(n.component.name) - 1); break; }
}

void RobotCommManager::SelectItem(int index) {
    for (auto& n : m_Nodes) n.isSelected = false;
    if (index >= 0 && index < (int)m_Nodes.size())
        m_Nodes[index].isSelected = true;
}

RobotCommNode* RobotCommManager::GetSelectedNode()
{
    for (auto& n : m_Nodes) if (n.isSelected) return &n;
    return nullptr;
}

void RobotCommManager::DrawContent() {
    ImGui::Indent(10.0f);
    ImGui::Spacing();
    auto* sel = GetSelectedNode();
    if (!sel) { ImGui::TextDisabled("No item selected."); ImGui::Unindent(10.0f); return; }

    m_RobotComm.DrawControlPanel(sel->component, m_RobotMgr);
    ImGui::Unindent(10.0f);
}
