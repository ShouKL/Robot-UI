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

std::vector<StreamConfig> LiveStreamManager::GetAllStreamConfigs() const {
    std::vector<StreamConfig> configs;
    for (const auto& node : m_devices) {
        configs.push_back(node.stream->GetStreamConfig());
    }
    return configs;
}

void LiveStreamManager::LoadAllConfigs(const std::vector<StreamConfig>& configs) {
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

void LiveStreamManager::UpdateAll() {
    for (auto& node : m_devices) {                    // 遍历所有设备
        if (node.isStreaming) {                       // 只有正在运行的设备才执行更新
            node.stream->Update();                    // 处理帧拷贝、纹理上传等逻辑
        }
    }
}

// ======== UI 绘制 ========
void LiveStreamManager::DrawContent(float availableHeight) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
    if (ImGui::BeginTable("StreamLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("DeviceList", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Config", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("StreamDeviceList", ImVec2(0, availableHeight), true)) {
            int nodeToDelete = -1;
            for (auto& node : m_devices) {
                char label[128];
                snprintf(label, sizeof(label), "%s##%d", node.stream->GetStreamConfig().name, node.id);
                if (ImGui::Selectable(label, node.isSelected)) {
                    for (auto& n : m_devices) n.isSelected = false;
                    node.isSelected = true;
                }
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Delete Device"))
                        nodeToDelete = node.id;
                    ImGui::EndPopup();
                }
            }
            if (nodeToDelete != -1)
                RemoveItem(nodeToDelete);
            if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::MenuItem("Add Item"))
                    AddItem();
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("StreamConfigPanel", ImVec2(0, availableHeight), false))
        {
            ImGui::Indent(10.0f);
            DeviceNode* selectedNode = nullptr;
            for (auto& n : m_devices) if (n.isSelected) selectedNode = &n;
            if (!selectedNode) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select a device from the list to configure.");
            } else {
                auto& cfg = selectedNode->stream->GetStreamConfig();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", cfg.name);
                ImGui::TextDisabled("ID: %d | Status: %s", selectedNode->id, selectedNode->isStreaming ? "CONNECTED" : "OFFLINE");
                ImGui::Spacing();
                float bottomReservedHeight = ImGui::GetFrameHeightWithSpacing() * 1.5f + ImGui::GetStyle().ItemSpacing.y * 3.0f;
                float scrollWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x;
                ImGui::BeginChild("ConfigScroll", ImVec2(scrollWidth, -bottomReservedHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
                auto drawHeader = [](const char* title) {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                    bool open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);
                    ImGui::PopStyleColor(3);
                    return open;
                };
                if (drawHeader("Notice")) {
                    ImGui::TextWrapped("Performance Benchmark:");
                    ImGui::BulletText("CLI (Direct GPU Overlay): ~100ms latency.");
                    ImGui::BulletText("Software (AppSink + Texture Upload): ~200ms latency.");
                    ImGui::Spacing();
                    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().WindowPadding.x);
                    ImGui::TextDisabled("Note: The 100ms difference is the physical overhead of copying frames "
                        "from Video Memory back to System RAM for UI synchronization.");
                    ImGui::PopTextWrapPos();
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::Text("Reference CLI Command (Minimum Latency):");
                    const char* cliCmd = "gst-launch-1.0 -v rtspsrc location=\"rtsp://{user}:{password}@{ip}:554/h264/ch1/main/av_stream\" "
                        "latency=0 buffer-mode=0 drop-on-latency=true protocols=udp ! rtph264depay ! h264parse ! "
                        "d3d11h264dec ! d3d11videosink sync=false";
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                    ImVec2 textSize = ImGui::CalcTextSize(cliCmd, nullptr, false, ImGui::GetContentRegionAvail().x - 20.0f);
                    float childHeight = textSize.y + ImGui::GetStyle().FramePadding.y * 4.0f + 15.0f;
                    if (ImGui::BeginChild("##CLI_Box", ImVec2(-1.0f, childHeight), true, ImGuiWindowFlags_NoScrollbar)) {
                        ImGui::PushTextWrapPos(0.0f);
                        ImGui::TextUnformatted(cliCmd);
                        ImGui::PopTextWrapPos();
                        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0))
                            ImGui::SetClipboardText(cliCmd);
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Click to copy text");
                }
                ImGui::Spacing();
                selectedNode->stream->DrawStreamConfigPanel();
                ImGui::PopStyleVar();
                ImGui::EndChild();
            }
            ImGui::Unindent(10.0f);
        }
        ImGui::EndChild();

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void LiveStreamManager::DrawItemList(float width) {
    int nodeToDelete = -1;
    for (auto& node : m_devices) {
        char label[128];
        snprintf(label, sizeof(label), "%s##%d", node.stream->GetStreamConfig().name, node.id);
        if (ImGui::Selectable(label, node.isSelected)) {
            for (auto& n : m_devices) n.isSelected = false;
            node.isSelected = true;
        }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Device"))
                nodeToDelete = node.id;
            ImGui::EndPopup();
        }
    }
    if (nodeToDelete != -1)
        RemoveItem(nodeToDelete);
    if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Add Item"))
            AddItem();
        ImGui::EndPopup();
    }
}

