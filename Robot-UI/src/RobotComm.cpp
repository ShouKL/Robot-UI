#include "RobotComm.h"
#include "RobotComponentManager.h"

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
void RobotComm::DrawWindow(std::vector<ProtocolSendConfig>& sendCfgs, std::vector<ProtocolReceiveConfig>& recvCfgs,
                                ActuatorConfig& actuator, const SensorConfig& sensor) {
    if (!m_Open) return;

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Protocol Field Configuration", &m_Open)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("ProtoTabs")) {
        if (ImGui::BeginTabItem("Send Frames")) {
            m_TabIndex = 0;
            ImGui::Text("Send Frames (%zu):", sendCfgs.size());
            ImGui::SameLine();
            if (ImGui::Button("+ Add Frame")) {
                ProtocolSendConfig newCfg;
                newCfg.command_byte = (uint8_t)sendCfgs.size();
                sendCfgs.push_back(newCfg);
            }

            if (ImGui::BeginChild("SendFramesListWin", ImVec2(0, 100), true)) {
                int delIdx = -1;
                for (int i = 0; i < (int)sendCfgs.size(); ++i) {
                    auto& sendCfg = sendCfgs[i];
                    ImGui::PushID(i);
                    if (m_EditingSendName == i) {
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputText("##sendRename", sendCfg.name, sizeof(sendCfg.name),
                                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                            m_EditingSendName = -1;
                        if (ImGui::IsItemDeactivatedAfterEdit())
                            m_EditingSendName = -1;
                    } else {
                        char label[64];
                        snprintf(label, sizeof(label), "%s  [0x%02X] %zu fields", sendCfg.name, sendCfg.command_byte, sendCfg.fields.size());
                        if (ImGui::Selectable(label, m_ActiveSendCfgIdx == i)) {
                            m_ActiveSendCfgIdx = i;
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                                m_EditingSendName = i;
                        }
                        if (ImGui::BeginPopupContextItem()) {
                            if (ImGui::MenuItem("Rename")) m_EditingSendName = i;
                            if (ImGui::MenuItem("Delete Frame")) delIdx = i;
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::PopID();
                }
                if (delIdx >= 0 && delIdx < (int)sendCfgs.size()) {
                    sendCfgs.erase(sendCfgs.begin() + delIdx);
                    if (m_ActiveSendCfgIdx >= (int)sendCfgs.size())
                        m_ActiveSendCfgIdx = std::max(0, (int)sendCfgs.size() - 1);
                }
            }
            ImGui::EndChild();

            ImGui::Separator();
            if (m_ActiveSendCfgIdx >= 0 && m_ActiveSendCfgIdx < (int)sendCfgs.size()) {
                DrawSendFieldConfig(sendCfgs[m_ActiveSendCfgIdx], actuator);
            } else {
                ImGui::TextDisabled("No send frame selected.");
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Receive Frames")) {
            m_TabIndex = 1;
            ImGui::Text("Receive Frames (%zu):", recvCfgs.size());
            ImGui::SameLine();
            if (ImGui::Button("+ Add Frame##W")) {
                ProtocolReceiveConfig newCfg;
                newCfg.command_byte = (uint8_t)recvCfgs.size();
                recvCfgs.push_back(newCfg);
            }

            if (ImGui::BeginChild("RecvFramesListWin", ImVec2(0, 100), true)) {
                int delIdx = -1;
                for (int i = 0; i < (int)recvCfgs.size(); ++i) {
                    auto& rc = recvCfgs[i];
                    ImGui::PushID(i + 1000);
                    if (m_EditingRecvName == i) {
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputText("##recvRename", rc.name, sizeof(rc.name),
                                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                            m_EditingRecvName = -1;
                        if (ImGui::IsItemDeactivatedAfterEdit())
                            m_EditingRecvName = -1;
                    } else {
                        char label[64];
                        snprintf(label, sizeof(label), "%s  [0x%02X] %zu fields", rc.name, rc.command_byte, rc.fields.size());
                        if (ImGui::Selectable(label, m_ActiveRecvCfgIdx == i)) {
                            m_ActiveRecvCfgIdx = i;
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                                m_EditingRecvName = i;
                        }
                        if (ImGui::BeginPopupContextItem()) {
                            if (ImGui::MenuItem("Rename")) m_EditingRecvName = i;
                            if (ImGui::MenuItem("Delete Frame")) delIdx = i;
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::PopID();
                }
                if (delIdx >= 0 && delIdx < (int)recvCfgs.size()) {
                    recvCfgs.erase(recvCfgs.begin() + delIdx);
                    if (m_ActiveRecvCfgIdx >= (int)recvCfgs.size())
                        m_ActiveRecvCfgIdx = std::max(0, (int)recvCfgs.size() - 1);
                }
            }
            ImGui::EndChild();

            ImGui::Separator();
            if (m_ActiveRecvCfgIdx >= 0 && m_ActiveRecvCfgIdx < (int)recvCfgs.size()) {
                ImGui::Text("Editing: %s", recvCfgs[m_ActiveRecvCfgIdx].name);
                DrawReceiveFieldConfig(recvCfgs[m_ActiveRecvCfgIdx], sensor);
            } else {
                ImGui::TextDisabled("No receive frame selected.");
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // 帧预览
    ImGui::Spacing();
    ImGui::Separator();
    if (m_TabIndex == 0) {
        if (ImGui::CollapsingHeader("Send Frame Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (sendCfgs.empty()) {
                ImGui::TextDisabled("No send frames configured.");
            } else {
                for (size_t fi = 0; fi < sendCfgs.size(); ++fi) {
                    const ProtocolSendConfig& sendCfg = sendCfgs[fi];
                    auto preview = BuildFrame(actuator, sendCfg);
                    std::string hexPreview;
                    for (size_t i = 0; i < preview.size(); ++i) {
                        char b[8];
                        snprintf(b, sizeof(b), i ? " %02X" : "%02X", preview[i]);
                        hexPreview += b;
                    }
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s  [0x%02X] (%zu bytes):", sendCfg.name, sendCfg.command_byte, preview.size());
                    ImGui::TextWrapped("%s", hexPreview.c_str());
                    for (const auto& f : sendCfg.fields) {
                        double val = 0;
                        if (GetActuatorField(actuator, f.field_path, val)) {
                            ImGui::Text("  %s (%s) = %.3f", f.name.c_str(), f.field_path.c_str(), val);
                        } else {
                            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  %s (%s) = NOT FOUND", f.name.c_str(), f.field_path.c_str());
                        }
                    }
                    if (fi < sendCfgs.size() - 1) ImGui::Separator();
                }
            }
        }
    } else {
        if (ImGui::CollapsingHeader("Receive Frame Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (recvCfgs.empty()) {
                ImGui::TextDisabled("No receive frames configured.");
            } else {
                for (size_t fi = 0; fi < recvCfgs.size(); ++fi) {
                    const auto& rc = recvCfgs[fi];
                    int totalBytes = 0;
                    for (const auto& f : rc.fields)
                        totalBytes += GetEncodingByteSize(f.encoding);

                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s  [0x%02X] — Payload: %d bytes (%zu fields):",
                        rc.name, rc.command_byte, totalBytes, rc.fields.size());
                    for (const auto& f : rc.fields) {
                        int sz = GetEncodingByteSize(f.encoding);
                        ImGui::Text("  %s (%s)  [%s, %d bytes]", f.name.c_str(), f.field_path.c_str(),
                            GetEncodingNames()[EncodingToIndex(f.encoding)], sz);
                    }
                    if (!rc.include_length)
                        ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "  Note: Payload length field is disabled.");
                    if (fi < recvCfgs.size() - 1) ImGui::Separator();
                }
            }
        }
    }

    ImGui::End();
}

// ==================== 发送协议字段配置 ====================
void RobotComm::DrawSendFieldConfig(ProtocolSendConfig& cfg, ActuatorConfig& actuator) {
    auto& fields = cfg.fields;
    auto encodingNames = GetEncodingNames();

    if (ImGui::CollapsingHeader("Frame Format", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Name", cfg.name, sizeof(cfg.name));
        int cmdByte = cfg.command_byte;
        ImGui::InputInt("Frame Type (0-255)", &cmdByte);
        if (cmdByte >= 0 && cmdByte <= 255) cfg.command_byte = (uint8_t)cmdByte;

        ImGui::Text("Header:"); HexEdit("##SendHeader", cfg.header);
        ImGui::Text("Tail:");   HexEdit("##SendTail", cfg.tail);

        const char* checksumItems[] = { "None", "Sum8", "XOR8", "CRC16" };
        int csIdx = (int)cfg.checksum;
        if (ImGui::Combo("Checksum", &csIdx, checksumItems, IM_ARRAYSIZE(checksumItems)))
            cfg.checksum = (ChecksumType)csIdx;

        ImGui::Checkbox("Include Payload Length (2 bytes LE)", &cfg.include_length);
        ImGui::Checkbox("Big Endian (network byte order)", &cfg.big_endian);
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
        if (ImGui::BeginTable("SendFieldsTable", 8,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("##drag", ImGuiTableColumnFlags_WidthFixed, 20);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 210);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 85);
            ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Fix", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableSetupColumn("Fix Val", ImGuiTableColumnFlags_WidthFixed, 70);
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
                if (ImGui::Checkbox("##sendfix", &f.fix)) {
                    // 勾选 fix 时，从执行器当前值自动填充 fix_value
                    if (f.fix) {
                        double curVal = 0.0;
                        if (GetActuatorField(actuator, f.field_path, curVal))
                            f.fix_value = curVal;
                    }
                }

                ImGui::TableSetColumnIndex(7);
                if (f.fix) {
                    ImGui::PushItemWidth(-1);
                    ImGui::InputDouble("##sendfixval", &f.fix_value, 0.0, 0.0, "%.4f");
                    ImGui::PopItemWidth();
                } else {
                    ImGui::TextDisabled("-");
                }

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
    }
    ImGui::EndChild();

    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "Drag handle to reorder. Right-click to delete.");

    // 发送帧格式（根据当前配置动态生成）
    if (ImGui::TreeNode("Send Frame Format")) {
        const char* csNames[] = { "None", "Sum8", "XOR8", "CRC16" };
        int csBytes = (cfg.checksum == ChecksumType::CRC16) ? 2 : (cfg.checksum != ChecksumType::None ? 1 : 0);

        // 统计 payload 大小
        int payloadBytes = 0;
        for (const auto& f : cfg.fields)
            payloadBytes += GetEncodingByteSize(f.encoding);

        // 构建结构行
        std::string structure;
        int totalBytes = 0;

        auto addBlock = [&](const char* label, int bytes) {
            if (bytes <= 0) return;
            if (!structure.empty()) structure += " ";
            structure += "[";
            structure += label;
            structure += "]";
            totalBytes += bytes;
        };

        addBlock("Header", (int)cfg.header.size());
        addBlock("CMD", 1);
        if (cfg.include_length)
            addBlock("Len(LE)", 2);
        addBlock("Payload", payloadBytes);
        if (csBytes > 0)
            addBlock(csNames[(int)cfg.checksum], csBytes);
        addBlock("Tail", (int)cfg.tail.size());

        ImGui::TextWrapped("Structure: %s", structure.c_str());
        ImGui::Text("Total frame size: %d bytes", totalBytes);

        // 字段明细
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Field layout:");
        int offset = 0;
        if (!cfg.header.empty()) {
            ImGui::Text("  [%2d-%2d] Header (%zu bytes)", offset, offset + (int)cfg.header.size() - 1, cfg.header.size());
            offset += (int)cfg.header.size();
        }
        ImGui::Text("  [%2d    ] Command Byte = 0x%02X", offset, cfg.command_byte);
        offset += 1;
        if (cfg.include_length) {
            ImGui::Text("  [%2d-%2d] Payload Length (LE)", offset, offset + 1);
            offset += 2;
        }
        int fieldStart = offset;
        for (const auto& f : cfg.fields) {
            int sz = GetEncodingByteSize(f.encoding);
            const char* encName = GetEncodingNames()[EncodingToIndex(f.encoding)];
            ImGui::Text("  [%2d-%2d] %-24s [%s, %d bytes]", offset, offset + sz - 1, f.name.c_str(), encName, sz);
            offset += sz;
        }
        if (offset > fieldStart && csBytes > 0) {
            ImGui::Text("  [%2d-%2d] %s (over payload [%d-%d])", offset, offset + csBytes - 1, csNames[(int)cfg.checksum], fieldStart, offset - 1);
            offset += csBytes;
        }
        if (!cfg.tail.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  [%2d-%2d] Tail (%zu bytes)", offset, offset + (int)cfg.tail.size() - 1, cfg.tail.size());
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Endian: %s", cfg.big_endian ? "Big Endian" : "Little Endian");
        ImGui::TreePop();
    }
}

// ==================== 接收协议字段配置 ====================
void RobotComm::DrawReceiveFieldConfig(ProtocolReceiveConfig& cfg, const SensorConfig& sensor) {
    auto& fields = cfg.fields;
    auto encodingNames = GetEncodingNames();
    auto components = GetRecvComponents(sensor);

    if (ImGui::CollapsingHeader("Frame Format", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Name", cfg.name, sizeof(cfg.name));
        ImGui::Text("Header:"); HexEdit("##RecvHeader", cfg.header);
        ImGui::Text("Tail:");   HexEdit("##RecvTail", cfg.tail);

        const char* checksumItems[] = { "None", "Sum8", "XOR8", "CRC16" };
        int csIdx = (int)cfg.checksum;
        if (ImGui::Combo("Checksum", &csIdx, checksumItems, IM_ARRAYSIZE(checksumItems)))
            cfg.checksum = (ChecksumType)csIdx;

        ImGui::Checkbox("Include Payload Length (2 bytes LE)", &cfg.include_length);
        ImGui::Checkbox("Big Endian (network byte order)", &cfg.big_endian);

        int cmdByte = cfg.command_byte;
        ImGui::InputInt("Frame Type (0-255)", &cmdByte);
        if (cmdByte >= 0 && cmdByte <= 255) cfg.command_byte = (uint8_t)cmdByte;
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
    }
    ImGui::EndChild();

    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "Drag handle to reorder. Right-click to delete.");

    // 接收帧格式（根据当前配置动态生成）
    if (ImGui::TreeNode("Receive Frame Format")) {
        const char* csNames[] = { "None", "Sum8", "XOR8", "CRC16" };
        int csBytes = (cfg.checksum == ChecksumType::CRC16) ? 2 : (cfg.checksum != ChecksumType::None ? 1 : 0);

        bool bRawMode = (cfg.header.empty() && cfg.tail.empty() && !cfg.include_length
                         && cfg.checksum == ChecksumType::None);

        // 统计 payload 大小
        int payloadBytes = 0;
        for (const auto& f : cfg.fields)
            payloadBytes += GetEncodingByteSize(f.encoding);

        // 构建结构行
        std::string structure;
        int totalBytes = 0;

        auto addBlock = [&](const char* label, int bytes) {
            if (bytes <= 0) return;
            if (!structure.empty()) structure += " ";
            structure += "[";
            structure += label;
            structure += "]";
            totalBytes += bytes;
        };

        if (bRawMode) {
            addBlock("Payload", payloadBytes);
        } else {
            addBlock("Header", (int)cfg.header.size());
            addBlock("MsgType", 1);
            if (cfg.include_length)
                addBlock("Len(LE)", 2);
            addBlock("Payload", payloadBytes);
            if (csBytes > 0)
                addBlock(csNames[(int)cfg.checksum], csBytes);
            addBlock("Tail", (int)cfg.tail.size());
        }

        ImGui::TextWrapped("Structure: %s", structure.c_str());
        ImGui::Text("Total frame size: %d bytes", totalBytes);

        // 字段明细
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Field layout:");
        if (bRawMode) {
            int off = 0;
            for (const auto& f : cfg.fields) {
                int sz = GetEncodingByteSize(f.encoding);
                const char* encName = GetEncodingNames()[EncodingToIndex(f.encoding)];
                ImGui::Text("  [%2d-%2d] %-24s [%s, %d bytes]", off, off + sz - 1, f.name.c_str(), encName, sz);
                off += sz;
            }
        } else {
            int offset = 0;
            if (!cfg.header.empty()) {
                ImGui::Text("  [%2d-%2d] Header (%zu bytes)", offset, offset + (int)cfg.header.size() - 1, cfg.header.size());
                offset += (int)cfg.header.size();
            }
            ImGui::Text("  [%2d    ] Command Byte = 0x%02X", offset, cfg.command_byte);
            offset += 1;
            if (cfg.include_length) {
                ImGui::Text("  [%2d-%2d] Payload Length (LE)", offset, offset + 1);
                offset += 2;
            }
            int fieldStart = offset;
            for (const auto& f : cfg.fields) {
                int sz = GetEncodingByteSize(f.encoding);
                const char* encName = GetEncodingNames()[EncodingToIndex(f.encoding)];
                ImGui::Text("  [%2d-%2d] %-24s [%s, %d bytes]", offset, offset + sz - 1, f.name.c_str(), encName, sz);
                offset += sz;
            }
            if (offset > fieldStart && csBytes > 0) {
                ImGui::Text("  [%2d-%2d] %s (over payload [%d-%d])", offset, offset + csBytes - 1, csNames[(int)cfg.checksum], fieldStart, offset - 1);
                offset += csBytes;
            }
            if (!cfg.tail.empty()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  [%2d-%2d] Tail (%zu bytes)", offset, offset + (int)cfg.tail.size() - 1, cfg.tail.size());
            }
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Endian: %s", cfg.big_endian ? "Big Endian" : "Little Endian");
        ImGui::TreePop();
    }
}


// ==================== 控制面板 ====================
void RobotComm::DrawControlPanel(RobotCommConfig& cfg,
                                  RobotComponentManager* robotMgr) {
    // 当前使用的机器人模式
    if (ImGui::CollapsingHeader("Robot Component", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (robotMgr) {
            auto& comps = robotMgr->GetComponents();
            int activeIdx = robotMgr->GetSelectedIndex();
            if (!comps.empty() && activeIdx >= 0 && activeIdx < (int)comps.size()) {
                std::string preview = comps[activeIdx].component.name;
                if (ImGui::BeginCombo("##RobotModeSelect", preview.c_str())) {
                    for (int i = 0; i < (int)comps.size(); ++i) {
                        bool isSelected = (i == activeIdx);
                        if (ImGui::Selectable(comps[i].component.name, isSelected)) {
                            robotMgr->SetSelectedIndex(i);
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        } else {
            ImGui::TextDisabled("Robot component not available");
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
        ImGui::InputInt("Send Freq (Hz)", &cfg.send_freq_hz, 1, 10);
        if (cfg.send_freq_hz < 1) cfg.send_freq_hz = 1;
        if (cfg.send_freq_hz > 1000) cfg.send_freq_hz = 1000;
    }

    if (ImGui::CollapsingHeader("Protocol Fields", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (robotMgr) {
            auto& comps = robotMgr->GetComponents();
            int idx = robotMgr->GetSelectedIndex();
            if (idx >= 0 && idx < (int)comps.size()) {
                auto& mode = comps[idx].component;
                if (ImGui::BeginTabBar("ProtoSubTabs")) {
                    if (ImGui::BeginTabItem("Send Frames")) {
                        ImGui::Text("Send Frames (%zu):", mode.protocol_send.size());
                        ImGui::SameLine();
                        if (ImGui::Button("+ Add Frame")) {
                            ProtocolSendConfig newCfg;
                            newCfg.command_byte = (uint8_t)mode.protocol_send.size();
                            mode.protocol_send.push_back(newCfg);
                        }

                        if (ImGui::BeginChild("SendFramesList", ImVec2(0, 120), true)) {
                            int delIdx = -1;
                            for (int i = 0; i < (int)mode.protocol_send.size(); ++i) {
                                auto& sendCfg = mode.protocol_send[i];
                                ImGui::PushID(i + 2000);
                                if (m_EditingSendName == i) {
                                    ImGui::SetNextItemWidth(-1);
                                    if (ImGui::InputText("##sendRename2", sendCfg.name, sizeof(sendCfg.name),
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                                        m_EditingSendName = -1;
                                    if (ImGui::IsItemDeactivatedAfterEdit())
                                        m_EditingSendName = -1;
                                } else {
                                    char label[64];
                                    snprintf(label, sizeof(label), "%s  [0x%02X] %zu fields", sendCfg.name, sendCfg.command_byte, sendCfg.fields.size());
                                    if (ImGui::Selectable(label, m_ActiveSendCfgIdx == i)) {
                                        m_ActiveSendCfgIdx = i;
                                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                                            m_EditingSendName = i;
                                    }
                                    if (ImGui::BeginPopupContextItem()) {
                                        if (ImGui::MenuItem("Rename")) m_EditingSendName = i;
                                        if (ImGui::MenuItem("Delete Frame")) delIdx = i;
                                        ImGui::EndPopup();
                                    }
                                }
                                ImGui::PopID();
                            }
                            if (delIdx >= 0 && delIdx < (int)mode.protocol_send.size()) {
                                mode.protocol_send.erase(mode.protocol_send.begin() + delIdx);
                                if (m_ActiveSendCfgIdx >= (int)mode.protocol_send.size())
                                    m_ActiveSendCfgIdx = std::max(0, (int)mode.protocol_send.size() - 1);
                            }
                        }
                        ImGui::EndChild();

                        ImGui::Separator();
                        if (m_ActiveSendCfgIdx >= 0 && m_ActiveSendCfgIdx < (int)mode.protocol_send.size()) {
                            ImGui::Text("Editing Frame [0x%02X]:", mode.protocol_send[m_ActiveSendCfgIdx].command_byte);
                            DrawSendFieldConfig(mode.protocol_send[m_ActiveSendCfgIdx], mode.actuator_config);
                        } else {
                            ImGui::TextDisabled("No send frame selected. Click '+' to add one.");
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Receive Frames")) {
                        ImGui::Text("Receive Frames (%zu):", mode.protocol_receive.size());
                        ImGui::SameLine();
                        if (ImGui::Button("+ Add Frame")) {
                            ProtocolReceiveConfig newCfg;
                            newCfg.command_byte = (uint8_t)mode.protocol_receive.size();
                            mode.protocol_receive.push_back(newCfg);
                        }

                        if (ImGui::BeginChild("RecvFramesList", ImVec2(0, 100), true)) {
                            int delIdx = -1;
                            for (int i = 0; i < (int)mode.protocol_receive.size(); ++i) {
                                auto& recvCfg = mode.protocol_receive[i];
                                ImGui::PushID(i + 3000);
                                if (m_EditingRecvName == i) {
                                    ImGui::SetNextItemWidth(-1);
                                    if (ImGui::InputText("##recvRename2", recvCfg.name, sizeof(recvCfg.name),
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                                        m_EditingRecvName = -1;
                                    if (ImGui::IsItemDeactivatedAfterEdit())
                                        m_EditingRecvName = -1;
                                } else {
                                    char label[64];
                                    snprintf(label, sizeof(label), "%s  [0x%02X] %zu fields", recvCfg.name, recvCfg.command_byte, recvCfg.fields.size());
                                    if (ImGui::Selectable(label, m_ActiveRecvCfgIdx == i)) {
                                        m_ActiveRecvCfgIdx = i;
                                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                                            m_EditingRecvName = i;
                                    }
                                    if (ImGui::BeginPopupContextItem()) {
                                        if (ImGui::MenuItem("Rename")) m_EditingRecvName = i;
                                        if (ImGui::MenuItem("Delete Frame")) delIdx = i;
                                        ImGui::EndPopup();
                                    }
                                }
                                ImGui::PopID();
                            }
                            if (delIdx >= 0 && delIdx < (int)mode.protocol_receive.size()) {
                                mode.protocol_receive.erase(mode.protocol_receive.begin() + delIdx);
                                if (m_ActiveRecvCfgIdx >= (int)mode.protocol_receive.size())
                                    m_ActiveRecvCfgIdx = std::max(0, (int)mode.protocol_receive.size() - 1);
                            }
                        }
                        ImGui::EndChild();

                        ImGui::Separator();
                        if (m_ActiveRecvCfgIdx >= 0 && m_ActiveRecvCfgIdx < (int)mode.protocol_receive.size()) {
                            ImGui::Text("Editing: %s", mode.protocol_receive[m_ActiveRecvCfgIdx].name);
                            DrawReceiveFieldConfig(mode.protocol_receive[m_ActiveRecvCfgIdx], mode.sensor_config);
                        } else {
                            ImGui::TextDisabled("No receive frame selected.");
                        }
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            } else {
                ImGui::TextDisabled("No Robot item selected.");
            }
        } else {
            ImGui::TextDisabled("RobotComponent not available.");
        }
    }
}

