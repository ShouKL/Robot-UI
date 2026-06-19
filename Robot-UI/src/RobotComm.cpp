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
    if (ImGui::InputText(label, buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::vector<uint8_t> newBytes;
        std::string s(buf);
        size_t pos = 0;
        while (pos < s.size()) {
            while (pos < s.size() && !isxdigit((unsigned char)s[pos])) ++pos;
            if (pos >= s.size()) break;
            char* end = nullptr;
            unsigned long val = strtoul(s.c_str() + pos, &end, 16);
            if (val <= 0xFF) newBytes.push_back((uint8_t)val);
            pos = end - s.c_str();
        }
        if (newBytes != bytes) bytes = std::move(newBytes);
    }
    ImGui::PopItemWidth();
}

// ==================== 发送协议字段配置 ====================
void RobotComm::DrawSendFieldConfig(ProtocolSendConfig& cfg, ActuatorConfig& actuator) {
    auto& fields = cfg.fields;
    auto encodingNames = GetEncodingNames();

    if (ImGui::CollapsingHeader("Frame Format", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Name", cfg.name, sizeof(cfg.name));
        ImGui::Text("Command Bytes:"); HexEdit("##SendCmd", cfg.command_bytes);

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

    auto components = GetSendComponents(actuator, SensorConfig{});

    if (ImGui::BeginChild("SendFieldsScroll", ImVec2(0, 260), true)) {
        // 右键空白区域弹出添加菜单
        if (ImGui::BeginPopupContextWindow("SendFieldsCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Add Field")) {
                SendField f;
                f.name = "new_field";
                fields.push_back(f);
            }
            if (ImGui::MenuItem("Add Raw Data")) {
                SendField f;
                f.name = "raw";
                f.raw_data.push_back(0x00);
                fields.push_back(f);
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginTable("SendFieldsTable", 9,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("##drag", ImGuiTableColumnFlags_WidthFixed, 20);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 180);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 45);
            ImGui::TableSetupColumn("Fix", ImGuiTableColumnFlags_WidthFixed, 35);
            ImGui::TableSetupColumn("Fix Val", ImGuiTableColumnFlags_WidthFixed, 65);
            ImGui::TableSetupColumn("Raw Data", ImGuiTableColumnFlags_WidthFixed, 90);
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
                    if (ImGui::MenuItem("Rename")) m_EditingSendField = i;
                    if (ImGui::MenuItem("Delete")) delIdx = i;
                    ImGui::EndPopup();
                }

                ImGui::TableSetColumnIndex(1);
                bool isRaw = !f.raw_data.empty();
                if (m_EditingSendField == i) {
                    char nameBuf[128] = {};
                    strncpy(nameBuf, f.name.c_str(), sizeof(nameBuf) - 1);
                    ImGui::PushItemWidth(-1);
                    ImGui::SetKeyboardFocusHere();
                    if (ImGui::InputText("##sendname", nameBuf, sizeof(nameBuf),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                        { f.name = nameBuf; m_EditingSendField = -1; }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        { f.name = nameBuf; m_EditingSendField = -1; }
                    ImGui::PopItemWidth();
                } else {
                    if (ImGui::Selectable(isRaw ? f.name.c_str() : (f.name.empty() ? "##" : f.name.c_str()), false,
                            ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            m_EditingSendField = i;
                    }
                }

                std::string curCompId = ResolveComponentId(f.field_path);
                std::string curSub = ResolveSubField(f.field_path);

                ImGui::TableSetColumnIndex(2);
                if (isRaw) {
                    ImGui::TextDisabled("-");
                } else {
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
                }

                ImGui::TableSetColumnIndex(3);
                if (isRaw) {
                    ImGui::TextDisabled("-");
                } else {
                ImGui::PushItemWidth(-1);
                int encIdx = EncodingToIndex(f.encoding);
                if (ImGui::Combo("##sendtype", &encIdx, encodingNames.data(), (int)encodingNames.size()))
                    f.encoding = IndexToEncoding(encIdx);
                ImGui::PopItemWidth();
                }

                ImGui::TableSetColumnIndex(4);
                if (isRaw) {
                    ImGui::TextDisabled("-");
                } else {
                char groupBuf[64] = {};
                strncpy(groupBuf, f.group.c_str(), sizeof(groupBuf) - 1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##sendgroup", groupBuf, sizeof(groupBuf)))
                    f.group = groupBuf;
                ImGui::PopItemWidth();
                }

                ImGui::TableSetColumnIndex(5);
                if (isRaw) {
                    ImGui::TextDisabled("-");
                } else {
                ImGui::Checkbox("##sendvis", &f.visible);
                }

                ImGui::TableSetColumnIndex(6);
                if (isRaw) {
                    ImGui::TextDisabled("-");
                } else {
                if (ImGui::Checkbox("##sendfix", &f.fix)) {
                    // 勾选 fix 时，从执行器当前值自动填充 fix_value
                    if (f.fix) {
                        double curVal = 0.0;
                        GetActuatorField(actuator, f.field_path, curVal);
                        f.fix_value = curVal;
                    }
                }
                }

                ImGui::TableSetColumnIndex(7);
                if (isRaw) {
                    ImGui::TextDisabled("-");
                } else if (f.fix) {
                    ImGui::Text("%.4f", f.fix_value);
                } else {
                    ImGui::TextDisabled("-");
                }

                ImGui::TableSetColumnIndex(8);
                if (isRaw) {
                    HexEdit("##sendraw", f.raw_data);
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
        "Right-click empty to add. Double-click name to rename. Drag to reorder.");

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
        addBlock("CMD", (int)cfg.command_bytes.size());
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
        if (!cfg.command_bytes.empty()) {
            ImGui::Text("  [%2d-%2d] Command (%zu bytes)", offset, offset + (int)cfg.command_bytes.size() - 1, cfg.command_bytes.size());
            offset += (int)cfg.command_bytes.size();
        }
        if (cfg.include_length) {
            ImGui::Text("  [%2d-%2d] Payload Length (LE)", offset, offset + 1);
            offset += 2;
        }
        int fieldStart = offset;
        for (const auto& f : cfg.fields) {
            int sz;
            const char* typeLabel;
            if (!f.raw_data.empty()) {
                sz = (int)f.raw_data.size();
                typeLabel = "raw";
            } else {
                sz = GetEncodingByteSize(f.encoding);
                typeLabel = GetEncodingNames()[EncodingToIndex(f.encoding)];
            }
            ImGui::Text("  [%2d-%2d] %-24s [%s, %d bytes]", offset, offset + sz - 1, f.name.c_str(), typeLabel, sz);
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

        ImGui::Text("Command Bytes:"); HexEdit("##RecvCmd", cfg.command_bytes);
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text("Receive Fields");

    if (ImGui::BeginChild("RecvFieldsScroll", ImVec2(0, 260), true)) {
        // 右键空白区域弹出添加菜单
        if (ImGui::BeginPopupContextWindow("RecvFieldsCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Add Field")) {
                ReceiveField f;
                f.name = "new_sensor";
                fields.push_back(f);
            }
            ImGui::EndPopup();
        }
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
                    if (ImGui::MenuItem("Rename")) m_EditingRecvField = i;
                    if (ImGui::MenuItem("Delete")) delIdx = i;
                    ImGui::EndPopup();
                }

                ImGui::TableSetColumnIndex(1);
                if (m_EditingRecvField == i) {
                    char nameBuf[128] = {};
                    strncpy(nameBuf, f.name.c_str(), sizeof(nameBuf) - 1);
                    ImGui::PushItemWidth(-1);
                    ImGui::SetKeyboardFocusHere();
                    if (ImGui::InputText("##recvname", nameBuf, sizeof(nameBuf),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                        { f.name = nameBuf; m_EditingRecvField = -1; }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        { f.name = nameBuf; m_EditingRecvField = -1; }
                    ImGui::PopItemWidth();
                } else {
                    if (ImGui::Selectable(f.name.empty() ? "##" : f.name.c_str(), false,
                            ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            m_EditingRecvField = i;
                    }
                }

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
        "Right-click empty to add. Double-click name to rename. Drag to reorder.");

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
            addBlock("CMD", (int)cfg.command_bytes.size());
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
            if (!cfg.command_bytes.empty()) {
                ImGui::Text("  [%2d-%2d] Command (%zu bytes)", offset, offset + (int)cfg.command_bytes.size() - 1, cfg.command_bytes.size());
                offset += (int)cfg.command_bytes.size();
            }
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
    // 当前使用的机器人模式（每个 comm 节点独立存储自己的 component 选择）
    if (ImGui::CollapsingHeader("Robot Component", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (robotMgr) {
            auto& comps = robotMgr->GetComponents();
            if (cfg.active_component_idx < 0 || cfg.active_component_idx >= (int)comps.size())
                cfg.active_component_idx = 0;
            if (!comps.empty()) {
                std::string preview = comps[cfg.active_component_idx].name;
                if (ImGui::BeginCombo("##RobotModeSelect", preview.c_str())) {
                    for (int i = 0; i < (int)comps.size(); ++i) {
                        bool isSelected = (i == cfg.active_component_idx);
                        if (ImGui::Selectable(comps[i].name, isSelected)) {
                            cfg.active_component_idx = i;
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
            if (cfg.active_component_idx < 0 || cfg.active_component_idx >= (int)comps.size())
                cfg.active_component_idx = 0;
            if (cfg.active_component_idx >= 0 && cfg.active_component_idx < (int)comps.size()) {
                auto& mode = comps[cfg.active_component_idx];
                if (ImGui::BeginTabBar("ProtoSubTabs")) {
                    if (ImGui::BeginTabItem("Send Frames")) {
                        ImGui::Text("Send Frames (%zu):", cfg.protocol_send.size());
                        ImGui::SameLine();
                        if (ImGui::Button("+ Add Frame")) {
                            ProtocolSendConfig newCfg;
                            newCfg.command_bytes.push_back((uint8_t)cfg.protocol_send.size());
                            cfg.protocol_send.push_back(newCfg);
                        }

                        if (ImGui::BeginChild("SendFramesList", ImVec2(0, 120), true)) {
                            int delIdx = -1;
                            for (int i = 0; i < (int)cfg.protocol_send.size(); ++i) {
                                auto& sendCfg = cfg.protocol_send[i];
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
                                    if (sendCfg.command_bytes.size() == 1)
                                        snprintf(label, sizeof(label), "%s  [0x%02X] %zu fields", sendCfg.name, sendCfg.command_bytes[0], sendCfg.fields.size());
                                    else
                                        snprintf(label, sizeof(label), "%s  [cmd:%zub] %zu fields", sendCfg.name, sendCfg.command_bytes.size(), sendCfg.fields.size());
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
                            if (delIdx >= 0 && delIdx < (int)cfg.protocol_send.size()) {
                                cfg.protocol_send.erase(cfg.protocol_send.begin() + delIdx);
                                if (m_ActiveSendCfgIdx >= (int)cfg.protocol_send.size())
                                    m_ActiveSendCfgIdx = std::max(0, (int)cfg.protocol_send.size() - 1);
                            }
                        }
                        ImGui::EndChild();

                        ImGui::Separator();
                        if (m_ActiveSendCfgIdx >= 0 && m_ActiveSendCfgIdx < (int)cfg.protocol_send.size()) {
                            auto& activeCmd = cfg.protocol_send[m_ActiveSendCfgIdx].command_bytes;
                            if (activeCmd.size() == 1)
                                ImGui::Text("Editing Frame [0x%02X]:", activeCmd[0]);
                            else
                                ImGui::Text("Editing Frame [cmd:%zub]:", activeCmd.size());
                            DrawSendFieldConfig(cfg.protocol_send[m_ActiveSendCfgIdx], mode.actuator_config);
                        } else {
                            ImGui::TextDisabled("No send frame selected. Click '+' to add one.");
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Receive Frames")) {
                        ImGui::Text("Receive Frames (%zu):", cfg.protocol_receive.size());
                        ImGui::SameLine();
                        if (ImGui::Button("+ Add Frame")) {
                            ProtocolReceiveConfig newCfg;
                            newCfg.command_bytes.push_back((uint8_t)cfg.protocol_receive.size());
                            cfg.protocol_receive.push_back(newCfg);
                        }

                        if (ImGui::BeginChild("RecvFramesList", ImVec2(0, 100), true)) {
                            int delIdx = -1;
                            for (int i = 0; i < (int)cfg.protocol_receive.size(); ++i) {
                                auto& recvCfg = cfg.protocol_receive[i];
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
                                    if (recvCfg.command_bytes.size() == 1)
                                        snprintf(label, sizeof(label), "%s  [0x%02X] %zu fields", recvCfg.name, recvCfg.command_bytes[0], recvCfg.fields.size());
                                    else
                                        snprintf(label, sizeof(label), "%s  [cmd:%zub] %zu fields", recvCfg.name, recvCfg.command_bytes.size(), recvCfg.fields.size());
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
                            if (delIdx >= 0 && delIdx < (int)cfg.protocol_receive.size()) {
                                cfg.protocol_receive.erase(cfg.protocol_receive.begin() + delIdx);
                                if (m_ActiveRecvCfgIdx >= (int)cfg.protocol_receive.size())
                                    m_ActiveRecvCfgIdx = std::max(0, (int)cfg.protocol_receive.size() - 1);
                            }
                        }
                        ImGui::EndChild();

                        ImGui::Separator();
                        if (m_ActiveRecvCfgIdx >= 0 && m_ActiveRecvCfgIdx < (int)cfg.protocol_receive.size()) {
                            ImGui::Text("Editing: %s", cfg.protocol_receive[m_ActiveRecvCfgIdx].name);
                            DrawReceiveFieldConfig(cfg.protocol_receive[m_ActiveRecvCfgIdx], mode.sensor_config);
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

