#include "RobotCommManager.h"
#include "Walnut/Core/Log.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

// ==================== 构造/析构 ====================
RobotCommManager::RobotCommManager() {
    m_RobotAPI = std::make_shared<HardwareInterface>();
    // Add a default config
    AddConfig("Default");
    m_Nodes[0].isSelected = true;
}

RobotCommManager::~RobotCommManager() {
    Disconnect();
}

// ==================== 设备管理 ====================
void RobotCommManager::AddConfig(const char* name) {
    RobotCommNode node;
    node.id = NextId();
    strncpy(node.config.name, name, sizeof(node.config.name) - 1);
    m_Nodes.push_back(node);
    WL_INFO_TAG("COMM", "Config added: {} (id={})", name, node.id);
}

void RobotCommManager::RemoveConfig(int id) {
    int idx = FindNodeIndex(m_Nodes, id);
    if (idx < 0) return;
    auto& node = m_Nodes[idx];
    WL_INFO_TAG("COMM", "Config removed: {} (id={})", node.config.name, id);
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
    auto& cfg = node.config;
    WL_INFO_TAG("COMM", "Connecting to {}:{} (local: {})...", cfg.host_ip, cfg.remote_port, cfg.local_port);

    bool ok = m_RobotAPI->Initialize(cfg.host_ip, cfg.remote_port, cfg.local_port);
    if (ok) {
        m_IsConnected = true;
        m_ActiveId = id;
        node.isConnected = true;

        // 同步 active_mode_index 到 Comm 面板所选模式
        if (m_RobotComponent) {
            m_RobotComponent->SetActiveModeIndex(m_RobotComponent->GetEditModeIndex());
            if (m_OnActiveModeChanged)
                m_OnActiveModeChanged(m_RobotComponent->GetActiveModeIndex());
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
void RobotCommManager::SendActuatorData(const ActuatorData& data) {
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
    for (const auto& n : m_Nodes) out.push_back(n.config);
    return out;
}

RobotCommConfig* RobotCommManager::GetActiveConfig() {
    for (auto& n : m_Nodes)
        if (n.id == m_ActiveId) return &n.config;
    return nullptr;
}

RobotCommNode* RobotCommManager::GetActiveNode() {
    for (auto& n : m_Nodes)
        if (n.id == m_ActiveId) return &n;
    return nullptr;
}

void RobotCommManager::DrawUI(const char* windowName, bool* p_open) {
    if (!p_open || !*p_open) return;

    const auto getName = [](const RobotCommNode& n) -> const char* { return n.config.name; };
    const auto getActive = [](const RobotCommNode& n) -> bool { return n.isConnected; };

    // 设备列表窗口
    ImGui::Begin(windowName, p_open, ImGuiWindowFlags_MenuBar);
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("COMM LIST")) {
            if (ImGui::MenuItem("Add New Comm"))
                AddConfig("New_Comm");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    DrawItemTree("Delete Comm", m_Nodes, getName, getActive);
    ImGui::End();

    // 配置面板窗口（含 Network/Transport 控制面板 + 协议字段 Tab）
    ImGui::Begin("Comm Configuration", p_open);
    RobotCommNode* selNode = nullptr;
    for (auto& n : m_Nodes) if (n.isSelected) { selNode = &n; break; }
    if (!selNode) {
        ImGui::TextDisabled("Select a config from the list to configure.");
    } else {
        if (ImGui::BeginTabBar("CommConfigTabs")) {
            if (ImGui::BeginTabItem("Network")) {
                m_RobotComm.DrawControlPanel(selNode->config, selNode->isConnected, selNode->id,
                                             m_RobotComponent,
                                             [this](int id) { Connect(id); },
                                             [this]() { Disconnect(); },
                                             m_OnActiveModeChanged);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Protocol")) {
                if (m_RobotComponent) {
                    auto& modes = m_RobotComponent->GetModes();
                    int idx = m_RobotComponent->GetEditModeIndex();
                    if (idx >= 0 && idx < (int)modes.size()) {
                        auto& mode = modes[idx];
                        if (ImGui::BeginTabBar("ProtoSubTabs")) {
                            if (ImGui::BeginTabItem("Send Fields")) {
                                m_RobotComm.DrawSendFieldConfig(mode.protocol_send, mode.actuator_config);
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Receive Fields")) {
                                m_RobotComm.DrawReceiveFieldConfig(mode.protocol_receive,
                                    mode.has_temperature, mode.has_humidity, mode.has_depth);
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                    } else {
                        ImGui::TextDisabled("No active Robot mode selected.");
                    }
                } else {
                    ImGui::TextDisabled("RobotComponent not connected.");
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
