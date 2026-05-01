#include "Robot_Config.h"

Robot_Config::Robot_Config() {
    for (int i = 0; i < 6; i++) {
        BrushlessMotor motor;
        motor.id = i;
        actuator_config.brushlessmotor[i] = motor;
    }
    Servo servo;
    servo.id = 0;
    actuator_config.servo[0] = servo;

    ApplyConfig();
}

Robot_Config::~Robot_Config() {
}

bool Robot_Config::DrawRobotConfigPanel() {
    bool changed = false;

    ImGui::Text("Network Configuration");
    ImGui::Separator();

    char ip_buf[128];
    strncpy(ip_buf, host_ip.c_str(), sizeof(ip_buf));
    if (ImGui::InputText("Host IP", ip_buf, sizeof(ip_buf))) {
        host_ip = ip_buf;
        changed = true;
    }
    if (ImGui::InputInt("Remote Port", &remote_port)) changed = true;
    if (ImGui::InputInt("Local Port", &local_port)) changed = true;

    ImGui::Spacing();
    ImGui::Text("Sensors");
    ImGui::Separator();
    if (ImGui::BeginTable("SensorsTable", 3)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Checkbox("Temperature", &has_temperature)) changed = true;
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Checkbox("Humidity", &has_humidity)) changed = true;
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Checkbox("Depth", &has_depth)) changed = true;
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Text("Actuators");
    ImGui::Separator();

    bool motor_node_open = ImGui::TreeNode("Brushless Motors");
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Add Motor")) {
            int next_id = actuator_config.brushlessmotor.empty() ? 0 : actuator_config.brushlessmotor.rbegin()->first + 1;
            BrushlessMotor motor;
            motor.id = next_id;
            actuator_config.brushlessmotor[next_id] = motor;
            changed = true;
        }
        ImGui::EndPopup();
    }

    if (motor_node_open) {
        for (auto it = actuator_config.brushlessmotor.begin(); it != actuator_config.brushlessmotor.end(); ) {
            auto& m = it->second;
            bool m_node_open = ImGui::TreeNode((void*)(intptr_t)m.id, "Motor ID: %d", m.id);

            bool delete_motor = false;
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete Motor")) {
                    delete_motor = true;
                    changed = true;
                }
                ImGui::EndPopup();
            }

            if (m_node_open) {
                if (ImGui::InputInt("Internal ID", &m.id)) changed = true;
                ImGui::Text("Thrust Curve:");
                float np_mid = (float)m.curve.np_mid;
                float np_ini = (float)m.curve.np_ini;
                float pp_ini = (float)m.curve.pp_ini;
                float pp_mid = (float)m.curve.pp_mid;
                float nt_end = (float)m.curve.nt_end;
                float nt_mid = (float)m.curve.nt_mid;
                float pt_mid = (float)m.curve.pt_mid;
                float pt_end = (float)m.curve.pt_end;

                if (ImGui::DragFloat("np_mid", &np_mid, 0.01f)) { m.curve.np_mid = np_mid; changed = true; }
                if (ImGui::DragFloat("np_ini", &np_ini, 0.01f)) { m.curve.np_ini = np_ini; changed = true; }
                if (ImGui::DragFloat("pp_ini", &pp_ini, 0.01f)) { m.curve.pp_ini = pp_ini; changed = true; }
                if (ImGui::DragFloat("pp_mid", &pp_mid, 0.01f)) { m.curve.pp_mid = pp_mid; changed = true; }
                if (ImGui::DragFloat("nt_end", &nt_end, 0.01f)) { m.curve.nt_end = nt_end; changed = true; }
                if (ImGui::DragFloat("nt_mid", &nt_mid, 0.01f)) { m.curve.nt_mid = nt_mid; changed = true; }
                if (ImGui::DragFloat("pt_mid", &pt_mid, 0.01f)) { m.curve.pt_mid = pt_mid; changed = true; }
                if (ImGui::DragFloat("pt_end", &pt_end, 0.01f)) { m.curve.pt_end = pt_end; changed = true; }
                ImGui::TreePop();
            }

            if (delete_motor) {
                it = actuator_config.brushlessmotor.erase(it);
            } else {
                ++it;
            }
        }
        ImGui::TreePop();
    }

    bool servo_node_open = ImGui::TreeNode("Servos");
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Add Servo")) {
            int next_id = actuator_config.servo.empty() ? 0 : actuator_config.servo.rbegin()->first + 1;
            Servo servo;
            servo.id = next_id;
            actuator_config.servo[next_id] = servo;
            changed = true;
        }
        ImGui::EndPopup();
    }

    if (servo_node_open) {
        for (auto it = actuator_config.servo.begin(); it != actuator_config.servo.end(); ) {
            auto& s = it->second;
            bool s_node_open = ImGui::TreeNode((void*)(intptr_t)s.id, "Servo ID: %d", s.id);

            bool delete_servo = false;
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete Servo")) {
                    delete_servo = true;
                    changed = true;
                }
                ImGui::EndPopup();
            }

            if (s_node_open) {
                if (ImGui::InputInt("Internal ID", &s.id)) changed = true;
                ImGui::Text("Initial/Current Angle:");
                float angle = (float)s.angle;
                if (ImGui::DragFloat("Angle", &angle, 1.0f)) { s.angle = angle; changed = true; }
                ImGui::TreePop();
            }

            if (delete_servo) {
                it = actuator_config.servo.erase(it);
            } else {
                ++it;
            }
        }
        ImGui::TreePop();
    }

    return changed;
}

bool Robot_Config::ApplyConfig() {
    bool networkChanged = (active_host_ip != host_ip) || (active_remote_port != remote_port) || (active_local_port != local_port);

    active_host_ip = host_ip;
    active_remote_port = remote_port;
    active_local_port = local_port;
    active_has_temperature = has_temperature;
    active_has_humidity = has_humidity;
    active_has_depth = has_depth;
    active_actuator_config = actuator_config;

    return networkChanged;
}

void Robot_Config::RevertConfig() {
    host_ip = active_host_ip;
    remote_port = active_remote_port;
    local_port = active_local_port;
    has_temperature = active_has_temperature;
    has_humidity = active_has_humidity;
    has_depth = active_has_depth;
    actuator_config = active_actuator_config;
}