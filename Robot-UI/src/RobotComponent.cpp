#include "RobotComponent.h"
#include "Walnut/Core/Log.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cctype>

void RobotComponent::DrawConfigPanel() {
    auto& actuator_config = this->actuator_config;

    // ---- 马达编辑 ----
    ImGui::PushID("MotorsSection");
    bool motor_node_open = ImGui::TreeNode("Brushless Motors");
    if (ImGui::BeginPopupContextItem("MotorTreeCtx")) {
        if (ImGui::MenuItem("Add Motor")) {
            int next_id = actuator_config.brushlessmotor.empty() ? 0 : actuator_config.brushlessmotor.back().id + 1;
            BrushlessMotor motor;
            motor.id = next_id;
            actuator_config.brushlessmotor.push_back(motor);
        }
        ImGui::EndPopup();
    }
    if (motor_node_open) {
        int motorIdx = 0;
        for (auto it = actuator_config.brushlessmotor.begin(); it != actuator_config.brushlessmotor.end(); ++motorIdx) {
            auto& m = *it;
            ImGui::PushID(motorIdx);
            bool m_node_open = ImGui::TreeNode((void*)(intptr_t)(motorIdx + 1), "%s",
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
                    if (ImGui::InputText("##CurveStr", curveBuf, sizeof(curveBuf))) {
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
            ImGui::PopID();
            if (delete_motor) it = actuator_config.brushlessmotor.erase(it);
            else ++it;
        }
        ImGui::TreePop();
    }
    ImGui::PopID();

    // ---- 舵机编辑 ----
    ImGui::PushID("ServosSection");
    bool servo_node_open = ImGui::TreeNode("Servos");
    if (ImGui::BeginPopupContextItem("ServoTreeCtx")) {
        if (ImGui::MenuItem("Add Servo")) {
            int next_id = actuator_config.servo.empty() ? 0 : actuator_config.servo.back().id + 1;
            Servo sv;
            sv.id = next_id;
            actuator_config.servo.push_back(sv);
        }
        ImGui::EndPopup();
    }
    if (servo_node_open) {
        int servoIdx = 0;
        for (auto it = actuator_config.servo.begin(); it != actuator_config.servo.end(); ++servoIdx) {
            auto& s = *it;
            ImGui::PushID(servoIdx);
            bool s_node_open = ImGui::TreeNode((void*)(intptr_t)(servoIdx + 1000), "%s",
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
            ImGui::PopID();
            if (delete_servo) it = actuator_config.servo.erase(it);
            else ++it;
        }
        ImGui::TreePop();
    }
    ImGui::PopID();

    // ---- Motion ----
    ImGui::PushID("MotionSection");
    bool motion_enabled = actuator_config.has_motion;
    if (ImGui::Checkbox("Motion Control", &motion_enabled)) {
        actuator_config.has_motion = motion_enabled;
    }
    if (actuator_config.has_motion) {
        ImGui::Indent();
        auto& m = actuator_config.motion;
        float fv;
        fv = (float)m.x.value;  ImGui::InputFloat("X",  &fv); m.x.value = fv;
        fv = (float)m.y.value;  ImGui::InputFloat("Y",  &fv); m.y.value = fv;
        fv = (float)m.z.value;  ImGui::InputFloat("Z",  &fv); m.z.value = fv;
        fv = (float)m.rx.value; ImGui::InputFloat("RX", &fv); m.rx.value = fv;
        fv = (float)m.ry.value; ImGui::InputFloat("RY", &fv); m.ry.value = fv;
        fv = (float)m.rz.value; ImGui::InputFloat("RZ", &fv); m.rz.value = fv;
        ImGui::Unindent();
    }
    ImGui::PopID();

    // ---- Sensors ----
    ImGui::Spacing();
    ImGui::Text("Sensors");
    ImGui::Separator();
    ImGui::Checkbox("Temperature", &sensor_config.has_temperature);
    ImGui::Checkbox("Humidity",    &sensor_config.has_humidity);
    ImGui::Checkbox("Depth",       &sensor_config.has_depth);

    ImGui::Spacing(); ImGui::Separator();
}

