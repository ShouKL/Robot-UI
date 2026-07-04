#include "LiveStreamManager.h"

LiveStreamManager::LiveStreamManager() {
    AddItem();
}

LiveStreamManager::~LiveStreamManager() {
    for (auto& stream : m_devices) {
        stream->Close();
    }
    m_devices.clear();
}

// ========    ========
void LiveStreamManager::AddItem() {
    auto stream = std::make_unique<LiveStream>();
    stream->id = NextId();

    char buf[64];
    snprintf(buf, sizeof(buf), "Item_%d", stream->id);
    strncpy_s(stream->name, buf, sizeof(stream->name) - 1);
    strncpy_s(stream->ip, "0.0.0.0", sizeof(stream->ip) - 1);

    m_devices.push_back(std::move(stream));
    if (m_devices.size() == 1) {
        m_devices[0]->isSelected = true;
    }
}

void LiveStreamManager::RemoveItem(int id) {
    //    unique_ptr  ，  FindNodeIndex（  .id → ->id）
    int index = -1;
    for (int i = 0; i < (int)m_devices.size(); ++i)
        if (m_devices[i]->id == id) { index = i; break; }
    if (index < 0 || index >= (int)m_devices.size()) return;
    if (m_devices.size() <= 1) return;
    if (m_devices[index]->isStreaming) {
        m_devices[index]->Close();
    }
    m_devices.erase(m_devices.begin() + index);
}

std::vector<LiveStream> LiveStreamManager::GetAllItems() const {
    std::vector<LiveStream> configs;
    for (const auto& stream : m_devices) {
        configs.push_back(*stream);
    }
    return configs;
}

void LiveStreamManager::LoadItems(const std::vector<LiveStream>& configs) {
    for (auto& stream : m_devices) {
        if (stream->isStreaming)
            stream->Close();
    }

    // Remember previously selected item's name
    std::string prevSelectedName;
    for (auto& s : m_devices)
        if (s->isSelected) { prevSelectedName = s->name; break; }

    m_devices.clear();
    ResetNextId(1);

    if (configs.empty()) {
        AddItem();
        return;
    }

    int newSelIdx = 0;
    for (size_t i = 0; i < configs.size(); ++i) {
        auto stream = std::make_unique<LiveStream>(configs[i]);
        stream->id = NextId();
        if (strcmp(stream->name, prevSelectedName.c_str()) == 0) newSelIdx = (int)i;
        m_devices.push_back(std::move(stream));
    }
    for (auto& s : m_devices) s->isSelected = false;
    if (newSelIdx >= 0 && newSelIdx < (int)m_devices.size())
        m_devices[newSelIdx]->isSelected = true;
    else if (!m_devices.empty())
        m_devices[0]->isSelected = true;
}

void LiveStreamManager::ResetToDefault()
{
    for (auto& stream : m_devices) {
        if (stream->isStreaming)
            stream->Close();
    }
    m_devices.clear();
    ResetNextId(1);
    AddItem();
    if (!m_devices.empty())
        m_devices[0]->isSelected = true;
}

void LiveStreamManager::RenameItem(int id, const char* newName)
{
    for (auto& stream : m_devices)
        if (stream->id == id) { strncpy_s(stream->name, newName, sizeof(stream->name) - 1); break; }
}

void LiveStreamManager::SelectItem(int index) {
    for (auto& n : m_devices) n->isSelected = false;
    if (index >= 0 && index < (int)m_devices.size())
        m_devices[index]->isSelected = true;
}

int LiveStreamManager::GetSelectedIndex() const {
    for (int i = 0; i < (int)m_devices.size(); ++i)
        if (m_devices[i]->isSelected) return i;
    return -1;
}

