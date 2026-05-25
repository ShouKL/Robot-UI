#include "Robot_Config.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cctype>

Robot_Config::Robot_Config() {
    modes.clear();
    active_modes.clear();
    active_mode_index = 0;
}

Robot_Config::~Robot_Config() {}

// ==================== 草稿机制 ====================

void Robot_Config::BeginEdit() {
    m_EditingModes = modes;                // 深拷贝
    m_EditingModeIndex = edit_mode_index;
    m_IsEditing = true;
}

void Robot_Config::ApplyEdit() {
    if (!m_IsEditing) return;

    // 1. 保存草稿到正式模式列表
    modes = m_EditingModes;
    edit_mode_index = m_EditingModeIndex;

    // 2. 同步到活跃配置（控制线程可见）
    active_modes = modes;
    if (active_mode_index >= active_modes.size())
        active_mode_index = 0;

    // 3. 退出编辑
    m_IsEditing = false;
}

void Robot_Config::CancelEdit() {
    m_IsEditing = false;
    // modes 不受影响，保持原样
}

int& Robot_Config::GetEditModeIndex() {
    return m_IsEditing ? m_EditingModeIndex : edit_mode_index;
}

// ==================== 模式增删 ====================

void Robot_Config::AddMode() {
    if (m_IsEditing) {
        RobotMode new_mode;
        int idx = 0;
        while (true) {
            snprintf(new_mode.name, sizeof(new_mode.name), "Mode %d", idx);
            bool exists = false;
            for (auto& m : m_EditingModes) {
                if (m.name == new_mode.name) { exists = true; break; }
            }
            if (!exists) break;
            ++idx;
        }
        m_EditingModes.push_back(new_mode);
        m_EditingModeIndex = (int)m_EditingModes.size() - 1;
    }
    else {
        RobotMode new_mode;
        int idx = 0;
        while (true) {
            snprintf(new_mode.name, sizeof(new_mode.name), "Mode %d", idx);
            bool exists = false;
            for (auto& m : modes) {
                if (m.name == new_mode.name) { exists = true; break; }
            }
            if (!exists) break;
            ++idx;
        }
        modes.push_back(new_mode);
        edit_mode_index = (int)modes.size() - 1;
    }
}

void Robot_Config::DeleteMode(int index) {
    if (m_IsEditing) {
        if (m_EditingModes.empty()) return;
        if (index < 0 || index >= (int)m_EditingModes.size()) return;
        m_EditingModes.erase(m_EditingModes.begin() + index);
        if (m_EditingModeIndex >= (int)m_EditingModes.size())
            m_EditingModeIndex = std::max(0, (int)m_EditingModes.size() - 1);
    }
    else {
        if (modes.empty()) return;
        if (index < 0 || index >= (int)modes.size()) return;
        modes.erase(modes.begin() + index);
        if (edit_mode_index >= (int)modes.size())
            edit_mode_index = std::max(0, (int)modes.size() - 1);
    }
}

// ==================== UI 绘制 ====================

