#include "RobotCommManager.h"
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
    node.id = m_NextId++;
    strncpy(node.config.name, name, sizeof(node.config.name) - 1);
    m_Nodes.push_back(node);
}

void RobotCommManager::RemoveConfig(int id) {
    auto it = std::find_if(m_Nodes.begin(), m_Nodes.end(),
        [id](const RobotCommNode& n) { return n.id == id; });
    if (it != m_Nodes.end()) {
        if (it->isConnected) Disconnect();
        m_Nodes.erase(it);
        if (m_ActiveId == id) m_ActiveId = m_Nodes.empty() ? -1 : m_Nodes[0].id;
    }
}

// ==================== 连接控制 ====================
bool RobotCommManager::Connect(int id) {
    auto it = std::find_if(m_Nodes.begin(), m_Nodes.end(),
        [id](const RobotCommNode& n) { return n.id == id; });
    if (it == m_Nodes.end()) return false;

    // Disconnect any previous
    if (m_IsConnected) Disconnect();

    auto& cfg = it->config;
    bool ok = m_RobotAPI->Initialize(cfg.host_ip, cfg.remote_port, cfg.local_port);
    if (ok) {
        m_IsConnected = true;
        m_ActiveId = id;
        it->isConnected = true;
    }
    return ok;
}

void RobotCommManager::Disconnect() {
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

// ==================== UI ====================
void RobotCommManager::DrawUI(const char* windowName, bool* p_open) {
    if (!p_open || !*p_open) return;
    m_ConfigWindowOpen = *p_open;

    // Main window with menu bar
    ImGui::Begin(windowName, p_open, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("COMM LIST")) {
            if (ImGui::MenuItem("Add New Comm"))
                AddConfig("New_Comm");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    DrawConfigTree();
    ImGui::End();

    // Control panel window
    ImGui::Begin("Comm Configuration", p_open);
    DrawControlPanel();
    ImGui::End();

    m_ConfigWindowOpen = *p_open;
}

void RobotCommManager::OpenConfigWindow() { m_ConfigWindowOpen = true; }

void RobotCommManager::DrawUI() {
    DrawUI("Robot Comm", &m_ConfigWindowOpen);
}

void RobotCommManager::DrawConfigTree() {
    int nodeToDelete = -1;

    for (auto& node : m_Nodes) {
        char label[128];
        snprintf(label, sizeof(label), "%s##%d", node.config.name, node.id);

        if (ImGui::Selectable(label, node.isSelected)) {
            for (auto& n : m_Nodes) n.isSelected = false;
            node.isSelected = true;
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Comm"))
                nodeToDelete = node.id;
            ImGui::EndPopup();
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
        if (node.isConnected)
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "ON");
        else
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "OFF");
    }

    if (nodeToDelete != -1) RemoveConfig(nodeToDelete);
}


void RobotCommManager::DrawControlPanel() {
    RobotCommNode* selNode = nullptr;
    for (auto& n : m_Nodes) if (n.isSelected) { selNode = &n; break; }

    if (!selNode) {
        ImGui::TextDisabled("Select a config from the list to configure.");
        return;
    }

    auto& cfg = selNode->config;
    ImGui::InputText("Name", cfg.name, sizeof(cfg.name));
    ImGui::TextDisabled("ID: %d | Status: %s", selNode->id, selNode->isConnected ? "CONNECTED" : "OFFLINE");
    ImGui::Separator();
    ImGui::Spacing();

    // Network
    if (ImGui::CollapsingHeader("Network", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Host IP", cfg.host_ip, sizeof(cfg.host_ip));
        ImGui::InputInt("Remote Port", &cfg.remote_port);
        ImGui::InputInt("Local Port", &cfg.local_port);
    }

    // Transport
    if (ImGui::CollapsingHeader("Transport", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* protocols[] = { "UDP", "TCP", "Serial" };
        ImGui::Combo("Protocol", &cfg.transport_type, protocols, IM_ARRAYSIZE(protocols));
    }

    // Type selector (Robot mode)
    if (ImGui::CollapsingHeader("Type", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_RobotConfig) {
            auto& activeModes = m_RobotConfig->GetActiveModes();
            int activeIdx = m_RobotConfig->GetActiveModeIndex();
            if (!activeModes.empty() && activeIdx >= 0 && activeIdx < (int)activeModes.size()) {
                std::string preview = activeModes[activeIdx].name;
                if (ImGui::BeginCombo("##TypeSelect", preview.c_str())) {
                    for (int i = 0; i < (int)activeModes.size(); ++i) {
                        bool isSelected = (i == activeIdx);
                        if (ImGui::Selectable(activeModes[i].name, isSelected)) {
                            m_RobotConfig->SetActiveModeIndex(i);
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        } else {
            ImGui::TextDisabled("Robot config not available");
        }
    }

    // Connect / Disconnect button
    ImGui::Spacing();
    ImVec2 btnSize(-1.0f, ImGui::GetFrameHeightWithSpacing() * 1.5f);
    if (selNode->isConnected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("DISCONNECT", btnSize)) {
            Disconnect();
        }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.8f, 0.5f, 1.0f));
        if (ImGui::Button("APPLY & CONNECT", btnSize)) {
            Connect(selNode->id);
        }
        ImGui::PopStyleColor(3);
    }
}
