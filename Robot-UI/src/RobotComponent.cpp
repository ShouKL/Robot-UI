#include "RobotComponentManager.h"
#include "RobotComponent.h"

void RobotComponentManager::DrawConfigPanel(RobotComponent& comp) {
    auto& actuator_config = comp.actuator_config;
    auto& sensor_config   = comp.sensor_config;

    // ==================== Actuator ====================
    ImGui::Text("Actuator");
    ImGui::Separator();

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
        int motorDelIdx = -1, motorRenameIdx = -1;
        static int s_RenamingMotorIdx = -1;
        static int s_LastRenamingMotorIdx = -1;
        static char s_MotorRenameBuf[64];

        for (int mi = 0; mi < (int)actuator_config.brushlessmotor.size(); ++mi) {
            auto& m = actuator_config.brushlessmotor[mi];
            ImGui::PushID(mi);

            bool renaming = (s_RenamingMotorIdx == mi);
            if (renaming) {
                if (s_LastRenamingMotorIdx != mi) {
                    strncpy_s(s_MotorRenameBuf, m.name.c_str(), sizeof(s_MotorRenameBuf) - 1);
                    s_LastRenamingMotorIdx = mi;
                }
                ImGui::TreeNodeEx("##MotorRnDummy", ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowItemOverlap);
                ImGui::SameLine(0, 0);
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##MotorRn", s_MotorRenameBuf, sizeof(s_MotorRenameBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                    m.name = s_MotorRenameBuf;
                    s_RenamingMotorIdx = -1;
                }
                if (ImGui::IsItemDeactivated() && !ImGui::IsItemActive())
                    s_RenamingMotorIdx = -1;
            } else {
                std::string fallbackLabel = "Motor " + std::to_string(m.id);
                const char* displayLabel = m.name.empty() ? fallbackLabel.c_str() : m.name.c_str();

                bool m_node_open = ImGui::TreeNode((void*)(intptr_t)(mi + 1), "%s", displayLabel);
                if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    motorRenameIdx = mi;
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) motorRenameIdx = mi;
                    if (ImGui::MenuItem("Delete Motor")) motorDelIdx = mi;
                    ImGui::EndPopup();
                }
                if (m_node_open) {
                    ImGui::InputInt("Internal ID", &m.id);
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
                    float fp = (float)m.target_position.value;
                    ImGui::InputFloat("target_position", &fp); m.target_position.value = fp;
                    ImGui::TreePop();
                }
            }

            ImGui::PopID();
        }
        if (motorDelIdx >= 0 && motorDelIdx < (int)actuator_config.brushlessmotor.size()) {
            actuator_config.brushlessmotor.erase(actuator_config.brushlessmotor.begin() + motorDelIdx);
            if (s_RenamingMotorIdx == motorDelIdx) s_RenamingMotorIdx = -1;
            else if (s_RenamingMotorIdx > motorDelIdx) s_RenamingMotorIdx--;
        }
        if (motorRenameIdx >= 0) s_RenamingMotorIdx = motorRenameIdx;
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
        int servoDelIdx = -1, servoRenameIdx = -1;
        static int s_RenamingServoIdx = -1;
        static int s_LastRenamingServoIdx = -1;
        static char s_ServoRenameBuf[64];

        for (int si = 0; si < (int)actuator_config.servo.size(); ++si) {
            auto& s = actuator_config.servo[si];
            ImGui::PushID(si);

            bool renaming = (s_RenamingServoIdx == si);
            if (renaming) {
                if (s_LastRenamingServoIdx != si) {
                    strncpy_s(s_ServoRenameBuf, s.name.c_str(), sizeof(s_ServoRenameBuf) - 1);
                    s_LastRenamingServoIdx = si;
                }
                ImGui::TreeNodeEx("##ServoRnDummy", ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowItemOverlap);
                ImGui::SameLine(0, 0);
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##ServoRn", s_ServoRenameBuf, sizeof(s_ServoRenameBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                    s.name = s_ServoRenameBuf;
                    s_RenamingServoIdx = -1;
                }
                if (ImGui::IsItemDeactivated() && !ImGui::IsItemActive())
                    s_RenamingServoIdx = -1;
            } else {
                std::string fallbackLabel = "Servo #" + std::to_string(s.id);
                const char* displayLabel = s.name.empty() ? fallbackLabel.c_str() : s.name.c_str();

                bool s_node_open = ImGui::TreeNode((void*)(intptr_t)(si + 1000), "%s", displayLabel);
                if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    servoRenameIdx = si;
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) servoRenameIdx = si;
                    if (ImGui::MenuItem("Delete Servo")) servoDelIdx = si;
                    ImGui::EndPopup();
                }
                if (s_node_open) {
                    ImGui::InputInt("Internal ID", &s.id);
                    float fv = (float)s.angle.value;
                    ImGui::InputFloat("Angle", &fv); s.angle.value = fv;
                    ImGui::TreePop();
                }
            }

            ImGui::PopID();
        }
        if (servoDelIdx >= 0 && servoDelIdx < (int)actuator_config.servo.size()) {
            actuator_config.servo.erase(actuator_config.servo.begin() + servoDelIdx);
            if (s_RenamingServoIdx == servoDelIdx) s_RenamingServoIdx = -1;
            else if (s_RenamingServoIdx > servoDelIdx) s_RenamingServoIdx--;
        }
        if (servoRenameIdx >= 0) s_RenamingServoIdx = servoRenameIdx;
        ImGui::TreePop();
    }
    ImGui::PopID();

    // ---- Motion ----
    ImGui::PushID("MotionSection");
    auto& motions = actuator_config.motions;
    bool motion_node_open = ImGui::TreeNode("Motion Controls");
    if (ImGui::BeginPopupContextItem("MotionTreeCtx")) {
        if (ImGui::MenuItem("Add Motion")) {
            MotionControl mc;
            motions.push_back(mc);
        }
        ImGui::EndPopup();
    }
    if (motion_node_open) {
        int motionDelIdx = -1, motionRenameIdx = -1;
        static int s_RenamingMotionIdx = -1;
        static int s_LastRenamingMotionIdx = -1;
        static char s_MotionRenameBuf[64];

        for (int mi = 0; mi < (int)motions.size(); ++mi) {
            auto& m = motions[mi];
            ImGui::PushID(3000 + mi);

            bool renaming = (s_RenamingMotionIdx == mi);
            if (renaming) {
                if (s_LastRenamingMotionIdx != mi) {
                    strncpy_s(s_MotionRenameBuf, m.name.c_str(), sizeof(s_MotionRenameBuf) - 1);
                    s_LastRenamingMotionIdx = mi;
                }
                ImGui::TreeNodeEx("##MotionRnDummy", ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowItemOverlap);
                ImGui::SameLine(0, 0);
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##MotionRn", s_MotionRenameBuf, sizeof(s_MotionRenameBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                    m.name = s_MotionRenameBuf;
                    s_RenamingMotionIdx = -1;
                }
                // Click away to cancel
                if (ImGui::IsItemDeactivated() && !ImGui::IsItemActive())
                    s_RenamingMotionIdx = -1;
            } else {
                std::string fallbackLabel = "Motion #" + std::to_string(mi);
                const char* displayLabel = m.name.empty() ? fallbackLabel.c_str() : m.name.c_str();

                bool mNodeOpen = ImGui::TreeNode((void*)(intptr_t)(3000 + mi), "%s", displayLabel);
                if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    motionRenameIdx = mi;
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) motionRenameIdx = mi;
                    if (ImGui::MenuItem("Delete Motion")) motionDelIdx = mi;
                    ImGui::EndPopup();
                }
                if (mNodeOpen) {
                    float fv;
                    fv = (float)m.x.value;  ImGui::InputFloat("X",  &fv); m.x.value = fv;
                    fv = (float)m.y.value;  ImGui::InputFloat("Y",  &fv); m.y.value = fv;
                    fv = (float)m.z.value;  ImGui::InputFloat("Z",  &fv); m.z.value = fv;
                    fv = (float)m.rx.value; ImGui::InputFloat("RX", &fv); m.rx.value = fv;
                    fv = (float)m.ry.value; ImGui::InputFloat("RY", &fv); m.ry.value = fv;
                    fv = (float)m.rz.value; ImGui::InputFloat("RZ", &fv); m.rz.value = fv;
                    ImGui::TreePop();
                }
            }

            ImGui::PopID();
        }
        if (motionDelIdx >= 0 && motionDelIdx < (int)motions.size()) {
            motions.erase(motions.begin() + motionDelIdx);
            if (s_RenamingMotionIdx == motionDelIdx) s_RenamingMotionIdx = -1;
            else if (s_RenamingMotionIdx > motionDelIdx) s_RenamingMotionIdx--;
        }
        if (motionRenameIdx >= 0) s_RenamingMotionIdx = motionRenameIdx;
        ImGui::TreePop();
    }
    ImGui::PopID();

    // ---- GPIO ----
    ImGui::PushID("GpioSection");
    auto& gpio_pins = actuator_config.gpio_pins;
    bool gpio_node_open = ImGui::TreeNode("GPIO");
    if (ImGui::BeginPopupContextItem("GpioTreeCtx")) {
        if (ImGui::MenuItem("Add GPIO")) {
            int next_id = gpio_pins.empty() ? 0 : gpio_pins.back().id + 1;
            GpioPin pin;
            pin.id = next_id;
            gpio_pins.push_back(pin);
        }
        ImGui::EndPopup();
    }
    if (gpio_node_open) {
        int gpioDelIdx = -1, gpioRenameIdx = -1;
        static int s_RenamingGpioIdx = -1;
        static int s_LastRenamingGpioIdx = -1;
        static char s_GpioRenameBuf[64];

        for (int i = 0; i < (int)gpio_pins.size(); ++i) {
            auto& pin = gpio_pins[i];
            ImGui::PushID(2000 + i);

            bool renaming = (s_RenamingGpioIdx == i);
            if (renaming) {
                if (s_LastRenamingGpioIdx != i) {
                    strncpy_s(s_GpioRenameBuf, pin.name.c_str(), sizeof(s_GpioRenameBuf) - 1);
                    s_LastRenamingGpioIdx = i;
                }
                ImGui::TreeNodeEx("##GpioRnDummy", ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowItemOverlap);
                ImGui::SameLine(0, 0);
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##GpioRn", s_GpioRenameBuf, sizeof(s_GpioRenameBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                    pin.name = s_GpioRenameBuf;
                    s_RenamingGpioIdx = -1;
                }
                if (ImGui::IsItemDeactivated() && !ImGui::IsItemActive())
                    s_RenamingGpioIdx = -1;
            } else {
                std::string fallbackLabel = "GPIO #" + std::to_string(pin.id);
                const char* displayLabel = pin.name.empty() ? fallbackLabel.c_str() : pin.name.c_str();

                bool pinNodeOpen = ImGui::TreeNode((void*)(intptr_t)(2000 + i), "%s", displayLabel);
                if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    gpioRenameIdx = i;
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) gpioRenameIdx = i;
                    if (ImGui::MenuItem("Delete GPIO")) gpioDelIdx = i;
                    ImGui::EndPopup();
                }
                if (pinNodeOpen) {
                    ImGui::InputInt("ID", &pin.id);
                    ImGui::InputInt("Pin Number", &pin.pin_number);

                    bool bv = (pin.value != 0);
                    if (ImGui::Checkbox("Value", &bv))
                        pin.value = bv ? 1 : 0;

                    ImGui::TreePop();
                }
            }

            ImGui::PopID();
        }
        if (gpioDelIdx >= 0 && gpioDelIdx < (int)gpio_pins.size()) {
            gpio_pins.erase(gpio_pins.begin() + gpioDelIdx);
            if (s_RenamingGpioIdx == gpioDelIdx) s_RenamingGpioIdx = -1;
            else if (s_RenamingGpioIdx > gpioDelIdx) s_RenamingGpioIdx--;
        }
        if (gpioRenameIdx >= 0) s_RenamingGpioIdx = gpioRenameIdx;
        ImGui::TreePop();
    }
    ImGui::PopID();

    // ---- Sensors ----
    ImGui::Spacing();
    ImGui::Text("Sensors");
    ImGui::Separator();
    ImGui::Checkbox("Temperature", &comp.sensor_config.has_temperature);
    ImGui::Checkbox("Humidity",    &comp.sensor_config.has_humidity);
    ImGui::Checkbox("Depth",       &comp.sensor_config.has_depth);

    ImGui::Spacing(); ImGui::Separator();
}