void Robot_Config::DrawRobotConfigPanel() {
    // 确保在编辑状态下绘制
    if (!m_IsEditing) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Not in edit mode.");
        return;
    }

    auto& modesRef = m_EditingModes;                 // 编辑草稿
    int& modeIdx = m_EditingModeIndex;

    // Close ThrustCurveEditor when user switches editing mode
    if (m_LastEditingModeIndex != modeIdx && m_ThrustCurveEditor.IsOpen()) {
        m_ThrustCurveEditor.Close();
    }
    m_LastEditingModeIndex = modeIdx;

    if (modesRef.empty()) return;
    auto& mode = modesRef[modeIdx];

    // ---------- 名称 ----------
    ImGui::InputText("Name", mode.name, sizeof(mode.name));

    ImGui::Spacing();
    ImGui::Text("Network & Protocol Configuration");
    ImGui::Separator();

    char ip_buf[128];
    strncpy(ip_buf, mode.host_ip.c_str(), sizeof(ip_buf));
    ImGui::InputText("Host IP", ip_buf, sizeof(ip_buf));
    mode.host_ip = ip_buf;

    ImGui::InputInt("Remote Port", &mode.remote_port);
    ImGui::InputInt("Local Port", &mode.local_port);

    ImGui::Spacing();
    ImGui::Text("Actuators");
    ImGui::Separator();

    auto& actuator_config = mode.actuator_config;

    // 马达编辑
    ImGui::PushID("MotorsSection");
    bool motor_node_open = ImGui::TreeNode("Brushless Motors");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
    if (ImGui::SmallButton("+")) {
        int next_id = actuator_config.brushlessmotor.empty() ? 0 : actuator_config.brushlessmotor.rbegin()->first + 1;
        BrushlessMotor motor;
        motor.id = next_id;
        actuator_config.brushlessmotor[next_id] = motor;
    }
    if (ImGui::BeginPopupContextItem("MotorTreeCtx")) {
        if (ImGui::MenuItem("Add Motor")) {
            int next_id = actuator_config.brushlessmotor.empty() ? 0 : actuator_config.brushlessmotor.rbegin()->first + 1;
            BrushlessMotor motor;
            motor.id = next_id;
            actuator_config.brushlessmotor[next_id] = motor;
        }
        ImGui::EndPopup();
    }
    if (motor_node_open) {
        for (auto it = actuator_config.brushlessmotor.begin(); it != actuator_config.brushlessmotor.end(); ) {
            auto& m = it->second;
            bool m_node_open = ImGui::TreeNode((void*)(intptr_t)m.id, "%s",
                m.name.empty() ? (std::string("Motor #") + std::to_string(m.id)).c_str() : m.name.c_str());
            bool delete_motor = false;
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete Motor")) delete_motor = true;
                ImGui::EndPopup();
            }
            if (m_node_open) {
                ImGui::InputInt("Internal ID", &m.id);
                char nameBuf[64] = {};
                strncpy(nameBuf, m.name.c_str(), sizeof(nameBuf) - 1);
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                    m.name = nameBuf;

                auto encNames = GetEncodingNames();

                // 辅助：InputFloat + 编码下拉（同行）
                auto EncodedInput = [&](const char* label, EncodedValue& ev, float step = 0.f) {
                    float fv = (float)ev.value;
                    ImGui::InputFloat(label, &fv); ev.value = fv;
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(110);
                    int idx = EncodingToIndex(ev.encoding);
                    char comboId[64]; snprintf(comboId, sizeof(comboId), "##enc_%s", label);
                    if (ImGui::Combo(comboId, &idx, encNames.data(), (int)encNames.size()))
                        ev.encoding = IndexToEncoding(idx);
                };

                // Thrust curve — single formatted string (np_ini, np_mid, pp_ini, pp_mid, nt_end, nt_mid, pt_mid, pt_end)
                {
                    char curveBuf[256] = {};
                    _snprintf_s(curveBuf, sizeof(curveBuf),
                        "%.3f, %.3f, %.3f, %.3f,  %.3f, %.3f, %.3f, %.3f",
                        m.curve.np_ini.value, m.curve.np_mid.value,
                        m.curve.pp_ini.value, m.curve.pp_mid.value,
                        m.curve.nt_end.value, m.curve.nt_mid.value,
                        m.curve.pt_mid.value, m.curve.pt_end.value);

                    ImGui::Text("Thrust Curve (np_ini,np_mid,pp_ini,pp_mid, nt_end,nt_mid,pt_mid,pt_end):");
                    ImGui::PushItemWidth(-120);
                    if (ImGui::InputText("##CurveStr", curveBuf, sizeof(curveBuf)))
                    {
                        double v[8] = {};
                        sscanf_s(curveBuf, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                            &v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7]);
                        m.curve.np_ini.value=v[0]; m.curve.np_mid.value=v[1];
                        m.curve.pp_ini.value=v[2]; m.curve.pp_mid.value=v[3];
                        m.curve.nt_end.value=v[4]; m.curve.nt_mid.value=v[5];
                        m.curve.pt_mid.value=v[6]; m.curve.pt_end.value=v[7];
                    }
                    ImGui::PopItemWidth();
                }

                EncodedInput("target_speed", m.target_speed);
                ImGui::TreePop();
            }
            if (delete_motor) it = actuator_config.brushlessmotor.erase(it);
            else ++it;
        }
        ImGui::TreePop();
    }

    ImGui::PopID();

    // 舵机编辑
    ImGui::PushID("ServosSection");
    bool servo_node_open = ImGui::TreeNode("Servos");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
    if (ImGui::SmallButton("+")) {
        int next_id = actuator_config.servo.empty() ? 0 : actuator_config.servo.rbegin()->first + 1;
        Servo servo;
        servo.id = next_id;
        actuator_config.servo[next_id] = servo;
    }
    if (ImGui::BeginPopupContextItem("ServoTreeCtx")) {
        if (ImGui::MenuItem("Add Servo")) {
            int next_id = actuator_config.servo.empty() ? 0 : actuator_config.servo.rbegin()->first + 1;
            Servo servo;
            servo.id = next_id;
            actuator_config.servo[next_id] = servo;
        }
        ImGui::EndPopup();
    }
    if (servo_node_open) {
        for (auto it = actuator_config.servo.begin(); it != actuator_config.servo.end(); ) {
            auto& s = it->second;
            bool s_node_open = ImGui::TreeNode((void*)(intptr_t)s.id, "%s",
                s.name.empty() ? (std::string("Servo #") + std::to_string(s.id)).c_str() : s.name.c_str());
            bool delete_servo = false;
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete Servo")) delete_servo = true;
                ImGui::EndPopup();
            }
            if (s_node_open) {
                ImGui::InputInt("Internal ID", &s.id);
                char nameBuf[64] = {};
                strncpy(nameBuf, s.name.c_str(), sizeof(nameBuf) - 1);
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                    s.name = nameBuf;

                auto encNames = GetEncodingNames();
                auto EncodedInput = [&](const char* label, EncodedValue& ev) {
                    float fv = (float)ev.value;
                    ImGui::InputFloat(label, &fv); ev.value = fv;
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(110);
                    char comboId[64]; snprintf(comboId, sizeof(comboId), "##enc_%s", label);
                    int idx = EncodingToIndex(ev.encoding);
                    if (ImGui::Combo(comboId, &idx, encNames.data(), (int)encNames.size()))
                        ev.encoding = IndexToEncoding(idx);
                };
                EncodedInput("Angle", s.angle);
                ImGui::TreePop();
            }
            if (delete_servo) it = actuator_config.servo.erase(it);
            else ++it;
        }
        ImGui::TreePop();
    }

    ImGui::PopID();

    // Motion 组件
    ImGui::PushID("MotionSection");
    bool motion_enabled = mode.actuator_config.has_motion;
    if (ImGui::Checkbox("Motion Control", &motion_enabled)) {
        mode.actuator_config.has_motion = motion_enabled;
    }
    if (mode.actuator_config.has_motion) {
        ImGui::Indent();
        auto& m = mode.actuator_config.motion;
        auto encNames = GetEncodingNames();
        auto EncodedInput = [&](const char* label, EncodedValue& ev) {
            float fv = (float)ev.value;
            ImGui::PushItemWidth(120);
            ImGui::InputFloat(label, &fv); ev.value = fv;
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110);
            char comboId[64]; snprintf(comboId, sizeof(comboId), "##enc_m_%s", label);
            int idx = EncodingToIndex(ev.encoding);
            if (ImGui::Combo(comboId, &idx, encNames.data(), (int)encNames.size()))
                ev.encoding = IndexToEncoding(idx);
        };
        EncodedInput("X",  m.x);
        EncodedInput("Y",  m.y);
        EncodedInput("Z",  m.z);
        EncodedInput("RX", m.rx);
        EncodedInput("RY", m.ry);
        EncodedInput("RZ", m.rz);
        ImGui::Unindent();
    }

    // Sensor 组件
    ImGui::Spacing();
    ImGui::Text("Sensors");
    ImGui::Separator();
    auto encNames = GetEncodingNames();
    ImGui::Checkbox("Temperature", &mode.has_temperature);
    ImGui::SameLine(200);
    ImGui::SetNextItemWidth(130);
    int tEnc = EncodingToIndex(mode.temp_encoding);
    if (ImGui::Combo("##tempen", &tEnc, encNames.data(), (int)encNames.size()))
        mode.temp_encoding = IndexToEncoding(tEnc);

    ImGui::Checkbox("Humidity",    &mode.has_humidity);
    ImGui::SameLine(200);
    ImGui::SetNextItemWidth(130);
    int hEnc = EncodingToIndex(mode.hum_encoding);
    if (ImGui::Combo("##humen", &hEnc, encNames.data(), (int)encNames.size()))
        mode.hum_encoding = IndexToEncoding(hEnc);

    ImGui::Checkbox("Depth",       &mode.has_depth);
    ImGui::SameLine(200);
    ImGui::SetNextItemWidth(130);
    int dEnc = EncodingToIndex(mode.depth_encoding);
    if (ImGui::Combo("##depenc", &dEnc, encNames.data(), (int)encNames.size()))
        mode.depth_encoding = IndexToEncoding(dEnc);

    ImGui::PopID();

    // ==================== 协议配置按钮 ====================
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Protocol Field Config...", ImVec2(-1, 0))) {
        m_ProtocolConfigOpen = true;
        m_ProtocolTabIndex = 0;
    }
    // 预览已移到 DrawProtocolConfigWindow 中
}

