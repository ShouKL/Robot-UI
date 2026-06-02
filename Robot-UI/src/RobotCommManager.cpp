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
    if (m_ActiveId == id) m_ActiveId = m_Nodes.empty() ? -1 : m_Nodes[0].id;
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
        m_ActiveId = id;
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
    m_ActiveId = -1;
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
std::vector<RobotCommConfig> RobotCommManager::GetAllConfigs() const {
    std::vector<RobotCommConfig> out;
    for (const auto& n : m_Nodes) out.push_back(n.component);
    return out;
}

void RobotCommManager::LoadConfigs(const std::vector<RobotCommConfig>& configs, int activeId) {
    // 断开现有连接
    if (m_IsConnected) Disconnect();

    // 清空现有节点
    m_Nodes.clear();
    m_ActiveId = -1;

    // 从配置列表重建节点
    for (const auto& cfg : configs) {
        RobotCommNode node;
        node.id = NextId();
        node.component = cfg;
        m_Nodes.push_back(node);
    }

    // 恢复活跃节点
    if (!m_Nodes.empty()) {
        if (activeId >= 0) {
            // 查找匹配的节点（通过名字和配置匹配，因为 ID 会重新分配）
            // 如果 activeId 在范围内，直接使用索引
            if (activeId < (int)m_Nodes.size()) {
                m_ActiveId = m_Nodes[activeId].id;
                m_Nodes[activeId].isSelected = true;
            } else {
                m_ActiveId = m_Nodes[0].id;
                m_Nodes[0].isSelected = true;
            }
        } else {
            m_ActiveId = m_Nodes[0].id;
            m_Nodes[0].isSelected = true;
        }
    }

    WL_INFO_TAG("COMM", "Loaded {} comm configs", configs.size());
}

RobotCommConfig* RobotCommManager::GetActiveConfig() {
    for (auto& n : m_Nodes)
        if (n.id == m_ActiveId) return &n.component;
    return nullptr;
}

RobotCommNode* RobotCommManager::GetActiveNode() {
    for (auto& n : m_Nodes)
        if (n.id == m_ActiveId) return &n;
    return nullptr;
}

void RobotCommManager::DrawItemList(float width) {
    auto& nodes = m_Nodes;
    for (int i = 0; i < (int)nodes.size(); ++i) {
        auto& node = nodes[i];
        ImGui::PushID(node.id);
        if (ImGui::Selectable(node.component.name, node.isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30))) {
            for (auto& n : nodes) n.isSelected = false;
            node.isSelected = true;
        }
        if (nodes.size() > 1) {
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete Comm"))
                    RemoveConfig(node.id);
                ImGui::EndPopup();
            }
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
        if (node.isConnected)
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "ON");
        else
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "OFF");
        ImGui::PopID();
    }
    if (ImGui::BeginPopupContextWindow("CommListPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Add Item"))
            AddItem();
        ImGui::EndPopup();
    }
}

void RobotCommManager::DrawContent() {
    RobotCommNode* selNode = nullptr;
    for (auto& n : m_Nodes) if (n.isSelected) { selNode = &n; break; }
    if (!selNode) {
        ImGui::TextDisabled("Select a component from the list to configure.");
    } else {
        if (ImGui::BeginTabBar("CommConfigTabs")) {
            if (ImGui::BeginTabItem("Network")) {
                m_RobotComm.DrawControlPanel(selNode->component, selNode->isConnected, selNode->id, m_RobotMgr, m_GamepadMgr,
                                             [this](int id) { Connect(id); },
                                             [this]() { Disconnect(); },
                                             m_OnActiveModeChanged,
                                             m_OnGamepadModeChanged);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Protocol")) {
                if (m_RobotMgr) {
                    auto& comps = m_RobotMgr->GetComponents();
                    int idx = m_RobotMgr->GetSelectedIndex();
                    if (idx >= 0 && idx < (int)comps.size()) {
                        auto& mode = comps[idx].component;
                        if (ImGui::BeginTabBar("ProtoSubTabs")) {
                            if (ImGui::BeginTabItem("Send Fields")) {
                                m_RobotComm.DrawSendFieldConfig(mode.protocol_send, mode.actuator_config);
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Receive Fields")) {
                                m_RobotComm.DrawReceiveFieldConfig(mode.protocol_receive,
                                    mode.sensor_config);
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                    } else {
                        ImGui::TextDisabled("No selected Robot item selected.");
                    }
                } else {
                    ImGui::TextDisabled("RobotComponent not connected.");
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
}

void RobotCommManager::DrawContent(float availableHeight) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
    if (ImGui::BeginTable("CommLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("CommList", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("component", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("CommDeviceList", ImVec2(0, availableHeight), true)) {
            auto& nodes = m_Nodes;
            for (int i = 0; i < (int)nodes.size(); ++i) {
                auto& node = nodes[i];
                ImGui::PushID(node.id);
                if (ImGui::Selectable(node.component.name, node.isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30))) {
                    for (auto& n : nodes) n.isSelected = false;
                    node.isSelected = true;
                }
                if (nodes.size() > 1) {
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Delete Comm"))
                            RemoveConfig(node.id);
                        ImGui::EndPopup();
                    }
                }
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
                if (node.isConnected)
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "ON");
                else
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "OFF");
                ImGui::PopID();
            }
            if (ImGui::BeginPopupContextWindow("CommListPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::MenuItem("Add Item"))
                    AddItem();
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("CommConfigPanel", ImVec2(0, availableHeight), false))
        {
            ImGui::Indent(10.0f);
            RobotCommNode* selNode = nullptr;
            for (auto& n : m_Nodes) if (n.isSelected) { selNode = &n; break; }
            if (!selNode) {
                ImGui::TextDisabled("Select a component from the list to configure.");
            } else {
                if (ImGui::BeginTabBar("CommConfigTabs")) {
                    if (ImGui::BeginTabItem("Network")) {
                        m_RobotComm.DrawControlPanel(selNode->component, selNode->isConnected, selNode->id, m_RobotMgr, m_GamepadMgr,
                                                     [this](int id) { Connect(id); },
                                                     [this]() { Disconnect(); },
                                                     m_OnActiveModeChanged,
                                                     m_OnGamepadModeChanged);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Protocol")) {
                        if (m_RobotMgr) {
                            auto& comps = m_RobotMgr->GetComponents();
                            int idx = m_RobotMgr->GetSelectedIndex();
                            if (idx >= 0 && idx < (int)comps.size()) {
                                auto& mode = comps[idx].component;
                                if (ImGui::BeginTabBar("ProtoSubTabs")) {
                                    if (ImGui::BeginTabItem("Send Fields")) {
                                        m_RobotComm.DrawSendFieldConfig(mode.protocol_send, mode.actuator_config);
                                        ImGui::EndTabItem();
                                    }
                                    if (ImGui::BeginTabItem("Receive Fields")) {
                                        m_RobotComm.DrawReceiveFieldConfig(mode.protocol_receive,
                                            mode.sensor_config);
                                        ImGui::EndTabItem();
                                    }
                                    ImGui::EndTabBar();
                                }
                            } else {
                                ImGui::TextDisabled("No selected Robot item selected.");
                            }
                        } else {
                            ImGui::TextDisabled("RobotComponent not connected.");
                        }
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::Unindent(10.0f);
        }
        ImGui::EndChild();

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

