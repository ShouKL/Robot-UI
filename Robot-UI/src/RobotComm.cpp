#include "RobotComm.h"

// ==================== 通用 Hex 编辑器 ====================
static void HexEdit(const char* label, std::vector<uint8_t>& bytes) {
    std::string hexStr;
    for (size_t i = 0; i < bytes.size(); ++i) {
        char buf[8];
        snprintf(buf, sizeof(buf), i ? ", 0x%02X" : "0x%02X", bytes[i]);
        hexStr += buf;
    }
    char buf[256] = {};
    strncpy(buf, hexStr.c_str(), sizeof(buf) - 1);
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText(label, buf, sizeof(buf))) {
        bytes.clear();
        std::string s(buf);
        size_t pos = 0;
        while (pos < s.size()) {
            while (pos < s.size() && !isxdigit((unsigned char)s[pos])) ++pos;
            if (pos >= s.size()) break;
            char* end = nullptr;
            unsigned long val = strtoul(s.c_str() + pos, &end, 16);
            if (val <= 0xFF) bytes.push_back((uint8_t)val);
            pos = end - s.c_str();
        }
    }
    ImGui::PopItemWidth();
}

// ==================== 主窗口 ====================
void RobotComm::DrawWindow(ProtocolSendConfig& sendCfg, ProtocolReceiveConfig& recvCfg,
                                ActuatorConfig& actuator, const SensorConfig& sensor) {
    if (!m_Open) return;

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Protocol Field Configuration", &m_Open)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("ProtoTabs")) {
        if (ImGui::BeginTabItem("Send Fields")) {
            m_TabIndex = 0;
            DrawSendFieldConfig(sendCfg, actuator);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Receive Fields")) {
            m_TabIndex = 1;
            DrawReceiveFieldConfig(recvCfg, sensor);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // 帧预览
    ImGui::Spacing();
    ImGui::Separator();
    if (m_TabIndex == 0) {
        if (ImGui::CollapsingHeader("Send Frame Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto preview = BuildFrame(actuator, sendCfg);
            std::string hexPreview;
            for (size_t i = 0; i < preview.size(); ++i) {
                char b[8];
                snprintf(b, sizeof(b), i ? " %02X" : "%02X", preview[i]);
                hexPreview += b;
            }
            ImGui::TextWrapped("Frame (%zu bytes): %s", preview.size(), hexPreview.c_str());
            for (const auto& f : sendCfg.fields) {
                double val = 0;
                if (GetActuatorField(actuator, f.field_path, val)) {
                    ImGui::Text("  %s (%s) = %.3f", f.name.c_str(), f.field_path.c_str(), val);
                } else {
                    ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  %s (%s) = NOT FOUND", f.name.c_str(), f.field_path.c_str());
                }
            }
        }
    } else {
        if (ImGui::CollapsingHeader("Receive Frame Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& fields = recvCfg.fields;
            int totalBytes = 0;
            for (const auto& f : fields)
                totalBytes += GetEncodingByteSize(f.encoding);

            ImGui::Text("Expected Payload: %d bytes (%zu fields)", totalBytes, fields.size());
            for (const auto& f : fields) {
                int sz = GetEncodingByteSize(f.encoding);
                ImGui::Text("  %s (%s)  [%s, %d bytes]", f.name.c_str(), f.field_path.c_str(),
                    GetEncodingNames()[EncodingToIndex(f.encoding)], sz);
            }
            if (!recvCfg.include_length)
                ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "Note: Payload length field is disabled.");
        }
    }

    ImGui::End();
}

// ==================== 发送协议字段配置 ====================
void RobotComm::DrawSendFieldConfig(ProtocolSendConfig& cfg, ActuatorConfig& actuator) {
    auto& fields = cfg.fields;
    auto encodingNames = GetEncodingNames();

    if (ImGui::CollapsingHeader("Frame Format", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Header:"); HexEdit("##SendHeader", cfg.header);
        ImGui::Text("Tail:");   HexEdit("##SendTail", cfg.tail);

        const char* checksumItems[] = { "None", "Sum8", "XOR8", "CRC16" };
        int csIdx = (int)cfg.checksum;
        if (ImGui::Combo("Checksum", &csIdx, checksumItems, IM_ARRAYSIZE(checksumItems)))
            cfg.checksum = (ChecksumType)csIdx;

        ImGui::Checkbox("Include Payload Length (2 bytes LE)", &cfg.include_length);
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text("Send Fields");
    ImGui::SameLine();
    if (ImGui::Button("+ Add Field")) {
        SendField f;
        f.name = "new_field";
        fields.push_back(f);
    }

    auto components = GetSendComponents(actuator, SensorConfig{});

    if (ImGui::BeginChild("SendFieldsScroll", ImVec2(0, 260), true)) {
        if (ImGui::BeginTable("SendFieldsTable", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("##drag", ImGuiTableColumnFlags_WidthFixed, 20);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 210);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 85);
            ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Fix", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableHeadersRow();

            int delIdx = -1;
            int dragFrom = -1, dragTo = -1;

            for (int i = 0; i < (int)fields.size(); ++i) {
                auto& f = fields[i];
                ImGui::PushID(i);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f, 0.25f, 0.25f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
                ImGui::SmallButton("##dragbtn");
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("SEND_FIELD_REORDER", &i, sizeof(int));
                        ImGui::TextUnformatted(f.name.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SEND_FIELD_REORDER")) {
                        dragFrom = *(const int*)payload->Data;
                        dragTo = i;
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::PopStyleColor(3);

                if (ImGui::BeginPopupContextItem("SendFieldCtx")) {
                    if (ImGui::MenuItem("Delete")) delIdx = i;
                    ImGui::EndPopup();
                }

                ImGui::TableSetColumnIndex(1);
                char nameBuf[128] = {};
                strncpy(nameBuf, f.name.c_str(), sizeof(nameBuf) - 1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
                    f.name = nameBuf;
                ImGui::PopItemWidth();

                std::string curCompId = ResolveComponentId(f.field_path);
                std::string curSub = ResolveSubField(f.field_path);

                ImGui::TableSetColumnIndex(2);
                ImGui::PushItemWidth(-1);
                std::string pathLabel = "---";
                for (auto& c : components) {
                    if (c.id == curCompId) {
                        auto sfs = GetSubFields(c);
                        for (auto& sf : sfs) {
                            if (sf.key == curSub) {
                                pathLabel = c.label + " > " + sf.label;
                                break;
                            }
                        }
                        if (pathLabel == "---") pathLabel = c.label + " > ...";
                        break;
                    }
                }
                if (ImGui::BeginCombo("##path", pathLabel.c_str())) {
                    for (auto& c : components) {
                        if (ImGui::BeginMenu(c.label.c_str())) {
                            auto sfs = GetSubFields(c);
                            for (auto& sf : sfs) {
                                bool sel = (c.id == curCompId && sf.key == curSub);
                                if (ImGui::MenuItem(sf.label.c_str(), nullptr, sel)) {
                                    f.field_path = c.path_prefix + sf.key;
                                }
                                if (sel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndMenu();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ImGui::TableSetColumnIndex(3);
                ImGui::PushItemWidth(-1);
                int encIdx = EncodingToIndex(f.encoding);
                if (ImGui::Combo("##sendtype", &encIdx, encodingNames.data(), (int)encodingNames.size()))
                    f.encoding = IndexToEncoding(encIdx);
                ImGui::PopItemWidth();

                ImGui::TableSetColumnIndex(4);
                char groupBuf[64] = {};
                strncpy(groupBuf, f.group.c_str(), sizeof(groupBuf) - 1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##sendgroup", groupBuf, sizeof(groupBuf)))
                    f.group = groupBuf;
                ImGui::PopItemWidth();

                ImGui::TableSetColumnIndex(5);
                ImGui::Checkbox("##sendvis", &f.visible);

                ImGui::TableSetColumnIndex(6);
                ImGui::Checkbox("##sendfix", &f.fix);

                ImGui::PopID();
            }

            if (dragFrom >= 0 && dragTo >= 0 && dragFrom != dragTo
                && dragFrom < (int)fields.size() && dragTo < (int)fields.size()) {
                SendField moved = fields[dragFrom];
                fields.erase(fields.begin() + dragFrom);
                if (dragTo > dragFrom) dragTo--;
                fields.insert(fields.begin() + dragTo, moved);
            }
            if (delIdx >= 0 && delIdx < (int)fields.size())
                fields.erase(fields.begin() + delIdx);

            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "Drag handle to reorder. Right-click to delete.");
}

// ==================== 接收协议字段配置 ====================
void RobotComm::DrawReceiveFieldConfig(ProtocolReceiveConfig& cfg, const SensorConfig& sensor) {
    auto& fields = cfg.fields;
    auto encodingNames = GetEncodingNames();
    auto components = GetRecvComponents(sensor);

    if (ImGui::CollapsingHeader("Frame Format", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Header:"); HexEdit("##RecvHeader", cfg.header);
        ImGui::Text("Tail:");   HexEdit("##RecvTail", cfg.tail);

        const char* checksumItems[] = { "None", "Sum8", "XOR8", "CRC16" };
        int csIdx = (int)cfg.checksum;
        if (ImGui::Combo("Checksum", &csIdx, checksumItems, IM_ARRAYSIZE(checksumItems)))
            cfg.checksum = (ChecksumType)csIdx;

        ImGui::Checkbox("Include Payload Length (2 bytes LE)", &cfg.include_length);

        int msgType = cfg.msg_type;
        ImGui::InputInt("Message Type (byte)", &msgType);
        if (msgType >= 0 && msgType <= 255) cfg.msg_type = (uint8_t)msgType;
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text("Receive Fields");
    ImGui::SameLine();
    if (ImGui::Button("+ Add Field")) {
        ReceiveField f;
        f.name = "new_sensor";
        fields.push_back(f);
    }

    if (ImGui::BeginChild("RecvFieldsScroll", ImVec2(0, 260), true)) {
        if (ImGui::BeginTable("RecvFieldsTable", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("##drag", ImGuiTableColumnFlags_WidthFixed, 20);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 210);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 85);
            ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Fix", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableHeadersRow();

            int delIdx = -1;
            int dragFrom = -1, dragTo = -1;

            for (int i = 0; i < (int)fields.size(); ++i) {
                auto& f = fields[i];
                ImGui::PushID(i);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f, 0.25f, 0.25f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
                ImGui::SmallButton("##dragbtn");
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("RECV_FIELD_REORDER", &i, sizeof(int));
                        ImGui::TextUnformatted(f.name.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RECV_FIELD_REORDER")) {
                        dragFrom = *(const int*)payload->Data;
                        dragTo = i;
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::PopStyleColor(3);
                if (ImGui::BeginPopupContextItem("RecvFieldCtx")) {
                    if (ImGui::MenuItem("Delete")) delIdx = i;
                    ImGui::EndPopup();
                }

                ImGui::TableSetColumnIndex(1);
                char nameBuf[128] = {};
                strncpy(nameBuf, f.name.c_str(), sizeof(nameBuf) - 1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
                    f.name = nameBuf;
                ImGui::PopItemWidth();

                std::string curCompId = ResolveComponentId(f.field_path);
                std::string curSub = ResolveSubField(f.field_path);

                ImGui::TableSetColumnIndex(2);
                ImGui::PushItemWidth(-1);
                std::string pathLabel = "---";
                for (auto& c : components) {
                    if (c.id == curCompId) {
                        auto sfs = GetSubFields(c);
                        for (auto& sf : sfs) {
                            if (sf.key == curSub) {
                                pathLabel = c.label + " > " + sf.label;
                                break;
                            }
                        }
                        if (pathLabel == "---") pathLabel = c.label + " > ...";
                        break;
                    }
                }
                if (ImGui::BeginCombo("##path", pathLabel.c_str())) {
                    for (auto& c : components) {
                        if (ImGui::BeginMenu(c.label.c_str())) {
                            auto sfs = GetSubFields(c);
                            for (auto& sf : sfs) {
                                bool sel = (c.id == curCompId && sf.key == curSub);
                                if (ImGui::MenuItem(sf.label.c_str(), nullptr, sel)) {
                                    f.field_path = c.path_prefix + sf.key;
                                }
                                if (sel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndMenu();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ImGui::TableSetColumnIndex(3);
                ImGui::PushItemWidth(-1);
                int encIdx = EncodingToIndex(f.encoding);
                if (ImGui::Combo("##recvtype", &encIdx, encodingNames.data(), (int)encodingNames.size()))
                    f.encoding = IndexToEncoding(encIdx);
                ImGui::PopItemWidth();

                ImGui::TableSetColumnIndex(4);
                char groupBuf[64] = {};
                strncpy(groupBuf, f.group.c_str(), sizeof(groupBuf) - 1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##recvgroup", groupBuf, sizeof(groupBuf)))
                    f.group = groupBuf;
                ImGui::PopItemWidth();

                ImGui::TableSetColumnIndex(5);
                ImGui::Checkbox("##recvvis", &f.visible);

                ImGui::TableSetColumnIndex(6);
                ImGui::Checkbox("##recvfix", &f.fix);

                ImGui::PopID();
            }

            if (dragFrom >= 0 && dragTo >= 0 && dragFrom != dragTo
                && dragFrom < (int)fields.size() && dragTo < (int)fields.size()) {
                ReceiveField moved = fields[dragFrom];
                fields.erase(fields.begin() + dragFrom);
                if (dragTo > dragFrom) dragTo--;
                fields.insert(fields.begin() + dragTo, moved);
            }
            if (delIdx >= 0 && delIdx < (int)fields.size())
                fields.erase(fields.begin() + delIdx);

            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "Drag handle to reorder. Right-click to delete.");
}


// ==================== 控制面板 ====================
void RobotComm::DrawControlPanel(RobotCommConfig& cfg, bool isConnected, int nodeId,
                                  RobotComponent* robotCfg,
                                  GamepadMapper* gamepadMapper,
                                  std::function<void(int)> onConnect,
                                  std::function<void()>   onDisconnect,
                                  std::function<void(int, int)> onActiveModeChanged,
                                  std::function<void(int, int)> onGamepadModeChanged) {
    ImGui::TextDisabled("ID: %d | Status: %s", nodeId, isConnected ? "CONNECTED" : "OFFLINE");
    ImGui::Separator();
    ImGui::Spacing();

    // 当前使用的机器人模式
    if (ImGui::CollapsingHeader("Robot Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (robotCfg) {
            auto& modes = robotCfg->GetModes();
            int activeIdx = robotCfg->GetActiveModeIndex();
            if (!modes.empty() && activeIdx >= 0 && activeIdx < (int)modes.size()) {
                std::string preview = modes[activeIdx].name;
                if (ImGui::BeginCombo("##RobotModeSelect", preview.c_str())) {
                    for (int i = 0; i < (int)modes.size(); ++i) {
                        bool isSelected = (i == activeIdx);
                        if (ImGui::Selectable(modes[i].name, isSelected)) {
                            int oldIdx = activeIdx;
                            robotCfg->SetActiveModeIndex(i);
                            robotCfg->GetEditModeIndex() = i;
                            if (onActiveModeChanged)
                                onActiveModeChanged(oldIdx, i);
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

    // 当前使用的游戏手柄模式
    if (ImGui::CollapsingHeader("Gamepad Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (gamepadMapper) {
            auto& gpModes = gamepadMapper->GetModes();
            int gpIdx = gamepadMapper->GetActiveModeIndex();
            if (!gpModes.empty() && gpIdx >= 0 && gpIdx < (int)gpModes.size()) {
                std::string gpPreview = gpModes[gpIdx].name;
                if (ImGui::BeginCombo("##GamepadModeSelect", gpPreview.c_str())) {
                    for (int i = 0; i < (int)gpModes.size(); ++i) {
                        bool isSelected = (i == gpIdx);
                        if (ImGui::Selectable(gpModes[i].name, isSelected)) {
                            int oldGpIdx = gpIdx;
                            gamepadMapper->SetActiveModeByIndex(i);
                            if (onGamepadModeChanged)
                                onGamepadModeChanged(oldGpIdx, i);
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        } else {
            ImGui::TextDisabled("Gamepad config not available");
        }
    }

    if (ImGui::CollapsingHeader("Network", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Host IP", cfg.host_ip, sizeof(cfg.host_ip));
        ImGui::InputInt("Remote Port", &cfg.remote_port);
        ImGui::InputInt("Local Port", &cfg.local_port);
    }

    if (ImGui::CollapsingHeader("Transport", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* protocols[] = { "UDP", "TCP", "Serial" };
        ImGui::Combo("Protocol", &cfg.transport_type, protocols, IM_ARRAYSIZE(protocols));
    }

    ImGui::Spacing();
    ImVec2 btnSize(-1.0f, ImGui::GetFrameHeightWithSpacing() * 1.5f);
    if (isConnected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("DISCONNECT", btnSize)) onDisconnect();
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.8f, 0.5f, 1.0f));
        if (ImGui::Button("APPLY & CONNECT", btnSize)) onConnect(nodeId);
        ImGui::PopStyleColor(3);
    }
}
