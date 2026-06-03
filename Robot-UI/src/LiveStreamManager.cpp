#include "LiveStreamManager.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include "imgui.h"
#include <vector>
#include <memory>

LiveStreamManager::LiveStreamManager() {
    AddItem();
}

LiveStreamManager::~LiveStreamManager() {
    for (auto& node : m_devices) {
        node.stream->Close();
    }
    m_devices.clear();
}

// ======== 设备管理 ========
void LiveStreamManager::AddItem() {
    DeviceNode node;
    node.id = NextId();
    node.stream = std::make_unique<LiveStream>();

    auto& config = node.stream->GetStreamConfig();
    char buf[64];
    snprintf(buf, sizeof(buf), "Item_%d", node.id);
    strncpy(config.name, buf, sizeof(config.name) - 1);
    config.name[sizeof(config.name) - 1] = '\0';
    strncpy(config.ip, "0.0.0.0", sizeof(config.ip) - 1);
    config.ip[sizeof(config.ip) - 1] = '\0';

    m_devices.push_back(std::move(node));
    if (m_devices.size() == 1) {
        m_devices[0].isSelected = true;
    }
}

void LiveStreamManager::RemoveItem(int id) {
    int index = FindNodeIndex(m_devices, id);
    if (index < 0) return;
    DeleteByIndex(index);
}

void LiveStreamManager::DeleteByIndex(int index) {
    if (index < 0 || index >= (int)m_devices.size()) return;
    if (m_devices.size() <= 1) return;
    if (m_devices[index].isStreaming) {
        m_devices[index].stream->Close();
    }
    m_devices.erase(m_devices.begin() + index);
}

std::vector<StreamConfig> LiveStreamManager::GetAllItems() const {
    std::vector<StreamConfig> configs;
    for (const auto& node : m_devices) {
        configs.push_back(node.stream->GetStreamConfig());
    }
    return configs;
}

void LiveStreamManager::LoadItems(const std::vector<StreamConfig>& configs) {
    for (auto& node : m_devices) {
        if (node.isStreaming)
            node.stream->Close();
    }
    m_devices.clear();
    ResetNextId(1000);

    if (configs.empty()) {
        // 始终至少有一个默认项
        AddItem();
        return;
    }

    for (const auto& cfg : configs) {
        DeviceNode node;
        node.id = NextId();
        node.stream = std::make_unique<LiveStream>();
        node.stream->GetStreamConfig() = cfg;
        m_devices.push_back(std::move(node));
    }
}

void LiveStreamManager::RenameItem(int id, const char* newName)
{
    for (auto& node : m_devices)
        if (node.id == id) { strncpy_s(node.stream->GetStreamConfig().name, newName, sizeof(node.stream->GetStreamConfig().name) - 1); break; }
}

void LiveStreamManager::SelectItem(int index) {
    for (auto& n : m_devices) n.isSelected = false;
    if (index >= 0 && index < (int)m_devices.size())
        m_devices[index].isSelected = true;
}

void LiveStreamManager::DrawContent() {
    ImGui::Indent(10.0f);
    ImGui::Spacing();
    auto* sel = GetSelectedDevice();
    if (!sel) { ImGui::TextDisabled("No item selected."); ImGui::Unindent(10.0f); return; }

    auto& cfg = sel->stream->GetStreamConfig();
    ImGui::Separator();
    float bottomReservedHeight = ImGui::GetFrameHeightWithSpacing() * 1.5f + ImGui::GetStyle().ItemSpacing.y * 3.0f;
    float scrollWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x;
    if (ImGui::BeginChild("ConfigScroll", ImVec2(scrollWidth, -bottomReservedHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        sel->stream->DrawStreamConfigPanel();
    }
    ImGui::EndChild();
    ImGui::Unindent(10.0f);
}

DeviceNode* LiveStreamManager::GetSelectedDevice()
{
    for (auto& n : m_devices) if (n.isSelected) return &n;
    return nullptr;
}