// ==================== 协议配置独立子窗口（在 OnUIRender 中调用） ====================
void Robot_Config::DrawProtocolConfigWindow() {
    if (!m_ProtocolConfigOpen) return;
    if (!m_IsEditing) return;

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Protocol Field Configuration", &m_ProtocolConfigOpen)) {
        ImGui::End();
        return;
    }

    auto& mode = m_EditingModes[m_EditingModeIndex];

    // 传输协议
    const char* protocols[] = { "UDP", "TCP", "Serial" };
    ImGui::Combo("Transport Protocol", &mode.protocol_type, protocols, IM_ARRAYSIZE(protocols));

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::BeginTabBar("ProtoTabs")) {
        if (ImGui::BeginTabItem("Send Fields")) {
            m_ProtocolTabIndex = 0;
            DrawProtocolFieldConfig(mode.protocol_send, mode.actuator_config, true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Receive Fields")) {
            m_ProtocolTabIndex = 1;
            DrawProtocolReceiveFieldConfig(mode.protocol_receive, mode.has_temperature, mode.has_humidity, mode.has_depth);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // 帧预览（根据当前标签页切换）
    ImGui::Spacing();
    ImGui::Separator();
    if (m_ProtocolTabIndex == 0) {
        // 发送帧预览
        if (ImGui::CollapsingHeader("Send Frame Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto preview = BuildFrame(mode.actuator_config, mode.protocol_send);
            std::string hexPreview;
            for (size_t i = 0; i < preview.size(); ++i) {
                char b[8];
                snprintf(b, sizeof(b), i ? " %02X" : "%02X", preview[i]);
                hexPreview += b;
            }
            ImGui::TextWrapped("Frame (%zu bytes): %s", preview.size(), hexPreview.c_str());
            for (const auto& f : mode.protocol_send.fields) {
                double val = 0;
                if (GetActuatorField(mode.actuator_config, f.field_path, val)) {
                    ImGui::Text("  %s (%s) = %.3f", f.name.c_str(), f.field_path.c_str(), val);
                } else {
                    ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  %s (%s) = NOT FOUND", f.name.c_str(), f.field_path.c_str());
                }
            }
        }
    } else {
        // 接收帧预览：用默认传感器数据构造帧
        if (ImGui::CollapsingHeader("Receive Frame Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            // 构建接收帧的预期格式（按 ReceiveField 顺序显示各字段占用字节）
            auto& fields = mode.protocol_receive.fields;
            int totalBytes = 0;
            for (const auto& f : fields)
                totalBytes += GetEncodingByteSize(f.encoding);

            ImGui::Text("Expected Payload: %d bytes (%zu fields)", totalBytes, fields.size());
            for (const auto& f : fields) {
                int sz = GetEncodingByteSize(f.encoding);
                ImGui::Text("  %s (%s)  [%s, %d bytes]", f.name.c_str(), f.field_path.c_str(),
                    GetEncodingNames()[EncodingToIndex(f.encoding)], sz);
            }
            if (!mode.protocol_receive.include_length)
                ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "Note: Payload length field is disabled.");
        }
    }

    ImGui::End();
}

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

// ==================== 发送协议字段配置 ====================
void Robot_Config::DrawProtocolFieldConfig(ProtocolSendConfig& cfg, ActuatorData& actuator, bool isSend) {
    auto& fields = cfg.fields;
    auto encodingNames = GetEncodingNames();

    // 帧格式
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

    // 字段列表标题
    ImGui::Text("Send Fields");
    ImGui::SameLine();
    if (ImGui::Button("+ Add Field")) {
        SendField f;
        f.name = "new_field";
        fields.push_back(f);
    }

    auto components = GetSendComponents(actuator, false, false, false);

    // 字段表格
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

                // 拖拽
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

                // 名称
                ImGui::TableSetColumnIndex(1);
                char nameBuf[128] = {};
                strncpy(nameBuf, f.name.c_str(), sizeof(nameBuf) - 1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
                    f.name = nameBuf;
                ImGui::PopItemWidth();

                // 解析当前 field_path
                std::string curCompId = ResolveComponentId(f.field_path);
                std::string curSub = ResolveSubField(f.field_path);

                // 级联路径下拉框：组件 > 子字段
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
                                    f.encoding = sf.encoding;
                                }
                                if (sel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndMenu();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(85);
                int encIdx = EncodingToIndex(f.encoding);
                if (ImGui::Combo("##sendenc", &encIdx, encodingNames.data(), (int)encodingNames.size()))
                    f.encoding = IndexToEncoding(encIdx);
                ImGui::PopItemWidth();

                // 类型（只读）
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(encodingNames[EncodingToIndex(f.encoding)]);

                // 分组
                ImGui::TableSetColumnIndex(4);
                char groupBuf[64] = {};
                strncpy(groupBuf, f.group.c_str(), sizeof(groupBuf) - 1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##sendgroup", groupBuf, sizeof(groupBuf)))
                    f.group = groupBuf;
                ImGui::PopItemWidth();

                // Visible
                ImGui::TableSetColumnIndex(5);
                ImGui::Checkbox("##sendvis", &f.visible);

                // Fix
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
void Robot_Config::DrawProtocolReceiveFieldConfig(ProtocolReceiveConfig& cfg, bool hasTemp, bool hasHum, bool hasDep) {
    auto& fields = cfg.fields;
    auto encodingNames = GetEncodingNames();
    auto components = GetRecvComponents(hasTemp, hasHum, hasDep);

    // 帧格式
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

                // 名称
                ImGui::TableSetColumnIndex(1);
                char nameBuf[128] = {};
                strncpy(nameBuf, f.name.c_str(), sizeof(nameBuf) - 1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
                    f.name = nameBuf;
                ImGui::PopItemWidth();

                std::string curCompId = ResolveComponentId(f.field_path);
                std::string curSub = ResolveSubField(f.field_path);

                // 级联路径下拉框：组件 > 子字段
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
                                    f.encoding = sf.encoding;
                                }
                                if (sel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndMenu();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(85);
                int encIdx = EncodingToIndex(f.encoding);
                if (ImGui::Combo("##recvenc", &encIdx, encodingNames.data(), (int)encodingNames.size()))
                    f.encoding = IndexToEncoding(encIdx);
                ImGui::PopItemWidth();

                // 类型（只读）
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(encodingNames[EncodingToIndex(f.encoding)]);

                // 分组
                ImGui::TableSetColumnIndex(4);
                char groupBuf[64] = {};
                strncpy(groupBuf, f.group.c_str(), sizeof(groupBuf) - 1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##recvgroup", groupBuf, sizeof(groupBuf)))
                    f.group = groupBuf;
                ImGui::PopItemWidth();

                // Visible
                ImGui::TableSetColumnIndex(5);
                ImGui::Checkbox("##recvvis", &f.visible);

                // Fix
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