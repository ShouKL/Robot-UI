#include "RobotCommManager.h"
#include "Walnut/Core/Log.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

// ==================== 构造/析构 ====================
RobotCommManager::RobotCommManager() {
    m_RobotAPI = std::make_shared<HardwareInterface>();
    AddItem();
    m_Nodes[0].isSelected = true;
}

RobotCommManager::~RobotCommManager() {
    Disconnect();
}

// ==================== 设备管理 ====================
void RobotCommManager::AddConfig(const char* name) {
    RobotCommNode node;
    node.id = NextId();
    strncpy(node.component.name, name, sizeof(node.component.name) - 1);
    m_Nodes.push_back(node);
    WL_INFO_TAG("COMM", "component added: {} (id={})", name, node.id);
}

void RobotCommManager::RemoveConfig(int id) {
    if (m_Nodes.size() <= 1) return;  // 至少保留一个
    int idx = FindNodeIndex(m_Nodes, id);
    if (idx < 0) return;
    auto& node = m_Nodes[idx];
    WL_INFO_TAG("COMM", "component removed: {} (id={})", node.component.name, id);
    if (node.isConnected) Disconnect();
    m_Nodes.erase(m_Nodes.begin() + idx);
}

// ==================== 连接控制 ====================
bool RobotCommManager::Connect(int id) {
    int idx = FindNodeIndex(m_Nodes, id);
    if (idx < 0) return false;

    // Disconnect any previous
    if (m_IsConnected) Disconnect();

    auto& node = m_Nodes[idx];
    auto& cfg = node.component;
    WL_INFO_TAG("COMM", "Connecting to {}:{} (local: {})...", cfg.host_ip, cfg.remote_port, cfg.local_port);

    bool ok = m_RobotAPI->Initialize(cfg.host_ip, cfg.remote_port, cfg.local_port);
    if (ok) {
        m_IsConnected = true;
        node.isConnected = true;

        // 同步 active_mode_index 到 Comm 面板所选模式
        if (m_RobotMgr) {
            int oldIdx = m_RobotMgr->GetSelectedIndex();
            int newIdx = m_RobotMgr->GetSelectedIndex();
            m_RobotMgr->SetSelectedIndex(newIdx);
            if (m_OnActiveModeChanged)
                m_OnActiveModeChanged(oldIdx, newIdx);
        }

        WL_INFO_TAG("COMM", "Connected successfully: {} ({})", cfg.name, cfg.host_ip);
    }
    else
    {
        WL_ERROR_TAG("COMM", "Connection failed: {} ({})", cfg.name, cfg.host_ip);
    }
    return ok;
}

void RobotCommManager::Disconnect() {
    if (m_IsConnected)
        WL_INFO_TAG("COMM", "Disconnected");
    for (auto& n : m_Nodes) n.isConnected = false;
    m_IsConnected = false;
}

// ==================== 数据收发 ====================
void RobotCommManager::SendActuatorData(const ActuatorConfig& data) {
    if (m_IsConnected)
        m_RobotAPI->SendActuatorData(data);
}

SensorData RobotCommManager::GetSensorData() {
    if (m_IsConnected)
        return m_RobotAPI->GetSensorData();
    SensorData d; d.is_valid = false; return d;
}

// ==================== 配置访问 ====================
std::vector<RobotCommConfig> RobotCommManager::GetAllItems() const {
    std::vector<RobotCommConfig> out;
    for (const auto& n : m_Nodes) out.push_back(n.component);
    return out;
}

void RobotCommManager::LoadItems(const std::vector<RobotCommConfig>& configs) {
    // 断开现有连接
    if (m_IsConnected) Disconnect();

    // 清空现有节点
    m_Nodes.clear();

    // 从配置列表重建节点
    for (const auto& cfg : configs) {
        RobotCommNode node;
        node.id = NextId();
        node.component = cfg;
        m_Nodes.push_back(node);
    }

    // 默认选中第一个
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

void RobotCommManager::DrawItemExtras(int index) {
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
    if (m_Nodes[index].isConnected)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "ON");
    else
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "OFF");
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

    m_RobotComm.DrawControlPanel(sel->component, sel->isConnected, sel->id, m_RobotMgr, m_GamepadMgr,
                                 [this](int id) { Connect(id); },
                                 [this]() { Disconnect(); },
                                 m_OnActiveModeChanged,
                                 m_OnGamepadModeChanged);
    ImGui::Unindent(10.0f);
}