void LiveStreamManager::DrawContent() {
    DeviceNode* selectedNode = nullptr;
    for (auto& n : m_devices) if (n.isSelected) selectedNode = &n;
    if (!selectedNode) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select a device from the list to configure.");
        return;
    }
    auto& cfg = selectedNode->stream->GetStreamConfig();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", cfg.name);
    ImGui::TextDisabled("ID: %d | Status: %s", selectedNode->id, selectedNode->isStreaming ? "CONNECTED" : "OFFLINE");
    ImGui::Spacing();
    float bottomReservedHeight = ImGui::GetFrameHeightWithSpacing() * 1.5f + ImGui::GetStyle().ItemSpacing.y * 3.0f;
    float scrollWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x;
    ImGui::BeginChild("ConfigScroll", ImVec2(scrollWidth, -bottomReservedHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    auto drawHeader = [](const char* title) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
        bool open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);
        ImGui::PopStyleColor(3);
        return open;
    };
    if (drawHeader("Notice")) {
        ImGui::TextWrapped("Performance Benchmark:");
        ImGui::BulletText("CLI (Direct GPU Overlay): ~100ms latency.");
        ImGui::BulletText("Software (AppSink + Texture Upload): ~200ms latency.");
        ImGui::Spacing();
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().WindowPadding.x);
        ImGui::TextDisabled("Note: The 100ms difference is the physical overhead of copying frames "
            "from Video Memory back to System RAM for UI synchronization.");
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Reference CLI Command (Minimum Latency):");
        const char* cliCmd = "gst-launch-1.0 -v rtspsrc location=\"rtsp://{user}:{password}@{ip}:554/h264/ch1/main/av_stream\" "
            "latency=0 buffer-mode=0 drop-on-latency=true protocols=udp ! rtph264depay ! h264parse ! "
            "d3d11h264dec ! d3d11videosink sync=false";
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImVec2 textSize = ImGui::CalcTextSize(cliCmd, nullptr, false, ImGui::GetContentRegionAvail().x - 20.0f);
        float childHeight = textSize.y + ImGui::GetStyle().FramePadding.y * 4.0f + 15.0f;
        if (ImGui::BeginChild("##CLI_Box", ImVec2(-1.0f, childHeight), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(cliCmd);
            ImGui::PopTextWrapPos();
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0))
                ImGui::SetClipboardText(cliCmd);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to copy text");
    }
    ImGui::Spacing();
    selectedNode->stream->DrawStreamConfigPanel();
    ImGui::PopStyleVar();
    ImGui::EndChild();
}