void LiveStreamManager::DrawContent() {
    ImGui::Indent(10.0f);
    ImGui::Spacing();
    auto* sel = GetSelectedDevice();
    if (!sel) { ImGui::TextDisabled("No item selected."); ImGui::Unindent(10.0f); return; }

    ImGui::Separator();
    float bottomReservedHeight = ImGui::GetFrameHeightWithSpacing() * 1.5f + ImGui::GetStyle().ItemSpacing.y * 3.0f;
    float scrollWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x;
    if (ImGui::BeginChild("ConfigScroll", ImVec2(scrollWidth, -bottomReservedHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        DrawStreamConfigPanel(*sel);
    }
    ImGui::EndChild();
    ImGui::Unindent(10.0f);
}

LiveStream* LiveStreamManager::GetSelectedDevice()
{
    for (auto& n : m_devices) if (n->isSelected) return n.get();
    return nullptr;
}

LiveStream* LiveStreamManager::GetDeviceByIndex(int index)
{
    if (index < 0 || index >= (int)m_devices.size()) return nullptr;
    return m_devices[index].get();
}

// ============================================================================
// UI    （  LiveStream    ）
// ============================================================================
void LiveStreamManager::DrawPropertyLabel(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

void LiveStreamManager::DrawConnectionSettings(LiveStream& stream) {
    if (ImGui::CollapsingHeader("Connection Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("ConnTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            DrawPropertyLabel("IP Address");  ImGui::InputText("##IP", stream.ip, IM_ARRAYSIZE(stream.ip));
            DrawPropertyLabel("Username");    ImGui::InputText("##User", stream.user, IM_ARRAYSIZE(stream.user));
            DrawPropertyLabel("Password");    ImGui::InputText("##Pass", stream.pass, IM_ARRAYSIZE(stream.pass), ImGuiInputTextFlags_Password);
            DrawPropertyLabel("Port");        ImGui::InputInt("##Port", &stream.port);
            ImGui::EndTable();
        }
    }
}

void LiveStreamManager::DrawProtocolCodecSettings(LiveStream& stream) {
    if (ImGui::CollapsingHeader("Protocol & Codec", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("ProtoTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            // ---  ---
            const char* brands[] = { "Hikvision", "Dahua", "Custom" };
            int current_brand = (int)stream.brand;
            DrawPropertyLabel("Camera Brand");
            if (ImGui::Combo("##Brand", &current_brand, brands, IM_ARRAYSIZE(brands)))
                stream.brand = (CameraBrand)current_brand;
            // ---  ---
            DrawPropertyLabel(stream.brand == CameraBrand::CUSTOM ? "Custom Path" : "Channel");
            if (stream.brand == CameraBrand::CUSTOM) {
                ImGui::InputText("##CustomPath", stream.customPath, IM_ARRAYSIZE(stream.customPath));
            }
            else {
                ImGui::InputInt("##Channel", &stream.channel);
            }
            // ---  (H.264 / H.265 / H.265+) ---
            const char* codecs[] = { "H264", "H265", "H.265+" };
            int current_codec = (int)stream.codec;
            DrawPropertyLabel("Codec");
            if (ImGui::Combo("##Codec", &current_codec, codecs, IM_ARRAYSIZE(codecs)))
                stream.codec = (CodecType)current_codec;
            // ---  ( / ) ---
            const char* streamTypes[] = { "Main Stream", "Sub Stream" };
            int current_stream_type = (int)stream.streamType;
            DrawPropertyLabel("Stream Type");
            if (ImGui::Combo("##StreamType", &current_stream_type, streamTypes, IM_ARRAYSIZE(streamTypes)))
                stream.streamType = (StreamType)current_stream_type;
            // ---  (TCP / UDP) ---
            const char* protocols[] = { "TCP", "UDP" };
            int current_proto = (int)stream.protocol;
            DrawPropertyLabel("Protocol");
            if (ImGui::Combo("##Protocol", &current_proto, protocols, IM_ARRAYSIZE(protocols)))
                stream.protocol = (TransportProto)current_proto;
            ImGui::EndTable();
        }
    }
}

void LiveStreamManager::DrawNetworkBufferSettings(LiveStream& stream) {
    if (ImGui::CollapsingHeader("Network & Buffer Limits", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("NetBufTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            // ---  (ms) ---
            DrawPropertyLabel("Latency (ms)");
            ImGui::InputInt("##Latency", &stream.latency);
            // --- UDP  (bytes) ---
            DrawPropertyLabel("UDP Buffer Size");
            ImGui::InputInt("##UDPSize", &stream.udpBufferSize);
            // ---  (us) ---
            DrawPropertyLabel("Timeout (us)");
            ImGui::InputInt("##Timeout", &stream.timeout);
            // ---  ---
            DrawPropertyLabel("Drop On Latency");
            ImGui::Checkbox("##DropLatency", &stream.dropOnLatency);
            // --- NTP  ---
            DrawPropertyLabel("NTP Sync");
            ImGui::Checkbox("##NTPSync", &stream.ntpSync);
            // ---  ---
            const char* bufferModes[] = { "Auto", "Slave", "Buffer", "Sync" };
            int current_buf_mode = (int)stream.bufferMode;
            DrawPropertyLabel("Buffer Mode");
            if (ImGui::Combo("##BufferMode", &current_buf_mode, bufferModes, IM_ARRAYSIZE(bufferModes))) {
                stream.bufferMode = (BufferMode)current_buf_mode;
            }
            ImGui::EndTable();
        }
    }
}

void LiveStreamManager::DrawDecoderRenderingSettings(LiveStream& stream) {
    if (ImGui::CollapsingHeader("Decoder & Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("DecTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            const char* decoders[] = { "Software (Safe)", "NVIDIA (CUVID)", "DirectX 11 (D3D11)", "Intel QSV" };
            int current_decoder = (int)stream.decoder;
            DrawPropertyLabel("Decoder Type");
            if (ImGui::Combo("##DecoderMode", &current_decoder, decoders, 4))
                stream.decoder = (DecoderType)current_decoder;
            DrawPropertyLabel("Swap Red/Blue");    ImGui::Checkbox("##SwapRB", &stream.useBGRA);
            ImGui::EndTable();
            ImGui::Spacing();
        }
    }
}

void LiveStreamManager::DrawNoticePanel(LiveStream& stream) {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    if (ImGui::CollapsingHeader("Notice", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
        ImGui::PopStyleColor(3);
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
        std::string cliCmd = stream.BuildCLIReferenceString();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImVec2 textSize = ImGui::CalcTextSize(cliCmd.c_str(), nullptr, false, ImGui::GetContentRegionAvail().x - 20.0f);
        float childHeight = textSize.y + ImGui::GetStyle().FramePadding.y * 4.0f + 15.0f;
        if (ImGui::BeginChild("##CLI_Box", ImVec2(-1.0f, childHeight), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(cliCmd.c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0))
                ImGui::SetClipboardText(cliCmd.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to copy text");
    } else {
        ImGui::PopStyleColor(3);
    }
}

void LiveStreamManager::DrawStreamConfigPanel(LiveStream& stream) {
    ImGui::PushItemWidth(-1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    DrawNoticePanel(stream);
    ImGui::Spacing();
    DrawConnectionSettings(stream);
    DrawProtocolCodecSettings(stream);
    DrawNetworkBufferSettings(stream);
    DrawDecoderRenderingSettings(stream);
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
}


std::string LiveStreamManager::ClipboardCopySelected()
{
    int idx = -1;
    for (int i = 0; i < (int)m_devices.size(); ++i)
        if (m_devices[i]->isSelected) { idx = i; break; }
    if (idx < 0 || idx >= (int)m_devices.size()) return {};

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "type"  << YAML::Value << "livestream";
    out << YAML::Key << "name"  << YAML::Value << m_devices[idx]->name;
    out << YAML::Key << "index" << YAML::Value << idx;
    out << YAML::EndMap;
    return out.c_str();
}

void LiveStreamManager::ClipboardPaste(const std::string& yaml)
{
    try {
        YAML::Node root = YAML::Load(yaml);
        if (!root.IsMap()) return;
        std::string type = root["type"] ? root["type"].as<std::string>() : "";
        int srcIdx = root["index"] ? root["index"].as<int>() : -1;
        if (type != "livestream" || srcIdx < 0 || srcIdx >= (int)m_devices.size()) return;

        std::string baseName = root["name"] ? root["name"].as<std::string>() : "Pasted";
        int n = 1;
        std::string finalName = baseName;
        while (true) {
            bool dup = false;
            for (auto& d : m_devices)
                if (std::string(d->name) == finalName) { dup = true; break; }
            if (!dup) break;
            finalName = baseName + "_" + std::to_string(n++);
        }

        AddItem();
        auto& dst = m_devices.back();
        strncpy_s(dst->name, sizeof(dst->name), finalName.c_str(), sizeof(dst->name) - 1);
        *dst = *m_devices[srcIdx];
        strncpy_s(dst->name, sizeof(dst->name), finalName.c_str(), sizeof(dst->name) - 1);
    } catch (...) {}
}
