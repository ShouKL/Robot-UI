#include "RobotComponent.h"
#include "Walnut/Core/Log.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cctype>

RobotComponent::RobotComponent() {
    modes.clear();
    active_mode_index = 0;
    WL_INFO_TAG("ROBOT", "RobotComponent created");
}

RobotComponent::~RobotComponent() {}

// ==================== 模式增删 ====================

void RobotComponent::AddMode() {
    RobotMode new_mode;
    int idx = 0;
    while (true) {
        snprintf(new_mode.name, sizeof(new_mode.name), "Mode %d", idx);
        bool exists = false;
        for (auto& m : modes)
            if (strcmp(m.name, new_mode.name) == 0) { exists = true; break; }
        if (!exists) break;
        ++idx;
    }
    modes.push_back(new_mode);
    edit_mode_index = (int)modes.size() - 1;
    WL_INFO_TAG("ROBOT", "Mode added: {}", modes.back().name);
}

void RobotComponent::DeleteMode(int index) {
    if (modes.empty()) return;
    if (index < 0 || index >= (int)modes.size()) return;
    modes.erase(modes.begin() + index);
    if (edit_mode_index >= (int)modes.size())
        edit_mode_index = std::max(0, (int)modes.size() - 1);
}

// ==================== UI 绘制（马达/舵机/Motion/Sensor） ====================

void RobotComponent::DrawRobotConfigPanel() {
    if (modes.empty()) return;
    auto& mode = modes[edit_mode_index];

    // ---------- 名称 ----------
    ImGui::InputText("Name", mode.name, sizeof(mode.name));

    auto& actuator_config = mode.actuator_config;

    // 马达编辑
    ImGui::PushID("MotorsSection");
    bool motor_node_open = ImGui::TreeNode("Brushless Motors");
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
                m.name.empty() ? (std::string("Motor") + std::to_string(m.id)).c_str() : m.name.c_str());
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

                // Thrust curve
                {
                    char curveBuf[256] = {};
                    _snprintf_s(curveBuf, sizeof(curveBuf),
                        "%.3f, %.3f, %.3f, %.3f,  %.3f, %.3f, %.3f, %.3f,  %.3f, %.3f",
                        m.curve.np_ini.value, m.curve.np_mid.value,
                        m.curve.pp_ini.value, m.curve.pp_mid.value,
                        m.curve.nt_end.value, m.curve.nt_mid.value,
                        m.curve.pt_mid.value, m.curve.pt_end.value,
                        m.curve.pwm_min, m.curve.pwm_max);

                    ImGui::Text("Thrust Curve (np_ini,np_mid,pp_ini,pp_mid, nt_end,nt_mid,pt_mid,pt_end, pwm_min,pwm_max):");
                    ImGui::PushItemWidth(-120);
                    if (ImGui::InputText("##CurveStr", curveBuf, sizeof(curveBuf)))
                    {
                        double v[10] = {};
                        int parsed = sscanf_s(curveBuf, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                            &v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&v[8],&v[9]);
                        if (parsed == 10) {
                            m.curve.np_ini.value=v[0]; m.curve.np_mid.value=v[1];
                            m.curve.pp_ini.value=v[2]; m.curve.pp_mid.value=v[3];
                            m.curve.nt_end.value=v[4]; m.curve.nt_mid.value=v[5];
                            m.curve.pt_mid.value=v[6]; m.curve.pt_end.value=v[7];
                            m.curve.pwm_min=(float)v[8]; m.curve.pwm_max=(float)v[9];
                        }
                    }
                    ImGui::PopItemWidth();
                }

                float fv = (float)m.target_speed.value;
                ImGui::InputFloat("target_speed", &fv); m.target_speed.value = fv;
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

                float fv = (float)s.angle.value;
                ImGui::InputFloat("Angle", &fv); s.angle.value = fv;
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
        {
            float fv;
            fv = (float)m.x.value;  ImGui::InputFloat("X",  &fv); m.x.value = fv;
            fv = (float)m.y.value;  ImGui::InputFloat("Y",  &fv); m.y.value = fv;
            fv = (float)m.z.value;  ImGui::InputFloat("Z",  &fv); m.z.value = fv;
            fv = (float)m.rx.value; ImGui::InputFloat("RX", &fv); m.rx.value = fv;
            fv = (float)m.ry.value; ImGui::InputFloat("RY", &fv); m.ry.value = fv;
            fv = (float)m.rz.value; ImGui::InputFloat("RZ", &fv); m.rz.value = fv;
        }
        ImGui::Unindent();
    }

    // Sensor 组件
    ImGui::Spacing();
    ImGui::Text("Sensors");
    ImGui::Separator();
    ImGui::Checkbox("Temperature", &mode.sensor_config.has_temperature);
    ImGui::Checkbox("Humidity",    &mode.sensor_config.has_humidity);
    ImGui::Checkbox("Depth",       &mode.sensor_config.has_depth);
    ImGui::PopID();

    // ==================== 底部分隔线 ====================
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
}
